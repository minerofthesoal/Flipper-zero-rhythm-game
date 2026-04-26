#include "audio.h"
#include <furi_hal_speaker.h>

/* Convert percent (0..100) to the 0.0..1.0 range furi_hal_speaker_start
 * expects, staying entirely in float so -Wdouble-promotion doesn't bite. */
static float vol_to_float(uint8_t v) {
    float f = (float)v * (1.0f / 100.0f);
    if(f < 0.0f) f = 0.0f;
    if(f > 1.0f) f = 1.0f;
    return f;
}

static bool acquire(AudioEngine* a) {
    if(a->speaker_held) return true;
    /* Wait up to ~250ms — the splash sequence at app start can briefly hold
     * the speaker, so a short retry budget makes the first tone reliable. */
    if(!furi_hal_speaker_acquire(250)) return false;
    a->speaker_held = true;
    return true;
}

static void stop_tone(AudioEngine* a) {
    if(a->speaker_held && a->current_freq != 0) {
        furi_hal_speaker_stop();
    }
    a->current_freq = 0;
}

static void play_tone_with_base(AudioEngine* a, uint16_t freq,
                                uint8_t per_note_volume, uint8_t base_volume) {
    if(freq == 0 || base_volume == 0) { stop_tone(a); return; }
    if(!acquire(a)) return;
    uint16_t mixed = ((uint16_t)base_volume * per_note_volume) / 100;
    if(mixed == 0) { stop_tone(a); return; }
    if(a->current_freq != 0) furi_hal_speaker_stop();
    furi_hal_speaker_start((float)freq, vol_to_float((uint8_t)mixed));
    a->current_freq = freq;
}

void audio_init(AudioEngine* a, uint8_t sfx_volume, uint8_t music_volume) {
    memset(a, 0, sizeof(*a));
    a->volume = sfx_volume;
    a->music_volume = music_volume;
}

void audio_deinit(AudioEngine* a) {
    audio_silence(a);
}

void audio_reset(AudioEngine* a) {
    /* Stop any tone but keep the speaker handle so the first chart tone after
     * the lead-in can fire without an acquire-hiccup. */
    stop_tone(a);
    a->cursor = 0;
    a->tone_off_ms = 0;
}

void audio_set_volume(AudioEngine* a, uint8_t v)       { a->volume = v; }
void audio_set_music_volume(AudioEngine* a, uint8_t v) { a->music_volume = v; }

void audio_silence(AudioEngine* a) {
    stop_tone(a);
    if(a->speaker_held) {
        furi_hal_speaker_release();
        a->speaker_held = false;
    }
    a->tone_off_ms = 0;
}

void audio_tick(AudioEngine* a, const Chart* chart, uint32_t time_ms) {
    /* End the currently-sustaining tone when its window closes. */
    if(a->tone_off_ms && time_ms >= a->tone_off_ms) {
        stop_tone(a);
        a->tone_off_ms = 0;
    }

    uint32_t count = 0;
    const AudioTone* tones = chart_audio(chart, &count);
    /* Walk forward through any tones whose time has come. We may have skipped
     * past several during a slow tick, so loop until caught up. */
    while(a->cursor < count) {
        const AudioTone* tn = &tones[a->cursor];
        if(tn->time_ms > time_ms) break;
        if(tn->freq_hz > 0 && tn->volume > 0) {
            /* Chart tones use the music volume slider. */
            play_tone_with_base(a, tn->freq_hz, tn->volume, a->music_volume);
            a->tone_off_ms = tn->time_ms + tn->duration_ms;
        } else {
            stop_tone(a);
            a->tone_off_ms = 0;
        }
        a->cursor++;
    }
}

void audio_click(AudioEngine* a, JudgeResult r, uint8_t note_type) {
    /* Short pitched blip on a hit. Each note type has its own pitch family so
     * the player can hear the difference between a tap, a hold, a chain, etc.
     * Pitch is bumped a bit on a Perfect, dropped on a Good. */
    if(r == JudgeNone || r == JudgeMiss) return;
    if(a->volume == 0) return;
    uint16_t base;
    switch(note_type) {
        case NoteHold:  base = 880;  break;   /* A5  — round tone for sustains */
        case NoteBurst: base = 1976; break;   /* B6  — bright rapid clicks */
        case NoteSlide: base = 1568; break;   /* G6  — gliss-friendly */
        case NoteChain: base = 660;  break;   /* E5  — soft pulse */
        case NoteFake:  base = 220;  break;   /* A3  — low penalty thud */
        case NoteTap:
        default:        base = 1320; break;   /* E6  — default tap */
    }
    if(r == JudgePerfect)               base = (uint16_t)(base * 11 / 10);
    else if(r == JudgeGood)             base = (uint16_t)(base * 9  / 10);
    /* Early/Late are even softer than Good — drop pitch and per-note
     * volume so the player hears that they were off without the click
     * masking the music. */
    uint8_t per_note_vol = 60;
    if(r == JudgeEarly || r == JudgeLate) {
        base = (uint16_t)(base * 4 / 5);
        per_note_vol = 35;
    }
    /* Clicks use the SFX volume so the player can balance them against the
     * music independently. */
    play_tone_with_base(a, base, per_note_vol, a->volume);
    a->tone_off_ms = 0;
}
