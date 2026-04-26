#pragma once

#include "chart.h"
#include "judge.h"

/* Pulse's anomaly system — a recollection-style "Succession Gauge" rises with
 * combos and falls with misses. While the gauge is high enough, scheduled
 * anomaly windows in the chart "rewrite themselves": the chart is mirrored,
 * sped up, infested with extra chains, etc. The result is that good players
 * get harder charts; weaker players get an easier ride. */

typedef struct {
    int16_t  gauge;          /* 0..100 */
    uint8_t  active_index;   /* into chart anomaly list */
    bool     is_active;
    uint32_t since_active_ms;
} AnomalyState;

struct CharacterState;

void anomaly_init(AnomalyState* s);
void anomaly_reset(AnomalyState* s);
void anomaly_on_judge(AnomalyState* s, JudgeResult r, struct CharacterState* character);
void anomaly_tick(AnomalyState* s, const Chart* chart, uint32_t time_ms,
                  AnomalyMod* out_mod, uint8_t* out_arg, uint32_t* out_flash_until);
const char* anomaly_mod_name(AnomalyMod m);
