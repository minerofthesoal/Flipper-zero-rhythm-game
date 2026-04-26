#pragma once

#include <gui/scene_manager.h>

/* X-macro list of scenes — keeps the enum, the handler arrays, and the
 * forward declarations in lockstep so adding a scene is one line. */
#define PULSE_SCENE_LIST(X) \
    X(Splash)               \
    X(Menu)                 \
    X(SongSelect)           \
    X(Difficulty)           \
    X(Character)            \
    X(Game)                 \
    X(Results)              \
    X(Settings)             \
    X(Credits)

typedef enum {
#define X(name) PulseScene##name,
    PULSE_SCENE_LIST(X)
#undef X
        PulseSceneCount,
} PulseScene;

#define X(name)                                                  \
    void pulse_scene_##name##_on_enter(void* ctx);               \
    bool pulse_scene_##name##_on_event(void* ctx, SceneManagerEvent evt); \
    void pulse_scene_##name##_on_exit(void* ctx);
PULSE_SCENE_LIST(X)
#undef X

extern const SceneManagerHandlers pulse_scene_handlers;

/* Custom events posted from views to scenes */
typedef enum {
    PulseEventBack          = 0x100,
    PulseEventGameFinished  = 0x101,
    PulseEventGameFailed    = 0x102,
    PulseEventStartGame     = 0x103,
    PulseEventCharacterPick = 0x104,
} PulseCustomEvent;
