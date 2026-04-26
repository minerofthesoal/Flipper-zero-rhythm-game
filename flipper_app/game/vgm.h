#pragma once

#include <furi.h>

/* Wraps the Flipper Video Game Module (RP2040 + composite/VGA out) so the
 * gameplay can be mirrored to a 320x240 external display. The module is
 * entirely optional — when absent or disabled, every public function below
 * silently no-ops, keeping the gameplay code free of #ifdefs.
 *
 * On firmwares that ship the upstream `vgm` driver, vgm.c calls into it. On
 * firmwares that don't, vgm.c falls back to a NULL pipe so the build still
 * succeeds. The public ABI is identical either way. */

typedef struct {
    bool     opened;
    bool     available;        /* hardware actually plugged in */
    uint32_t last_render_ms;
} VgmDisplay;

void vgm_init(VgmDisplay* v);
void vgm_deinit(VgmDisplay* v);
bool vgm_open(VgmDisplay* v);    /* attempt to open the device */
void vgm_close(VgmDisplay* v);

/* Render the current game model onto the external screen. The model pointer
 * is opaque here (it's `GameModel*`) — vgm.c casts internally. Called from
 * the game tick. */
void vgm_render(VgmDisplay* v, void* game_model, uint32_t time_ms);
