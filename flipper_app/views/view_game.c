#include "view_game.h"
#include "../pulse_app.h"
#include "../scenes/scenes.h"

#include <gui/elements.h>

/* ------- model ----------------------------------------------------------- */

#define LANE_COUNT     4
#define RECEPTOR_Y     54
#define LANE_W         16
#define LANE_X0        32      /* 4 lanes × 16 = 64, centered: 32..96 */
#define HIT_WINDOW_GREAT 50    /* ms */
#define HIT_WINDOW_GOOD  100   /* ms */
#define HIT_WINDOW_MISS  140   /* late = miss */

/* Lane button mapping.
 *   lane 0 = Left, 1 = Up, 2 = Down, 3 = Right
 * On Flipper this is the obvious dpad mapping. The OK button is reserved
 * for slide confirm + pause toggle (long press). */

typedef struct {
    Chart*          chart;
    const ChartDiff* diff;
    Judge*          judge;
    AnomalyState*   anomaly;
    AudioEngine*    audio;
    CharacterState* character;
    VgmDisplay*     vgm;
    NotificationApp* haptics;

    bool            running;
    bool            paused;
    uint32_t        start_tick_ms;
    int16_t         offset_ms;
    uint8_t         scroll_speed;

    /* per-lane state */
    bool            held[LANE_COUNT];
    uint32_t        held_since[LANE_COUNT];

    /* anomaly visual state */
    AnomalyMod      active_mod;
    uint8_t         active_mod_arg;
    uint32_t        flash_until_ms;

    /* last judgement label */
    JudgeResult     last_result;
    uint32_t        last_result_until_ms;

    /* splash override */
    GameViewSplashFn splash;
} GameModel;

struct GameView {
    View*           view;
    PulseApp*       app;
};

/* ------- helpers --------------------------------------------------------- */

static uint32_t now_ms(void) { return furi_get_tick(); }

static int lane_x(uint8_t lane) {
    return LANE_X0 + lane * LANE_W;
}

static int lane_apply_mods(int lane, AnomalyMod mod) {
    if(mod == AnomalyModMirror) return LANE_COUNT - 1 - lane;
    if(mod == AnomalyModInvert) return (lane ^ 1); /* swap up/down, left/right */
    return lane;
}

static const char* note_glyph(NoteType t) {
    switch(t) {
        case NoteTap:   return "T";
        case NoteHold:  return "H";
        case NoteBurst: return "B";
        case NoteSlide: return "S";
        case NoteFake:  return "?";
        case NoteChain: return ".";
    }
    return "?";
}

/* ------- draw ------------------------------------------------------------ */

