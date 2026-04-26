#include "anomaly.h"
#include "character.h"

void anomaly_init(AnomalyState* s) { memset(s, 0, sizeof(*s)); }

void anomaly_reset(AnomalyState* s) {
    s->gauge = 30;            /* start at 30 — characters can shift */
    s->is_active = false;
    s->active_index = 0;
    s->since_active_ms = 0;
}

void anomaly_on_judge(AnomalyState* s, JudgeResult r, CharacterState* character) {
    int delta = 0;
    switch(r) {
        case JudgePerfect: delta = +3; break;
        case JudgeGreat:   delta = +2; break;
        case JudgeGood:    delta = +1; break;
        case JudgeMiss:    delta = -6; break;
        default: break;
    }
    if(character) {
        const CharacterDef* def = character_def(character->id);
        if(delta > 0) delta = (delta * def->gauge_gain_x100) / 100;
        else          delta = (delta * def->gauge_loss_x100) / 100;
    }
    s->gauge += delta;
    if(s->gauge < 0)   s->gauge = 0;
    if(s->gauge > 100) s->gauge = 100;
}

void anomaly_tick(AnomalyState* s, const Chart* chart, uint32_t time_ms,
                  AnomalyMod* out_mod, uint8_t* out_arg, uint32_t* out_flash_until) {
    *out_mod = AnomalyModNone;
    *out_arg = 0;

    uint32_t count = 0;
    const AnomalyEntry* a = chart_anomalies(chart, &count);
    bool fired = false;
    for(uint32_t i = 0; i < count; i++) {
        if(time_ms < a[i].start_ms || time_ms > a[i].end_ms) continue;
        if(s->gauge < a[i].threshold) continue;
        *out_mod = a[i].mod;
        *out_arg = (uint8_t)a[i].arg;
        if(!s->is_active || s->active_index != i) {
            s->is_active = true;
            s->active_index = (uint8_t)i;
            s->since_active_ms = time_ms;
            *out_flash_until = furi_get_tick() + 200; /* visual ping */
        }
        fired = true;
        break;
    }
    if(!fired) s->is_active = false;

    /* Cosmetic events (BG/FX/TXT) are returned via chart_events; the VGM
     * renderer iterates them itself. We don't need to track here. */
    UNUSED(out_flash_until);
}

const char* anomaly_mod_name(AnomalyMod m) {
    switch(m) {
        case AnomalyModSpeed:  return "SPEED";
        case AnomalyModMirror: return "MIRROR";
        case AnomalyModInvert: return "INVERT";
        case AnomalyModChaos:  return "CHAOS";
        case AnomalyModAdd:    return "ADD";
        case AnomalyModGhost:  return "GHOST";
        case AnomalyModStorm:  return "STORM";
        default: return "";
    }
}
