# Pulse — `.frgl` editor

A self-contained, install-free editor for Pulse rhythm game charts. Works on
desktop browsers and on phones/tablets.

## Run

Just open `editor/index.html` in any modern browser (Chrome 88+, Firefox 90+,
Safari 15+). No build step, no server, no dependencies.

```sh
# desktop
open editor/index.html
# or:
xdg-open editor/index.html
```

For mobile: copy the whole `editor/` directory to your phone and open
`index.html` in your browser. Or host it on any static webserver.

## Workflow

1. Set song metadata (title, BPM, length, etc.) in the left sidebar.
2. Pick a difficulty (Easy / Normal / Fun / Nitro).
3. Choose a tool (Tap / Hold / Burst / Slide / Fake / Chain / Erase) and
   click on a lane to place notes.
4. Snap controls quantisation. Zoom controls vertical pixels-per-second.
5. Add anomaly windows in the sidebar — they will fire during gameplay if the
   player's *Succession Gauge* exceeds the threshold.
6. Hit **▶ Play** to preview audio + click track in the browser.
7. Hit **Export .frgl** to download the file. Drop it onto the Flipper at
   `/ext/apps_data/pulse_rhythm/songs/your_song.frgl` and it will appear in
   the in-game song list.

## Keyboard

| Key             | Action                          |
|-----------------|---------------------------------|
| `1`–`4`         | reassign selected note's lane   |
| `T H B S F X E` | select tool                     |
| `Space`         | play / pause                    |
| `↑ ↓`           | scroll                          |
| `← →`           | switch difficulty               |
| `Ctrl/⌘+S`      | export                          |
| `Ctrl/⌘+Z / Y`  | undo / redo                     |
| `Del`           | delete selected note            |

## Mobile

Touch-first interactions:

* **Tap** an empty lane cell → place a note with the active tool.
* **Tap** an existing note → select / cycle.
* **Long-press + drag** on a hold note → resize the tail.
* Pinch-zoom is left to the browser (use the Zoom slider for chart zoom).

## File compatibility

`.frgl` written by this editor round-trips with the Flipper engine: open an
exported file back in the editor and you get an identical chart. The format
is fully documented in `format/FRGL_SPEC.md`.
