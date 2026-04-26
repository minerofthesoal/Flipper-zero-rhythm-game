#include "../pulse_app.h"

static const char credits_text[] =
    "PULSE\n"
    "rhythm engine for Flipper Zero\n"
    "\n"
    "Engine, charts and editor:\n"
    "  Pulse Team\n"
    "\n"
    "Original soundtrack:\n"
    "  NEO//SHARD\n"
    "  Mira Hex\n"
    "  Solenne\n"
    "\n"
    "Built around the .frgl format.\n"
    "Compatible with the Video Game\n"
    "Module for external display.\n"
    "\n"
    "Press Back to return.\n";

void pulse_scene_Credits_on_enter(void* ctx) {
    PulseApp* app = ctx;
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, credits_text);
    view_dispatcher_switch_to_view(app->view_dispatcher, PulseViewTextBox);
}

bool pulse_scene_Credits_on_event(void* ctx, SceneManagerEvent evt) {
    UNUSED(ctx); UNUSED(evt);
    return false;
}

void pulse_scene_Credits_on_exit(void* ctx) {
    PulseApp* app = ctx;
    text_box_reset(app->text_box);
}
