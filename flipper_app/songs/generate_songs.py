#!/usr/bin/env python3
"""Generate the four built-in songs for Pulse.

Output:
  flipper_app/songs/<song>.frgl    plain-text chart files (also playable from SD)
  flipper_app/songs/builtin_songs.c   embeds those files as C string constants

Run from anywhere:  python3 flipper_app/songs/generate_songs.py
"""

from __future__ import annotations
import os, math, random, textwrap, pathlib

HERE = pathlib.Path(__file__).resolve().parent

# ------------------------------------------------------------------
# music — extremely simple chiptune melodies in note-name format.
# A full song = 4 sections of 4 bars each, ~80 seconds.
# ------------------------------------------------------------------

NOTES = {
    'C4': 261, 'D4': 294, 'E4': 329, 'F4': 349, 'G4': 392, 'A4': 440, 'B4': 493,
    'C5': 523, 'D5': 587, 'E5': 659, 'F5': 698, 'G5': 783, 'A5': 880, 'B5': 987,
    'C6':1046, 'D6':1174, 'E6':1318, 'F6':1396, 'G6':1567,
    'C3': 130, 'D3': 146, 'E3': 164, 'F3': 174, 'G3': 196, 'A3': 220, 'B3': 246,
    'R':   0,
}

SONGS = [
    {
        'id':     'pulse_drive',
        'title':  'Pulse Drive',
        'artist': 'NEO//SHARD',
        'bpm':    140,
        'character': 'mira',
        'levels': {'easy': 30, 'normal': 60, 'fun': 90, 'nitro': 115},
        # A driving 4-on-the-floor melody
        'melody': [
            # 16 beats × 4 sections = 64 beats; we'll loop through with variations
            'A4','C5','E5','C5', 'A4','C5','E5','G5',
            'F4','A4','C5','A4', 'F4','A4','C5','E5',
            'G4','B4','D5','B4', 'G4','B4','D5','F5',
            'A4','C5','E5','C5', 'A4','E5','C5','A4',
        ],
    },
    {
        'id':     'crystal_cascade',
        'title':  'Crystal Cascade',
        'artist': 'Mira Hex',
        'bpm':    128,
        'character': 'solenne',
        'levels': {'easy': 25, 'normal': 50, 'fun': 80, 'nitro': 105},
        'melody': [
            'C5','E5','G5','C6', 'B5','G5','E5','G5',
            'A4','C5','E5','A5', 'G5','E5','C5','E5',
            'F4','A4','C5','F5', 'E5','C5','A4','C5',
            'G4','B4','D5','G5', 'F5','D5','B4','D5',
        ],
    },
    {
        'id':     'neon_heartbeat',
        'title':  'Neon Heartbeat',
        'artist': 'Solenne',
        'bpm':    160,
        'character': 'glitch',
        'levels': {'easy': 35, 'normal': 65, 'fun': 95, 'nitro': 120},
        'melody': [
            'E5','D5','E5','G5', 'A5','G5','E5','D5',
            'C5','D5','E5','G5', 'A5','B5','A5','G5',
            'E5','G5','A5','C6', 'B5','A5','G5','E5',
            'D5','E5','G5','A5', 'B5','D6','B5','G5',
        ],
    },
    {
        'id':     'glitch_garden',
        'title':  'Glitch Garden',
        'artist': 'NEO//SHARD',
        'bpm':    110,
        'character': 'axon',
        'levels': {'easy': 40, 'normal': 70, 'fun': 100, 'nitro': 125},
        'melody': [
            'A3','E4','A4','E4', 'A3','E4','A4','C5',
            'F3','C4','F4','C4', 'F3','C4','F4','A4',
            'G3','D4','G4','D4', 'G3','D4','G4','B4',
            'A3','E4','A4','C5', 'B4','A4','G4','E4',
        ],
    },
    # ---- Locked tier — unlocked one-by-one as the player clears the prior. --
    {
        'id':     'moonlit_drift',
        'title':  'Moonlit Drift',
        'artist': 'Hex Lullaby',
        'bpm':    96,
        'character': 'solenne',
        'levels': {'easy': 30, 'normal': 55, 'fun': 85, 'nitro': 110},
        'melody': [
            'D4','F4','A4','D5', 'C5','A4','F4','A4',
            'B3','D4','G4','B4', 'A4','G4','D4','G4',
            'C4','E4','G4','C5', 'B4','G4','E4','G4',
            'D4','F4','A4','D5', 'F5','D5','A4','F4',
        ],
    },
    {
        'id':     'volt_reactor',
        'title':  'Volt Reactor',
        'artist': 'NEO//SHARD',
        'bpm':    175,
        'character': 'glitch',
        'levels': {'easy': 45, 'normal': 75, 'fun': 110, 'nitro': 135},
        'melody': [
            'E4','G4','B4','E5', 'D5','B4','G4','B4',
            'A4','C5','E5','A5', 'G5','E5','C5','E5',
            'F4','A4','C5','F5', 'E5','C5','A4','C5',
            'B4','D5','F5','B5', 'A5','F5','D5','F5',
        ],
    },
    {
        'id':     'phantom_echo',
        'title':  'Phantom Echo',
        'artist': 'Mira Hex',
        'bpm':    120,
        'character': 'mira',
        'levels': {'easy': 50, 'normal': 80, 'fun': 115, 'nitro': 140},
        'melody': [
            'A4','C5','E5','C5', 'G4','B4','D5','B4',
            'F4','A4','C5','A4', 'E4','G4','B4','G4',
            'D4','F4','A4','F4', 'C4','E4','G4','E4',
            'A3','C4','E4','C4', 'G3','B3','D4','G4',
        ],
    },
    {
        'id':     'static_surge',
        'title':  'Static Surge',
        'artist': 'AXON.exe',
        'bpm':    190,
        'character': 'axon',
        'levels': {'easy': 60, 'normal': 90, 'fun': 125, 'nitro': 150},
        'melody': [
            'C5','E5','G5','C6', 'B5','G5','E5','G5',
            'F5','A5','C6','F6', 'E6','C6','A5','C6',
            'D5','F5','A5','D6', 'C6','A5','F5','A5',
            'G4','B4','D5','G5', 'F5','D5','B4','D5',
        ],
    },
]


