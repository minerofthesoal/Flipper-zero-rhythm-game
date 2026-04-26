#include "../pulse_app.h"

enum {
    MenuItemPlay,
    MenuItemCharacter,
    MenuItemSettings,
    MenuItemCredits,
};

static void menu_cb(void* ctx, uint32_t index) {
    PulseApp* app = ctx;
    scene_manager_set_scene_state(app->scene_manager, PulseSceneMenu, index);
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void pulse_scene_Menu_on_enter(void* ctx) {
    PulseApp* app = ctx;
    Submenu* m = app->submenu;
    submenu_reset(m);
    submenu_set_header(m, "PULSE");
    submenu_add_item(m, "Play",        MenuItemPlay,      menu_cb, app);
    submenu_add_item(m, "Character",   MenuItemCharacter, menu_cb, app);
    submenu_add_item(m, "Settings",    MenuItemSettings,  menu_cb, app);
    submenu_add_item(m, "Credits",     MenuItemCredits,   menu_cb, app);
    submenu_set_selected_item(m, scene_manager_get_scene_state(app->scene_manager, PulseSceneMenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, PulseViewSubmenu);
}

bool pulse_scene_Menu_on_event(void* ctx, SceneManagerEvent evt) {
    PulseApp* app = ctx;
    if(evt.type == SceneManagerEventTypeBack) {
        /* Top-level menu — back exits the app rather than falling back to
         * Splash (which would just re-enter Menu and loop forever). */
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }
    if(evt.type != SceneManagerEventTypeCustom) return false;
    switch(evt.event) {
        case MenuItemPlay:      scene_manager_next_scene(app->scene_manager, PulseSceneSongSelect); return true;
        case MenuItemCharacter: scene_manager_next_scene(app->scene_manager, PulseSceneCharacter);  return true;
        case MenuItemSettings:  scene_manager_next_scene(app->scene_manager, PulseSceneSettings);   return true;
        case MenuItemCredits:   scene_manager_next_scene(app->scene_manager, PulseSceneCredits);    return true;
    }
    return false;
}

void pulse_scene_Menu_on_exit(void* ctx) {
    PulseApp* app = ctx;
    submenu_reset(app->submenu);
}
