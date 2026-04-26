#include "../pulse_app.h"

static const char* grade_label(uint32_t score) {
    if(score >= 980000) return "EX+";
    if(score >= 950000) return "EX";
    if(score >= 900000) return "AA";
    if(score >= 800000) return "A";
    if(score >= 700000) return "B";
    if(score >= 600000) return "C";
    return "D";
}

static void results_draw(Canvas* c, void* m) {
    PulseApp* app = m;
    const Judge* j = &app->judge;
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 2, 10, j->failed ? "FAILED" : "CLEAR");

    canvas_set_font(c, FontSecondary);
    char buf[32];
    snprintf(buf, sizeof(buf), "Score %lu", (unsigned long)j->score);
    canvas_draw_str(c, 2, 22, buf);
    snprintf(buf, sizeof(buf), "Grade %s", grade_label(j->score));
    canvas_draw_str(c, 2, 32, buf);
    snprintf(buf, sizeof(buf), "Combo %u",  (unsigned)j->max_combo);
    canvas_draw_str(c, 2, 42, buf);
    snprintf(buf, sizeof(buf), "P%u G%u M%u",
             (unsigned)j->perfects, (unsigned)j->goods, (unsigned)j->misses);
    canvas_draw_str(c, 2, 52, buf);
    canvas_draw_str(c, 2, 62, "Back: continue");
}

void pulse_scene_Results_on_enter(void* ctx) {
    PulseApp* app = ctx;

    /* Persist best score per (song,difficulty) */
    save_record_score(&app->save, app->selected_song_index,
                      app->difficulty, app->judge.score, app->judge.max_combo,
                      app->judge.failed ? 0 : 1);
    save_store(&app->save, app->storage);

    game_view_set_splash(app->game_view, results_draw);
    view_dispatcher_switch_to_view(app->view_dispatcher, PulseViewGame);
}

bool pulse_scene_Results_on_event(void* ctx, SceneManagerEvent evt) {
    PulseApp* app = ctx;
    if(evt.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, PulseSceneMenu);
        return true;
    }
    return false;
}

void pulse_scene_Results_on_exit(void* ctx) {
    PulseApp* app = ctx;
    game_view_set_splash(app->game_view, NULL);
}
