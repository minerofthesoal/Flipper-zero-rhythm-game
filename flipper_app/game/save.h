#pragma once

#include <furi.h>
#include <storage/storage.h>

#define SAVE_SONG_SLOTS 32

typedef struct {
    /* per-(song,diff) records.
     * song slot is hashed from path (FNV-1a 16-bit modulo SAVE_SONG_SLOTS).
     * Collisions just overwrite the older entry. */
    uint16_t song_hash;        /* 0 = empty */
    uint8_t  difficulty;
    uint8_t  cleared;          /* bitmask of difficulties cleared */
    uint32_t best_score;
    uint16_t best_combo;
    uint8_t  fc;               /* full-combo flag */
} SongRecord;

typedef struct {
    uint32_t magic;            /* 'PSAV' */
    uint16_t version;
    int16_t  offset_ms;
    uint8_t  volume;
    uint8_t  scroll_speed;
    uint8_t  vgm_enabled;
    uint8_t  vibrate;
    uint8_t  character_id;
    /* RGB backlight: light the notification LED on hits — handy on Flipper
     * mods with the colour LED swap. Default 0 (off). Reuses the previously
     * reserved byte so the save format stays at version 1. */
    uint8_t  rgb_enabled;

    uint32_t total_perfects;
    uint32_t total_plays;
    uint32_t total_score;

    SongRecord songs[SAVE_SONG_SLOTS];
} SaveData;

void save_load(SaveData* s, Storage* storage);
void save_store(const SaveData* s, Storage* storage);
void save_record_score(SaveData* s, uint32_t song_index, uint8_t diff,
                       uint32_t score, uint16_t combo, uint8_t cleared);
const SongRecord* save_song(const SaveData* s, uint32_t song_index, uint8_t diff);