static void game_draw(Canvas* c, void* model) {
    GameModel* m = model;
    if(m->splash) { m->splash(c, m); return; }

    canvas_clear(c);
    canvas_set_color(c, ColorBlack);

    /* Top bar: combo / score / gauge */
    char buf[32];
    canvas_set_font(c, FontSecondary);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)m->judge->score);
    canvas_draw_str(c, 2, 8, buf);
    snprintf(buf, sizeof(buf), "x%u", (unsigned)m->judge->combo);
    canvas_draw_str(c, 96, 8, buf);

    /* Succession Gauge — vertical bar on the right edge */
    int gauge = m->anomaly->gauge; /* 0..100 */
    canvas_draw_frame(c, 124, 12, 4, 40);
    int fill = (gauge * 40) / 100;
    canvas_draw_box(c, 124, 12 + (40 - fill), 4, fill);

    /* Anomaly indicator */
    if(m->active_mod != AnomalyModNone) {
        canvas_draw_str(c, 50, 8, anomaly_mod_name(m->active_mod));
    }

    /* Lanes */
    for(int i = 0; i < LANE_COUNT; i++) {
        int x = lane_x(i);
        canvas_draw_line(c, x, 12, x, 60);
    }
    canvas_draw_line(c, lane_x(LANE_COUNT), 12, lane_x(LANE_COUNT), 60);

    /* Receptor row */
    canvas_draw_line(c, LANE_X0, RECEPTOR_Y, LANE_X0 + LANE_COUNT * LANE_W, RECEPTOR_Y);
    for(int i = 0; i < LANE_COUNT; i++) {
        int x = lane_x(i) + LANE_W / 2;
        if(m->held[i]) canvas_draw_disc(c, x, RECEPTOR_Y, 3);
        else           canvas_draw_circle(c, x, RECEPTOR_Y, 3);
    }

    if(!m->running) {
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, 64, 32, AlignCenter, AlignCenter, "READY");
        return;
    }

    /* Compute current chart time using signed math — during the lead-in
     * t_signed is negative, and an unsigned subtraction would wrap. */
    int64_t t_signed = (int64_t)now_ms() - (int64_t)m->start_tick_ms
                       + (int64_t)m->offset_ms;
    if(t_signed < 0) {
        /* Lead-in countdown: show "3 / 2 / 1 / GO" big in the middle. */
        int64_t remain = -t_signed;
        const char* lbl = "GO";
        if(remain > 2000)      lbl = "3";
        else if(remain > 1000) lbl = "2";
        else if(remain > 250)  lbl = "1";
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, 64, 32, AlignCenter, AlignCenter, lbl);
        return;
    }
    uint32_t t  = (uint32_t)t_signed;
    int speed   = m->scroll_speed;
    if(m->active_mod == AnomalyModSpeed) speed = (speed * m->active_mod_arg) / 100;
    if(speed < 1) speed = 1;

    /* Note rendering window. The * 4 factor (was * 30) gives a calmer
     * scroll: at speed=10 a note takes ~1s to travel from the top to the
     * receptor, instead of ~140ms. */
    uint32_t lookahead_ms = (RECEPTOR_Y - 12) * 1000u / (speed * 4);
    /* Walk active notes for the difficulty */
    const Note* notes = m->diff->notes;
    uint32_t n = m->diff->note_count;
    for(uint32_t i = 0; i < n; i++) {
        const Note* nt = &notes[i];
        if(nt->time_ms + 200 < t) continue; /* fully past */
        if(nt->time_ms > t + lookahead_ms) break;

        int dt   = (int)nt->time_ms - (int)t;
        int y    = RECEPTOR_Y - (dt * speed * 4) / 1000;
        int lane = lane_apply_mods(nt->lane, m->active_mod);
        int x    = lane_x(lane) + LANE_W / 2;

        if(nt->type == NoteFake)  canvas_set_color(c, ColorXOR);
        if(nt->type == NoteHold) {
            int dur = nt->arg;
            int yy  = y - (dur * speed * 4) / 1000;
            canvas_draw_box(c, x - 2, yy, 4, y - yy);
        } else if(nt->type == NoteChain) {
            canvas_draw_dot(c, x, y);
        } else {
            canvas_draw_disc(c, x, y, 3);
        }
        canvas_set_color(c, ColorBlack);

        /* glyph for non-tap to hint mechanic */
        if(nt->type != NoteTap && nt->type != NoteChain && nt->type != NoteHold) {
            canvas_draw_str(c, x - 2, y + 8, note_glyph(nt->type));
        }
    }

    /* Last judgement flash */
    if(m->last_result_until_ms > now_ms()) {
        canvas_set_font(c, FontPrimary);
        const char* s = "";
        switch(m->last_result) {
            case JudgePerfect: s = "PERFECT"; break;
            case JudgeGreat:   s = "GREAT";   break;
            case JudgeGood:    s = "GOOD";    break;
            case JudgeMiss:    s = "MISS";    break;
            case JudgeNone:    break;
        }
        canvas_draw_str_aligned(c, 64, 30, AlignCenter, AlignCenter, s);
    }

    /* Full-screen flash for FX */
    if(m->flash_until_ms > now_ms()) {
        canvas_invert_color(c);
    }
}

/* ------- input ----------------------------------------------------------- */

static int input_to_lane(InputKey k) {
    switch(k) {
        case InputKeyLeft:  return 0;
        case InputKeyUp:    return 1;
        case InputKeyDown:  return 2;
        case InputKeyRight: return 3;
        default: return -1;
    }
}

static bool game_input(InputEvent* e, void* ctx) {
    GameView* v = ctx;
    PulseApp* app = v->app;
    bool consumed = false;

    with_view_model(v->view, GameModel * m, {
        if(!m->running) {
            if(e->key == InputKeyBack && e->type == InputTypeShort) {
                view_dispatcher_send_custom_event(app->view_dispatcher, PulseEventBack);
                consumed = true;
            }
        } else {
            if(e->key == InputKeyOk && e->type == InputTypeLong) {
                m->paused = !m->paused;
                if(m->paused) audio_silence(m->audio);
                consumed = true;
            } else if(e->key == InputKeyBack && e->type == InputTypeLong) {
                /* abort */
                m->judge->failed = true;
                m->running = false;
                view_dispatcher_send_custom_event(app->view_dispatcher, PulseEventGameFailed);
                consumed = true;
            } else {
                int lane = input_to_lane(e->key);
                if(lane >= 0) {
                    int64_t t_signed = (int64_t)now_ms() - (int64_t)m->start_tick_ms
                                       + (int64_t)m->offset_ms;
                    /* Ignore presses during the lead-in countdown — there's
                     * nothing to hit yet, and feeding judge_press a wrapped
                     * timestamp poisons the score. */
                    if(t_signed < 0) {
                        consumed = true;
                    } else if(e->type == InputTypePress) {
                        m->held[lane] = true;
                        m->held_since[lane] = now_ms();
                        JudgeResult r = judge_press(m->judge, m->diff, lane, (uint32_t)t_signed,
                                                    HIT_WINDOW_GREAT, HIT_WINDOW_GOOD,
                                                    m->active_mod);
                        if(r != JudgeNone) {
                            m->last_result = r;
                            m->last_result_until_ms = now_ms() + 250;
                            anomaly_on_judge(m->anomaly, r, m->character);
                            audio_click(m->audio, r);
                            if(m->haptics && r == JudgePerfect) {
                                notification_message(m->haptics, &sequence_blink_blue_10);
                            }
                        }
                        consumed = true;
                    } else if(e->type == InputTypeRelease) {
                        judge_release(m->judge, m->diff, lane, (uint32_t)t_signed,
                                      HIT_WINDOW_GREAT, HIT_WINDOW_GOOD);
                        m->held[lane] = false;
                        consumed = true;
                    }
                }
            }
        }
    }, true);
    return consumed;
}

