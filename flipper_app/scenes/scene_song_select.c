#include "../pulse_app.h"
#include <storage/storage.h>

/* Lists every .frgl in PULSE_SONGS_FOLDER plus the embedded built-ins. */

#define BUILTIN_COUNT 8
/* Built-ins are listed in unlock order. Song N becomes selectable once song
 * N-1 has been cleared on any difficulty. The first song is always
 * unlocked. Custom user .frgl files are always unlocked. */
static const char* const builtin_files[BUILTIN_COUNT] = {
    "/builtin/pulse_drive.frgl",
    "/builtin/crystal_cascade.frgl",
    "/builtin/neon_heartbeat.frgl",
    "/builtin/glitch_garden.frgl",
    "/builtin/moonlit_drift.frgl",
    "/builtin/volt_reactor.frgl",
    "/builtin/phantom_echo.frgl",
    "/builtin/static_surge.frgl",
};
static const char* const builtin_titles[BUILTIN_COUNT] = {
    "Pulse Drive",
    "Crystal Cascade",
    "Neon Heartbeat",
    "Glitch Garden",
    "Moonlit Drift",
    "Volt Reactor",
    "Phantom Echo",
    "Static Surge",
};

typedef struct {
    FuriString* path;   /* either prefixed "/builtin/..." or absolute storage path */
    FuriString* title;
    bool        locked;
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

static void songs_push(const char* path, const char* title, bool locked) {
    if(s_song_count >= SONG_LIST_MAX) return;
    s_songs[s_song_count].path   = furi_string_alloc_set(path);
    s_songs[s_song_count].title  = furi_string_alloc_set(title);
    s_songs[s_song_count].locked = locked;
    s_song_count++;
}

/* A built-in is "cleared" if any difficulty has the cleared bit set. We treat
 * song_index here as the position in builtin_files[]. */
static bool builtin_cleared(const PulseApp* app, uint8_t i) {
    for(uint8_t d = 0; d < 4; d++) {
        const SongRecord* r = save_song(&app->save, i, d);
        if(r && (r->cleared & (1u << d))) return true;
    }
    return false;
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
            songs_push(furi_string_get_cstr(full), furi_string_get_cstr(title), false);
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
    /* First built-in is always unlocked; subsequent ones only after the prior
     * is cleared on any difficulty. */
    bool prev_cleared = true;
    for(uint8_t i = 0; i < BUILTIN_COUNT; i++) {
        bool locked = !prev_cleared;
        FuriString* label = furi_string_alloc_set(builtin_titles[i]);
        if(locked) furi_string_cat_str(label, "  [LOCKED]");
        songs_push(builtin_files[i], furi_string_get_cstr(label), locked);
        furi_string_free(label);
        prev_cleared = builtin_cleared(app, i);
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

    if(s_songs[evt.event].locked) {
        DialogEx* d = app->dialog;
        dialog_ex_reset(d);
        dialog_ex_set_header(d, "Locked", 64, 12, AlignCenter, AlignCenter);
        dialog_ex_set_text(d, "Clear the previous\nsong to unlock.", 64, 30, AlignCenter, AlignCenter);
        dialog_ex_set_center_button_text(d, "OK");
        view_dispatcher_switch_to_view(app->view_dispatcher, PulseViewDialog);
        return true;
    }

    /* Show a real loading screen while we open and parse the chart. The
     * splash override stays in place until on_exit clears it — clearing it
     * here would unmask the (still-uninitialised) game model and crash
     * draw_callback before the next scene switches the view away. */
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
    /* Clear the loading-screen splash now that the next scene has switched
     * the view away from PulseViewGame. */
    game_view_set_splash(app->game_view, NULL);
    /* don't free entries; we only repopulate on enter */
}
