#pragma once

#include <furi.h>
#include <storage/storage.h>

typedef enum {
    NoteTap   = 'T',
    NoteHold  = 'H',
    NoteBurst = 'B',
    NoteSlide = 'S',
    NoteFake  = 'F',
    NoteChain = 'X',
} NoteType;

typedef struct {
    uint32_t time_ms;
    uint8_t  lane;       /* 0..3 */
    uint8_t  type;       /* NoteType */
    uint16_t arg;        /* hold-duration / burst-count / slide-dest-lane */

    /* runtime */
    uint8_t  hit;        /* 0=pending 1=hit 2=missed 3=held */
    uint8_t  burst_left; /* burst remaining taps */
} Note;

typedef struct {
    uint16_t level;       /* level * 10 (e.g., 5.5 = 55) */
    uint32_t note_count;
    Note*    notes;
} ChartDiff;

typedef enum {
    AudioToneRest = 0,
} AudioFlags;

typedef struct {
    uint32_t time_ms;
    uint16_t freq_hz;
    uint16_t duration_ms;
    uint8_t  volume;
} AudioTone;

typedef enum {
    AnomalyModNone   = 0,
    AnomalyModSpeed,
    AnomalyModMirror,
    AnomalyModInvert,
    AnomalyModChaos,
    AnomalyModAdd,
    AnomalyModGhost,
    AnomalyModStorm,
} AnomalyMod;

typedef struct {
    uint8_t    threshold;
    uint32_t   start_ms;
    uint32_t   end_ms;
    AnomalyMod mod;
    uint16_t   arg;
} AnomalyEntry;

typedef enum {
    EventBg  = 0,
    EventFx  = 1,
    EventTxt = 2,
} EventType;

typedef struct {
    uint32_t  time_ms;
    EventType type;
    char      data[16]; /* small fixed buffer */
} EventEntry;

typedef struct Chart Chart;

Chart* chart_load(Storage* storage, const char* path);
void   chart_free(Chart* chart);

const char* chart_title(const Chart* c);
const char* chart_artist(const Chart* c);
uint32_t    chart_length(const Chart* c);
uint32_t    chart_bpm_x100(const Chart* c);
const ChartDiff* chart_difficulty(const Chart* c, int diff);

/* Anomaly + audio + events */
const AnomalyEntry* chart_anomalies(const Chart* c, uint32_t* count);
const AudioTone*    chart_audio(const Chart* c, uint32_t* count);
const EventEntry*   chart_events(const Chart* c, uint32_t* count);

/* Built-in chart support: paths starting with "/builtin/" are resolved from a
 * compile-time table rather than the storage layer. */
bool chart_path_is_builtin(const char* path);
