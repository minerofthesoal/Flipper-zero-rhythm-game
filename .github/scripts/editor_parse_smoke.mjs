// Smoke test: feed every passed .frgl through the editor's parser.
// We re-implement the parse path here instead of evaling editor.js (which
// touches the DOM). Kept in lockstep with editor.js by code review — see
// `parse()` in editor/editor.js.
import fs from 'node:fs';

const DIFFS = ['easy', 'normal', 'fun', 'nitro'];

function parse(text) {
  const lines = text.split(/\r?\n/);
  if (!lines[0] || !lines[0].startsWith('FRGL')) throw new Error('not FRGL');
  const out = {
    meta: { title:'', artist:'', bpm:140, offset:0, length:0, preview:0, author:'', character:'null' },
    diffs: Object.fromEntries(DIFFS.map(d => [d, { level: '0', notes: [] }])),
    audio: [], anomalies: [], events: [],
  };
  let sec = null, diff = null;
  for (let i = 1; i < lines.length; i++) {
    const ln = lines[i].trim();
    if (!ln || ln.startsWith('#')) continue;
    const m = ln.match(/^\[(\w+)\]$/);
    if (m) {
      const name = m[1];
      if (['meta', 'audio', 'anomalies', 'events'].includes(name)) { sec = name; diff = null; }
      else if (DIFFS.includes(name)) { sec = 'diff'; diff = name; }
      else sec = null;
      continue;
    }
    if (sec === 'meta') {
      const eq = ln.indexOf('=');
      if (eq < 0) continue;
      const k = ln.slice(0, eq), v = ln.slice(eq + 1);
      if (['bpm', 'offset', 'length', 'preview'].includes(k)) out.meta[k] = +v;
      else out.meta[k] = v;
    } else if (sec === 'diff' && diff) {
      if (ln.startsWith('diff=')) out.diffs[diff].level = ln.slice(5);
      else {
        const p = ln.split(',');
        out.diffs[diff].notes.push({ time: +p[0], lane: +p[1], type: p[2], arg: +p[3] || 0 });
      }
    } else if (sec === 'audio') {
      const [t, f, d] = ln.split(',').map(Number);
      out.audio.push({ time: t, freq: f, dur: d });
    } else if (sec === 'anomalies') {
      const p = ln.split(',');
      out.anomalies.push({ th: +p[0], start: +p[1], end: +p[2], mod: p[3] });
    } else if (sec === 'events') {
      const p = ln.split(',');
      out.events.push({ time: +p[0], type: p[1] });
    }
  }
  return out;
}

let bad = 0;
for (const path of process.argv.slice(2)) {
  try {
    const t = fs.readFileSync(path, 'utf8');
    const out = parse(t);
    const counts = DIFFS.map(d => `${d}=${out.diffs[d].notes.length}`).join(' ');
    if (out.diffs.nitro.notes.length === 0) {
      console.error(`fail ${path} (no nitro notes)`);
      bad++; continue;
    }
    console.log(`ok   ${path}  ${counts}  audio=${out.audio.length}  anom=${out.anomalies.length}`);
  } catch (e) {
    console.error(`fail ${path}: ${e.message}`);
    bad++;
  }
}
process.exit(bad ? 1 : 0);
