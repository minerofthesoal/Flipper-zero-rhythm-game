#include "chart.h"
#include "../songs/builtin_songs.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_NOTES_PER_DIFF 4096   /* hard ceiling — defends against malformed files */
#define INIT_NOTES_PER_DIFF 64    /* start small; grow on demand */
#define MAX_AUDIO_TONES    2048
#define MAX_ANOMALIES      32
#define MAX_EVENTS         64

struct Chart {
    char        title[33];
    char        artist[33];
    uint32_t    bpm_x100;
    int32_t     offset_ms;
    uint32_t    length_ms;
    uint32_t    preview_ms;
    char        author[33];
    char        character[16];

    ChartDiff   diff[4];           /* easy, normal, fun, nitro */
    uint32_t    diff_capacity[4];  /* internal grow cap, hidden from view */

    AnomalyEntry anomalies[MAX_ANOMALIES];
    uint32_t     anomaly_count;
    AudioTone    audio[MAX_AUDIO_TONES];
    uint32_t     audio_count;
    EventEntry   events[MAX_EVENTS];
    uint32_t     event_count;
};

/* ----------- string utils ------------------------------------------------ */

static void rstrip(char* s) {
    size_t n = strlen(s);
    while(n && (s[n-1] == '\r' || s[n-1] == '\n' || s[n-1] == ' ' || s[n-1] == '\t')) {
        s[--n] = 0;
    }
}

static void copy_str(char* dst, size_t cap, const char* src) {
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = 0;
}

static int diff_index(const char* name) {
    if(strcmp(name, "easy")   == 0) return 0;
    if(strcmp(name, "normal") == 0) return 1;
    if(strcmp(name, "fun")    == 0) return 2;
    if(strcmp(name, "nitro")  == 0) return 3;
    return -1;
}

static AnomalyMod parse_mod(const char* s) {
    if(strcmp(s, "SPEED")  == 0) return AnomalyModSpeed;
    if(strcmp(s, "MIRROR") == 0) return AnomalyModMirror;
    if(strcmp(s, "INVERT") == 0) return AnomalyModInvert;
    if(strcmp(s, "CHAOS")  == 0) return AnomalyModChaos;
    if(strcmp(s, "ADD")    == 0) return AnomalyModAdd;
    if(strcmp(s, "GHOST")  == 0) return AnomalyModGhost;
    if(strcmp(s, "STORM")  == 0) return AnomalyModStorm;
    return AnomalyModNone;
}

static EventType parse_event_type(const char* s) {
    if(strcmp(s, "BG") == 0)  return EventBg;
    if(strcmp(s, "FX") == 0)  return EventFx;
    if(strcmp(s, "TXT") == 0) return EventTxt;
    return EventFx;
}

/* ----------- line streaming --------------------------------------------- */

typedef struct {
    /* one of these is non-null */
    File*       file;
    const char* mem;
    size_t      mem_len;
    size_t      mem_pos;
} LineSource;

