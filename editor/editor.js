/* Pulse — FRGL editor.
 *
 * The editor lives in a single-page app. State is held in a `state` object;
 * everything else (canvas redraws, sidebar bindings, undo stack, audio
 * playback via Web Audio) just observes that state.
 *
 * The export pipeline is round-trippable: you can `Export .frgl`, drop the
 * file back into `Open`, and get an identical chart.
 */
(() => {
'use strict';

// ---------- state ---------------------------------------------------------
const LANES = 4;
const DIFFS = ['easy', 'normal', 'fun', 'nitro'];
const TYPE_COLORS = {
  T: '#00e5ff', H: '#50ffaa', B: '#ffff66',
  S: '#ffaa50', F: '#c060ff', X: '#aaaaaa',
};

const state = {
  meta: {
    title: 'Untitled', artist: 'anonymous',
    bpm: 140, offset: 0, length: 60000, preview: 0,
    author: 'me', character: 'null',
  },
  diffs: {
    easy:   { level: '3',  notes: [] },
    normal: { level: '6',  notes: [] },
    fun:    { level: '9',  notes: [] },
    nitro:  { level: '12', notes: [] },
  },
  audio: [],          // [{time, freq, dur, vol}]
  anomalies: [],      // [{th, start, end, mod, arg}]
  events: [],
  active: 'easy',
  tool:   'T',
  snap:   4,          // 1/4
  zoom:   120,        // pixels per second
  scroll: 0,          // ms
  selected: null,     // index in current diff
  playing: false,
  playStart: 0,
};

const undoStack = [];
const redoStack = [];

function snapshot() {
  undoStack.push(JSON.stringify({ meta: state.meta, diffs: state.diffs,
                                   audio: state.audio, anomalies: state.anomalies,
                                   events: state.events }));
  if(undoStack.length > 100) undoStack.shift();
  redoStack.length = 0;
}
function restore(json) {
  const o = JSON.parse(json);
  Object.assign(state.meta, o.meta);
  state.diffs = o.diffs;
  state.audio = o.audio;
  state.anomalies = o.anomalies;
  state.events = o.events;
  syncForms(); render(); updateStats();
}

// ---------- canvas --------------------------------------------------------
const canvas = document.getElementById('board');
const ctx = canvas.getContext('2d');

function fitCanvas() {
  const r = canvas.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  canvas.width  = r.width  * dpr;
  canvas.height = r.height * dpr;
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
}
new ResizeObserver(() => { fitCanvas(); render(); }).observe(canvas);
window.addEventListener('resize', () => { fitCanvas(); render(); });

function laneRect(lane) {
  const w = canvas.clientWidth;
  const margin = 60;
  const usable = w - margin * 2;
  const lw = usable / LANES;
  return { x: margin + lane * lw, y: 0, w: lw, h: canvas.clientHeight };
}

function timeToY(t) {
  const px_per_ms = state.zoom / 1000;
  // we draw with newer time at top, older at bottom (scroll = ms shown at top)
  return (t - state.scroll) * px_per_ms;
}
function yToTime(y) {
  const px_per_ms = state.zoom / 1000;
  return state.scroll + y / px_per_ms;
}
function snapMs(t) {
  const beat = 60000 / state.meta.bpm;
  const step = beat / state.snap;
  return Math.round(t / step) * step;
}

function render() {
  if(!canvas.width) fitCanvas();
  const w = canvas.clientWidth, h = canvas.clientHeight;
  ctx.clearRect(0, 0, w, h);

  // grid: lanes
  ctx.fillStyle = '#0d0d18';
  for(let i = 0; i < LANES; i++) {
    const r = laneRect(i);
    if(i % 2 === 0) ctx.fillRect(r.x, 0, r.w, h);
  }

  // beat lines
  const beat = 60000 / state.meta.bpm;
  const sub = beat / state.snap;
  const startBeat = Math.floor(state.scroll / sub) * sub;
  for(let t = startBeat; t < state.scroll + (h * 1000 / state.zoom); t += sub) {
    const y = timeToY(t);
    if(y < 0 || y > h) continue;
    const onBeat = Math.abs((t / beat) - Math.round(t / beat)) < 0.001;
    ctx.strokeStyle = onBeat ? '#3a3a55' : '#202030';
    ctx.lineWidth   = onBeat ? 1.2 : 0.6;
    ctx.beginPath();
    ctx.moveTo(60, y); ctx.lineTo(w - 60, y); ctx.stroke();

    if(onBeat) {
      ctx.fillStyle = '#5a5a7a';
      ctx.font = '10px monospace';
      const sec = (t / 1000).toFixed(2);
      ctx.fillText(sec + 's', 4, y + 3);
    }
  }

  // anomaly bands
  for(const a of state.anomalies) {
    const y0 = timeToY(a.start), y1 = timeToY(a.end);
    ctx.fillStyle = 'rgba(255,80,120,0.10)';
    ctx.fillRect(60, y0, w - 120, y1 - y0);
    ctx.strokeStyle = 'rgba(255,80,120,0.6)';
    ctx.strokeRect(60, y0, w - 120, y1 - y0);
    ctx.fillStyle = '#ff5577';
    ctx.font = 'bold 10px monospace';
    ctx.fillText(`${a.mod} @${a.th}`, 64, y0 + 12);
  }

  // notes
  const cur = state.diffs[state.active];
  for(let i = 0; i < cur.notes.length; i++) {
    const n = cur.notes[i];
    const r = laneRect(n.lane);
    const cx = r.x + r.w / 2;
    const y = timeToY(n.time);
    if(y < -100 || y > h + 100) continue;
    ctx.fillStyle = TYPE_COLORS[n.type] || '#ccc';
    ctx.strokeStyle = '#000';
    ctx.lineWidth = 1;

    if(n.type === 'H') {
      const yEnd = timeToY(n.time + (n.arg || 0));
      const x = cx - 6;
      ctx.fillRect(x, yEnd, 12, y - yEnd);
      ctx.fillStyle = '#fff';
      ctx.fillRect(cx - 8, y - 1, 16, 2);
    } else if(n.type === 'X') {
      ctx.beginPath(); ctx.arc(cx, y, 3, 0, Math.PI * 2); ctx.fill();
    } else if(n.type === 'F') {
      ctx.globalAlpha = 0.7;
      ctx.beginPath(); ctx.arc(cx, y, 8, 0, Math.PI * 2); ctx.stroke(); ctx.globalAlpha = 1;
      ctx.font = 'bold 10px sans-serif';
      ctx.fillText('?', cx - 3, y + 3);
    } else if(n.type === 'S') {
      const dest = laneRect(n.arg || 0);
      ctx.beginPath(); ctx.moveTo(cx, y); ctx.lineTo(dest.x + dest.w / 2, y); ctx.stroke();
      ctx.beginPath(); ctx.arc(cx, y, 6, 0, Math.PI * 2); ctx.fill();
    } else if(n.type === 'B') {
      ctx.beginPath(); ctx.arc(cx, y, 9, 0, Math.PI * 2); ctx.fill();
      ctx.fillStyle = '#000';
      ctx.font = 'bold 10px sans-serif';
      ctx.fillText(`B${n.arg || 3}`, cx - 7, y + 3);
    } else { // T
      ctx.beginPath(); ctx.arc(cx, y, 8, 0, Math.PI * 2); ctx.fill();
    }

    if(state.selected === i) {
      ctx.strokeStyle = '#fff';
      ctx.lineWidth = 2;
      ctx.strokeRect(r.x + 4, y - 12, r.w - 8, 24);
    }
  }

  // playhead
  if(state.playing) {
    const t = (performance.now() - state.playStart);
    const y = timeToY(t);
    ctx.strokeStyle = '#ff66cc';
    ctx.beginPath(); ctx.moveTo(60, y); ctx.lineTo(w - 60, y); ctx.stroke();
  }

  // top bar: lane labels
  ctx.fillStyle = '#5a5a7a';
  ctx.font = 'bold 11px sans-serif';
  ['Left', 'Up', 'Down', 'Right'].forEach((label, i) => {
    const r = laneRect(i);
    ctx.fillText(label, r.x + r.w/2 - 12, 14);
  });
}

// ---------- input: pointer ------------------------------------------------
function pointToLane(x) {
  for(let i = 0; i < LANES; i++) {
    const r = laneRect(i);
    if(x >= r.x && x < r.x + r.w) return i;
  }
  return -1;
}

let down = null;
canvas.addEventListener('pointerdown', e => {
  canvas.setPointerCapture(e.pointerId);
  const r = canvas.getBoundingClientRect();
  const x = e.clientX - r.left, y = e.clientY - r.top;
  const lane = pointToLane(x);
  if(lane < 0) return;
  const t = snapMs(yToTime(y));
  down = { x, y, lane, t, time: performance.now(), drag: false };

  // hit-test existing notes
  const cur = state.diffs[state.active];
  for(let i = cur.notes.length - 1; i >= 0; i--) {
    const n = cur.notes[i];
    if(n.lane !== lane) continue;
    const ny = timeToY(n.time);
    if(Math.abs(ny - y) < 12) {
      down.hit = i;
      state.selected = i;
      render();
      return;
    }
  }
  // place
  if(state.tool === 'E') return;
  snapshot();
  const note = { lane, time: t, type: state.tool, arg: 0 };
  if(state.tool === 'H') note.arg = 250;
  if(state.tool === 'B') note.arg = 3;
  if(state.tool === 'S') note.arg = (lane + 1) % LANES;
  cur.notes.push(note);
  cur.notes.sort((a, b) => a.time - b.time);
  state.selected = cur.notes.indexOf(note);
  render(); updateStats();
});

canvas.addEventListener('pointermove', e => {
  if(!down) return;
  const r = canvas.getBoundingClientRect();
  const y = e.clientY - r.top;
  const dt = performance.now() - down.time;
  if(dt > 200 && Math.abs(y - down.y) > 5) {
    down.drag = true;
    if(down.hit != null) {
      const cur = state.diffs[state.active];
      const n = cur.notes[down.hit];
      if(n.type === 'H') {
        const t = snapMs(yToTime(y));
        n.arg = Math.max(50, t - n.time);
        render();
      } else {
        n.time = snapMs(yToTime(y));
        cur.notes.sort((a, b) => a.time - b.time);
        down.hit = cur.notes.indexOf(n);
        state.selected = down.hit;
        render();
      }
    }
  }
});

canvas.addEventListener('pointerup', () => {
  if(!down) return;
  const dt = performance.now() - down.time;
  if(down.hit != null && (state.tool === 'E' || dt > 500) && !down.drag) {
    snapshot();
    state.diffs[state.active].notes.splice(down.hit, 1);
    state.selected = null;
    render(); updateStats();
  }
  down = null;
});

canvas.addEventListener('wheel', e => {
  e.preventDefault();
  state.scroll += e.deltaY * 4;
  if(state.scroll < 0) state.scroll = 0;
  render();
}, { passive: false });

// ---------- input: keyboard ----------------------------------------------
window.addEventListener('keydown', e => {
  if(e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;
  if(e.key === 'ArrowDown') { state.scroll += 250; render(); }
  else if(e.key === 'ArrowUp') { state.scroll = Math.max(0, state.scroll - 250); render(); }
  else if(e.key === 'ArrowLeft' || e.key === 'ArrowRight') {
    const d = e.key === 'ArrowRight' ? 1 : -1;
    let i = DIFFS.indexOf(state.active) + d;
    if(i < 0) i = DIFFS.length - 1;
    if(i >= DIFFS.length) i = 0;
    selectDiff(DIFFS[i]);
  }
  else if('TBHSFXE'.includes(e.key.toUpperCase())) selectTool(e.key.toUpperCase());
  else if('1234'.includes(e.key) && state.selected != null) {
    state.diffs[state.active].notes[state.selected].lane = +e.key - 1;
    render();
  }
  else if(e.key === ' ') { e.preventDefault(); togglePlay(); }
  else if((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 's') { e.preventDefault(); doExport(); }
  else if((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'z') {
    e.preventDefault();
    if(undoStack.length) {
      redoStack.push(JSON.stringify({ meta: state.meta, diffs: state.diffs,
                                       audio: state.audio, anomalies: state.anomalies,
                                       events: state.events }));
      restore(undoStack.pop());
    }
  }
  else if((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'y') {
    e.preventDefault();
    if(redoStack.length) {
      undoStack.push(JSON.stringify({ meta: state.meta, diffs: state.diffs,
                                       audio: state.audio, anomalies: state.anomalies,
                                       events: state.events }));
      restore(redoStack.pop());
    }
  }
  else if(e.key === 'Delete' && state.selected != null) {
    snapshot();
    state.diffs[state.active].notes.splice(state.selected, 1);
    state.selected = null;
    render(); updateStats();
  }
});

// ---------- sidebar -------------------------------------------------------
const $ = id => document.getElementById(id);

function syncForms() {
  $('metaTitle').value     = state.meta.title;
  $('metaArtist').value    = state.meta.artist;
  $('metaBpm').value       = state.meta.bpm;
  $('metaOffset').value    = state.meta.offset;
  $('metaLength').value    = state.meta.length;
  $('metaPreview').value   = state.meta.preview;
  $('metaAuthor').value    = state.meta.author;
  $('metaCharacter').value = state.meta.character;
  $('diffLevel').value     = state.diffs[state.active].level;
  renderAnomalies();
}

['metaTitle','metaArtist','metaBpm','metaOffset','metaLength','metaPreview','metaAuthor','metaCharacter']
  .forEach(id => {
    $(id).addEventListener('input', e => {
      const key = id.replace('meta','').toLowerCase();
      state.meta[key] = (id === 'metaBpm' || id.startsWith('metaOffset') || id === 'metaLength' || id === 'metaPreview')
        ? +e.target.value : e.target.value;
      render();
    });
  });

$('diffLevel').addEventListener('input', e => {
  state.diffs[state.active].level = e.target.value;
});

function selectDiff(d) {
  state.active = d;
  document.querySelectorAll('.tab').forEach(t => t.classList.toggle('active', t.dataset.diff === d));
  $('diffLevel').value = state.diffs[d].level;
  state.selected = null;
  render(); updateStats();
}
document.querySelectorAll('.tab').forEach(t => {
  t.addEventListener('click', () => selectDiff(t.dataset.diff));
});

function selectTool(tool) {
  state.tool = tool;
  document.querySelectorAll('.tool').forEach(b => b.classList.toggle('active', b.dataset.tool === tool));
}
document.querySelectorAll('.tool').forEach(b => {
  b.addEventListener('click', () => selectTool(b.dataset.tool));
});

$('snap').addEventListener('change', e => { state.snap = +e.target.value; render(); });
$('zoom').addEventListener('input', e => { state.zoom = +e.target.value; render(); });

// ---------- anomalies UI --------------------------------------------------
function renderAnomalies() {
  const list = $('anomalyList');
  list.innerHTML = '';
  state.anomalies.forEach((a, i) => {
    const row = document.createElement('div');
    row.className = 'anomaly-row';
    row.innerHTML = `
      <input type="number" min="0" max="100" value="${a.th}" title="threshold">
      <input type="number" value="${a.start}" title="start ms">
      <input type="number" value="${a.end}" title="end ms">
      <select>
        ${['SPEED','MIRROR','INVERT','CHAOS','ADD','GHOST','STORM']
          .map(m => `<option ${m === a.mod ? 'selected' : ''}>${m}</option>`).join('')}
      </select>
      <button class="x">×</button>`;
    const inputs = row.querySelectorAll('input, select');
    inputs[0].addEventListener('input', e => { a.th = +e.target.value; render(); });
    inputs[1].addEventListener('input', e => { a.start = +e.target.value; render(); });
    inputs[2].addEventListener('input', e => { a.end = +e.target.value; render(); });
    inputs[3].addEventListener('change', e => { a.mod = e.target.value; render(); });
    row.querySelector('.x').addEventListener('click', () => {
      snapshot(); state.anomalies.splice(i, 1); renderAnomalies(); render();
    });
    list.appendChild(row);
  });
}
$('addAnomaly').addEventListener('click', () => {
  snapshot();
  state.anomalies.push({ th: 70, start: state.scroll, end: state.scroll + 6000, mod: 'SPEED', arg: 130 });
  renderAnomalies(); render();
});

// ---------- stats ---------------------------------------------------------
function updateStats() {
  const el = $('stats');
  const out = [];
  for(const d of DIFFS) {
    out.push(`<b>${d}</b> ${state.diffs[d].notes.length} notes (lvl ${state.diffs[d].level})`);
  }
  out.push(`<b>length</b> ${(state.meta.length/1000).toFixed(1)}s`);
  out.push(`<b>bpm</b> ${state.meta.bpm}`);
  el.innerHTML = out.join('<br>');
}

// ---------- file IO -------------------------------------------------------
function serialise() {
  const out = ['FRGL v1', ''];
  out.push('[meta]');
  for(const k of ['title','artist','bpm','offset','length','preview','author','character']) {
    out.push(`${k}=${state.meta[k]}`);
  }
  if(state.audio.length) {
    out.push('', '[audio]');
    for(const a of state.audio) out.push(`${a.time},${a.freq},${a.dur},${a.vol}`);
  }
  for(const d of DIFFS) {
    out.push('', `[${d}]`);
    out.push(`diff=${state.diffs[d].level}`);
    for(const n of state.diffs[d].notes) {
      let line = `${Math.round(n.time)},${n.lane},${n.type}`;
      if(n.type === 'H' || n.type === 'B' || n.type === 'S') line += `,${n.arg|0}`;
      out.push(line);
    }
  }
  if(state.anomalies.length) {
    out.push('', '[anomalies]');
    for(const a of state.anomalies) {
      let line = `${a.th},${a.start},${a.end},${a.mod}`;
      if(a.arg) line += `,${a.arg}`;
      out.push(line);
    }
  }
  if(state.events.length) {
    out.push('', '[events]');
    for(const ev of state.events) {
      out.push(`${ev.time},${ev.type}${ev.arg ? ',' + ev.arg : ''}`);
    }
  }
  return out.join('\n') + '\n';
}

function parse(text) {
  const lines = text.split(/\r?\n/);
  if(!lines[0] || !lines[0].startsWith('FRGL')) throw new Error('not a FRGL file');
  state.meta = { title:'', artist:'', bpm:140, offset:0, length:60000, preview:0, author:'', character:'null' };
  state.audio = []; state.anomalies = []; state.events = [];
  state.diffs = {
    easy:   { level: '3',  notes: [] }, normal: { level: '6',  notes: [] },
    fun:    { level: '9',  notes: [] }, nitro:  { level: '12', notes: [] },
  };
  let sec = null, diff = null;
  for(let i = 1; i < lines.length; i++) {
    const ln = lines[i].trim();
    if(!ln || ln.startsWith('#')) continue;
    const m = ln.match(/^\[(\w+)\]$/);
    if(m) {
      const name = m[1];
      if(['meta','audio','anomalies','events'].includes(name)) { sec = name; diff = null; }
      else if(DIFFS.includes(name)) { sec = 'diff'; diff = name; }
      else sec = null;
      continue;
    }
    if(sec === 'meta') {
      const eq = ln.indexOf('=');
      if(eq < 0) continue;
      const k = ln.slice(0, eq), v = ln.slice(eq + 1);
      if(['bpm','offset','length','preview'].includes(k)) state.meta[k] = +v;
      else state.meta[k] = v;
    } else if(sec === 'audio') {
      const [t, f, d, vo] = ln.split(',').map(Number);
      state.audio.push({ time: t, freq: f, dur: d, vol: vo || 80 });
    } else if(sec === 'anomalies') {
      const p = ln.split(',');
      state.anomalies.push({ th: +p[0], start: +p[1], end: +p[2], mod: p[3], arg: +p[4] || 0 });
    } else if(sec === 'events') {
      const p = ln.split(',');
      state.events.push({ time: +p[0], type: p[1], arg: p[2] || '' });
    } else if(sec === 'diff' && diff) {
      if(ln.startsWith('diff=')) state.diffs[diff].level = ln.slice(5);
      else {
        const p = ln.split(',');
        state.diffs[diff].notes.push({
          time: +p[0], lane: +p[1], type: p[2], arg: +p[3] || 0,
        });
      }
    }
  }
  syncForms(); render(); updateStats();
}

$('saveBtn').addEventListener('click', doExport);
function doExport() {
  const text = serialise();
  const blob = new Blob([text], { type: 'text/plain' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = (state.meta.title || 'untitled').replace(/\W+/g, '_').toLowerCase() + '.frgl';
  a.click();
  URL.revokeObjectURL(a.href);
}

$('openInput').addEventListener('change', async e => {
  const f = e.target.files[0];
  if(!f) return;
  const txt = await f.text();
  try { snapshot(); parse(txt); }
  catch(err) { alert('Failed to load: ' + err.message); }
});

$('newBtn').addEventListener('click', () => {
  if(!confirm('Clear current chart?')) return;
  snapshot();
  state.diffs = {
    easy:   { level: '3', notes: [] }, normal: { level: '6', notes: [] },
    fun:    { level: '9', notes: [] }, nitro:  { level: '12', notes: [] },
  };
  state.audio = []; state.anomalies = []; state.events = [];
  render(); updateStats();
});

$('helpBtn').addEventListener('click', () => $('helpDialog').showModal());

// ---------- audio preview (Web Audio) -------------------------------------
let audioCtx = null;
let scheduledNodes = [];
let raf = null;

function ensureAudio() { if(!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)(); }

function togglePlay() {
  if(state.playing) stopPlay();
  else startPlay();
}
function startPlay() {
  ensureAudio();
  state.playing = true;
  state.playStart = performance.now() - state.scroll;
  $('playBtn').disabled = true;
  $('stopBtn').disabled = false;
  // schedule audio tones
  for(const a of state.audio) {
    if(a.time < state.scroll) continue;
    if(a.freq === 0) continue;
    const osc = audioCtx.createOscillator();
    const gain = audioCtx.createGain();
    osc.type = 'square';
    osc.frequency.value = a.freq;
    const t0 = audioCtx.currentTime + (a.time - state.scroll) / 1000;
    gain.gain.setValueAtTime(0, t0);
    gain.gain.linearRampToValueAtTime((a.vol || 80)/300, t0 + 0.01);
    gain.gain.linearRampToValueAtTime(0, t0 + a.dur / 1000);
    osc.connect(gain).connect(audioCtx.destination);
    osc.start(t0);
    osc.stop(t0 + a.dur / 1000 + 0.05);
    scheduledNodes.push(osc);
  }
  // tap clicks for current diff — pitch matches the per-note-type sound the
  // game emits (Tap E6, Hold A5, Burst B6, Slide G6, Chain E5; Fake silent).
  const clickHz = { T: 1320, H: 880, B: 1976, S: 1568, X: 660 };
  for(const n of state.diffs[state.active].notes) {
    if(n.time < state.scroll) continue;
    if(n.type === 'F') continue;
    const hz = clickHz[n.type] || 1320;
    const osc = audioCtx.createOscillator();
    const gain = audioCtx.createGain();
    osc.type = 'triangle';
    osc.frequency.value = hz;
    const t0 = audioCtx.currentTime + (n.time - state.scroll) / 1000;
    gain.gain.setValueAtTime(0, t0);
    gain.gain.linearRampToValueAtTime(0.15, t0 + 0.001);
    gain.gain.exponentialRampToValueAtTime(0.001, t0 + 0.04);
    osc.connect(gain).connect(audioCtx.destination);
    osc.start(t0);
    osc.stop(t0 + 0.05);
    scheduledNodes.push(osc);
  }
  const tick = () => {
    if(!state.playing) return;
    const t = performance.now() - state.playStart;
    state.scroll = t;
    render();
    if(t > state.meta.length) stopPlay();
    else raf = requestAnimationFrame(tick);
  };
  tick();
}
function stopPlay() {
  state.playing = false;
  $('playBtn').disabled = false;
  $('stopBtn').disabled = true;
  if(raf) cancelAnimationFrame(raf);
  for(const n of scheduledNodes) try { n.stop(); } catch {}
  scheduledNodes = [];
}
$('playBtn').addEventListener('click', startPlay);
$('stopBtn').addEventListener('click', stopPlay);

// ---------- bootstrap -----------------------------------------------------
syncForms();
fitCanvas();
render();
updateStats();
})();
