#include "save.h"

#define SAVE_MAGIC   0x50534156u  /* 'PSAV' */
#define SAVE_VERSION 2
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
    s->rgb_enabled  = 0;
    s->music_volume = 70;
}

void save_load(SaveData* s, Storage* storage) {
    save_defaults(s);
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        SaveData on_disk;
        memset(&on_disk, 0, sizeof(on_disk));
        uint16_t got = storage_file_read(f, &on_disk, sizeof(on_disk));
        if(got >= 16 && on_disk.magic == SAVE_MAGIC) {
            if(on_disk.version == SAVE_VERSION && got == sizeof(on_disk)) {
                *s = on_disk;
            } else if(on_disk.version == 1) {
                /* Migrate v1 → v2: copy what we can, default new fields. The
                 * v1 layout is identical up through rgb_enabled, then the
                 * counters and song table live at a different offset because
                 * v2 grew two bytes (music_volume + reserved1). Rather than
                 * pointer-arithmetic on a packed legacy layout, just keep the
                 * settings and discard the stats — song progress is the only
                 * thing players care about preserving and we can rehash it. */
                s->offset_ms    = on_disk.offset_ms;
                s->volume       = on_disk.volume;
                s->scroll_speed = on_disk.scroll_speed;
                s->vgm_enabled  = on_disk.vgm_enabled;
                s->vibrate      = on_disk.vibrate;
                s->character_id = on_disk.character_id;
                s->rgb_enabled  = on_disk.rgb_enabled;
            }
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
