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
    canvas_draw_str_aligned(c, 64, 22, AlignCenter, AlignCenter, "PULSE");
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 64, 36, AlignCenter, AlignCenter, "rhythm engine");
    /* loading bar */
    int w = (s_splash.ticks * 100) / SplashTickHold;
    if(w > 100) w = 100;
    canvas_draw_frame(c, 14, 50, 100, 6);
    canvas_draw_box(c, 14, 50, w, 6);
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
            scene_manager_next_scene(app->scene_manager, PulseSceneMenu);
            return true;
        }
    } else if(evt.type == SceneManagerEventTypeBack) {
        scene_manager_next_scene(app->scene_manager, PulseSceneMenu);
        return true;
    }
    return false;
}

void pulse_scene_Splash_on_exit(void* ctx) {
    PulseApp* app = ctx;
    game_view_set_splash(app->game_view, NULL);
}
