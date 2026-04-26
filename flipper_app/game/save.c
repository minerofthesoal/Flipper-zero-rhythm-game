#include "save.h"

#define SAVE_MAGIC   0x50534156u  /* 'PSAV' */
#define SAVE_VERSION 1
#define SAVE_PATH    APP_DATA_PATH("pulse.save")

static void save_defaults(SaveData* s) {
    memset(s, 0, sizeof(*s));
    s->magic        = SAVE_MAGIC;
    s->version      = SAVE_VERSION;
    s->offset_ms    = 0;
    s->volume       = 60;
    s->scroll_speed = 10;
    s->vgm_enabled  = 0;
    s->vibrate      = 1;
    s->character_id = 0;
}

void save_load(SaveData* s, Storage* storage) {
    save_defaults(s);
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        SaveData on_disk;
        uint16_t got = storage_file_read(f, &on_disk, sizeof(on_disk));
        if(got == sizeof(on_disk) &&
           on_disk.magic == SAVE_MAGIC && on_disk.version == SAVE_VERSION) {
            *s = on_disk;
        }
    }
    storage_file_close(f);
    storage_file_free(f);
}

void save_store(const SaveData* s, Storage* storage) {
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, s, sizeof(*s));
    }
    storage_file_close(f);
    storage_file_free(f);
}

static uint16_t hash_index(uint32_t song_index, uint8_t diff) {
    /* tiny FNV-1a over (index, diff) — enough for a 32-slot table */
    uint16_t h = 0x811cu;
    h = (h ^ (uint8_t)(song_index)) * 0x9101u;
    h = (h ^ (uint8_t)(song_index >> 8)) * 0x9101u;
    h = (h ^ diff) * 0x9101u;
    if(h == 0) h = 1;
    return h;
}

void save_record_score(SaveData* s, uint32_t song_index, uint8_t diff,
                       uint32_t score, uint16_t combo, uint8_t cleared) {
    uint16_t hash = hash_index(song_index, diff);
    uint8_t  slot = hash % SAVE_SONG_SLOTS;
    SongRecord* r = &s->songs[slot];
    bool same = (r->song_hash == hash) && (r->difficulty == diff);
    if(!same) {
        r->song_hash  = hash;
        r->difficulty = diff;
        r->best_score = 0;
        r->best_combo = 0;
        r->cleared    = 0;
        r->fc         = 0;
    }
    if(score > r->best_score) r->best_score = score;
    if(combo > r->best_combo) r->best_combo = combo;
    if(cleared) r->cleared |= (1u << diff);
    s->total_plays++;
    s->total_score += score;
}

const SongRecord* save_song(const SaveData* s, uint32_t song_index, uint8_t diff) {
    uint16_t hash = hash_index(song_index, diff);
    uint8_t  slot = hash % SAVE_SONG_SLOTS;
    const SongRecord* r = &s->songs[slot];
    if(r->song_hash == hash && r->difficulty == diff) return r;
    return NULL;
}
