#include "../pulse_app.h"

void pulse_scene_Game_on_enter(void* ctx) {
    PulseApp* app = ctx;
    judge_reset(&app->judge);
    anomaly_reset(&app->anomaly);
    audio_reset(&app->audio);
    if(app->vgm_enabled) vgm_open(&app->vgm);

    /* Pin the backlight on so the screen never sleeps mid-song. Released in
     * on_exit. */
    notification_message_block(app->notifications, &sequence_display_backlight_enforce_on);

    const ChartDiff* d = chart_difficulty(app->chart, app->difficulty);
    game_view_start(app->game_view, app->chart, d, &app->judge,
                    &app->anomaly, &app->audio, &app->character,
                    app->offset_ms, app->scroll_speed,
                    app->vgm_enabled ? &app->vgm : NULL,
                    app->notifications, app->vibrate, app->rgb_enabled);

    view_dispatcher_switch_to_view(app->view_dispatcher, PulseViewGame);
}

bool pulse_scene_Game_on_event(void* ctx, SceneManagerEvent evt) {
    PulseApp* app = ctx;
    if(evt.type == SceneManagerEventTypeTick) {
        game_view_tick(app->game_view);
        return true;
    }
    if(evt.type == SceneManagerEventTypeCustom) {
        if(evt.event == PulseEventGameFinished || evt.event == PulseEventGameFailed) {
            scene_manager_next_scene(app->scene_manager, PulseSceneResults);
            return true;
        }
    }
    return false;
}

void pulse_scene_Game_on_exit(void* ctx) {
    PulseApp* app = ctx;
    game_view_stop(app->game_view);
    audio_silence(&app->audio);
    if(app->vgm_enabled) vgm_close(&app->vgm);
    notification_message_block(app->notifications, &sequence_display_backlight_enforce_auto);
}