static bool ls_open_file(LineSource* ls, Storage* st, const char* path) {
    memset(ls, 0, sizeof(*ls));
    ls->file = storage_file_alloc(st);
    if(!storage_file_open(ls->file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(ls->file);
        return false;
    }
    return true;
}

static void ls_open_mem(LineSource* ls, const char* data, size_t len) {
    memset(ls, 0, sizeof(*ls));
    ls->mem = data;
    ls->mem_len = len;
}

static void ls_close(LineSource* ls) {
    if(ls->file) {
        storage_file_close(ls->file);
        storage_file_free(ls->file);
    }
}

static bool ls_read_line(LineSource* ls, char* out, size_t cap) {
    size_t i = 0;
    if(ls->file) {
        char ch;
        for(;;) {
            uint16_t got = storage_file_read(ls->file, &ch, 1);
            if(got == 0) {
                if(i == 0) return false;
                break;
            }
            if(ch == '\n') break;
            if(i + 1 < cap) out[i++] = ch;
        }
    } else {
        if(ls->mem_pos >= ls->mem_len) return false;
        for(; ls->mem_pos < ls->mem_len; ls->mem_pos++) {
            char ch = ls->mem[ls->mem_pos];
            if(ch == '\n') { ls->mem_pos++; break; }
            if(i + 1 < cap) out[i++] = ch;
        }
    }
    out[i] = 0;
    rstrip(out);
    return true;
}

/* ----------- parser ----------------------------------------------------- */

/* Parse a fixed-point decimal "<int>[.<frac>]" into an integer scaled by
 * `scale` (e.g. 100 for hundredths). atof is disabled in the firmware API
 * and we don't want to drag in floating point just to read a BPM. */
static uint32_t parse_fixed(const char* v, uint32_t scale) {
    char* end;
    uint32_t whole = (uint32_t)strtoul(v, &end, 10);
    uint32_t result = whole * scale;
    if(*end == '.') {
        end++;
        uint32_t frac = 0, mult = scale;
        while(*end >= '0' && *end <= '9' && mult > 1) {
            mult /= 10;
            frac += (uint32_t)(*end - '0') * mult;
            end++;
        }
        result += frac;
    }
    return result;
}

static void parse_meta(Chart* c, char* line) {
    char* eq = strchr(line, '=');
    if(!eq) return;
    *eq = 0;
    const char* k = line;
    const char* v = eq + 1;
    if(strcmp(k, "title")     == 0) copy_str(c->title,   sizeof(c->title),   v);
    else if(strcmp(k, "artist")    == 0) copy_str(c->artist,  sizeof(c->artist),  v);
    else if(strcmp(k, "bpm")       == 0) c->bpm_x100 = parse_fixed(v, 100);
    else if(strcmp(k, "offset")    == 0) c->offset_ms = atoi(v);
    else if(strcmp(k, "length")    == 0) c->length_ms = (uint32_t)atoi(v);
    else if(strcmp(k, "preview")   == 0) c->preview_ms = (uint32_t)atoi(v);
    else if(strcmp(k, "author")    == 0) copy_str(c->author,    sizeof(c->author),    v);
    else if(strcmp(k, "character") == 0) copy_str(c->character, sizeof(c->character), v);
}

static uint16_t parse_level(const char* v) {
    /* "5", "5+", "5.5", "11" — store as level*10. "+" is 0.5. */
    char* dot = strchr((char*)v, '.');
    char* plus = strchr((char*)v, '+');
    if(dot) return (uint16_t)parse_fixed(v, 10);
    int n = atoi(v);
    return (uint16_t)(n * 10 + (plus ? 5 : 0));
}

static bool ensure_diff_capacity(Chart* c, int diff) {
    ChartDiff* d = &c->diff[diff];
    if(d->note_count < c->diff_capacity[diff]) return true;
    uint32_t new_cap = c->diff_capacity[diff] ? c->diff_capacity[diff] * 2 : INIT_NOTES_PER_DIFF;
    if(new_cap > MAX_NOTES_PER_DIFF) new_cap = MAX_NOTES_PER_DIFF;
    if(d->note_count >= new_cap) return false;
    Note* grown = realloc(d->notes, sizeof(Note) * new_cap);
    if(!grown) return false;
    d->notes = grown;
    c->diff_capacity[diff] = new_cap;
    return true;
}

static void parse_note(Chart* c, int diff, char* line) {
    if(!ensure_diff_capacity(c, diff)) return;
    ChartDiff* d = &c->diff[diff];
    /* time_ms,lane,type[,arg] */
    char* p = line;
    uint32_t time_ms = (uint32_t)strtoul(p, &p, 10);
    if(*p != ',') return;
    p++;
    uint8_t lane = (uint8_t)strtoul(p, &p, 10);
    if(*p != ',') return;
    p++;
    char tch = *p++;
    uint16_t arg = 0;
    if(*p == ',') { p++; arg = (uint16_t)strtoul(p, NULL, 10); }
    Note* nt = &d->notes[d->note_count++];
    nt->time_ms = time_ms;
    nt->lane    = lane & 3;
    nt->type    = (uint8_t)tch;
    nt->arg     = arg;
    nt->hit     = 0;
    nt->burst_left = (tch == NoteBurst) ? (uint8_t)arg : 0;
}

static void parse_anomaly(Chart* c, char* line) {
    if(c->anomaly_count >= MAX_ANOMALIES) return;
    char* p = line;
    uint8_t  th = (uint8_t)strtoul(p, &p, 10);   if(*p != ',') return; p++;
    uint32_t s  = (uint32_t)strtoul(p, &p, 10);  if(*p != ',') return; p++;
    uint32_t e  = (uint32_t)strtoul(p, &p, 10);  if(*p != ',') return; p++;

    char tag[16] = {0};
    int  i = 0;
    while(*p && *p != ',' && i + 1 < (int)sizeof(tag)) tag[i++] = *p++;
    tag[i] = 0;
    uint16_t arg = 0;
    if(*p == ',') { p++; arg = (uint16_t)strtoul(p, NULL, 10); }
    AnomalyEntry* a = &c->anomalies[c->anomaly_count++];
    a->threshold = th;
    a->start_ms  = s;
    a->end_ms    = e;
    a->mod       = parse_mod(tag);
    a->arg       = arg;
}

static void parse_audio(Chart* c, char* line) {
    if(c->audio_count >= MAX_AUDIO_TONES) return;
    char* p = line;
    uint32_t t = (uint32_t)strtoul(p, &p, 10);  if(*p != ',') return; p++;
    uint16_t f = (uint16_t)strtoul(p, &p, 10);  if(*p != ',') return; p++;
    uint16_t d = (uint16_t)strtoul(p, &p, 10);
    uint8_t  v = 80;
    if(*p == ',') { p++; v = (uint8_t)strtoul(p, NULL, 10); }
    AudioTone* a = &c->audio[c->audio_count++];
    a->time_ms = t;
    a->freq_hz = f;
    a->duration_ms = d;
    a->volume = v;
}

static void parse_event(Chart* c, char* line) {
    if(c->event_count >= MAX_EVENTS) return;
    char* p = line;
    uint32_t t = (uint32_t)strtoul(p, &p, 10);  if(*p != ',') return; p++;
    char tag[8] = {0};
    int i = 0;
    while(*p && *p != ',' && i + 1 < (int)sizeof(tag)) tag[i++] = *p++;
    tag[i] = 0;
    EventEntry* ev = &c->events[c->event_count++];
    ev->time_ms = t;
    ev->type = parse_event_type(tag);
    ev->data[0] = 0;
    if(*p == ',') {
        p++;
        copy_str(ev->data, sizeof(ev->data), p);
    }
}

static Chart* chart_parse(LineSource* ls) {
    Chart* c = malloc(sizeof(Chart));
    if(!c) return NULL;
    memset(c, 0, sizeof(*c));
    /* note buffers are grown on demand by ensure_diff_capacity */

    char line[160];
    if(!ls_read_line(ls, line, sizeof(line))) { chart_free(c); return NULL; }
    if(strncmp(line, "FRGL", 4) != 0) { chart_free(c); return NULL; }

    enum { SecNone, SecMeta, SecAnom, SecAudio, SecEvent, SecDiff } sec = SecNone;
    int diff = -1;

    while(ls_read_line(ls, line, sizeof(line))) {
        if(line[0] == 0 || line[0] == '#') continue;
        if(line[0] == '[') {
            char* end = strchr(line, ']');
            if(!end) continue;
            *end = 0;
            const char* name = line + 1;
            if(strcmp(name, "meta") == 0)      sec = SecMeta;
            else if(strcmp(name, "anomalies") == 0) sec = SecAnom;
            else if(strcmp(name, "audio") == 0)     sec = SecAudio;
            else if(strcmp(name, "events") == 0)    sec = SecEvent;
            else if(strcmp(name, "character") == 0) sec = SecMeta;
            else {
                int d = diff_index(name);
                if(d >= 0) { sec = SecDiff; diff = d; }
                else       { sec = SecNone; }
            }
            continue;
        }
        switch(sec) {
            case SecMeta:  parse_meta(c, line); break;
            case SecAnom:  parse_anomaly(c, line); break;
            case SecAudio: parse_audio(c, line); break;
            case SecEvent: parse_event(c, line); break;
            case SecDiff:
                if(diff < 0) break;
                if(strncmp(line, "diff=", 5) == 0) {
                    c->diff[diff].level = parse_level(line + 5);
                } else {
                    parse_note(c, diff, line);
                }
                break;
            case SecNone: default: break;
        }
    }

    /* trim oversized note buffers to actual size to save RAM */
    for(int i = 0; i < 4; i++) {
        if(c->diff[i].note_count == 0) {
            if(c->diff[i].notes) free(c->diff[i].notes);
            c->diff[i].notes = NULL;
            c->diff_capacity[i] = 0;
        } else if(c->diff[i].note_count < c->diff_capacity[i]) {
            Note* shrunk = realloc(c->diff[i].notes, sizeof(Note) * c->diff[i].note_count);
            if(shrunk) c->diff[i].notes = shrunk;
            c->diff_capacity[i] = c->diff[i].note_count;
        }
    }

    return c;
}

bool chart_path_is_builtin(const char* path) {
    return strncmp(path, "/builtin/", 9) == 0;
}

Chart* chart_load(Storage* storage, const char* path) {
    LineSource ls;
    Chart* c;
    if(chart_path_is_builtin(path)) {
        const char* name = path + 9; /* after "/builtin/" */
        const char* data; size_t len;
        if(!builtin_song_lookup(name, &data, &len)) return NULL;
        ls_open_mem(&ls, data, len);
        c = chart_parse(&ls);
    } else {
        if(!ls_open_file(&ls, storage, path)) return NULL;
        c = chart_parse(&ls);
    }
    ls_close(&ls);
    return c;
}

void chart_free(Chart* c) {
    if(!c) return;
    for(int i = 0; i < 4; i++) {
        if(c->diff[i].notes) free(c->diff[i].notes);
    }
    free(c);
}

const char* chart_title(const Chart* c)  { return c->title; }
const char* chart_artist(const Chart* c) { return c->artist; }
uint32_t chart_length(const Chart* c)    { return c->length_ms; }
uint32_t chart_bpm_x100(const Chart* c)  { return c->bpm_x100; }

const ChartDiff* chart_difficulty(const Chart* c, int diff) {
    if(diff < 0 || diff > 3) return NULL;
    return &c->diff[diff];
}

const AnomalyEntry* chart_anomalies(const Chart* c, uint32_t* count) {
    *count = c->anomaly_count;
    return c->anomalies;
}

const AudioTone* chart_audio(const Chart* c, uint32_t* count) {
    *count = c->audio_count;
    return c->audio;
}

const EventEntry* chart_events(const Chart* c, uint32_t* count) {
    *count = c->event_count;
    return c->events;
}
