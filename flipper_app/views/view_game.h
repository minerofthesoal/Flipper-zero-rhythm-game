#pragma once

#include <gui/view.h>
#include "../game/chart.h"
#include "../game/judge.h"
#include "../game/audio.h"
#include "../game/anomaly.h"
#include "../game/character.h"
#include "../game/vgm.h"

typedef struct GameView GameView;
struct PulseApp; /* fwd */

GameView* game_view_alloc(struct PulseApp* app);
void      game_view_free(GameView* v);
View*     game_view_get_view(GameView* v);

/* Splash override: when set, draw_callback delegates to fn instead of game. */
typedef void (*GameViewSplashFn)(Canvas* canvas, void* model_ctx);
void game_view_set_splash(GameView* v, GameViewSplashFn fn);
void game_view_request_redraw(GameView* v);

/* Begin gameplay with the given chart+difficulty. */
void game_view_start(
    GameView*               v,
    Chart*                  chart,
    const ChartDiff*        diff,
    Judge*                  judge,
    AnomalyState*           anomaly,
    AudioEngine*            audio,
    CharacterState*         character,
    int16_t                 offset_ms,
    uint8_t                 scroll_speed,
    VgmDisplay*             vgm_optional,
    NotificationApp*        haptics_optional);

void game_view_stop(GameView* v);

/* Drive one logic step. The scene that owns the game view should call this
 * from its tick handler so we can advance audio, judge, and VGM mirror. */
void game_view_tick(GameView* v);
