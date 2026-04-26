#include "../pulse_app.h"

static const char* const diff_names[PulseDiffCount] = {
    "Easy",
    "Normal",
    "Fun",
    "Nitro",
};

static void diff_cb(void* ctx, uint32_t index) {
    PulseApp* app = ctx;
    app->difficulty = (PulseDifficulty)index;
    scene_manager_set_scene_state(app->scene_manager, PulseSceneDifficulty, index);
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void pulse_scene_Difficulty_on_enter(void* ctx) {
    PulseApp* app = ctx;
    Submenu* m = app->submenu;
    submenu_reset(m);

    if(app->chart) {
        FuriString* hdr = furi_string_alloc_printf("%s", chart_title(app->chart));
        submenu_set_header(m, furi_string_get_cstr(hdr));
        furi_string_free(hdr);

        for(uint8_t i = 0; i < PulseDiffCount; i++) {
            const ChartDiff* d = chart_difficulty(app->chart, (PulseDifficulty)i);
            FuriString* label;
            if(d && d->note_count > 0) {
                label = furi_string_alloc_printf(
                    "%s  %d.%d",
                    diff_names[i],
                    d->level / 10,
                    d->level % 10);
            } else {
                label = furi_string_alloc_printf("%s  --", diff_names[i]);
            }
            submenu_add_item(m, furi_string_get_cstr(label), i, diff_cb, app);
            furi_string_free(label);
        }
    }
    submenu_set_selected_item(m, scene_manager_get_scene_state(app->scene_manager, PulseSceneDifficulty));
    view_dispatcher_switch_to_view(app->view_dispatcher, PulseViewSubmenu);
}

bool pulse_scene_Difficulty_on_event(void* ctx, SceneManagerEvent evt) {
    PulseApp* app = ctx;
    if(evt.type != SceneManagerEventTypeCustom) return false;
    if(evt.event >= PulseDiffCount) return false;

    const ChartDiff* d = chart_difficulty(app->chart, (PulseDifficulty)evt.event);
    if(!d || d->note_count == 0) return true; /* not playable */

    scene_manager_next_scene(app->scene_manager, PulseSceneGame);
    return true;
}

void pulse_scene_Difficulty_on_exit(void* ctx) {
    PulseApp* app = ctx;
    submenu_reset(app->submenu);
}
