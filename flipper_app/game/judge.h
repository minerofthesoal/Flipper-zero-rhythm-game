#pragma once

#include "chart.h"

typedef enum {
    JudgeNone = 0,
    JudgePerfect,
    JudgeGreat,
    JudgeGood,
    JudgeMiss,
} JudgeResult;

typedef struct {
    uint32_t score;        /* 0..1,000,000 normalised */
    uint32_t raw_score;    /* sum of per-note points */
    uint32_t raw_max;      /* max possible raw given chart density */

    uint16_t combo;
    uint16_t max_combo;

    uint16_t perfects;
    uint16_t greats;
    uint16_t goods;
    uint16_t misses;
    uint16_t fakes_hit;

    uint32_t cursor;       /* note index to scan from */
    bool     failed;
} Judge;

struct AnomalyState;
struct CharacterState;

void   judge_reset(Judge* j);
JudgeResult judge_press(Judge* j, const ChartDiff* d, uint8_t lane,
                        uint32_t now_ms, uint32_t great_ms, uint32_t good_ms,
                        AnomalyMod active_mod);
void   judge_release(Judge* j, const ChartDiff* d, uint8_t lane,
                     uint32_t now_ms, uint32_t great_ms, uint32_t good_ms);
uint32_t judge_advance(Judge* j, const ChartDiff* d, uint32_t now_ms,
                       uint32_t miss_ms, struct AnomalyState* anomaly,
                       struct CharacterState* character);
bool   judge_should_fail(const Judge* j, const struct CharacterState* character);
void   judge_finalise(Judge* j);
