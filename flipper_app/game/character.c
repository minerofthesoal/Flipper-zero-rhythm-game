#include "character.h"

static const CharacterDef defs[CharacterCount] = {
    [CharacterNull] = {
        .name = "Null",
        .blurb = "balanced",
        .gauge_gain_x100 = 100,
        .gauge_loss_x100 = 100,
        .score_bonus_x100 = 100,
        .anomaly_threshold_delta = 0,
        .no_fail = false,
    },
    [CharacterMira] = {
        .name = "Mira",
        .blurb = "score+10%",
        .gauge_gain_x100 = 90,
        .gauge_loss_x100 = 110,
        .score_bonus_x100 = 110,
        .anomaly_threshold_delta = +5,
        .no_fail = false,
    },
    [CharacterAxon] = {
        .name = "Axon",
        .blurb = "tank, half drain",
        .gauge_gain_x100 = 80,
        .gauge_loss_x100 = 50,
        .score_bonus_x100 = 90,
        .anomaly_threshold_delta = +10,
        .no_fail = false,
    },
    [CharacterGlitch] = {
        .name = "Glitch",
        .blurb = "anomalies +",
        .gauge_gain_x100 = 120,
        .gauge_loss_x100 = 120,
        .score_bonus_x100 = 100,
        .anomaly_threshold_delta = -15,
        .no_fail = false,
    },
    [CharacterSolenne] = {
        .name = "Solenne",
        .blurb = "no-fail",
        .gauge_gain_x100 = 70,
        .gauge_loss_x100 = 70,
        .score_bonus_x100 = 70,
        .anomaly_threshold_delta = 0,
        .no_fail = true,
    },
};

void character_init(CharacterState* s, uint8_t saved_id) {
    if(saved_id >= CharacterCount) saved_id = CharacterNull;
    s->id = (CharacterId)saved_id;
}

void character_set(CharacterState* s, CharacterId id) {
    if(id >= CharacterCount) id = CharacterNull;
    s->id = id;
}

const CharacterDef* character_def(CharacterId id) {
    if(id >= CharacterCount) id = CharacterNull;
    return &defs[id];
}
