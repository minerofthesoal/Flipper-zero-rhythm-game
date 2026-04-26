#include "scenes.h"

#define X(name) pulse_scene_##name##_on_enter,
static void (*const on_enter[])(void*) = { PULSE_SCENE_LIST(X) };
#undef X

#define X(name) pulse_scene_##name##_on_event,
static bool (*const on_event[])(void*, SceneManagerEvent) = { PULSE_SCENE_LIST(X) };
#undef X

#define X(name) pulse_scene_##name##_on_exit,
static void (*const on_exit[])(void*) = { PULSE_SCENE_LIST(X) };
#undef X

const SceneManagerHandlers pulse_scene_handlers = {
    .on_enter_handlers = on_enter,
    .on_event_handlers = on_event,
    .on_exit_handlers  = on_exit,
    .scene_num         = PulseSceneCount,
};
