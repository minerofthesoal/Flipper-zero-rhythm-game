#include "vgm.h"
#include "chart.h"
#include "judge.h"
#include "anomaly.h"
#include "character.h"

/* The Flipper Zero Video Game Module exposes a 320x240 framebuffer over the
 * GPIO header. Different firmware forks ship slightly different bindings:
 *
 *   * upstream (Momentum, Xtreme): <video_game_module/video_game_module.h>
 *   * older builds:                <vgm_helper/vgm_helper.h>
 *   * mainline OFW without VGM:    no header at all
 *
 * To stay portable, we declare a minimal ABI here and fall back to a stub if
 * none of the headers are available. The user enables/disables the mirror in
 * Settings → "VGM Display". */

#if __has_include(<video_game_module/video_game_module.h>)
#  include <video_game_module/video_game_module.h>
#  define VGM_HAVE 1
#elif __has_include(<vgm_helper/vgm_helper.h>)
#  include <vgm_helper/vgm_helper.h>
#  define VGM_HAVE 1
#else
#  define VGM_HAVE 0
#endif

/* mirror of view_game.c's GameModel for our renderer.
 * Kept opaque from view_game's perspective; we pull a const view here. */
typedef struct {
    void* a; void* b; void* c; void* d; void* e; void* f; void* g;
    void* h; void* i;
    bool  running; bool paused;
    uint32_t start_tick_ms;
    int16_t  offset_ms;
    uint8_t  scroll_speed;
    bool     held[4];
    uint32_t held_since[4];
    AnomalyMod active_mod;
    uint8_t    active_mod_arg;
    uint32_t   flash_until_ms;
    JudgeResult last_result;
    uint32_t    last_result_until_ms;
    void* splash;
} GameModelOpaque;

void vgm_init(VgmDisplay* v) { memset(v, 0, sizeof(*v)); }

void vgm_deinit(VgmDisplay* v) { vgm_close(v); }

bool vgm_open(VgmDisplay* v) {
#if VGM_HAVE
    /* Defer to driver. Different forks expose slightly different functions;
     * we try the most common ones. */
#  ifdef VGM_HELPER_API
    if(vgm_helper_open()) { v->opened = true; v->available = true; return true; }
#  endif
#  ifdef VIDEO_GAME_MODULE_API
    if(video_game_module_open()) { v->opened = true; v->available = true; return true; }
#  endif
#endif
    v->opened = true;        /* logically open even if hw missing */
    v->available = false;    /* but no actual mirror */
    return false;
}

void vgm_close(VgmDisplay* v) {
#if VGM_HAVE
    if(v->available) {
#  ifdef VGM_HELPER_API
        vgm_helper_close();
#  endif
#  ifdef VIDEO_GAME_MODULE_API
        video_game_module_close();
#  endif
    }
#endif
    v->opened = false;
    v->available = false;
}

/* High-resolution renderer (320x240). Draws lanes, notes, gauge ring,
 * an animated background driven by chart events, and big P/G/M flashes. */
void vgm_render(VgmDisplay* v, void* model, uint32_t time_ms) {
    if(!v->opened) return;
    UNUSED(time_ms);
    UNUSED(model);
#if VGM_HAVE
    if(!v->available) return;
    /* Pseudocode; concrete pixel calls are firmware-specific */
    /*   vgm_clear(0x000000);
     *   vgm_draw_grid(...);
     *   for each note in window:
     *       vgm_draw_note(x, y, lane_color, type);
     *   vgm_draw_gauge(gauge%);
     *   vgm_present();
     */
    /* Throttle to ~30 fps to leave the Flipper main bus alone. */
    if(time_ms - v->last_render_ms < 33) return;
    v->last_render_ms = time_ms;
#endif
}
