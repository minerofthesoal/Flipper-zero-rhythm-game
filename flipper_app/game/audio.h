#pragma once

#include <furi.h>
#include "chart.h"
#include "judge.h"

typedef struct {
    bool     speaker_held;
    uint8_t  volume;        /* 0..100 */
    uint32_t cursor;        /* into chart audio array */
    uint32_t tone_off_ms;   /* when current tone should stop */
    uint16_t current_freq;
} AudioEngine;

void audio_init(AudioEngine* a, uint8_t volume);
void audio_deinit(AudioEngine* a);
void audio_reset(AudioEngine* a);
void audio_set_volume(AudioEngine* a, uint8_t v);
void audio_silence(AudioEngine* a);

/* Schedule audio against the chart. Call frequently from tick. */
void audio_tick(AudioEngine* a, const Chart* chart, uint32_t time_ms);

/* Tiny click sound when the player hits a note. */
void audio_click(AudioEngine* a, JudgeResult r);