# Note-density per difficulty: ratio of melody beats turned into chart notes.
# Higher difficulties layer in subdivisions / holds / bursts.
# ------------------------------------------------------------------
DIFF_SEED = {'easy': 1, 'normal': 2, 'fun': 3, 'nitro': 4}


def chart_for(diff: str, melody: list[str], bpm: int, length_ms: int, level: int):
    beat_ms = 60_000 // bpm
    bars = length_ms // (beat_ms * 4)
    out = [f'diff={level/10:g}']
    # Deterministic seed: never use hash() of a string — that's randomised by
    # PYTHONHASHSEED across processes and would produce different .frgl bytes
    # on different machines.
    rng = random.Random(DIFF_SEED[diff] * 100_000 + level * 100 + bpm)

    # density profile
    if diff == 'easy':
        hits_per_beat = 1     # one note per beat
        hold_chance = 0.05
        burst_chance = 0
        slide_chance = 0
        chain_chance = 0
        fake_chance = 0
    elif diff == 'normal':
        hits_per_beat = 1
        hold_chance = 0.10
        burst_chance = 0.0
        slide_chance = 0.05
        chain_chance = 0.10
        fake_chance = 0
    elif diff == 'fun':
        hits_per_beat = 2     # eighth notes
        hold_chance = 0.12
        burst_chance = 0.05
        slide_chance = 0.10
        chain_chance = 0.20
        fake_chance = 0.03
    else:  # nitro
        hits_per_beat = 4     # sixteenth notes
        hold_chance = 0.10
        burst_chance = 0.08
        slide_chance = 0.12
        chain_chance = 0.25
        fake_chance = 0.06

    last_lane = 1
    last_t = -10_000
    for bar in range(bars):
        for beat in range(4):
            for sub in range(hits_per_beat):
                t = bar * beat_ms * 4 + beat * beat_ms + sub * (beat_ms // hits_per_beat)
                if t >= length_ms - 200:
                    break
                # avoid overlaps that are too close to the previous one
                if t - last_t < 40:
                    continue
                last_t = t

                # pick lane: tend to alternate but not always
                lane = (last_lane + rng.choice([1, 2, 3, -1])) % 4
                last_lane = lane

                # decide note type
                r = rng.random()
                if r < fake_chance:
                    out.append(f'{t},{lane},F')
                    continue
                if r < fake_chance + chain_chance and sub != 0:
                    out.append(f'{t},{lane},X')
                    continue
                if r < fake_chance + chain_chance + slide_chance and sub == 0:
                    dest = (lane + rng.choice([-1, 1])) % 4
                    out.append(f'{t},{lane},S,{dest}')
                    continue
                if r < fake_chance + chain_chance + slide_chance + burst_chance:
                    out.append(f'{t},{lane},B,3')
                    continue
                if r < fake_chance + chain_chance + slide_chance + burst_chance + hold_chance:
                    dur = beat_ms * rng.choice([1, 2])
                    out.append(f'{t},{lane},H,{dur}')
                    continue
                out.append(f'{t},{lane},T')

    return '\n'.join(out)


# ------------------------------------------------------------------
def audio_section(melody: list[str], bpm: int, length_ms: int) -> str:
    """Sequence of tone events. Loops the melody to fill length_ms."""
    beat_ms = 60_000 // bpm
    out = []
    t = 0
    i = 0
    while t < length_ms:
        n = melody[i % len(melody)]
        freq = NOTES.get(n, 0)
        out.append(f'{t},{freq},{int(beat_ms*0.85)},80')
        t += beat_ms
        i += 1
    return '\n'.join(out)


def anomaly_section(length_ms: int) -> str:
    """Anomaly windows scaled to song length: ramping intensity."""
    # divide song into thirds, fire a different anomaly in each at descending
    # gauge thresholds — players who hold combo see them all.
    a = length_ms // 4
    return '\n'.join([
        f'60,{a*1},{a*2},SPEED,130',
        f'80,{a*2},{a*3},MIRROR',
        f'95,{a*3},{a*4},STORM',
    ])


def event_section(length_ms: int) -> str:
    a = length_ms // 4
    return '\n'.join([
        '0,BG,grid',
        f'{a*1},BG,wave',
        f'{a*2},FX',
        f'{a*2},BG,tunnel',
        f'{a*3},BG,void',
    ])


# ------------------------------------------------------------------
def render_song(song: dict) -> str:
    bpm = song['bpm']
    # target ~95s per song. 95s = bars × 4 × beat_ms
    bars = max(32, min(72, (95_000 * bpm) // (60_000 * 4)))
    length_ms = bars * 4 * (60_000 // bpm)
    head = textwrap.dedent(f"""\
        FRGL v1

        [meta]
        title={song['title']}
        artist={song['artist']}
        bpm={bpm}
        offset=0
        length={length_ms}
        preview={length_ms // 4}
        author=pulse-team
        character={song['character']}

        [audio]
        """)
    audio = audio_section(song['melody'], bpm, length_ms)
    diffs = ''
    for diff_name, level in song['levels'].items():
        diffs += f'\n[{diff_name}]\n' + chart_for(diff_name, song['melody'], bpm, length_ms, level) + '\n'
    out = head + audio + '\n' + diffs
    out += '\n[anomalies]\n' + anomaly_section(length_ms) + '\n'
    out += '\n[events]\n' + event_section(length_ms) + '\n'
    return out


def c_escape(s: str) -> str:
    out = []
    for ch in s:
        if ch == '\n':
            out.append('\\n"\n    "')
        elif ch == '"':
            out.append('\\"')
        elif ch == '\\':
            out.append('\\\\')
        else:
            out.append(ch)
    return ''.join(out)


def main():
    rendered = {}
    for song in SONGS:
        text = render_song(song)
        path = HERE / f"{song['id']}.frgl"
        path.write_text(text, encoding='utf-8')
        rendered[song['id']] = text
        print(f'wrote {path}  ({len(text)} bytes)')

    out = ['/* AUTO-GENERATED by generate_songs.py — do not edit by hand. */',
           '#include "builtin_songs.h"',
           '#include <string.h>',
           '']
    for k, v in rendered.items():
        out.append(f'static const char SONG_{k.upper()}[] =\n    "' + c_escape(v) + '";')
        out.append('')
    out.append('typedef struct { const char* name; const char* data; size_t len; } Entry;')
    out.append('static const Entry table[] = {')
    for k in rendered:
        out.append(f'    {{ "{k}.frgl", SONG_{k.upper()}, sizeof(SONG_{k.upper()}) - 1 }},')
    out.append('};')
    out.append('')
    out.append(textwrap.dedent('''\
        bool builtin_song_lookup(const char* name, const char** data, size_t* len) {
            for(size_t i = 0; i < sizeof(table)/sizeof(table[0]); i++) {
                if(strcmp(name, table[i].name) == 0) {
                    *data = table[i].data;
                    *len  = table[i].len;
                    return true;
                }
            }
            return false;
        }
    '''))
    (HERE / 'builtin_songs.c').write_text('\n'.join(out), encoding='utf-8')
    print(f'wrote {HERE / "builtin_songs.c"}')


if __name__ == '__main__':
    main()
