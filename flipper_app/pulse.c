#include "pulse_app.h"

static bool pulse_custom_event_cb(void* ctx, uint32_t evt) {
    furi_assert(ctx);
    PulseApp* app = ctx;
    return scene_manager_handle_custom_event(app->scene_manager, evt);
}

static bool pulse_back_event_cb(void* ctx) {
    furi_assert(ctx);
    PulseApp* app = ctx;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void pulse_tick_event_cb(void* ctx) {
    furi_assert(ctx);
    PulseApp* app = ctx;
    scene_manager_handle_tick_event(app->scene_manager);
}

PulseApp* pulse_app_alloc(void) {
    PulseApp* app = malloc(sizeof(PulseApp));
    memset(app, 0, sizeof(*app));

    app->gui            = furi_record_open(RECORD_GUI);
    app->notifications  = furi_record_open(RECORD_NOTIFICATION);
    app->storage        = furi_record_open(RECORD_STORAGE);

    /* ensure folders exist */
    storage_simply_mkdir(app->storage, PULSE_APP_FOLDER);
    storage_simply_mkdir(app->storage, PULSE_SONGS_FOLDER);

    save_load(&app->save, app->storage);
    app->offset_ms    = app->save.offset_ms;
    app->volume       = app->save.volume;
    app->scroll_speed = app->save.scroll_speed;
    app->vgm_enabled  = app->save.vgm_enabled;
    app->vibrate      = app->save.vibrate;
    app->rgb_enabled  = app->save.rgb_enabled;
    app->difficulty   = PulseDiffNormal;

    audio_init(&app->audio, app->volume);
    anomaly_init(&app->anomaly);
    character_init(&app->character, app->save.character_id);
    vgm_init(&app->vgm);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager   = scene_manager_alloc(&pulse_scene_handlers, app);

    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, pulse_custom_event_cb);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, pulse_back_event_cb);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, pulse_tick_event_cb, 50);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu  = submenu_alloc();
    app->dialog   = dialog_ex_alloc();
    app->var_list = variable_item_list_alloc();
    app->text_box = text_box_alloc();
    app->text_buf = furi_string_alloc();
    app->game_view = game_view_alloc(app);

    view_dispatcher_add_view(app->view_dispatcher, PulseViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(app->view_dispatcher, PulseViewDialog,  dialog_ex_get_view(app->dialog));
    view_dispatcher_add_view(app->view_dispatcher, PulseViewVarList, variable_item_list_get_view(app->var_list));
    view_dispatcher_add_view(app->view_dispatcher, PulseViewTextBox, text_box_get_view(app->text_box));
    view_dispatcher_add_view(app->view_dispatcher, PulseViewGame,    game_view_get_view(app->game_view));

    return app;
}

void pulse_app_free(PulseApp* app) {
    furi_assert(app);

    /* Persist settings into save and write out */
    app->save.offset_ms    = app->offset_ms;
    app->save.volume       = app->volume;
    app->save.scroll_speed = app->scroll_speed;
    app->save.vgm_enabled  = app->vgm_enabled;
    app->save.vibrate      = app->vibrate;
    app->save.rgb_enabled  = app->rgb_enabled;
    app->save.character_id = app->character.id;
    save_store(&app->save, app->storage);

    if(app->chart) chart_free(app->chart);
    vgm_deinit(&app->vgm);
    audio_deinit(&app->audio);

    view_dispatcher_remove_view(app->view_dispatcher, PulseViewGame);
    view_dispatcher_remove_view(app->view_dispatcher, PulseViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, PulseViewVarList);
    view_dispatcher_remove_view(app->view_dispatcher, PulseViewDialog);
    view_dispatcher_remove_view(app->view_dispatcher, PulseViewSubmenu);

    game_view_free(app->game_view);
    furi_string_free(app->text_buf);
    text_box_free(app->text_box);
    variable_item_list_free(app->var_list);
    dialog_ex_free(app->dialog);
    submenu_free(app->submenu);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t pulse_app(void* p) {
    UNUSED(p);
    PulseApp* app = pulse_app_alloc();
    scene_manager_next_scene(app->scene_manager, PulseSceneSplash);
    view_dispatcher_run(app->view_dispatcher);
    pulse_app_free(app);
    return 0;
}
