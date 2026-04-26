#include "audio.h"
#include <furi_hal_speaker.h>

static float vol_to_float(uint8_t v) { return (float)v / 100.0f; }

void audio_init(AudioEngine* a, uint8_t volume) {
    memset(a, 0, sizeof(*a));
    a->volume = volume;
}

void audio_deinit(AudioEngine* a) {
    audio_silence(a);
}

void audio_reset(AudioEngine* a) {
    audio_silence(a);
    a->cursor = 0;
    a->tone_off_ms = 0;
    a->current_freq = 0;
}

void audio_set_volume(AudioEngine* a, uint8_t v) { a->volume = v; }

void audio_silence(AudioEngine* a) {
    if(a->speaker_held) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
        a->speaker_held = false;
    }
    a->current_freq = 0;
    a->tone_off_ms = 0;
}

static void start_tone(AudioEngine* a, uint16_t freq) {
    if(freq == 0) { audio_silence(a); return; }
    if(!a->speaker_held) {
        if(!furi_hal_speaker_acquire(30)) return;
        a->speaker_held = true;
    }
    furi_hal_speaker_start((float)freq, vol_to_float(a->volume));
    a->current_freq = freq;
}

void audio_tick(AudioEngine* a, const Chart* chart, uint32_t time_ms) {
    /* turn off finished tones */
    if(a->tone_off_ms && time_ms >= a->tone_off_ms) {
        audio_silence(a);
    }

    /* schedule new tones */
    uint32_t count = 0;
    const AudioTone* tones = chart_audio(chart, &count);
    while(a->cursor < count) {
        const AudioTone* t = &tones[a->cursor];
        if(t->time_ms > time_ms) break;
        /* start this tone */
        if(t->volume > 0) {
            uint8_t saved = a->volume;
            a->volume = (a->volume * t->volume) / 100;
            start_tone(a, t->freq_hz);
            a->volume = saved;
        } else {
            audio_silence(a);
        }
        a->tone_off_ms = t->time_ms + t->duration_ms;
        a->cursor++;
    }
}

void audio_click(AudioEngine* a, JudgeResult r) {
    /* extremely short click overlaid on top — abuse the same channel for a
     * few ms then return to whatever was scheduled. */
    UNUSED(a); UNUSED(r);
    /* No-op when chart audio is playing — adding a click would cut the tone.
     * We rely on haptic feedback instead. Empty intentionally. */
}
