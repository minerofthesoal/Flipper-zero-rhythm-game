#include "../pulse_app.h"
#include <storage/storage.h>

/* Lists every .frgl in PULSE_SONGS_FOLDER plus the embedded built-ins. */

#define BUILTIN_COUNT 4
static const char* const builtin_files[BUILTIN_COUNT] = {
    "/builtin/pulse_drive.frgl",
    "/builtin/crystal_cascade.frgl",
    "/builtin/neon_heartbeat.frgl",
    "/builtin/glitch_garden.frgl",
};
static const char* const builtin_titles[BUILTIN_COUNT] = {
    "Pulse Drive",
    "Crystal Cascade",
    "Neon Heartbeat",
    "Glitch Garden",
};

typedef struct {
    FuriString* path;   /* either prefixed "/builtin/..." or absolute storage path */
    FuriString* title;
} SongEntry;

#define SONG_LIST_MAX 64
static SongEntry s_songs[SONG_LIST_MAX];
static uint8_t   s_song_count = 0;

static void song_select_cb(void* ctx, uint32_t index) {
    PulseApp* app = ctx;
    app->selected_song_index = index;
    scene_manager_set_scene_state(app->scene_manager, PulseSceneSongSelect, index);
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void songs_clear(void) {
    for(uint8_t i = 0; i < s_song_count; i++) {
        furi_string_free(s_songs[i].path);
        furi_string_free(s_songs[i].title);
    }
    s_song_count = 0;
}

static void songs_push(const char* path, const char* title) {
    if(s_song_count >= SONG_LIST_MAX) return;
    s_songs[s_song_count].path  = furi_string_alloc_set(path);
    s_songs[s_song_count].title = furi_string_alloc_set(title);
    s_song_count++;
}

static void songs_scan_user(PulseApp* app) {
    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, PULSE_SONGS_FOLDER)) {
        FileInfo info;
        char name[64];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(info.flags & FSF_DIRECTORY) continue;
            size_t n = strlen(name);
            if(n < 5 || strcmp(name + n - 5, PULSE_FRGL_EXT) != 0) continue;
            FuriString* full = furi_string_alloc_printf("%s/%s", PULSE_SONGS_FOLDER, name);
            FuriString* title = furi_string_alloc_set(name);
            furi_string_replace_str(title, PULSE_FRGL_EXT, "");
            songs_push(furi_string_get_cstr(full), furi_string_get_cstr(title));
            furi_string_free(full);
            furi_string_free(title);
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
}

void pulse_scene_SongSelect_on_enter(void* ctx) {
    PulseApp* app = ctx;
    songs_clear();
    for(uint8_t i = 0; i < BUILTIN_COUNT; i++) {
        songs_push(builtin_files[i], builtin_titles[i]);
    }
    songs_scan_user(app);

    Submenu* m = app->submenu;
    submenu_reset(m);
    submenu_set_header(m, "Song Select");
    for(uint8_t i = 0; i < s_song_count; i++) {
        submenu_add_item(m, furi_string_get_cstr(s_songs[i].title), i, song_select_cb, app);
    }
    submenu_set_selected_item(m, scene_manager_get_scene_state(app->scene_manager, PulseSceneSongSelect));
    view_dispatcher_switch_to_view(app->view_dispatcher, PulseViewSubmenu);
}

static uint8_t s_load_progress = 0;

static void loading_draw(Canvas* c, void* model_ctx) {
    UNUSED(model_ctx);
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 22, AlignCenter, AlignCenter, "Loading");
    /* Progress bar that fills as we walk chart_load. The caller bumps
     * s_load_progress between phases. */
    int w = s_load_progress;
    if(w > 100) w = 100;
    canvas_draw_frame(c, 14, 38, 100, 8);
    canvas_draw_box(c, 14, 38, w, 8);
    canvas_set_font(c, FontSecondary);
    char buf[12];
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)s_load_progress);
    canvas_draw_str_aligned(c, 64, 56, AlignCenter, AlignCenter, buf);
}

bool pulse_scene_SongSelect_on_event(void* ctx, SceneManagerEvent evt) {
    PulseApp* app = ctx;
    if(evt.type != SceneManagerEventTypeCustom) return false;
    if(evt.event >= s_song_count) return false;

    /* Show a real loading screen while we open and parse the chart. The
     * splash that runs at app start is just branding — actual loading work
     * happens here. */
    s_load_progress = 5;
    game_view_set_splash(app->game_view, loading_draw);
    view_dispatcher_switch_to_view(app->view_dispatcher, PulseViewGame);
    game_view_request_redraw(app->game_view);

    if(app->chart) { chart_free(app->chart); app->chart = NULL; }
    s_load_progress = 35;
    game_view_request_redraw(app->game_view);

    app->chart = chart_load(app->storage, furi_string_get_cstr(s_songs[evt.event].path));
    s_load_progress = 100;
    game_view_request_redraw(app->game_view);

    game_view_set_splash(app->game_view, NULL);

    if(!app->chart) {
        DialogEx* d = app->dialog;
        dialog_ex_reset(d);
        dialog_ex_set_header(d, "Load failed", 64, 12, AlignCenter, AlignCenter);
        dialog_ex_set_text(d, "Bad or missing\n.frgl file.", 64, 30, AlignCenter, AlignCenter);
        dialog_ex_set_center_button_text(d, "OK");
        view_dispatcher_switch_to_view(app->view_dispatcher, PulseViewDialog);
        return true;
    }
    scene_manager_next_scene(app->scene_manager, PulseSceneDifficulty);
    return true;
}

void pulse_scene_SongSelect_on_exit(void* ctx) {
    PulseApp* app = ctx;
    submenu_reset(app->submenu);
    /* don't free entries; we only repopulate on enter */
}
