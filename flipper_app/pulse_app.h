#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_box.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#include "scenes/scenes.h"
#include "views/view_game.h"
#include "game/chart.h"
#include "game/judge.h"
#include "game/audio.h"
#include "game/save.h"
#include "game/anomaly.h"
#include "game/character.h"
#include "game/vgm.h"

#define PULSE_TAG          "Pulse"
#define PULSE_APP_FOLDER   APP_DATA_PATH("")
#define PULSE_SONGS_FOLDER APP_DATA_PATH("songs")
#define PULSE_SAVE_PATH    APP_DATA_PATH("pulse.save")
#define PULSE_FRGL_EXT     ".frgl"

typedef enum {
    PulseViewSubmenu,
    PulseViewDialog,
    PulseViewVarList,
    PulseViewTextBox,
    PulseViewGame,
    PulseViewCount,
} PulseView;

typedef enum {
    PulseDiffEasy   = 0,
    PulseDiffNormal = 1,
    PulseDiffFun    = 2,
    PulseDiffNitro  = 3,
    PulseDiffCount,
} PulseDifficulty;

typedef struct PulseApp {
    Gui*                gui;
    NotificationApp*    notifications;
    Storage*            storage;
    ViewDispatcher*     view_dispatcher;
    SceneManager*       scene_manager;

    Submenu*            submenu;
    DialogEx*           dialog;
    VariableItemList*   var_list;
    TextBox*            text_box;
    FuriString*         text_buf;

    GameView*           game_view;

    /* Currently loaded chart + active difficulty */
    Chart*              chart;
    PulseDifficulty     difficulty;
    uint32_t            selected_song_index;

    /* Subsystems */
    Judge               judge;
    AudioEngine         audio;
    AnomalyState        anomaly;
    CharacterState      character;
    VgmDisplay          vgm;
    SaveData            save;

    /* Settings (mirrored into save) */
    int16_t             offset_ms;
    uint8_t             volume;       /* 0..100 */
    uint8_t             scroll_speed; /* 5..15 ; default 10 */
    bool                vgm_enabled;
    bool                vibrate;
} PulseApp;

PulseApp* pulse_app_alloc(void);
void      pulse_app_free(PulseApp* app);
