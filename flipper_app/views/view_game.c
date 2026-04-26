#include "view_game.h"
#include "../pulse_app.h"
#include "../scenes/scenes.h"

#include <gui/elements.h>

/* ------- model ----------------------------------------------------------- */

#define LANE_COUNT     4
#define LANE_W         12
#define LANE_X0        12      /* 4 lanes × 12 = 48 → x=12..60, leaves 64..127 for HUD */
#define RECEPTOR_Y     54
#define LANE_TOP_Y     14
#define HIT_WINDOW_GREAT 50    /* ms */
#define HIT_WINDOW_GOOD  100   /* ms */
#define HIT_WINDOW_MISS  140   /* late = miss */
#define GAUGE_PASS     65      /* gauge ≥ this at song end = pass */

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
    NotificationApp* notify;
    bool            vibrate;
    bool            rgb;

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

static int lane_apply_mods(int lane, AnomalyMod mod) {
    if(mod == AnomalyModMirror) return LANE_COUNT - 1 - lane;
    if(mod == AnomalyModInvert) return (lane ^ 1); /* swap up/down, left/right */
    return lane;
}

/* ------- draw ------------------------------------------------------------ */

static const char* lane_letter(int lane) {
    static const char* k[LANE_COUNT] = {"L", "U", "D", "R"};
    return k[lane];
}

static void draw_gauge(Canvas* c, int gauge) {
    /* Wide horizontal Succession Gauge on the right column so it's actually
     * legible. Frame: x=66..125, y=14..58 (44 tall). Fills bottom-up. */
    const int gx = 66, gy = 14, gw = 14, gh = 44;
    canvas_set_color(c, ColorBlack);
    canvas_draw_str(c, gx + 1, gy - 2, "GAUGE");
    canvas_draw_frame(c, gx, gy, gw, gh);
    if(gauge < 0) gauge = 0; if(gauge > 100) gauge = 100;
    int fill = (gauge * (gh - 2)) / 100;
    if(fill > 0) canvas_draw_box(c, gx + 1, gy + (gh - 1) - fill, gw - 2, fill);
    /* Pass-line marker (65%): little notch on the right edge. */
    int pass_y = gy + gh - 1 - ((GAUGE_PASS * (gh - 2)) / 100);
    canvas_draw_line(c, gx + gw, pass_y, gx + gw + 2, pass_y);
}

static void draw_hud(Canvas* c, GameModel* m) {
    char buf[24];
    canvas_set_font(c, FontSecondary);
    canvas_set_color(c, ColorBlack);
    /* Score line — left of HUD column. */
    snprintf(buf, sizeof(buf), "%06lu", (unsigned long)m->judge->score);
    canvas_draw_str(c, 0, 8, buf);
    /* Combo — right side, big, only when meaningful. */
    if(m->judge->combo > 0) {
        snprintf(buf, sizeof(buf), "x%u", (unsigned)m->judge->combo);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, 127, 9, AlignRight, AlignBottom, buf);
        canvas_set_font(c, FontSecondary);
    }
    /* Anomaly mod tag, when active, between gauge label and combo. */
    if(m->active_mod != AnomalyModNone) {
        canvas_draw_str(c, 86, 8, anomaly_mod_name(m->active_mod));
    }
}

static void draw_lanes(Canvas* c, GameModel* m) {
    canvas_set_color(c, ColorBlack);
    /* Lane separators. */
    for(int i = 0; i <= LANE_COUNT; i++) {
        int x = LANE_X0 + i * LANE_W;
        canvas_draw_line(c, x, LANE_TOP_Y, x, RECEPTOR_Y + 4);
    }
    /* Receptor row + lane-letter labels. */
    canvas_draw_line(c, LANE_X0, RECEPTOR_Y, LANE_X0 + LANE_COUNT * LANE_W, RECEPTOR_Y);
    canvas_set_font(c, FontSecondary);
    for(int i = 0; i < LANE_COUNT; i++) {
        int cx = LANE_X0 + i * LANE_W + LANE_W / 2;
        if(m->held[i]) {
            canvas_draw_disc(c, cx, RECEPTOR_Y, 4);
        } else {
            canvas_draw_circle(c, cx, RECEPTOR_Y, 4);
        }
        canvas_draw_str_aligned(c, cx, 63, AlignCenter, AlignBottom, lane_letter(i));
    }
}

