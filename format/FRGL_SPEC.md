# FRGL — Flipper Rhythm Game Level Format

`.frgl` is the on-disk format for charts in **Pulse**, the Flipper Zero rhythm
game. The format is **plain UTF-8 text** with INI-style sections so that:

* the on-device parser can stream it line-by-line (no JSON parser, no malloc
  storms, no full-file load on a 256KB heap),
* the desktop/mobile editor can read and write it without a binary toolchain,
* humans can hand-edit charts in any text editor.

Every file starts with a magic line so the engine can reject unknown content
quickly. Anything between sections is ignored, lines starting with `#` are
comments.

```
FRGL v1
```

## Sections

### `[meta]`

Required. One `key=value` pair per line.

| key       | type     | meaning                                                              |
|-----------|----------|----------------------------------------------------------------------|
| title     | string   | Song title (max 32 chars on device).                                 |
| artist    | string   | Artist / composer name (max 32 chars).                               |
| bpm       | float    | Primary tempo. Used for snap and visual beat pulse.                  |
| offset    | int      | Audio offset in milliseconds (negative = audio leads).               |
| length    | int      | Total chart length in milliseconds.                                  |
| preview   | int      | Preview start time in ms (used in song-select).                      |
| author    | string   | Charter handle (optional).                                           |
| character | string   | Default suggested character id (optional, see `[character]`).        |

### `[audio]`

Optional. The engine drives the Flipper's piezo through `furi_hal_speaker`.
Audio events are tone instructions: `time_ms,freq_hz,duration_ms,volume`.

```
0,440,200,80
200,523,200,80
```

`freq_hz=0` is a rest. `volume` is 0–100. If `[audio]` is absent the engine
falls back to a metronome tick locked to `bpm`.

### `[easy] [normal] [fun] [nitro]`

One section per difficulty. The first line must be `diff=N` where:

* easy   → `1`–`5`, half-step `5+` encoded as `5.5`–`5.9`
* normal → `6`–`7+`
* fun    → `8`–`10+`
* nitro  → `11` and above

Subsequent lines are notes:

```
time_ms,lane,type[,arg]
```

| field   | values                                            |
|---------|---------------------------------------------------|
| time_ms | absolute ms from chart start                      |
| lane    | 0=Left 1=Up 2=Down 3=Right                        |
| type    | T tap, H hold, B burst, S slide, F fake, X chain  |
| arg     | type-dependent (see below)                        |

Note types:

* **T** Tap — single press. `arg` ignored.
* **H** Hold — `arg` = duration in ms. Press lane down for the entire window.
* **B** Burst — `arg` = number of taps. Player must mash the lane that many
  times within 600ms of the head.
* **S** Slide — `arg` = destination lane. Press start lane, then press dest
  lane within 250ms (a swipe on touch / a chord on Flipper).
* **F** Fake — phantom note, do **not** press. Hitting it costs gauge.
* **X** Chain — micro-note worth half score, no miss penalty, fills gauge fast.

### `[anomalies]`

Optional. Anomalies are sections of the chart that **rewrite themselves while
you play** based on the *Succession Gauge* (Pulse's recollection-style meter).
Each line:

```
threshold,start_ms,end_ms,modifier[,arg]
```

* `threshold` 0–100, the minimum gauge level for the anomaly to fire.
* `start_ms` / `end_ms` the window that gets rewritten.
* `modifier`:
  * `SPEED` — scroll multiplier (`arg` = ×100, e.g. 150 = 1.5×)
  * `MIRROR` — flip lanes left↔right
  * `INVERT` — flip lanes top↔bottom
  * `CHAOS` — randomise lane per-note (seeded by `start_ms`)
  * `ADD`   — inject extra chain notes between every existing note
  * `GHOST` — hide the receptor line (judge by sound)
  * `STORM` — combine `SPEED` 130 + `ADD` + `MIRROR`

Multiple anomalies in the same window stack in declaration order.

### `[character]`

Optional override for default song character. See the character roster in
`game/character.c` — at present: `null`, `mira`, `axon`, `glitch`, `solenne`.

### `[events]`

Optional cosmetic events for the external VGM display.

```
time_ms,event[,arg]
```

* `BG` — background change, `arg` = id (`grid`, `wave`, `tunnel`, `void`)
* `FX` — full-screen flash
* `TXT` — overlay text, `arg` = up-to-16-char message

## Example

```
FRGL v1

[meta]
title=Pulse Drive
artist=NEO//SHARD
bpm=140
offset=0
length=96000
preview=18000
author=mira
character=mira

[easy]
diff=3
0,1,T
428,2,T
857,1,T
1285,2,T
...

[normal]
diff=6
0,1,T
214,2,T
428,0,T
642,3,T
...

[fun]
diff=9
0,1,T
107,2,T
214,3,T
321,0,T
428,1,H,856
...

[nitro]
diff=12
0,1,T
53,2,T
107,3,T
160,0,T
214,1,B,3
...

[anomalies]
60,30000,42000,SPEED,140
80,42000,54000,MIRROR
95,72000,84000,STORM

[events]
0,BG,grid
24000,BG,wave
48000,FX
72000,BG,tunnel
```

## On-device parsing notes

* Sections are read on demand; only the active difficulty is loaded into the
  note buffer.
* The note buffer is a fixed `RG_MAX_NOTES = 4096` entry ring that the parser
  refills as time advances.
* Anomalies and events are kept fully in RAM (small).
* `time_ms` must be monotonically non-decreasing within a section.
