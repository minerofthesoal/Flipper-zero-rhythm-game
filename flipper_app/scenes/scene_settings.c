#include "../pulse_app.h"

static void offset_changed(VariableItem* item) {
    PulseApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    int16_t v = ((int16_t)idx - 50) * 5; /* -250..+250 ms in 5ms steps */
    app->offset_ms = v;
    char buf[12];
    snprintf(buf, sizeof(buf), "%+dms", (int)v);
    variable_item_set_current_value_text(item, buf);
}

static void volume_changed(VariableItem* item) {
    PulseApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->volume = idx * 10;
    audio_set_volume(&app->audio, app->volume);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", app->volume);
    variable_item_set_current_value_text(item, buf);
}

static void music_changed(VariableItem* item) {
    PulseApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->music_volume = idx * 10;
    audio_set_music_volume(&app->audio, app->music_volume);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", app->music_volume);
    variable_item_set_current_value_text(item, buf);
}

static void scroll_changed(VariableItem* item) {
    PulseApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->scroll_speed = 5 + idx;
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", app->scroll_speed);
    variable_item_set_current_value_text(item, buf);
}

static void vgm_changed(VariableItem* item) {
    PulseApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->vgm_enabled = idx != 0;
    variable_item_set_current_value_text(item, app->vgm_enabled ? "On" : "Off");
}

static void vibrate_changed(VariableItem* item) {
    PulseApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->vibrate = idx != 0;
    variable_item_set_current_value_text(item, app->vibrate ? "On" : "Off");
}

static void rgb_changed(VariableItem* item) {
    PulseApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->rgb_enabled = idx != 0;
    variable_item_set_current_value_text(item, app->rgb_enabled ? "On" : "Off");
}

void pulse_scene_Settings_on_enter(void* ctx) {
    PulseApp* app = ctx;
    VariableItemList* l = app->var_list;
    variable_item_list_reset(l);

    /* Audio offset: -250..+250ms */
    VariableItem* it = variable_item_list_add(l, "Offset", 101, offset_changed, app);
    uint8_t off_idx = (uint8_t)((app->offset_ms / 5) + 50);
    variable_item_set_current_value_index(it, off_idx);
    char tmp[12];
    snprintf(tmp, sizeof(tmp), "%+dms", app->offset_ms);
    variable_item_set_current_value_text(it, tmp);

    /* SFX (hit-click) volume */
    it = variable_item_list_add(l, "SFX Vol", 11, volume_changed, app);
    variable_item_set_current_value_index(it, app->volume / 10);
    snprintf(tmp, sizeof(tmp), "%d%%", app->volume);
    variable_item_set_current_value_text(it, tmp);

    /* Music (chart-tone) volume */
    it = variable_item_list_add(l, "Music Vol", 11, music_changed, app);
    variable_item_set_current_value_index(it, app->music_volume / 10);
    snprintf(tmp, sizeof(tmp), "%d%%", app->music_volume);
    variable_item_set_current_value_text(it, tmp);

    /* Scroll speed: 5..15 */
    it = variable_item_list_add(l, "Scroll", 11, scroll_changed, app);
    variable_item_set_current_value_index(it, app->scroll_speed - 5);
    snprintf(tmp, sizeof(tmp), "%d", app->scroll_speed);
    variable_item_set_current_value_text(it, tmp);

    /* VGM Display */
    it = variable_item_list_add(l, "VGM Display", 2, vgm_changed, app);
    variable_item_set_current_value_index(it, app->vgm_enabled ? 1 : 0);
    variable_item_set_current_value_text(it, app->vgm_enabled ? "On" : "Off");

    /* Vibrate */
    it = variable_item_list_add(l, "Vibrate", 2, vibrate_changed, app);
    variable_item_set_current_value_index(it, app->vibrate ? 1 : 0);
    variable_item_set_current_value_text(it, app->vibrate ? "On" : "Off");

    /* RGB LED feedback (handy on Flipper RGB-mod hardware; harmless on stock) */
    it = variable_item_list_add(l, "RGB LED", 2, rgb_changed, app);
    variable_item_set_current_value_index(it, app->rgb_enabled ? 1 : 0);
    variable_item_set_current_value_text(it, app->rgb_enabled ? "On" : "Off");

    view_dispatcher_switch_to_view(app->view_dispatcher, PulseViewVarList);
}

bool pulse_scene_Settings_on_event(void* ctx, SceneManagerEvent evt) {
    UNUSED(ctx); UNUSED(evt);
    return false;
}

void pulse_scene_Settings_on_exit(void* ctx) {
    PulseApp* app = ctx;
    variable_item_list_reset(app->var_list);
}
