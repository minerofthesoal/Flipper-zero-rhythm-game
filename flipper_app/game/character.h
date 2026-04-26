#pragma once

#include <furi.h>

typedef enum {
    CharacterNull   = 0,  /* default — neutral, no buffs */
    CharacterMira   = 1,  /* score-focused */
    CharacterAxon   = 2,  /* tank — slower drain */
    CharacterGlitch = 3,  /* anomaly amplifier */
    CharacterSolenne= 4,  /* training wheels — no fail */
    CharacterCount,
} CharacterId;

typedef struct {
    const char* name;
    const char* blurb;
    /* gauge multiplier on gain (x100) */
    uint16_t gauge_gain_x100;
    /* gauge multiplier on loss (x100; lower = tankier) */
    uint16_t gauge_loss_x100;
    /* score multiplier per perfect-streak bracket */
    uint16_t score_bonus_x100;
    /* anomaly threshold offset (negative = anomalies fire earlier) */
    int8_t   anomaly_threshold_delta;
    /* characters that can't fail (training mode) */
    bool     no_fail;
} CharacterDef;

typedef struct {
    CharacterId id;
} CharacterState;

void                   character_init(CharacterState* s, uint8_t saved_id);
void                   character_set(CharacterState* s, CharacterId id);
const CharacterDef*    character_def(CharacterId id);