/* ------- tick / lifecycle ------------------------------------------------ */

static void game_tick(void* ctx) {
    GameView* v = ctx;
    PulseApp* app = v->app;
    with_view_model(v->view, GameModel * m, {
        if(!m->running || m->paused) {} else {
            /* Signed math: now < start_tick during the lead-in. Using uint32_t
             * here would underflow and make every note look "expired", which
             * insta-failed the song the moment gameplay started. */
            int64_t t_signed = (int64_t)now_ms() - (int64_t)m->start_tick_ms
                               + (int64_t)m->offset_ms;
            if(t_signed < 0) {
                /* Still in the lead-in countdown — nothing to score yet. */
                break;
            }
            uint32_t t = (uint32_t)t_signed;

            /* Anomaly window evaluation */
            anomaly_tick(m->anomaly, m->chart, t, &m->active_mod, &m->active_mod_arg, &m->flash_until_ms);

            /* Drive audio scheduler */
            audio_tick(m->audio, m->chart, t);

            /* Auto-miss expired notes */
            uint32_t expired = judge_advance(m->judge, m->diff, t, HIT_WINDOW_MISS, m->anomaly, m->character);
            if(expired) {
                m->last_result = JudgeMiss;
                m->last_result_until_ms = now_ms() + 250;
            }

            /* End of song — pass/fail decided by the Succession Gauge here.
             * Per design: you can only fail at the endpoint, and only when
             * the gauge is below 65. */
            if(t >= chart_length(m->chart) + 500) {
                m->running = false;
                bool no_fail = false;
                if(m->character) {
                    const CharacterDef* def = character_def(m->character->id);
                    if(def && def->no_fail) no_fail = true;
                }
                if(!no_fail && m->anomaly->gauge < 65) {
                    m->judge->failed = true;
                    view_dispatcher_send_custom_event(app->view_dispatcher, PulseEventGameFailed);
                } else {
                    view_dispatcher_send_custom_event(app->view_dispatcher, PulseEventGameFinished);
                }
            }

            /* Mirror onto VGM */
            if(m->vgm) vgm_render(m->vgm, m, t);
        }
    }, true);
}

/* ------- view alloc ------------------------------------------------------ */

GameView* game_view_alloc(PulseApp* app) {
    GameView* v = malloc(sizeof(GameView));
    v->app = app;
    v->view = view_alloc();
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(GameModel));
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, game_draw);
    view_set_input_callback(v->view, game_input);

    /* tick is driven from view dispatcher tick callback in scene_game; we use a
     * timer here for sub-frame audio/judge updates */
    view_set_enter_callback(v->view, NULL);
    view_set_exit_callback(v->view, NULL);
    view_set_custom_callback(v->view, NULL);
    /* hijack orientation set unused */
    UNUSED(game_tick);
    return v;
}

void game_view_free(GameView* v) {
    view_free(v->view);
    free(v);
}

View* game_view_get_view(GameView* v) { return v->view; }

void game_view_set_splash(GameView* v, GameViewSplashFn fn) {
    with_view_model(v->view, GameModel * m, { m->splash = fn; }, true);
}

void game_view_request_redraw(GameView* v) {
    with_view_model(v->view, GameModel * m, { (void)m; }, true);
}

void game_view_start(
    GameView* v, Chart* chart, const ChartDiff* diff, Judge* judge,
    AnomalyState* anomaly, AudioEngine* audio, CharacterState* character,
    int16_t offset_ms, uint8_t scroll_speed,
    VgmDisplay* vgm, NotificationApp* haptics) {

    with_view_model(v->view, GameModel * m, {
        memset(m, 0, sizeof(*m));
        m->chart = chart;
        m->diff = diff;
        m->judge = judge;
        m->anomaly = anomaly;
        m->audio = audio;
        m->character = character;
        m->vgm = vgm;
        m->haptics = haptics;
        m->offset_ms = offset_ms;
        m->scroll_speed = scroll_speed;
        m->start_tick_ms = now_ms() + 1500; /* 1.5s lead-in */
        m->running = true;
    }, true);

    /* Use a periodic tick via view dispatcher tick handler in scene_game.
     * Simpler: run a FuriTimer here. */
    /* We piggy-back: caller's scene_manager tick (50ms) will call us via
     * pulse_tick_event_cb -> scene_manager -> scene_game tick. To keep
     * timings tight, also schedule audio_tick in our own timer if we want.
     * For simplicity, scene_game forwards ticks here: */
    UNUSED(game_tick);
}

/* Public hook so scene_game can drive ticks from scene_manager */
void game_view_tick(GameView* v) { game_tick(v); }

void game_view_stop(GameView* v) {
    with_view_model(v->view, GameModel * m, { m->running = false; }, true);
}
