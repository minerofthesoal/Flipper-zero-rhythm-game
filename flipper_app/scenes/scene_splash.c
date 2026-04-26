#include "../pulse_app.h"

enum { SplashTickHold = 24 }; /* ~1.2s @ 50ms tick */

typedef struct { uint16_t ticks; } SplashState;
static SplashState s_splash;

static void splash_draw(Canvas* c, void* m) {
    UNUSED(m);
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    /* PULSE wordmark */
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 26, AlignCenter, AlignCenter, "PULSE");
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 64, 40, AlignCenter, AlignCenter, "rhythm engine");
    /* Pulsing dot — beats with the splash hold so the screen feels alive
     * without pretending we're loading anything (we're not — chart loading
     * happens later, in song_select). */
    int phase = s_splash.ticks % 12;
    int rad = phase < 6 ? phase : 12 - phase;
    canvas_draw_disc(c, 64, 54, 1 + rad / 2);
}

void pulse_scene_Splash_on_enter(void* ctx) {
    PulseApp* app = ctx;
    UNUSED(app);
    s_splash.ticks = 0;
    /* steal submenu's view as a quick canvas surface — we just want a draw cb */
    /* Instead: route to game view's "splash mode" via flag */
    game_view_set_splash(app->game_view, splash_draw);
    view_dispatcher_switch_to_view(app->view_dispatcher, PulseViewGame);
}

bool pulse_scene_Splash_on_event(void* ctx, SceneManagerEvent evt) {
    PulseApp* app = ctx;
    if(evt.type == SceneManagerEventTypeTick) {
        s_splash.ticks++;
        game_view_request_redraw(app->game_view);
        if(s_splash.ticks >= SplashTickHold) {
            /* Replace splash with menu (don't leave splash on the back stack
             * — otherwise Back from menu pops to splash and loops). */
            scene_manager_search_and_switch_to_another_scene(app->scene_manager, PulseSceneMenu);
            return true;
        }
    } else if(evt.type == SceneManagerEventTypeBack) {
        /* Skip the splash and go straight to the menu (which becomes the new
         * top of the stack via search-and-switch, so a later Back exits). */
        scene_manager_search_and_switch_to_another_scene(app->scene_manager, PulseSceneMenu);
        return true;
    }
    return false;
}

void pulse_scene_Splash_on_exit(void* ctx) {
    PulseApp* app = ctx;
    game_view_set_splash(app->game_view, NULL);
}
