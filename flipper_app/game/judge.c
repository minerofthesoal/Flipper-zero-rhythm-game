#include "judge.h"
#include "anomaly.h"
#include "character.h"
#include <stdlib.h>

static int abs_int(int v) { return v < 0 ? -v : v; }

void judge_reset(Judge* j) {
    memset(j, 0, sizeof(*j));
}

static void award(Judge* j, JudgeResult r) {
    int pts = 0;
    switch(r) {
        case JudgePerfect: pts = 1000; j->perfects++; j->combo++; break;
        case JudgeGreat:   pts = 700;  j->greats++;   j->combo++; break;
        case JudgeGood:    pts = 300;  j->goods++;    j->combo++; break;
        /* Early/Late: forgiving — keep the combo alive and award a small
         * score, but visibly less than a Good so the player still feels
         * the timing slip. */
        case JudgeEarly:   pts = 100;  j->earlies++;  j->combo++; break;
        case JudgeLate:    pts = 100;  j->lates++;    j->combo++; break;
        case JudgeMiss:    pts = 0;    j->misses++;   j->combo = 0; break;
        default: break;
    }
    j->raw_score += pts;
    j->raw_max   += 1000;
    if(j->combo > j->max_combo) j->max_combo = j->combo;
    if(j->raw_max > 0) j->score = (j->raw_score * 1000000u) / j->raw_max;
}

JudgeResult judge_press(Judge* j, const ChartDiff* d, uint8_t lane,
                        uint32_t now_ms, uint32_t great_ms, uint32_t good_ms,
                        uint32_t lenient_ms,
                        AnomalyMod active_mod) {
    /* Find earliest unhit note in this lane within +/- lenient_ms */
    int best = -1;
    int best_dt = (int)lenient_ms + 1;
    uint32_t i = j->cursor;
    /* lane is RAW from input; under MIRROR/INVERT we need to compare to the
     * displayed lane (which is what the player sees). Apply same transform to
     * the chart note for matching. */
    for(; i < d->note_count; i++) {
        const Note* n = &d->notes[i];
        if(n->hit) continue;
        int dt = (int)n->time_ms - (int)now_ms;
        if(dt > (int)lenient_ms) break;
        if(dt < -(int)lenient_ms) continue;

        uint8_t display_lane = n->lane;
        if(active_mod == AnomalyModMirror) display_lane = 3 - display_lane;
        if(active_mod == AnomalyModInvert) display_lane ^= 1;

        if(display_lane != lane) continue;

        /* Fake notes are negative — pressing them = penalty + no consume */
        if(n->type == NoteFake) {
            j->fakes_hit++;
            j->raw_score = j->raw_score > 200 ? j->raw_score - 200 : 0;
            j->combo = 0;
            return JudgeMiss;
        }

        if(abs_int(dt) < abs_int(best_dt)) { best_dt = dt; best = (int)i; }
    }
    if(best < 0) return JudgeNone;
    Note* n = (Note*)&d->notes[best];
    int adt = abs_int(best_dt);
    JudgeResult r;
    if(adt <= 30)               r = JudgePerfect;
    else if(adt <= (int)great_ms) r = JudgeGreat;
    else if(adt <= (int)good_ms)  r = JudgeGood;
    else                        r = (best_dt > 0) ? JudgeEarly : JudgeLate;

    if(n->type == NoteHold) {
        n->hit = 3; /* 3 = held; release will finalise */
    } else if(n->type == NoteBurst) {
        if(n->burst_left > 0) n->burst_left--;
        if(n->burst_left == 0) n->hit = 1;
        else { /* partial burst credit, don't consume */
            j->last_hit_type = n->type;
            return JudgeGood;
        }
    } else if(n->type == NoteSlide) {
        n->hit = 1; /* simplified: any hit on either lane completes */
    } else {
        n->hit = 1;
    }
    j->last_hit_type = n->type;
    award(j, r);
    return r;
}

void judge_release(Judge* j, const ChartDiff* d, uint8_t lane,
                   uint32_t now_ms, uint32_t great_ms, uint32_t good_ms) {
    UNUSED(great_ms); UNUSED(good_ms); UNUSED(lane);
    /* Find a hold currently being held in this lane */
    for(uint32_t i = j->cursor; i < d->note_count; i++) {
        Note* n = (Note*)&d->notes[i];
        if(n->hit != 3) continue;
        uint32_t end = n->time_ms + n->arg;
        int dt = (int)end - (int)now_ms;
        JudgeResult r;
        if(abs_int(dt) <= 50)        r = JudgePerfect;
        else if(abs_int(dt) <= 120)  r = JudgeGreat;
        else if(dt < -150)           r = JudgeMiss; /* released too late */
        else                         r = JudgeGood;
        n->hit = 1;
        award(j, r);
        break;
    }
}

uint32_t judge_advance(Judge* j, const ChartDiff* d, uint32_t now_ms,
                       uint32_t miss_ms, AnomalyState* anomaly,
                       CharacterState* character) {
    uint32_t expired = 0;
    while(j->cursor < d->note_count) {
        Note* n = (Note*)&d->notes[j->cursor];
        if(n->hit == 1 || n->hit == 2) { j->cursor++; continue; }
        if(n->hit == 3) {
            /* still held; auto-resolve when end is far past */
            if(n->time_ms + n->arg + miss_ms < now_ms) {
                n->hit = 1; award(j, JudgeGood);
            }
            break;
        }
        /* Pending */
        if(n->time_ms + miss_ms < now_ms) {
            if(n->type == NoteFake || n->type == NoteChain) {
                /* fake/chain: ignore on no-press */
            } else {
                n->hit = 2;
                award(j, JudgeMiss);
                anomaly_on_judge(anomaly, JudgeMiss, character);
                expired++;
            }
            j->cursor++;
            continue;
        }
        break;
    }
    return expired;
}

bool judge_should_fail(const Judge* j, const CharacterState* character) {
    /* Fail if we've taken way more misses than the song deserves and gauge
     * (tracked by anomaly state) drops to zero combined with low score. The
     * anomaly module computes a separate failure pressure, but we keep a
     * minimal rule here so judge can stand alone. */
    if(j->failed) return true;
    if(character && character->id != CharacterNull) {
        /* characters can disable hard fail */
        const CharacterDef* def = character_def(character->id);
        if(def->no_fail) return false;
    }
    if(j->raw_max < 12000) return false;       /* don't fail in the first ~12 notes */
    /* if score% drops under 35%, fail */
    return j->score < 350000;
}

void judge_finalise(Judge* j) {
    /* nothing extra for now */
    UNUSED(j);
}
