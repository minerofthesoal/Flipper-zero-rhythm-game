#pragma once

#include <furi.h>
#include "chart.h"
#include "judge.h"

typedef struct {
    bool     speaker_held;
    uint8_t  volume;        /* SFX / click volume, 0..100 */
    uint8_t  music_volume;  /* chart-tone volume, 0..100 */
    uint32_t cursor;        /* into chart audio array */
    uint32_t tone_off_ms;   /* when current tone should stop */
    uint16_t current_freq;
} AudioEngine;

void audio_init(AudioEngine* a, uint8_t sfx_volume, uint8_t music_volume);
void audio_deinit(AudioEngine* a);
void audio_reset(AudioEngine* a);
void audio_set_volume(AudioEngine* a, uint8_t v);            /* SFX */
void audio_set_music_volume(AudioEngine* a, uint8_t v);
void audio_silence(AudioEngine* a);

/* Schedule audio against the chart. Call frequently from tick. */
void audio_tick(AudioEngine* a, const Chart* chart, uint32_t time_ms);

/* Tiny click sound when the player hits a note. note_type is the chart's
 * NoteType (Tap/Hold/Burst/Slide/Chain/Fake) so each kind can have its own
 * audible signature. */
void audio_click(AudioEngine* a, JudgeResult r, uint8_t note_type);
