#include "../pulse_app.h"

static void char_cb(void* ctx, uint32_t index) {
    PulseApp* app = ctx;
    character_set(&app->character, (CharacterId)index);
    scene_manager_set_scene_state(app->scene_manager, PulseSceneCharacter, index);
    view_dispatcher_send_custom_event(app->view_dispatcher, PulseEventCharacterPick);
}

void pulse_scene_Character_on_enter(void* ctx) {
    PulseApp* app = ctx;
    Submenu* m = app->submenu;
    submenu_reset(m);
    submenu_set_header(m, "Character");

    for(uint8_t i = 0; i < CharacterCount; i++) {
        const CharacterDef* def = character_def((CharacterId)i);
        FuriString* s = furi_string_alloc_printf("%s — %s", def->name, def->blurb);
        submenu_add_item(m, furi_string_get_cstr(s), i, char_cb, app);
        furi_string_free(s);
    }
    submenu_set_selected_item(m, app->character.id);
    view_dispatcher_switch_to_view(app->view_dispatcher, PulseViewSubmenu);
}

bool pulse_scene_Character_on_event(void* ctx, SceneManagerEvent evt) {
    PulseApp* app = ctx;
    if(evt.type == SceneManagerEventTypeCustom && evt.event == PulseEventCharacterPick) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void pulse_scene_Character_on_exit(void* ctx) {
    PulseApp* app = ctx;
    submenu_reset(app->submenu);
}
