# Pulse · Flipper Zero Rhythm Game

A full rhythm game for the Flipper Zero, with a built-in soundtrack, a
shareable level format (`.frgl`), and an editor that runs in any desktop or
mobile browser. Optionally mirrors gameplay onto an external display via the
**Flipper Zero Video Game Module**.

```
+------------------------+         +-------------------------+
|   Flipper Zero         |  HDMI?  |   Video Game Module     |
|   piezo + screen       | <-----> |   320x240 mirror        |
|   gameplay + judge     |         |   (optional)            |
+------------------------+         +-------------------------+
            ^
            | .frgl  (USB / qFlipper / SD)
            |
+------------------------+
|  Browser Editor        |
|  (desktop & mobile)    |
+------------------------+
```

## Features

* **Four difficulties** per song:
  * **Easy**   1 → 5+   (5+ encoded as 5.5–5.9)
  * **Normal** 6 → 7+
  * **Fun**    8 → 10+
  * **Nitro**  11 and above
* **Six note types**: Tap, Hold, Burst (mash), Slide, Fake, Chain.
* **Succession Gauge** + **Anomalies** — songs literally rewrite themselves
  while you play. The better you do, the wilder it gets:
  * `SPEED` accelerates the scroll
  * `MIRROR` flips the lanes
  * `INVERT` swaps adjacent pairs
  * `CHAOS` randomises lanes per note
  * `ADD` injects extra chain notes
  * `GHOST` hides the receptor row (judge by sound)
  * `STORM` combines several at once
* **Five characters** with passive buffs: `Null`, `Mira` (score+10%),
  `Axon` (tank, half drain), `Glitch` (anomalies fire earlier), `Solenne`
  (no-fail).
* **Saves**: per-song best score, best combo, full-combo flag, lifetime
  perfects, settings.
* **Editor**: cross-platform (desktop + mobile). Single static HTML file —
  no install. Round-trips with the engine.
* **External display**: optional VGM module mirror at higher resolution.
* **Built-in soundtrack**: 4 chiptune tracks, ~95 s each, all 4 difficulties.

## Built-in songs

| Title             | Artist     | BPM | Length | Vibe                          |
|-------------------|-----------|----:|-------:|-------------------------------|
| Pulse Drive       | NEO//SHARD| 140 |   95 s | driving electronic            |
| Crystal Cascade   | Mira Hex  | 128 |   94 s | melodic / arpeggiated         |
| Neon Heartbeat    | Solenne   | 160 |   94 s | upbeat / fast                 |
| Glitch Garden     | NEO//SHARD| 110 |   94 s | mid-tempo low-end groove      |

All four are stored as `.frgl` text and embedded into the FAP at compile time
(`flipper_app/songs/*.frgl` + `builtin_songs.c`).

## Layout

```
flipper_app/                          on-Flipper FAP
  application.fam                     manifest
  pulse.c, pulse_app.h                lifecycle
  scenes/                             scene_manager scenes
    scene_splash, scene_menu,
    scene_song_select, scene_difficulty,
    scene_character, scene_settings,
    scene_game, scene_results, scene_credits
  views/view_game.{c,h}               canvas-based gameplay view
  game/                               engine subsystems
    chart.{c,h}                       .frgl parser, runtime note buffers
    judge.{c,h}                       timing windows, scoring, fail logic
    audio.{c,h}                       speaker tone scheduler
    anomaly.{c,h}                     succession gauge + anomalies
    character.{c,h}                   character roster + buffs
    save.{c,h}                        persistent saves
    vgm.{c,h}                         video-game-module mirror (optional)
  songs/                              built-in tracks
    *.frgl                            human-readable charts
    builtin_songs.{c,h}               generated embed
    generate_songs.py                 chart generator (re-run to refresh)

editor/                               browser editor
  index.html, editor.css, editor.js   single-page app (no build step)
  README.md                           editor docs

format/
  FRGL_SPEC.md                        the file format

docs/                                 design notes (this README, etc.)
```

## Building the FAP

Pulse builds with the standard Flipper Zero firmware build system. From the
firmware tree (Official, Momentum, RogueMaster, …):

```sh
# from inside the firmware checkout
cp -r path/to/Flipper-zero-rhythm-game/flipper_app applications_user/pulse_rhythm
./fbt fap_pulse_rhythm
# the .fap will land at dist/<target>/apps/Games/pulse_rhythm.fap
```

Copy `pulse_rhythm.fap` to `/ext/apps/Games/` on your Flipper (e.g. via
qFlipper). The first launch creates `/ext/apps_data/pulse_rhythm/songs/` —
drop user `.frgl` files there to add them to the song list.

## Controls

| Button       | In-game                                 |
|--------------|-----------------------------------------|
| `←`          | Hit lane 0 (Left)                       |
| `↑`          | Hit lane 1 (Up)                         |
| `↓`          | Hit lane 2 (Down)                       |
| `→`          | Hit lane 3 (Right)                      |
| `OK` (long)  | Pause / resume                          |
| `Back` (long)| Abort song (counts as a fail)           |

For the slide note type, press the start lane and the destination lane within
250 ms of each other. For burst notes, mash the same lane the requested
number of times.

## Editor

The editor lives in `editor/` and is a single static HTML page with no
dependencies. Open `editor/index.html` in any modern browser. See
`editor/README.md` for the full guide.

## .frgl format

A human-readable INI-style format with one section per topic
(`[meta]`, `[audio]`, `[easy]`, `[normal]`, `[fun]`, `[nitro]`, `[anomalies]`,
`[events]`). Full spec in `format/FRGL_SPEC.md`. The on-device parser streams
the file line-by-line and only allocates the active difficulty into RAM.

## Regenerating built-in songs

```sh
python3 flipper_app/songs/generate_songs.py
```

This rewrites the `.frgl` files and the `builtin_songs.c` embed. The script
is deterministic (seeded RNG) so charts won't drift between runs.

## License

MIT (suggested). Pulse is a fan/hobby project — not affiliated with Flipper
Devices or any music label. Soundtrack names are placeholders for the
generated chiptune; replace with your own under any license you like.