static void draw_notes(Canvas* c, GameModel* m, uint32_t t, int speed) {
    uint32_t lookahead_ms = (RECEPTOR_Y - LANE_TOP_Y) * 1000u / (speed * 4);
    const Note* notes = m->diff->notes;
    uint32_t n = m->diff->note_count;
    canvas_set_color(c, ColorBlack);
    for(uint32_t i = 0; i < n; i++) {
        const Note* nt = &notes[i];
        if(nt->time_ms + 200 < t) continue;
        if(nt->time_ms > t + lookahead_ms) break;

        int dt   = (int)nt->time_ms - (int)t;
        int y    = RECEPTOR_Y - (dt * speed * 4) / 1000;
        int lane = lane_apply_mods(nt->lane, m->active_mod);
        int cx   = LANE_X0 + lane * LANE_W + LANE_W / 2;

        switch(nt->type) {
        case NoteHold: {
            int dur = nt->arg;
            int yy  = y - (dur * speed * 4) / 1000;
            canvas_draw_rframe(c, cx - 3, yy, 7, y - yy + 1, 1);
            canvas_draw_box(c, cx - 1, yy + 1, 3, y - yy - 1);
            break;
        }
        case NoteChain:
            canvas_draw_box(c, cx - 1, y - 1, 3, 3);
            break;
        case NoteFake:
            /* hollow with X — penalty if pressed */
            canvas_draw_circle(c, cx, y, 3);
            canvas_draw_line(c, cx - 2, y - 2, cx + 2, y + 2);
            canvas_draw_line(c, cx - 2, y + 2, cx + 2, y - 2);
            break;
        case NoteBurst: {
            /* stacked discs to suggest tap-tap-tap */
            canvas_draw_disc(c, cx, y, 3);
            canvas_draw_circle(c, cx, y - 5, 2);
            canvas_draw_circle(c, cx, y - 9, 2);
            break;
        }
        case NoteSlide:
            canvas_draw_disc(c, cx, y, 3);
            canvas_draw_line(c, cx - 4, y, cx + 4, y);
            break;
        case NoteTap:
        default:
            canvas_draw_disc(c, cx, y, 3);
            break;
        }
    }
}

static void draw_judgment_flash(Canvas* c, GameModel* m) {
    if(m->last_result_until_ms <= now_ms()) return;
    const char* s = NULL;
    switch(m->last_result) {
        case JudgePerfect: s = "PERFECT"; break;
        case JudgeGreat:   s = "GREAT";   break;
        case JudgeGood:    s = "GOOD";    break;
        case JudgeMiss:    s = "MISS";    break;
        case JudgeNone:    return;
    }
    if(!s) return;
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontPrimary);
    /* Centered above the receptor row inside the lane area. */
    canvas_draw_str_aligned(c, LANE_X0 + (LANE_COUNT * LANE_W) / 2, 32,
                            AlignCenter, AlignCenter, s);
}

static void game_draw(Canvas* c, void* model) {
    GameModel* m = model;
    if(m->splash) { m->splash(c, m); return; }

    canvas_clear(c);

    draw_hud(c, m);
    draw_gauge(c, m->anomaly ? m->anomaly->gauge : 0);
    draw_lanes(c, m);

    if(!m->running) {
        canvas_set_color(c, ColorBlack);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, LANE_X0 + (LANE_COUNT * LANE_W) / 2, 32,
                                AlignCenter, AlignCenter, "READY");
        return;
    }

    int64_t t_signed = (int64_t)now_ms() - (int64_t)m->start_tick_ms
                       + (int64_t)m->offset_ms;
    if(t_signed < 0) {
        int64_t remain = -t_signed;
        const char* lbl = "GO";
        if(remain > 2000)      lbl = "3";
        else if(remain > 1000) lbl = "2";
        else if(remain > 250)  lbl = "1";
        canvas_set_color(c, ColorBlack);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, LANE_X0 + (LANE_COUNT * LANE_W) / 2, 32,
                                AlignCenter, AlignCenter, lbl);
        return;
    }

    uint32_t t = (uint32_t)t_signed;
    int speed = m->scroll_speed;
    if(m->active_mod == AnomalyModSpeed) speed = (speed * m->active_mod_arg) / 100;
    if(speed < 1) speed = 1;

    draw_notes(c, m, t, speed);
    draw_judgment_flash(c, m);

    if(m->paused) {
        canvas_set_color(c, ColorBlack);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, 64, 32, AlignCenter, AlignCenter, "PAUSED");
    }

    if(m->flash_until_ms > now_ms()) canvas_invert_color(c);
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
                            audio_click(m->audio, r, m->judge->last_hit_type);
                            if(m->notify && m->vibrate && r == JudgePerfect) {
                                notification_message(m->notify, &sequence_single_vibro);
                            }
                            if(m->notify && m->rgb) {
                                const NotificationSequence* seq = NULL;
                                switch(r) {
                                    case JudgePerfect: seq = &sequence_blink_blue_10;  break;
                                    case JudgeGreat:   seq = &sequence_blink_green_10; break;
                                    case JudgeGood:    seq = &sequence_blink_yellow_10;break;
                                    case JudgeMiss:    seq = &sequence_blink_red_10;   break;
                                    case JudgeNone:    break;
                                }
                                if(seq) notification_message(m->notify, seq);
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
    VgmDisplay* vgm, NotificationApp* notify, bool vibrate, bool rgb) {

    with_view_model(v->view, GameModel * m, {
        memset(m, 0, sizeof(*m));
        m->chart = chart;
        m->diff = diff;
        m->judge = judge;
        m->anomaly = anomaly;
        m->audio = audio;
        m->character = character;
        m->vgm = vgm;
        m->notify = notify;
        m->vibrate = vibrate;
        m->rgb = rgb;
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
