/* OpenX32 · M32 UI prototype — static, dependency-free UI specification.
   Screen: 800x480 (same framebuffer as OpenX32 on the i.MX25).
   This file is intentionally plain JS so it can be read as a LVGL porting spec. */

(() => {
  'use strict';

  const $ = (sel, root = document) => root.querySelector(sel);
  const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
  const z2 = n => String(n).padStart(2, '0');
  const PAGE_ORDER = ['overview', 'channel', 'eq', 'dyn', 'aux', 'setup', 'assign', 'monitor', 'routing', 'delay', 'meter', 'fx', 'card', 'macro', 'scenes', 'about'];
  const PAGE_TITLE = {
    overview: 'OVERVIEW', channel: 'CHANNEL STRIP', eq: 'EQUALIZER', dyn: 'DYNAMICS',
    aux: 'AUX SENDS', setup: 'SHOW CONFIG', assign: 'FADER ASSIGN', monitor: 'MONITOR AUDIO',
    routing: 'ROUTING MATRIX', delay: 'DELAY / ALIGNMENT', meter: 'METERBRIDGE', fx: 'EFFECTS RACK',
    card: 'X-LIVE CARD', macro: 'MACRO / RECALL', scenes: 'SCENE MANAGER', about: 'SYSTEM'
  };

  const mk = (prefix, start, count) => Array.from({ length: count }, (_, k) => prefix + ' ' + z2(start + k));
  const SOURCE_GROUPS = [
    { label: 'XLR 1–16', items: mk('XLR', 1, 16) },
    { label: 'XLR 17–32', items: mk('XLR', 17, 16) },
    { label: 'AUX 1–8', items: mk('AUX', 1, 8) },
    { label: 'AES50 A', items: mk('AES A', 1, 16) },
    { label: 'AES50 B', items: mk('AES B', 1, 16) },
    { label: 'ULTRANET', items: mk('UNET', 1, 16) },
    { label: 'CARD 1', items: mk('CD1', 1, 16) },
    { label: 'CARD 2', items: mk('CD2', 1, 16) },
    { label: 'USB 1–16', items: mk('USB', 1, 16) },
    { label: 'FX RET', items: Array.from({ length: 4 }, (_, k) => 'FX RET ' + (k + 1)) }
  ];
  const DEST_GROUPS = [
    { label: 'XLR OUT', items: mk('XLR', 1, 16) },
    { label: 'AUX OUT', items: mk('AUX', 1, 8) },
    { label: 'AES50 A', items: mk('AES A', 1, 16) },
    { label: 'AES50 B', items: mk('AES B', 1, 16) },
    { label: 'ULTRANET', items: mk('UNET', 1, 16) },
    { label: 'CARD 1', items: mk('CD1', 1, 16) },
    { label: 'CARD 2', items: mk('CD2', 1, 16) },
    { label: 'USB OUT', items: mk('USB', 1, 16) },
    { label: 'DSP 1–40', items: mk('DSP', 1, 16) }
  ];

  const NAMES = [
    'KICK', 'SNARE', 'OH L', 'OH R', 'BASS DI', 'BASS AMP', 'GTR L', 'GTR R',
    'KEYS', 'PIANO', 'VOX LEAD', 'VOX BCK', 'BRASS 1', 'BRASS 2', 'STRINGS', 'PERC',
    'FX REV', 'FX DLY', 'DJ L', 'DJ R', 'TALK', 'CLICK', 'STAGE L', 'STAGE R',
    'FX RET 1', 'FX RET 2', 'FX RET 3', 'FX RET 4',
    'MIXBUS 1', 'MIXBUS 2', 'MIXBUS 3', 'MIXBUS 4', 'MIXBUS 5', 'MIXBUS 6', 'MIXBUS 7', 'MIXBUS 8',
    ...Array.from({ length: 16 }, (_, k) => 'OUT ' + z2(k + 1))
  ];
  const SOURCES = [
    ...mk('XLR', 1, 24), ...mk('AUX', 1, 4),
    ...Array.from({ length: 4 }, (_, k) => 'FX RET ' + (k + 1)), ...mk('BUS', 1, 8),
    ...mk('XLR OUT', 1, 16)
  ];
  const BANK_COUNT = 12, SLOT_COUNT = 12, MATRIX_COUNT = 6;
  const INPUT_BANKS = 8;   // 2 rows x 4 input banks
  const OUTPUT_BANKS = 4;  // 1 row x 4 output banks
  const ASSIGN_LISTS = [
    { label: 'INPUT', ids: () => inputIds() },
    { label: 'FX RET', ids: () => fxIds().map(i => 24 + i) },
    { label: 'GROUP', ids: () => groupIds().map(i => 28 + i) },
    { label: 'OUT', ids: () => outIds().map(i => 36 + i) }
  ];
  /* X32/M32 EQ mode page: low cut, low shelf, parametric, VEQ, high shelf, high cut. */
  const EQ_MODES = ['LOW CUT', 'LOW SHELF', 'PEAK', 'VEQ', 'HIGH SHELF', 'HIGH CUT'];
  const EQ_MODE_LABEL = {
    'LOW CUT': 'LCUT', 'LOW SHELF': 'LSHF', 'PEAK': 'PEQ',
    'VEQ': 'VEQ', 'HIGH SHELF': 'HSHF', 'HIGH CUT': 'HCUT'
  };
  const AUX_EXT_GROUPS = [
    { i: 0, label: 'XLR' }, { i: 1, label: 'AUX' }, { i: 2, label: 'AES A' },
    { i: 3, label: 'AES B' }, { i: 4, label: 'UNET' }, { i: 5, label: 'CD1' },
    { i: 6, label: 'CD2' }, { i: 7, label: 'USB' }, { i: 8, label: 'GRP' },
    { i: 9, label: 'MTX' }, { i: 10, label: 'DSP' }
  ];
  const auxSourceIndex = a => { const k = auxIds().indexOf(a); return k < 0 ? a : k; };
  const patchKey = (sg, si, dg, di) => sg + '/' + si + '|' + dg + '/' + di;
  const selectableIds = () => [...new Set(activeLayers().flatMap(l => l.ids))];

  /* Straight mapping: input banks hold INPUT/FX RET/GROUP, output banks hold XLR OUT. */
  function straightBanks() {
    const inputOrder = [
      ...Array.from({ length: 24 }, (_, i) => i),
      ...Array.from({ length: 4 }, (_, i) => 24 + i),
      ...Array.from({ length: 8 }, (_, i) => 28 + i)
    ];
    const outputOrder = Array.from({ length: 16 }, (_, i) => 36 + i);
    return Array.from({ length: BANK_COUNT }, (_, b) => {
      const source = b < INPUT_BANKS ? inputOrder : outputOrder;
      const offset = (b < INPUT_BANKS ? b : b - INPUT_BANKS) * SLOT_COUNT;
      const name = b < INPUT_BANKS
        ? (b < 4 ? 'A' : 'B') + z2((b % 4) + 1)
        : 'O' + z2(b - INPUT_BANKS + 1);
      return { name, slots: Array.from({ length: SLOT_COUNT }, (_, s) => source[offset + s] ?? null) };
    });
  }

  /* ---------------- show configuration / active resources ---------------- */

  const idsFrom = flags => flags.map((on, i) => on ? i : -1).filter(i => i >= 0);
  const inputIds = () => state.setup.hideUnused ? idsFrom(state.setup.inputOn) : Array.from({ length: 24 }, (_, i) => i);
  const auxIds = () => state.setup.hideUnused ? idsFrom(state.setup.auxOn) : Array.from({ length: 8 }, (_, i) => i);
  const fxIds = () => state.setup.hideUnused ? idsFrom(state.setup.fxOn) : Array.from({ length: 4 }, (_, i) => i);
  const groupIds = () => state.setup.hideUnused ? idsFrom(state.setup.groupOn) : Array.from({ length: 8 }, (_, i) => i);
  const outIds = () => state.setup.hideUnused ? idsFrom(state.setup.outOn) : Array.from({ length: 16 }, (_, i) => i);
  const matrixIds = () => state.setup.hideUnused ? idsFrom(state.setup.matrixOn) : Array.from({ length: MATRIX_COUNT }, (_, i) => i);

  /* A bank ID is active only when the matching resource exists in the show. */
  function isActiveId(id) {
    if (id === null || id === undefined || !state.channels[id]) return false;
    if (!state.setup.hideUnused) return true;
    if (id < 24) return state.setup.inputOn[id];
    if (id < 28) return state.setup.fxOn[id - 24];
    if (id < 36) return state.setup.groupOn[id - 28];
    return state.setup.outOn[id - 36];
  }

  function activeLayers() {
    if (state.assign.enabled) {
      const layers = state.assign.banks.map((b, k) => {
        const ids = b.slots.filter(isActiveId);
        return { label: b.name + ' · ' + ids.length + ' FADER', ids };
      });
      return layers.length ? layers : [{ label: 'NO FADERS', ids: [] }];
    }
    const layers = [], ins = inputIds();
    for (let k = 0; k < ins.length; k += 12) {
      layers.push({
        label: 'INPUT ' + z2(ins[k] + 1) + '–' + z2(ins[Math.min(k + 11, ins.length - 1)] + 1),
        ids: ins.slice(k, k + 12)
      });
    }
    const fx = fxIds();
    if (fx.length) layers.push({ label: 'FX RET 1–' + fx.length, ids: fx });
    const grp = groupIds();
    if (grp.length) layers.push({ label: 'GROUP 01–' + z2(grp.length), ids: grp });
    if (!layers.length) layers.push({ label: 'NO SOURCES', ids: [] });
    return layers;
  }

  function sourceGroups() {
    const s = state.setup;
    return [
      { label: 'INPUT', items: inputIds().map(i => SOURCES[i]) },
      { label: 'AUX', items: auxIds().map(i => 'AUX ' + z2(i + 1)) },
      { label: 'AES50 A', items: mk('AES A', 1, 16) },
      { label: 'AES50 B', items: mk('AES B', 1, 16) },
      { label: 'ULTRANET', items: mk('UNET', 1, 16) },
      { label: 'CARD 1', items: mk('CD1', 1, 16) },
      { label: 'CARD 2', items: mk('CD2', 1, 16) },
      { label: 'USB', items: mk('USB', 1, 16) },
      { label: 'GROUP', items: groupIds().map(i => 'GRP ' + z2(i + 1)) },
      { label: 'FX RET', items: fxIds().map(i => 'FX RET ' + (i + 1)) },
      { label: 'MATRIX', items: matrixIds().map(i => 'MATRIX ' + z2(i + 1)) }
    ];
  }

  function destGroups() {
    return [
      { label: 'XLR OUT', items: outIds().map(i => 'XLR ' + z2(i + 1)) },
      { label: 'AUX OUT', items: auxIds().map(i => 'AUX ' + z2(i + 1)) },
      { label: 'AES50 A', items: mk('AES A', 1, 16) },
      { label: 'AES50 B', items: mk('AES B', 1, 16) },
      { label: 'ULTRANET', items: mk('UNET', 1, 16) },
      { label: 'CARD 1', items: mk('CD1', 1, 16) },
      { label: 'CARD 2', items: mk('CD2', 1, 16) },
      { label: 'USB OUT', items: mk('USB', 1, 16) },
      { label: 'GROUP IN', items: groupIds().map(i => 'GRP ' + z2(i + 1)) },
      { label: 'MATRIX IN', items: matrixIds().map(i => 'MATRIX ' + z2(i + 1)) },
      { label: 'DSP', items: mk('DSP', 1, 16) }
    ];
  }

  function setFlagCount(key, n) {
    const s = state.setup, arr = s[key];
    n = clamp(n, 0, arr.length);
    for (let k = 0; k < arr.length; k++) arr[k] = k < n;
    s.preset = 'CUSTOM';
  }

  function applyPreset(name) {
    const p = SETUP_PRESETS[name];
    if (!p) return;
    const s = state.setup;
    setFlagCount('inputOn', p.inputs);
    setFlagCount('auxOn', p.aux);
    setFlagCount('fxOn', p.fx);
    setFlagCount('groupOn', p.groups);
    setFlagCount('outOn', p.outs);
    setFlagCount('matrixOn', p.matrix);
    inputIds().forEach((i, k) => {
      state.channels[i].mode = k >= p.inputs - p.stereoInputs ? 'STEREO' : 'MONO';
      if (state.channels[i].mode === 'STEREO') state.channels[i].link = null;
    });
    for (let k = 0; k < s.auxFormat.length; k++) s.auxFormat[k] = k < p.stereoAux ? 'STEREO' : 'MONO';
    for (let k = 0; k < s.groupFormat.length; k++) s.groupFormat[k] = k < p.stereoGroups ? 'STEREO' : 'MONO';
    s.preset = name;
    syncSetup();
    setMsg('SETUP PRESET · ' + name);
  }

  /* Older scenes do not contain EQ modes; fill them in without losing saved values. */
  function normalizeEQ() {
    state.channels.forEach(c => {
      if (!Array.isArray(c.eq)) c.eq = [];
      c.eq.forEach((b, k) => {
        if (!b || typeof b !== 'object') return;
        if (!EQ_MODES.includes(b.type)) b.type = k === 0 ? 'LOW SHELF' : k === 3 ? 'HIGH SHELF' : 'PEAK';
        if (!Number.isFinite(b.f)) b.f = [120, 420, 2400, 9000][k] || 1000;
        if (!Number.isFinite(b.g)) b.g = 0;
        if (!Number.isFinite(b.q)) b.q = 1;
      });
    });
  }

  function syncSetup() {
    normalizeEQ();
    const s = state.setup;
    groupIds().forEach(k => { const c = state.channels[28 + k]; if (c) c.mode = s.groupFormat[k] || 'MONO'; });
    fxIds().forEach(k => { state.fx[k].pos = s.fxMix[k] === 'PRE' ? 'PRE FADER' : 'POST FADER'; });
    const layers = activeLayers();
    if (state.layer >= layers.length) state.layer = 0;
    state.assign.bank = clamp(state.assign.bank, 0, state.assign.banks.length - 1);
    state.assign.slot = clamp(state.assign.slot, 0, SLOT_COUNT - 1);
    state.assign.list = clamp(state.assign.list, 0, ASSIGN_LISTS.length - 1);
    state.layer = state.assign.enabled ? state.assign.bank : state.layer;
    const aux = auxIds();
    if (aux.length && !aux.includes(state.aux)) state.aux = aux[0];
    const active = state.assign.enabled
      ? [...new Set(activeLayers().flatMap(l => l.ids))]
      : [...inputIds(), ...fxIds(), ...groupIds()];
    if (active.length && !active.includes(state.selected)) state.selected = active[0];
    state.srcGroup = clamp(state.srcGroup, 0, sourceGroups().length - 1);
    state.dstGroup = clamp(state.dstGroup, 0, destGroups().length - 1);
    if (!outIds().includes(state.delayOut)) state.delayOut = outIds()[0] || 0;
    state.auxExtGroup = clamp(state.auxExtGroup, 0, AUX_EXT_GROUPS.length - 1);
    state.auxExtPage = clamp(state.auxExtPage, 0, 1);
    prunePatches();
  }

  /* Patches that point to resources removed in SHOW CONFIG must not survive. */
  function prunePatches() {
    const src = sourceGroups(), dst = destGroups();
    Object.keys(state.patch).forEach(k => {
      const m = /^(\d+)\/(\d+)\|(\d+)\/(\d+)$/.exec(k);
      if (!m || !src[+m[1]] || !dst[+m[3]] || !src[+m[1]].items[+m[2]] || !dst[+m[3]].items[+m[4]]) delete state.patch[k];
    });
  }

  const SPEED_OF_SOUND = 343;   // m/s at 20 °C
  const frameMs = () => 1000 / state.clock.fps;
  const delayInfo = ms => ({ m: (ms * SPEED_OF_SOUND / 1000).toFixed(2), ms: ms.toFixed(1), f: (ms / frameMs()).toFixed(2) });

  const state = {
    page: 'overview', layer: 0, selected: 4, eqBand: 0,
    msg: 'SYSTEM READY · LVGL 9.5 / 800×480',
    channels: NAMES.map((name, i) => ({
      name, source: SOURCES[i], gain: (i % 5) - 2, fader: 62 + (i % 4) * 3,
      phase: false, mute: false, solo: false, hpf: i % 3 !== 0, hpfFreq: 80,
      eq: [
        { f: 120, g: i % 2 ? 2.5 : -3, q: 1.0, type: 'LOW SHELF' },
        { f: 420, g: -2, q: 1.4, type: 'PEAK' },
        { f: 2400, g: 2, q: 1.2, type: 'PEAK' },
        { f: 9000, g: 3.5, q: 0.8, type: 'HIGH SHELF' }
      ],
      gate: { type: i % 5 === 0 ? 'EXPANDER' : 'GATE', on: i % 4 === 0, thr: -42, attack: 5, release: 120, range: -60 },
      comp: { type: i % 4 === 0 ? 'FET' : 'VCA', on: i % 3 === 0, thr: -18, ratio: 3.5, attack: 12, release: 180, auto: false },
      pan: (i % 7) - 3, insert: i % 2 ? 'Mustard' : '—', send: 12 + (i % 4) * 6,
      mode: i >= 24 ? 'STEREO' : 'MONO', link: null, delayMs: 0,
      insertSlot: i % 3 === 0 ? 'REVERB' : i % 3 === 1 ? 'CHORUS' : 'NONE',
      insertPos: 'POST FADER'
    })),
    srcGroup: 0, dstGroup: 0,
    patch: Object.fromEntries(Array.from({ length: 16 }, (_, k) => [`0/${k}|0/${k}`, true]).concat([['0/0|1/0', true], ['0/1|1/1', true]])),
    outputDelay: new Array(16).fill(0),
    delaySide: 'in', delayOut: 0,
    fx: [
      { name: 'REVERB', on: true, pos: 'PRE FADER', out: 'USB', params: [['SIZE', 62], ['DECAY', 48], ['MIX', 22]] },
      { name: 'CHORUS', on: false, pos: 'POST FADER', out: 'CARD 1', params: [['RATE', 35], ['DEPTH', 55], ['MIX', 18]] },
      { name: 'DELAY', on: true, pos: 'POST FADER', out: 'CARD 2', params: [['TIME', 41], ['FEEDBK', 32], ['MIX', 15]] },
      { name: 'OVERDRIVE', on: false, pos: 'PRE FADER', out: 'XLR', params: [['DRIVE', 28], ['TONE', 60], ['MIX', 12]] }
    ],
    card: { medium: 'SD', track: 2, playing: false, rec: false, pos: 74 },
    aux: 0, auxExtGroup: 0, auxExtPage: 0,
    master: { level: 72, mute: false, solo: false },
    masterMeter: { l: 0.12, r: 0.12 },
    sends: Array.from({ length: NAMES.length }, (_, i) => Array.from({ length: 8 }, (_, j) => clamp(38 + ((i * 7 + j * 13) % 55), 0, 100))),
    sendMute: Array.from({ length: NAMES.length }, () => new Array(8).fill(false)),
    auxMode: new Array(8).fill('POST'),
    auxMute: new Array(8).fill(false),
    auxLevel: new Array(8).fill(72),
    auxMeter: new Array(8).fill(0),
    levels: new Array(NAMES.length).fill(0.12),
    levelsL: new Array(NAMES.length).fill(0.12),
    levelsR: new Array(NAMES.length).fill(0.12),
    monitor: { source: 'LR', level: 72, dim: false, mono: false, mute: false, phones: 'PHONES 1' },
    clock: { rate: 48, fps: 25, source: 'INTERNAL' },
    macro: { idx: 0, recording: null, playing: null, step: 0 },
    macros: Array.from({ length: 8 }, (_, k) => ({ name: 'MACRO ' + z2(k + 1), steps: [] })),
    setup: {
      show: 'M32 SHOW', hideUnused: true, preset: 'DEFAULT',
      inputOn: new Array(24).fill(true),
      auxOn: new Array(8).fill(true),
      fxOn: new Array(4).fill(true),
      groupOn: new Array(8).fill(true),
      outOn: new Array(16).fill(true),
      matrixOn: new Array(MATRIX_COUNT).fill(true),
      auxFormat: new Array(8).fill('MONO'),
      groupFormat: new Array(8).fill('STEREO'),
      groupDest: ['LR', 'XLR 1-2', 'AUX 1', 'GRP 1', 'AES50 A', 'AES50 B', 'CARD 1', 'USB'],
      fxMix: new Array(4).fill('POST')
    },
    assign: { enabled: true, bank: 0, slot: 0, list: 0, banks: straightBanks() },
    scenes: new Array(32).fill(null),
    scene: {
      slot: 0, show: 'M32 SHOW', clipboard: null, autoSave: false,
      safe: { setup: false, assign: false, input: false, aux: false, routing: false, output: false, monitor: false, macro: false }
    }
  };

  const SCENE_BANKS = ['A', 'B', 'C', 'D'];
  const SCENE_SCOPE = [
    ['setup', 'SETUP / I/O'], ['assign', 'FADER MAP'], ['input', 'INPUT / CH'], ['aux', 'AUX SENDS'],
    ['routing', 'ROUTING'], ['output', 'OUT / FX / CLK'], ['monitor', 'MONITOR'], ['macro', 'MACRO']
  ];
  const GROUP_DESTS = ['LR', 'XLR 1-2', 'XLR 3-4', 'AUX 1', 'GRP 1', 'AES50 A', 'AES50 B', 'CARD 1', 'CARD 2', 'USB'];
  const SETUP_PRESETS = {
    DEFAULT: { inputs: 24, aux: 8, fx: 4, groups: 8, outs: 16, matrix: 6, stereoInputs: 4, stereoAux: 0, stereoGroups: 8 },
    BAND: { inputs: 16, aux: 8, fx: 4, groups: 4, outs: 12, matrix: 4, stereoInputs: 4, stereoAux: 0, stereoGroups: 4 },
    CORO: { inputs: 24, aux: 4, fx: 2, groups: 8, outs: 16, matrix: 6, stereoInputs: 8, stereoAux: 4, stereoGroups: 8 },
    THEATRE: { inputs: 12, aux: 6, fx: 3, groups: 6, outs: 12, matrix: 4, stereoInputs: 2, stereoAux: 2, stereoGroups: 6 },
    MIN: { inputs: 8, aux: 2, fx: 1, groups: 2, outs: 8, matrix: 2, stereoInputs: 0, stereoAux: 0, stereoGroups: 2 }
  };

  /* ---------------- helpers ---------------- */

  const faderDb = v => v <= 0 ? '-∞' : (v > 80 ? '+' : '') + ((v - 80) * 0.35).toFixed(1);
  const fmtFreq = f => f >= 1000 ? (f / 1000).toFixed(f >= 10000 ? 1 : 2) + 'k' : f.toFixed(0);
  const panLabel = v => Math.abs(v) < .25 ? 'C' : (v < 0 ? 'L' : 'R') + Math.abs(v).toFixed(Math.abs(v) % 1 ? 1 : 0);
  const fmtVal = (v, unit) => {
    switch (unit) {
      case 'hz': return fmtFreq(v);
      case 'db': return (v > 0 ? '+' : '') + v.toFixed(1);
      case 'ratio': return v.toFixed(1) + ':1';
      case 'ms': return v.toFixed(0);
      case 'pct': return v.toFixed(0) + '%';
      case 'masterdb': return faderDb(v);
      case 'pan': return panLabel(v);
      default: return v.toFixed(1);
    }
  };
  const rot = (v, min, max) => (clamp((v - min) / (max - min), 0, 1) * 270 - 135).toFixed(1);

  function knob(label, obj, key, i, value, min, max, unit, cls = '') {
    return `<div class="knob-cell ${cls}" data-act="knob" data-obj="${obj}" data-key="${key}" data-i="${i}" data-val="${value}" data-min="${min}" data-max="${max}" data-unit="${unit}">
      <div class="knob" style="--rot:${rot(value, min, max)}deg"><div class="knob-ptr"></div></div>
      <label>${label}</label><output>${fmtVal(value, unit)}</output>
    </div>`;
  }

  function toggle(label, obj, key, value, cls = '') {
    return `<button class="btn ${cls} ${value ? 'on' : ''}" data-act="toggle" data-obj="${obj}" data-key="${key}">${label}</button>`;
  }

  function faderHTML(i, big = false) {
    const c = state.channels[i];
    const cap = big ? 11 : 7;
    return `<div class="ftrack" data-ftrack="${i}">
        <div class="ffill" style="height:${c.fader}%"></div>
        <div class="fcap" data-act="fader" data-i="${i}" style="bottom:calc(${c.fader}% - ${cap}px)"></div>
      </div>`;
  }

  /* Stereo meter (L/R) when the channel is STEREO or linked, otherwise mono (M). */
  function metersHTML(i, big = false, monitor = false) {
    const c = state.channels[i];
    const stereo = c.mode === 'STEREO' || c.link !== null;
    const sides = stereo ? [['L', 'L'], ['R', 'R']] : [['M', 'M']];
    return `<div class="vum-group ${big ? 'big' : ''} ${monitor ? 'mon' : ''}">` + sides.map(([label, side]) =>
      `<div class="vum"><span>${label}</span><div class="fmeter" data-vumeter="${i}" data-side="${side}" ${monitor ? 'data-monmeter="1"' : ''}><div class="lvl"></div></div></div>`
    ).join('') + `</div>`;
  }

  function stripHTML(i, big = false) {
    const c = state.channels[i];
    const label = c.name;
    return `<div class="strip ${state.selected === i ? 'sel' : ''}" data-act="select" data-i="${i}">
      <div class="num">${z2(i + 1)}</div>
      <div class="name">${label}</div>
      <div class="src">${c.source}${c.mode === 'STEREO' || c.link !== null ? ' · ST' : ''}</div>
      <div class="pan-mini" data-act="pan" data-i="${i}" data-pan-i="${i}" title="PAN CH ${z2(i + 1)} · drag">
        <span class="pan-cap">PAN</span>
        <div class="pan-track"><i style="left:${((c.pan + 6) / 12 * 100).toFixed(1)}%"></i></div>
        <b>${panLabel(c.pan)}</b>
      </div>
      <div class="ms">
        <button class="btn mute ${c.mute ? 'on' : ''}" data-act="mute" data-i="${i}">M</button>
        <button class="btn solo ${c.solo ? 'on' : ''}" data-act="solo" data-i="${i}">S</button>
      </div>
      <div class="fader-wrap">${faderHTML(i)}${metersHTML(i)}</div>
      <div class="db" data-fval="${i}">${faderDb(c.fader)}</div>
      <div class="proc">
        <span class="${c.gate.on ? 'on' : ''}">GT</span><span class="on">EQ</span><span class="${c.comp.on ? 'on' : ''}">CP</span>
      </div>
    </div>`;
  }

  function head(note = '') {
    const layer = activeLayers()[state.layer];
    return `<div class="page-head">
      <div><span class="eyebrow">${PAGE_TITLE[state.page]}</span>${state.page === 'overview' ? `<span class="page-note"> · ${layer.label}</span>` : ''}</div>
      <div class="page-note">${note || 'SEL ' + z2(state.selected + 1) + ' · ' + state.channels[state.selected].name}</div>
    </div>`;
  }

  /* ---------------- pages ---------------- */

  function pageOverview() {
    const ids = activeLayers()[state.layer].ids;
    const layerTabs = activeLayers().map((l, i) => `<button class="bank-btn ${state.layer === i ? 'on' : ''}" data-act="layer" data-v="${i}"><b>${l.label.split(' · ')[0]}</b><em>${l.ids.length}</em></button>`).join('');
    const c = state.channels[state.selected];
    return `<div class="page">
      ${head()}
      <div class="panel" style="flex:1;padding:0;display:flex;flex-direction:column">
        <div class="page-head" style="padding:2px 5px;height:34px;align-items:flex-start">
          <div class="bank-picker">${layerTabs}</div>
          <div class="page-note" style="text-align:right">${ids.length} FADERS · ${activeLayers().length} BANKS<br>${activeLayers()[state.layer].label}</div>
        </div>
        <div class="strip-row" style="padding:2px 3px">${ids.map(i => stripHTML(i)).join('')}</div>
      </div>
      <div class="row" style="height:124px;flex:none">
        <div class="panel" style="width:236px">
          <div class="panel-title"><span>MASTER</span><em>STEREO L/R</em></div>
          <div class="master-knob-wrap sel">
            ${knob('MASTER LEVEL', 'master', 'level', 0, state.master.level, 0, 100, 'masterdb', 'xl')}
            <div class="mst-meters">
              <div class="mst-meter"><span>L</span><div class="fmeter" data-mastermeter="L"><div class="lvl"></div></div></div>
              <div class="mst-meter"><span>R</span><div class="fmeter" data-mastermeter="R"><div class="lvl"></div></div></div>
            </div>
            <div class="mst-actions">
              <button class="btn mute ${state.master.mute ? 'on' : ''}" data-act="toggle" data-obj="master" data-key="mute">M</button>
              <button class="btn solo ${state.master.solo ? 'on' : ''}" data-act="toggle" data-obj="master" data-key="solo">S</button>
            </div>
          </div>
        </div>
        <div class="panel quick">
          <div class="quick-title">QUICK CONTROL<br><span class="tag amber">CH ${z2(state.selected + 1)}</span></div>
          <div class="quick-grid">
            ${knob('GAIN', 'ch', 'gain', state.selected, c.gain, -12, 12, 'db')}
            ${knob('HPF', 'ch', 'hpfFreq', state.selected, c.hpfFreq, 20, 300, 'hz', 'blue')}
            ${knob('PAN', 'ch', 'pan', state.selected, c.pan, -6, 6, 'pan', 'purple')}
            <div style="display:flex;flex-direction:column;gap:4px">
              ${toggle('PHASE', 'ch', 'phase', c.phase)}
              ${toggle('HPF', 'ch', 'hpf', c.hpf)}
            </div>
            <div style="display:flex;flex-direction:column;gap:4px">
              ${toggle('GATE', 'gate', 'on', c.gate.on)}
              ${toggle('COMP', 'comp', 'on', c.comp.on)}
            </div>
            <div style="font:9px var(--mono);color:var(--dim);line-height:1.5">
              INSERT <b style="color:var(--amber)">${c.insert}</b><br>
              SEND <b style="color:var(--amber)">${c.send}</b>
            </div>
          </div>
        </div>
      </div>
    </div>`;
  }

  function procBlock(title, accent, body, footer) {
    return `<div class="panel proc-block">
      <div class="panel-title"><span>${title}</span><em>${accent}</em></div>
      <div class="body">${body}</div>
      <div class="footer">${footer}</div>
    </div>`;
  }

  function pageChannel() {
    const i = state.selected, c = state.channels[i];
    const ids = activeLayers()[state.layer].ids;
    const bank = ids.map(n => `<button class="btn ${n === i ? 'on' : ''}" data-act="select" data-i="${n}">${z2(n + 1)}</button>`).join('');
    return `<div class="page">
      ${head('CHANNEL ' + z2(i + 1) + ' · ' + c.name)}
      <div class="row" style="flex:1">
        <div class="panel" style="flex:1;padding:0">
          <div class="proc-grid">
            ${procBlock('INPUT', c.source,
              knob('GAIN', 'ch', 'gain', i, c.gain, -12, 12, 'db') + knob('PAN', 'ch', 'pan', i, c.pan, -6, 6, 'pan', 'purple'),
              toggle('PHASE', 'ch', 'phase', c.phase) + toggle('HPF', 'ch', 'hpf', c.hpf) + `<span class="tag">PAD 0dB</span>`)}
            ${procBlock('FILTER', 'HPF / LPF',
              knob('HPF FREQ', 'ch', 'hpfFreq', i, c.hpfFreq, 20, 300, 'hz', 'blue') + knob('LPF', 'ch', 'lpf', i, 18000, 2000, 20000, 'hz', 'blue'),
              `<span class="tag green">24 dB/OCT</span><span class="tag">LOW CUT ON</span>`)}
            ${procBlock('GATE', c.gate.on ? 'ACTIVE' : 'OFF',
              knob('THRESH', 'gate', 'thr', i, c.gate.thr, -70, 0, 'db') + knob('ATTACK', 'gate', 'attack', i, c.gate.attack, 1, 100, 'ms') + knob('RELEASE', 'gate', 'release', i, c.gate.release, 20, 500, 'ms'),
              toggle('GATE ON', 'gate', 'on', c.gate.on) + `<span class="tag">RANGE -60</span>`)}
            ${procBlock('COMPRESSOR', c.comp.on ? 'ACTIVE' : 'OFF',
              knob('THRESH', 'comp', 'thr', i, c.comp.thr, -60, 0, 'db') + knob('RATIO', 'comp', 'ratio', i, c.comp.ratio, 1, 10, 'ratio') + knob('RELEASE', 'comp', 'release', i, c.comp.release, 20, 600, 'ms'),
              toggle('COMP ON', 'comp', 'on', c.comp.on) + toggle('AUTO', 'comp', 'auto', false) + `<span class="tag">GR <b style="color:var(--amber)" data-gr>-3.2</b>dB</span>`)}
          </div>
        </div>
        <div class="panel" style="width:150px;display:flex;flex-direction:column;align-items:center">
          <div class="panel-title" style="width:100%"><span>FADER / VU</span><em>${c.mode}${c.link !== null ? ' · LNK' : ''}</em></div>
          <div class="big-fader">${faderHTML(i, true)}${metersHTML(i, true)}</div>
          <div class="fader-readout" data-fval="${i}">${faderDb(c.fader)}</div>
          <div style="display:flex;gap:5px;margin-top:6px">
            <button class="btn mute ${c.mute ? 'on' : ''}" data-act="mute" data-i="${i}">MUTE</button>
            <button class="btn solo ${c.solo ? 'on' : ''}" data-act="solo" data-i="${i}">SOLO</button>
          </div>
        </div>
      </div>
      <div class="panel insert-panel" style="height:76px;flex:none">
        <div class="panel-title"><span>MODE / INSERT · CH ${z2(i + 1)}</span><em>${c.mode} · ${c.insertSlot} · ${c.insertPos}</em></div>
        <div class="mode-bar">
          <div class="grp">
            <label>MODE</label>
            <button class="btn ${c.mode === 'MONO' ? 'on' : ''}" data-act="chanmode" data-v="MONO">MONO</button>
            <button class="btn ${c.mode === 'STEREO' ? 'on' : ''}" data-act="chanmode" data-v="STEREO">STEREO</button>
            <button class="btn ${c.link !== null ? 'on' : ''}" data-act="chanlink">${c.link !== null ? 'UNLINK' : 'LINK +1'}</button>
            <span class="tag amber">${c.link !== null ? 'PAIR ' + z2(i + 1) + '+' + z2(c.link + 1) : 'INDEPENDENT'}</span>
          </div>
          <div class="sep"></div>
          <div class="grp">
            <label>INSERT FX</label>
            <button class="btn ${c.insertSlot === 'NONE' ? 'on' : ''}" data-act="insertslot" data-v="NONE">OFF</button>
            ${fxIds().map(k => `<button class="btn ${c.insertSlot === state.fx[k].name ? 'on' : ''}" data-act="insertslot" data-v="${state.fx[k].name}" style="font-size:7px">${state.fx[k].name}</button>`).join('')}
          </div>
          <div class="sep"></div>
          <div class="grp">
            <label>POSITION</label>
            ${['PRE EQ', 'POST EQ', 'PRE FADER', 'POST FADER'].map(p => `<button class="btn ${c.insertPos === p ? 'on' : ''}" data-act="insertpos" data-v="${p}" style="font-size:7px">${p}</button>`).join('')}
          </div>
        </div>
        <div class="page-note" style="margin-top:3px">VU: <b style="color:var(--amber)">${c.mode === 'STEREO' || c.link !== null ? 'L / R' : 'MONO'}</b> · LINK pairs the next channel and forces STEREO · insert belongs to the channel</div>
      </div>
      <div class="panel" style="height:34px;flex:none">
        <div class="panel-title"><span>CHANNEL BANK · ${activeLayers()[state.layer].label}</span><em>SELECT</em></div>
        <div class="bank-mini">${bank}</div>
      </div>
    </div>`;
  }

  /* Approximate the selected surface EQ mode so the graph follows the mode choice. */
  function eqBandResponse(b, f) {
    const oct = Math.log2(f / b.f);
    const w = 1.1 / Math.max(0.35, b.q);
    if (b.type === 'LOW SHELF') return b.g / (1 + Math.pow(2, oct / w));
    if (b.type === 'HIGH SHELF') return b.g / (1 + Math.pow(2, -oct / w));
    if (b.type === 'LOW CUT') return -Math.abs(b.g) * clamp(-oct / 1.5, 0, 1);
    if (b.type === 'HIGH CUT') return -Math.abs(b.g) * clamp(oct / 1.5, 0, 1);
    if (b.type === 'VEQ') {
      const bell = b.g * Math.exp(-0.5 * (oct / (w * 1.5)) ** 2);
      const presence = (b.g > 0 ? 1.5 : 0) * Math.exp(-0.5 * (Math.log2(f / 3000) / 1.2) ** 2);
      return bell + presence;
    }
    return b.g * Math.exp(-0.5 * (oct / w) ** 2);
  }

  function eqCurve() {
    const c = state.channels[state.selected];
    let d = '';
    for (let px = 0; px <= 640; px += 4) {
      const x = px / 640, f = 20 * Math.pow(1000, x);
      let g = 0;
      c.eq.forEach(b => { g += eqBandResponse(b, f); });
      if (c.hpf) g -= 24 * clamp((Math.log2(c.hpfFreq) - Math.log2(f)) / 2, 0, 1);
      const y = clamp(100 - g * 4, 3, 197);
      d += (px ? 'L' : 'M') + px.toFixed(0) + ' ' + y.toFixed(1) + ' ';
    }
    return d;
  }

  function pageEQ() {
    const c = state.channels[state.selected];
    const grid = [40, 120, 240, 400, 640, 1000, 2400, 6000, 12000].map((f, k) => {
      const x = Math.log(f / 20) / Math.log(1000) * 640;
      return `<line class="gridline" x1="${x}" y1="0" x2="${x}" y2="200"/><text x="${x + 2}" y="196">${fmtFreq(f)}</text>`;
    }).join('');
    const nodes = c.eq.map((b, k) => {
      const x = Math.log(b.f / 20) / Math.log(1000) * 640;
      const y = clamp(100 - eqBandResponse(b, b.f) * 4, 3, 197);
      return `<circle class="node ${state.eqBand === k ? 'sel' : ''}" cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" r="6" data-act="eqnode" data-i="${k}"/>`;
    }).join('');
    const bands = c.eq.map((b, k) => {
      const modes = EQ_MODES.map(m => `<button class="btn ${b.type === m ? 'on' : ''}" data-act="eqtype" data-i="${k}" data-v="${m}" title="${m}" style="height:14px;padding:0;font-size:6px;min-width:0">${EQ_MODE_LABEL[m]}</button>`).join('');
      const cut = b.type === 'LOW CUT' || b.type === 'HIGH CUT';
      return `<div class="panel eq-band ${state.eqBand === k ? 'active' : ''}">
        <div class="panel-title"><span>BAND ${k + 1}</span><em>${b.type}</em></div>
        <div class="eq-band-modes">${modes}</div>
        <div class="knobs">
          ${knob('FREQ', 'eq', 'f', k, b.f, 20, 20000, 'hz')}
          ${knob(cut ? 'DEPTH' : 'GAIN', 'eq', 'g', k, b.g, -12, 12, 'db')}
          ${knob('Q', 'eq', 'q', k, b.q, 0.3, 6, 'ratio')}
        </div>
        <div style="text-align:center;margin-top:2px"><button class="btn ${state.eqBand === k ? 'on' : ''}" data-act="eqband" data-v="${k}" style="width:100%;height:16px">SELECT BAND</button></div>
      </div>`;
    }).join('');
    return `<div class="page">
      ${head('4-BAND EQ · SELECTABLE FILTER MODES')}
      <div class="row" style="flex:1">
        <div class="panel" style="flex:1;display:flex;flex-direction:column">
          <div class="panel-title"><span>EQ RESPONSE · CH ${z2(state.selected + 1)} ${c.name}</span><em>±12 dB</em></div>
          <div class="eq-graph-wrap">
            <svg class="graph" id="eqgraph" viewBox="0 0 640 200" preserveAspectRatio="none">
              ${grid}
              <line class="gridzero" x1="0" y1="100" x2="640" y2="100"/>
              <path class="curve" id="eqcurve" d="${eqCurve()}"/>
              ${nodes}
              <text x="4" y="12">+12</text><text x="4" y="94">0</text><text x="4" y="190">-12</text>
            </svg>
          </div>
        </div>
        <div class="panel" style="width:112px;display:flex;flex-direction:column">
          <div class="panel-title"><span>SPECTRUM</span><em>RTA</em></div>
          <div class="spectrum" id="spectrum">${Array.from({ length: 20 }, () => '<i style="height:10%"></i>').join('')}</div>
        </div>
      </div>
      <div class="eq-bands" style="height:128px">${bands}</div>
      <div class="panel" style="height:36px;flex:none;display:flex;align-items:center;gap:10px">
        ${knob('LOW CUT', 'ch', 'hpfFreq', state.selected, c.hpfFreq, 20, 300, 'hz', 'blue compact')}
        ${toggle('LOW CUT ON', 'ch', 'hpf', c.hpf)}
        <span class="page-note">MODE PER BAND · DRAG NODES TO EDIT FREQUENCY / LEVEL</span>
      </div>
    </div>`;
  }

  function dynCurve(kind) {
    const c = state.channels[state.selected];
    const p = kind === 'gate' ? c.gate : c.comp;
    let d = '';
    for (let x = 0; x <= 300; x += 3) {
      const inDb = -60 + x / 300 * 60;
      let outDb = inDb;
      if (kind === 'gate') {
        if (p.type === 'EXPANDER') {
          const open = clamp((inDb - p.thr) / 60, 0, 1);
          outDb = -60 + (inDb + 60) * (0.2 + 0.8 * open);
        } else if (p.type === 'DUCKER') {
          const duck = clamp((inDb - p.thr) / 18, 0, 1);
          outDb = inDb - duck * Math.abs(p.range || 30);
        } else {
          const open = clamp((inDb - p.thr) / 18, 0, 1);
          outDb = -60 + open * 60;
        }
      } else {
        const over = Math.max(0, inDb - p.thr);
        const ratio = p.type === 'FET' ? p.ratio * 1.15 : p.type === 'TUBE' ? p.ratio * .8 : p.ratio;
        const knee = p.type === 'OPTICAL' ? Math.min(over, 6) : 0;
        outDb = inDb - over * (1 - 1 / ratio) - knee * .12;
      }
      const px = x, py = clamp(190 - (outDb + 60) / 60 * 175, 5, 195);
      d += (x ? 'L' : 'M') + px + ' ' + py.toFixed(1) + ' ';
    }
    const tx = (p.thr + 60) / 60 * 300;
    return `<svg class="graph" viewBox="0 0 300 200" preserveAspectRatio="none">
      <line class="gridline" x1="0" y1="100" x2="300" y2="100"/><line class="gridline" x1="150" y1="0" x2="150" y2="200"/>
      <line class="gridzero" x1="0" y1="190" x2="300" y2="15"/>
      <line class="curve2" x1="${tx}" y1="0" x2="${tx}" y2="200"/>
      <path class="curve" d="${d}"/>
      <text x="3" y="12">0 dB</text><text x="3" y="188">-60 dB</text>
    </svg>`;
  }

  function pageDyn() {
    const i = state.selected, c = state.channels[i];
    return `<div class="page">
      ${head('GATE + COMPRESSOR · CH ' + z2(i + 1))}
      <div class="row" style="flex:1">
        <div class="panel" style="flex:1;display:flex;flex-direction:column">
          <div class="panel-title"><span>DYNAMICS · GATE</span><em>${c.gate.type} · ${c.gate.on ? 'ACTIVE' : 'OFF'}</em></div>
          <div class="type-bar">
            <span class="tag">TYPE</span>
            ${['GATE', 'EXPANDER', 'DUCKER'].map(t => `<button class="btn ${c.gate.type === t ? 'on' : ''}" data-act="gatetype" data-v="${t}" style="font-size:8px;flex:1">${t}</button>`).join('')}
          </div>
          <div style="flex:1;min-height:0">${dynCurve('gate')}</div>
          <div style="display:flex;justify-content:space-around;margin-top:5px">
            ${knob('THRESH', 'gate', 'thr', i, c.gate.thr, -70, 0, 'db')}
            ${knob('ATTACK', 'gate', 'attack', i, c.gate.attack, 1, 100, 'ms')}
            ${knob('RELEASE', 'gate', 'release', i, c.gate.release, 20, 500, 'ms')}
          </div>
          <div style="margin-top:5px">${toggle('GATE ON', 'gate', 'on', c.gate.on)} <span class="tag">RANGE -60 dB</span></div>
        </div>
        <div class="panel" style="flex:1;display:flex;flex-direction:column">
          <div class="panel-title"><span>COMPRESSOR</span><em>${c.comp.type} · ${c.comp.on ? 'ACTIVE' : 'OFF'}</em></div>
          <div class="type-bar">
            <span class="tag">TYPE</span>
            ${['VCA', 'FET', 'OPTICAL', 'TUBE'].map(t => `<button class="btn ${c.comp.type === t ? 'on' : ''}" data-act="comptype" data-v="${t}" style="font-size:8px;flex:1">${t}</button>`).join('')}
          </div>
          <div style="flex:1;min-height:0">${dynCurve('comp')}</div>
          <div style="display:flex;justify-content:space-around;margin-top:5px">
            ${knob('THRESH', 'comp', 'thr', i, c.comp.thr, -60, 0, 'db')}
            ${knob('RATIO', 'comp', 'ratio', i, c.comp.ratio, 1, 10, 'ratio')}
            ${knob('ATTACK', 'comp', 'attack', i, c.comp.attack, 1, 100, 'ms')}
            ${knob('RELEASE', 'comp', 'release', i, c.comp.release, 20, 600, 'ms')}
          </div>
          <div style="margin-top:5px">${toggle('COMP ON', 'comp', 'on', c.comp.on)} <span class="tag">GR <b style="color:var(--amber)" data-gr>-3.2</b> dB</span></div>
        </div>
      </div>
    </div>`;
  }

  function sendFaderHTML(i, big = false) {
    const v = state.sends[i][state.aux];
    const cap = big ? 11 : 7;
    return `<div class="ftrack" data-sendtrack="${state.aux}-${i}">
        <div class="ffill" style="height:${v}%"></div>
        <div class="fcap" data-act="sendfader" data-i="${i}" style="bottom:calc(${v}% - ${cap}px)"></div>
      </div>
      <div class="fmeter" data-sendmeter="${i}"><div class="lvl"></div></div>`;
  }

  function sendStripHTML(i) {
    const c = state.channels[i];
    const v = state.sends[i][state.aux];
    const mute = state.sendMute[i][state.aux];
    const pre = state.auxMode[state.aux] === 'PRE';
    return `<div class="strip ${state.selected === i ? 'sel' : ''}" data-act="select" data-i="${i}">
      <div class="num">${z2(i + 1)}</div>
      <div class="name">${c.name}</div>
      <div class="src">${c.source}</div>
      <div class="ms">
        <button class="btn mute ${mute ? 'on' : ''}" data-act="sendmute" data-i="${i}">M</button>
        <button class="btn ${pre ? 'on' : ''}" data-act="sendmode" data-i="${i}" style="font-size:7px">${pre ? 'PRE' : 'PST'}</button>
      </div>
      <div class="fader-wrap">${sendFaderHTML(i)}</div>
      <div class="db" data-sendval="${i}">${faderDb(v)}</div>
    </div>`;
  }

  function auxMasterFaderHTML(a) {
    const v = state.auxLevel[a];
    return `<div class="ftrack" data-auxmastertrack="${a}">
      <div class="ffill" style="height:${v}%"></div>
      <div class="fcap" data-act="auxmasterfader" data-i="${a}" style="bottom:calc(${v}% - 7px)"></div>
    </div>`;
  }

  function auxInternalMapHTML(a, ids) {
    return ids.map(id => {
      const send = state.sends[id][a], muted = state.sendMute[id][a];
      return `<button class="aux-map-cell ${!muted && send > 0 ? 'on' : ''}" data-act="auxmapsend" data-i="${id}" title="CH ${z2(id + 1)} ${state.channels[id].name} → AUX ${z2(a + 1)}">
        <span>${z2(id + 1)}</span><i style="width:${muted ? 0 : send}%"></i><em>${muted ? 'M' : Math.round(send)}</em>
      </button>`;
    }).join('');
  }

  function auxExternalMapHTML(a) {
    const g = AUX_EXT_GROUPS[state.auxExtGroup], dg = destGroups()[g.i];
    const si = auxSourceIndex(a), off = state.auxExtPage * 8;
    const tabs = AUX_EXT_GROUPS.map((x, k) => `<button class="btn ${k === state.auxExtGroup ? 'on' : ''}" data-act="auxextgroup" data-v="${k}" style="font-size:7px;padding:0 2px;min-width:0">${x.label}</button>`).join('');
    const cells = Array.from({ length: 8 }, (_, j) => {
      const di = off + j, label = dg.items[di];
      if (!label) return `<div class="aux-ext-cell empty">—</div>`;
      const on = !!state.patch[patchKey(1, si, g.i, di)];
      return `<button class="aux-ext-cell ${on ? 'on' : ''}" data-act="auxpatch" data-j="${di}" title="AUX ${z2(a + 1)} → ${label}">${label}</button>`;
    }).join('');
    const pager = dg.items.length > 8
      ? `<button class="btn ${state.auxExtPage === 0 ? 'on' : ''}" data-act="auxextpage" data-v="0" style="font-size:7px;padding:0 3px">1-8</button><button class="btn ${state.auxExtPage === 1 ? 'on' : ''}" data-act="auxextpage" data-v="1" style="font-size:7px;padding:0 3px">9-16</button>`
      : '';
    const count = Object.keys(state.patch).filter(k => k.startsWith('1/' + si + '|')).length;
    return { tabs, cells, pager, count };
  }

  function pageAux() {
    const a = state.aux, i = state.selected;
    const c = state.channels[i];
    const v = state.sends[i][a];
    const mute = state.sendMute[i][a];
    const pre = state.auxMode[a] === 'PRE';
    const ids = activeLayers()[state.layer].ids;
    const busButtons = auxIds().map(k => `<button class="btn ${a === k ? 'on' : ''}" data-act="auxsel" data-v="${k}">${z2(k + 1)}</button>`).join('');
    const ext = auxExternalMapHTML(a);
    return `<div class="page">
      ${head('AUX ' + z2(a + 1) + ' · ' + state.auxMode[a] + ' · ' + ids.length + ' SOURCES')}
      <div class="panel" style="height:34px;flex:none;display:flex;align-items:center;gap:8px">
        <span class="panel-title" style="margin:0">AUX BUS</span>
        <div style="display:flex;gap:3px">${busButtons}</div>
        <span class="tag amber">${state.auxMute[a] ? 'BUS MUTED' : 'BUS ON'}</span>
        <span class="tag">${state.auxMode[a]}</span>
        <span class="spacer"></span>
        <span class="page-note">BUS / MASTER + INTERNAL + EXTERNAL MAP</span>
      </div>
      <div class="row" style="flex:1;min-height:0">
        <div class="panel" style="flex:1;display:flex;flex-direction:column">
          <div class="panel-title"><span>SEND FADERS · ${activeLayers()[state.layer].label}</span><em>AUX ${z2(a + 1)}</em></div>
          <div class="send-row">${ids.map(sendStripHTML).join('')}</div>
        </div>
        <div class="panel" style="width:140px;display:flex;flex-direction:column;align-items:center">
          <div class="panel-title" style="width:100%"><span>SEND DETAIL</span><em>CH ${z2(i + 1)}</em></div>
          <div style="font:9px var(--mono);color:var(--dim);text-align:center">${c.name} → AUX ${z2(a + 1)}</div>
          <div class="big-fader">${sendFaderHTML(i, true)}</div>
          <div class="fader-readout" data-sendval="${i}">${faderDb(v)}</div>
          <div style="display:flex;gap:4px;margin-top:5px">
            <button class="btn mute ${mute ? 'on' : ''}" data-act="sendmute" data-i="${i}">MUTE</button>
            <button class="btn ${pre ? 'on' : ''}" data-act="sendmode" data-i="${i}">${pre ? 'PRE' : 'POST'}</button>
          </div>
          <div style="width:100%;margin-top:auto">
            <div class="panel-title"><span>BUS MUTE</span><em>${state.auxMute[a] ? 'MUTED' : 'ON'}</em></div>
            <div style="display:flex;gap:6px;align-items:center;justify-content:center">
              ${knob('LEVEL', 'aux', 'level', a, state.auxLevel[a], 0, 100, 'pct', 'compact')}
              ${toggle('MUTE', 'auxmute', String(a), state.auxMute[a], 'mute')}
            </div>
          </div>
        </div>
      </div>
      <div class="row" style="height:120px;flex:none;min-height:0">
        <div class="panel" style="width:112px;display:flex;flex-direction:column;align-items:center">
          <div class="panel-title" style="width:100%"><span>BUS MASTER</span><em>AUX ${z2(a + 1)}</em></div>
          <div class="aux-master">${auxMasterFaderHTML(a)}</div>
          <div class="mbar" style="width:44px;height:6px" data-auxmeter="${a}"><div class="lvl"></div></div>
          <div class="fader-readout" data-auxmasterval>${faderDb(state.auxLevel[a])}</div>
        </div>
        <div class="panel" style="flex:1;display:flex;flex-direction:column">
          <div class="panel-title"><span>INTERNAL MAP · SOURCES → AUX ${z2(a + 1)}</span><em>CLICK = ON/OFF</em></div>
          <div class="aux-map-grid">${auxInternalMapHTML(a, ids)}</div>
        </div>
        <div class="panel" style="flex:1.25;display:flex;flex-direction:column">
          <div class="panel-title"><span>EXTERNAL MAP · AUX ${z2(a + 1)} → OUT</span><em>${ext.pager}</em></div>
          <div class="aux-ext-tabs">${ext.tabs}</div>
          <div class="aux-ext-grid">${ext.cells}</div>
        </div>
      </div>
      <div class="panel" style="height:26px;flex:none;display:flex;align-items:center;gap:10px">
        <span class="page-note">INTERNAL = DSP1 MIXBUS SENDS · EXTERNAL = ${ext.count} PATCH(ES), SHARED WITH ROUTING MATRIX</span>
        <span class="spacer"></span>
        <button class="btn" data-act="sendclear">CLEAR SENDS</button>
        <button class="btn" data-act="auxextclear">CLEAR MAP</button>
      </div>
    </div>`;
  }

  function pageRouting() {
    const groups = sourceGroups(), dests = destGroups();
    const sg = groups[state.srcGroup], dg = dests[state.dstGroup];
    const srcButtons = groups.map((g, i) => `<button class="btn ${i === state.srcGroup ? 'on' : ''}" data-act="srcgroup" data-v="${i}" style="padding:0 3px;font-size:7px">${g.label}</button>`).join('');
    const dstButtons = dests.map((g, i) => `<button class="btn ${i === state.dstGroup ? 'on' : ''}" data-act="dstgroup" data-v="${i}" style="padding:0 3px;font-size:7px">${g.label}</button>`).join('');
    const nR = Math.max(1, Math.min(16, sg.items.length)), nC = Math.max(1, Math.min(16, dg.items.length));
    const cols = `92px repeat(${nC}, 1fr)`;
    const header = `<div class="mhead side">SRC ↓ / DST →</div>` + Array.from({ length: nC }, (_, j) => `<div class="mhead col">${dg.items[j] || ''}</div>`).join('');
    const rows = Array.from({ length: nR }, (_, i) => {
      const s = sg.items[i];
      const cells = Array.from({ length: nC }, (_, j) => {
        const d = dg.items[j];
        const key = state.srcGroup + '/' + i + '|' + state.dstGroup + '/' + j;
        return `<div class="mcell ${state.patch[key] ? 'on' : ''}" data-act="patch" data-i="${i}" data-j="${j}" title="${s} → ${d}"></div>`;
      }).join('');
      return `<div class="mhead side">${z2(i + 1) + ' ' + s}</div>${cells}`;
    }).join('');
    return `<div class="page">
      ${head(sg.label + ' → ' + dg.label)}
      <div class="panel" style="height:34px;flex:none;display:flex;align-items:center;gap:6px">
        <span class="panel-title" style="margin:0">SRC</span>
        <div style="display:flex;gap:3px">${srcButtons}</div>
      </div>
      <div class="panel" style="height:34px;flex:none;display:flex;align-items:center;gap:6px">
        <span class="panel-title" style="margin:0">DST</span>
        <div style="display:flex;gap:3px">${dstButtons}</div>
      </div>
      <div class="panel" style="flex:1;display:flex;flex-direction:column">
        <div class="panel-title"><span>MATRIX · ${sg.label} → ${dg.label}</span><em>${Object.keys(state.patch).length} PATCH · CLICK TO TOGGLE</em></div>
        <div class="matrix" style="grid-template-columns:${cols};grid-template-rows:16px repeat(${nR}, 1fr)">${header}${rows}</div>
      </div>
      <div class="panel" style="height:28px;flex:none;display:flex;align-items:center;gap:10px">
        <span class="page-note">SRC ${sg.items.length} · DST ${dg.items.length} · GROUP→GROUP / GROUP→AUX / MATRIX→GROUP · SETUP FILTERS RESOURCES</span>
        <span class="spacer"></span>
        <button class="btn" data-act="routeclear">CLEAR</button>
        <button class="btn" data-act="routepreset">LOAD PRESET</button>
      </div>
    </div>`;
  }

  function delayRow(side, i, label, ms, active) {
    const d = delayInfo(ms);
    return `<div class="drow ${active ? 'sel' : ''}" data-act="delayrow" data-side="${side}" data-i="${i}">
      <span>${label}</span>
      <span data-delaym="${ms}" data-side="${side}" data-i="${i}">${d.m} m</span>
      <span data-delayms="${ms}" data-side="${side}" data-i="${i}">${d.ms} ms</span>
      <span data-delayf="${ms}" data-side="${side}" data-i="${i}">${d.f} f</span>
    </div>`;
  }

  function pageDelay() {
    const side = state.delaySide;
    const idx = side === 'out' ? state.delayOut : state.selected;
    const ms = side === 'out' ? state.outputDelay[state.delayOut] : state.channels[state.selected].delayMs;
    const d = delayInfo(ms);
    const inRows = inputIds().map(i => delayRow('in', i, z2(i + 1) + ' ' + state.channels[i].name, state.channels[i].delayMs, side === 'in' && idx === i)).join('');
    const outRows = outIds().map(i => delayRow('out', i, z2(i + 1) + ' XLR', state.outputDelay[i], side === 'out' && idx === i)).join('');
    return `<div class="page">
      ${head(state.clock.rate + ' kHz · 343 m/s · ' + state.clock.fps + ' FPS · ' + frameMs().toFixed(1) + ' ms/FRAME')}
      <div class="row" style="flex:1">
        <div class="panel" style="flex:1;display:flex;flex-direction:column">
          <div class="panel-title"><span>INPUT DELAY</span><em>M · MS · FRAMES</em></div>
          <div class="dlist">${inRows}</div>
        </div>
        <div class="panel" style="flex:1;display:flex;flex-direction:column">
          <div class="panel-title"><span>OUTPUT DELAY</span><em>M · MS · FRAMES</em></div>
          <div class="dlist">${outRows}</div>
        </div>
      </div>
      <div class="panel" style="height:64px;flex:none;display:flex;align-items:center;gap:10px">
        <div style="display:flex;flex-direction:column;gap:4px">
          <button class="btn ${side === 'in' ? 'on' : ''}" data-act="delayside" data-v="in">INPUT</button>
          <button class="btn ${side === 'out' ? 'on' : ''}" data-act="delayside" data-v="out">OUTPUT</button>
        </div>
        <div style="min-width:88px;font:9px var(--mono);color:var(--dim)">
          SELECTED<br><b style="color:var(--amber)">${side === 'out' ? 'XLR ' + z2(state.delayOut + 1) : z2(state.selected + 1) + ' ' + state.channels[state.selected].name}</b>
        </div>
        ${knob('DELAY', 'delay', 'ms', idx, ms, 0, 100, 'ms')}
        <div class="delay-readouts">
          <div><b data-delaym="${ms}" data-side="${side}" data-i="${idx}">${d.m}</b> <span>meters</span></div>
          <div><b data-delayms="${ms}" data-side="${side}" data-i="${idx}">${d.ms}</b> <span>milliseconds</span></div>
          <div><b data-delayf="${ms}" data-side="${side}" data-i="${idx}">${d.f}</b> <span>frame @ ${state.clock.fps}fps</span></div>
        </div>
        <div style="display:flex;gap:3px;margin-left:auto">
          ${[0, 1, 5, 10].map(v => `<button class="btn" data-act="delaypreset" data-v="${v}">${v}ms</button>`).join('')}
        </div>
      </div>
    </div>`;
  }

  function pageSetup() {
    const s = state.setup;
    const countRow = (label, key, max, active) => `<div class="count-row">
      <span>${label}</span>
      <button class="btn mini" data-act="setupcount" data-v="${key}" data-d="-1">−</button>
      <b>${active}</b>
      <button class="btn mini" data-act="setupcount" data-v="${key}" data-d="1">+</button>
      <em>/${max}</em>
    </div>`;
    const inputCells = Array.from({ length: 24 }, (_, k) => {
      const on = s.inputOn[k], mode = state.channels[k].mode;
      return `<div class="ich ${on ? 'on' : ''} ${mode === 'STEREO' ? 'st' : ''}">
        <b>${z2(k + 1)}</b>
        <button class="mini ${on && mode === 'MONO' ? 'on' : ''}" data-act="setupinput" data-i="${k}" data-v="MONO">M</button>
        <button class="mini ${on && mode === 'STEREO' ? 'on' : ''}" data-act="setupinput" data-i="${k}" data-v="STEREO">S</button>
        <button class="mini off ${on ? '' : 'on'}" data-act="setupinput" data-i="${k}" data-v="OFF" title="disable">×</button>
      </div>`;
    }).join('');
    const auxCells = Array.from({ length: 8 }, (_, k) => `<div class="fmt-cell ${s.auxOn[k] ? 'on' : ''}">
      <b>${z2(k + 1)}</b>
      <button class="mini ${s.auxFormat[k] === 'MONO' ? 'on' : ''}" data-act="setupauxfmt" data-i="${k}" data-v="MONO">M</button>
      <button class="mini ${s.auxFormat[k] === 'STEREO' ? 'on' : ''}" data-act="setupauxfmt" data-i="${k}" data-v="STEREO">S</button>
    </div>`).join('');
    const groupRows = Array.from({ length: 8 }, (_, k) => {
      const on = s.groupOn[k], dest = s.groupDest[k];
      const next = GROUP_DESTS[(Math.max(0, GROUP_DESTS.indexOf(dest)) + 1) % GROUP_DESTS.length];
      return `<div class="dest-row ${on ? 'on' : ''}">
        <b>G${z2(k + 1)}</b>
        <button class="mini ${on && s.groupFormat[k] === 'MONO' ? 'on' : ''}" data-act="setupgroupfmt" data-i="${k}" data-v="MONO">M</button>
        <button class="mini ${on && s.groupFormat[k] === 'STEREO' ? 'on' : ''}" data-act="setupgroupfmt" data-i="${k}" data-v="STEREO">S</button>
        <button class="dest-btn" data-act="setupgroupdest" data-i="${k}" data-v="${next}">${dest}</button>
      </div>`;
    }).join('');
    const fxRows = Array.from({ length: 4 }, (_, k) => `<div class="fxmix-row ${s.fxOn[k] ? 'on' : ''}">
      <b>FX${k + 1}</b>
      <button class="mini ${s.fxMix[k] === 'PRE' ? 'on' : ''}" data-act="setupfxmode" data-i="${k}" data-v="PRE">PRE</button>
      <button class="mini ${s.fxMix[k] === 'POST' ? 'on' : ''}" data-act="setupfxmode" data-i="${k}" data-v="POST">POST</button>
    </div>`).join('');
    const presets = Object.keys(SETUP_PRESETS).map(p =>
      `<button class="btn ${s.preset === p ? 'on' : ''}" data-act="setuppreset" data-v="${p}" style="font-size:8px">${p}</button>`).join('');
    return `<div class="page">
      ${head('SHOW ' + s.show + ' · HIDE UNUSED ' + (s.hideUnused ? 'ON' : 'OFF'))}
      <div class="row" style="flex:1">
        <div class="panel setup-left">
          <div class="panel-title"><span>QUANTITY</span><em>${s.preset}</em></div>
          ${countRow('INPUT', 'inputOn', 24, idsFrom(s.inputOn).length)}
          ${countRow('AUX', 'auxOn', 8, idsFrom(s.auxOn).length)}
          ${countRow('FX', 'fxOn', 4, idsFrom(s.fxOn).length)}
          ${countRow('GROUP', 'groupOn', 8, idsFrom(s.groupOn).length)}
          ${countRow('OUTPUT', 'outOn', 16, idsFrom(s.outOn).length)}
          ${countRow('MATRIX', 'matrixOn', MATRIX_COUNT, idsFrom(s.matrixOn).length)}
          <button class="btn ${s.hideUnused ? 'on' : ''}" data-act="setuphide" style="width:100%;margin-top:4px">HIDE UNUSED · ${s.hideUnused ? 'ON' : 'OFF'}</button>
          <div class="panel-title" style="margin:8px 0 3px"><span>PRESET</span><em>SHOW</em></div>
          <div class="setup-presets">${presets}</div>
          <div class="page-note" style="margin-top:auto">The setup defines what exists in the show: everything else is hidden on the other pages.</div>
        </div>
        <div class="panel setup-center">
          <div class="panel-title"><span>INPUT CHANNELS · MONO / STEREO</span><em>${inputIds().length} ACTIVE</em></div>
          <div class="setup-grid input">${inputCells}</div>
          <div class="panel-title" style="margin:8px 0 3px"><span>AUX FORMAT · MONO / STEREO</span><em>${auxIds().length} BUS</em></div>
          <div class="setup-grid aux">${auxCells}</div>
          <div class="legend">M = MONO · S = STEREO · × = DISABLED · Disabled channels do not appear in strips, AUX sends or routing.</div>
        </div>
        <div class="panel setup-right">
          <div class="panel-title"><span>GROUP · DESTINATION</span><em>INTERNAL ROUTING</em></div>
          <div class="dest-list">${groupRows}</div>
          <div class="panel-title" style="margin:8px 0 3px"><span>FX MIX</span><em>PRE / POST FADER</em></div>
          <div class="fxmix-list">${fxRows}</div>
          <div class="page-note" style="margin-top:auto">Groups are channels 29–36: choose the format and the internal destination.</div>
        </div>
      </div>
      <div class="panel" style="height:28px;flex:none;display:flex;align-items:center;gap:10px">
        <span class="page-note">SHOW CONFIG · INPUT / AUX / FX / GROUP / OUTPUT · MONO/STEREO FORMATS · INTERNAL DESTINATIONS · HIDE UNUSED</span>
      </div>
    </div>`;
  }

  function pageAssign() {
    const a = state.assign, bank = a.banks[a.bank];
    const bankButtons = a.banks.map((b, k) => {
      const n = b.slots.filter(id => id !== null).length;
      const label = k === 0 ? '<span class="bank-sep">INPUT BANKS · 2 ROWS × 4</span>'
        : k === INPUT_BANKS ? '<span class="bank-sep">OUTPUT BANKS · 1 ROW × 4</span>' : '';
      return label + `<button class="bank-btn ${k === a.bank ? 'on' : ''}" data-act="assignbank" data-v="${k}">
        <b>${b.name}</b><em>${n}/${SLOT_COUNT}</em></button>`;
    }).join('');
    const slotCells = bank.slots.map((id, k) => {
      const c = id === null ? null : state.channels[id];
      return `<button class="slot-cell ${k === a.slot ? 'on' : ''} ${id === null ? 'empty' : ''}" data-act="assignslot" data-v="${k}">
        <b>${z2(k + 1)}</b><span>${c ? c.name : '—'}</span><em>${c ? z2(id + 1) : ''}</em></button>`;
    }).join('');
    const listTabs = ASSIGN_LISTS.map((l, k) => `<button class="btn ${k === a.list ? 'on' : ''}" data-act="assignlist" data-v="${k}" style="font-size:8px">${l.label}</button>`).join('');
    const items = ASSIGN_LISTS[a.list].ids().map(id => {
      const c = state.channels[id];
      const on = bank.slots.includes(id);
      return `<button class="assign-item ${on ? 'used' : ''}" data-act="assignitem" data-v="${id}">
        <b>${z2(id + 1)}</b><span>${c.name}</span><em>${c.source}</em></button>`;
    }).join('') || '<div class="page-note" style="padding:8px">NO ACTIVE RESOURCES · check SHOW CONFIG</div>';
    return `<div class="page">
      ${head('BANK ' + bank.name + ' · SLOT ' + z2(a.slot + 1) + ' · ' + (a.enabled ? 'CUSTOM BANK ON' : 'AUTO BANK'))}
      <div class="panel" style="height:96px;flex:none;padding:3px 4px">
        <div class="bank-picker fader-map">${bankButtons}</div>
      </div>
      <div class="row" style="flex:1">
        <div class="panel" style="width:302px;display:flex;flex-direction:column">
          <div class="panel-title"><span>FADER BANK · ${bank.name}</span><em>SEL SLOT → SEL SOURCE</em></div>
          <div class="slot-grid">${slotCells}</div>
          <div class="assign-actions">
            <button class="btn" data-act="assignfree">FREE ALL</button>
            <button class="btn" data-act="assignstraight">STRAIGHT</button>
            <button class="btn ${a.enabled ? 'on' : ''}" data-act="assignenable">CUSTOM ${a.enabled ? 'ON' : 'OFF'}</button>
          </div>
          <div class="page-note" style="margin-top:auto">12 faders per bank · 8 input banks (2×4) + 4 output banks (1×4) · saved in scene.</div>
        </div>
        <div class="panel" style="flex:1;display:flex;flex-direction:column">
          <div class="panel-title"><span>SOURCES / DESTINATIONS</span><em>CLICK = ASSIGN</em></div>
          <div style="display:flex;gap:3px;margin-bottom:4px">${listTabs}</div>
          <div class="assign-list">${items}</div>
        </div>
      </div>
      <div class="panel" style="height:26px;flex:none;display:flex;align-items:center;gap:10px">
        <span class="page-note">INPUT 2×4 + OUTPUT 1×4 · FREE ALL → SEL BANK → SEL SLOT → SEL IN/OUT · SAVED IN SCENE</span>
      </div>
    </div>`;
  }

  function pageMonitor() {
    const m = state.monitor;
    const sources = ['LR',
      ...auxIds().map(k => 'AUX ' + z2(k + 1)),
      ...groupIds().map(k => 'GRP ' + z2(k + 1)),
      'CARD 1', 'CARD 2', 'USB',
      ...fxIds().map(k => 'FX RET ' + (k + 1))];
    const srcButtons = sources.map(s => `<button class="btn ${m.source === s ? 'on' : ''}" data-act="monsrc" data-v="${s}" style="font-size:7px">${s}</button>`).join('');
    const dests = ['PHONES 1', 'PHONES 2', 'CR SPK'];
    return `<div class="page">
      ${head('CONTROL ROOM · PHONES · SOLO / DIM')}
      <div class="row" style="flex:1">
        <div class="panel" style="flex:1;display:flex;flex-direction:column">
          <div class="panel-title"><span>MONITOR SOURCE</span><em>${m.source}</em></div>
          <div class="mon-sources">${srcButtons}</div>
          <div class="page-note" style="margin-top:auto">SOURCE FROM DSP1 · AUX / MIXBUS / CARD / USB / FX RET · LR = MASTER</div>
        </div>
        <div class="panel" style="width:214px;display:flex;flex-direction:column;align-items:center">
          <div class="panel-title" style="width:100%"><span>LISTEN</span><em>${m.mute ? 'MUTED' : m.dim ? 'DIM' : 'ON'}</em></div>
          ${knob('MON LEVEL', 'mon', 'level', 0, m.level, 0, 100, 'pct', 'xl')}
          <div class="mst-meters" style="height:92px">
            <div class="mst-meter"><span>L</span><div class="fmeter" data-mastermeter="L" data-monmeter="1"><div class="lvl"></div></div></div>
            <div class="mst-meter"><span>R</span><div class="fmeter" data-mastermeter="R" data-monmeter="1"><div class="lvl"></div></div></div>
          </div>
          <div style="display:flex;gap:4px;margin-top:7px;flex-wrap:wrap;justify-content:center">
            <button class="btn ${m.dim ? 'on' : ''}" data-act="montoggle" data-v="dim">DIM -20</button>
            <button class="btn ${m.mono ? 'on' : ''}" data-act="montoggle" data-v="mono">MONO</button>
            <button class="btn mute ${m.mute ? 'on' : ''}" data-act="montoggle" data-v="mute">MUTE</button>
          </div>
          <div style="width:100%;margin-top:auto">
            <div class="panel-title"><span>DESTINATION</span><em>ANALOG OUT</em></div>
            <div style="display:flex;gap:3px">${dests.map(p => `<button class="btn ${m.phones === p ? 'on' : ''}" data-act="monphones" data-v="${p}" style="flex:1;font-size:7px">${p}</button>`).join('')}</div>
          </div>
        </div>
      </div>
      <div class="panel" style="height:30px;flex:none;display:flex;align-items:center;gap:10px">
        <span class="page-note">INDEPENDENT LISTENING FROM MASTER · DIM -20 dB · MONO SUMS L+R · PHONES 1/2 AND CONTROL ROOM</span>
      </div>
    </div>`;
  }

  function pageMacro() {
    const mc = state.macro, m = state.macros[mc.idx];
    const keys = state.macros.map((x, k) => `<button class="macro-key ${mc.idx === k ? 'sel' : ''} ${mc.recording === k ? 'rec' : ''} ${mc.playing === k ? 'play' : ''}" data-act="macroidx" data-v="${k}">
        <b>A${k + 1}</b><span>${x.name}</span><em>${x.steps.length} steps</em>
      </button>`).join('');
    const steps = m.steps.length ? m.steps.map((s, k) => `<div class="step ${mc.playing === mc.idx && mc.step === k + 1 ? 'cur' : ''}">
        <span>${z2(k + 1)}</span><b>${s.act}</b>
        <span>${s.value !== undefined ? (+s.value).toFixed(1) : (s.v || s.obj || '')}</span>
        <span>${s.i !== undefined && s.i !== '' ? 'CH ' + z2(+s.i + 1) : ''}</span>
      </div>`).join('') : '<div class="page-note" style="padding:8px">NO ACTIONS RECORDED · press REC and use the controls on other pages</div>';
    return `<div class="page">
      ${head('8 ASSIGNABLE KEYS · RECORD / PLAYBACK')}
      <div class="row" style="flex:1">
        <div class="panel" style="width:236px;display:flex;flex-direction:column">
          <div class="panel-title"><span>ASSIGNABLE</span><em>REC / PLAY</em></div>
          <div class="macro-keys">${keys}</div>
          <div class="macro-ctrl">
            <button class="btn mute ${mc.recording !== null ? 'on' : ''}" data-act="macrorec">● REC</button>
            <button class="btn ${mc.playing !== null ? 'on' : ''}" data-act="macroplay">▶ PLAY</button>
          </div>
          <div class="macro-ctrl">
            <button class="btn" data-act="macroclear">CLEAR</button>
            <button class="btn" data-act="macrosave">SAVE</button>
          </div>
          <div class="page-note" style="margin-top:auto">REC captures clicks, faders and knobs; PLAY replays them in sequence. SAVE persists in the browser.</div>
        </div>
        <div class="panel" style="flex:1;display:flex;flex-direction:column">
          <div class="panel-title"><span>SEQUENCE · ${m.name}</span><em><span id="macro-step">${mc.step}/${m.steps.length}</span> STEPS</em></div>
          <div class="steplist">${steps}</div>
        </div>
      </div>
      <div class="panel" style="height:30px;flex:none;display:flex;align-items:center;gap:10px">
        <span class="page-note">${mc.recording !== null ? '● RECORDING ON A' + z2(mc.recording + 1) : mc.playing !== null ? '▶ PLAYING MACRO A' + z2(mc.playing + 1) : 'SELECT A KEY AND PRESS REC'}</span>
      </div>
    </div>`;
  }

  function pageScenes() {
    const sc = state.scene, bank = Math.floor(sc.slot / 8), base = bank * 8;
    const sel = state.scenes[sc.slot];
    const rows = Array.from({ length: 8 }, (_, k) => {
      const slot = base + k, s = state.scenes[slot];
      const safes = s && s.safe ? SCENE_SCOPE.filter(([g]) => s.safe[g]).map(([g]) => g[0].toUpperCase()).join('') : '';
      return `<div class="scene-row ${sc.slot === slot ? 'sel' : ''}" data-act="sceneuse" data-v="${slot}">
        <span>${z2(slot + 1)}</span><b>${s ? s.name : '— EMPTY —'}</b>
        <span class="scene-time">${s ? s.time : ''}</span><span class="scene-safe">${safes || (s ? 'ALL' : '')}</span>
      </div>`;
    }).join('');
    const bankTabs = SCENE_BANKS.map((b, k) => `<button class="btn ${k === bank ? 'on' : ''}" data-act="scenebank" data-v="${k}">BANK ${b}</button>`).join('');
    const scope = SCENE_SCOPE.map(([key, label]) => `<button class="scene-scope ${sc.safe[key] ? 'on' : ''}" data-act="scenesafe" data-v="${key}">
      <span class="dot ${sc.safe[key] ? 'on' : ''}"></span>${label}</button>`).join('');
    return `<div class="page">
      ${head('SHOW: ' + sc.show + ' · STRUCTURE: SHOW → BANK → SCENE')}
      <div class="row" style="flex:1">
        <div class="panel" style="flex:1;display:flex;flex-direction:column">
          <div class="panel-title"><span>SCENE STRUCTURE · BANK ${SCENE_BANKS[bank]}</span><em>${state.scenes.filter(Boolean).length}/32 SCENES</em></div>
          <div class="scene-banks">${bankTabs}<span class="tag amber">${z2(sc.slot + 1)}</span></div>
          <div class="scenelist">${rows}</div>
          <div class="page-note" style="margin-top:auto">${sel ? 'SCENE ' + z2(sc.slot + 1) + ' · ' + sel.name + ' · ' + sel.time : 'EMPTY SLOT · press STORE to save the current state'}</div>
        </div>
        <div class="panel scene-editor" style="width:256px;display:flex;flex-direction:column;gap:5px">
          <div class="panel-title"><span>SCENE ${z2(sc.slot + 1)}</span><em>${sel ? 'STORED' : 'EMPTY'}</em></div>
          <label class="scene-label">NAME</label>
          <input id="scene-name" class="txt-input" maxlength="22" value="${sel ? sel.name : 'SCENE ' + z2(sc.slot + 1)}">
          <div class="scene-actions">
            <button class="btn" data-act="scenestore" data-v="${sc.slot}">STORE</button>
            <button class="btn" data-act="sceneload" data-v="${sc.slot}">RECALL</button>
            <button class="btn" data-act="scenecopy">COPY</button>
            <button class="btn" data-act="scenepaste">PASTE</button>
          </div>
          <div class="scene-actions">
            <button class="btn" data-act="sceneprev">◀ PREV</button>
            <button class="btn" data-act="scenenext">NEXT ▶</button>
            <button class="btn mute" data-act="scenedelete" data-v="${sc.slot}">DELETE</button>
            <button class="btn ${sc.autoSave ? 'on' : ''}" data-act="sceneauto">AUTO</button>
          </div>
          <div class="panel-title" style="margin:2px 0 0"><span>RECALL SCOPE / SAFE</span><em>SAFE = EXCLUDED</em></div>
          <div class="scene-scope-grid">${scope}</div>
          <div class="page-note" style="margin-top:auto">A SAFE group is excluded from recall. Structure saved in the browser.</div>
        </div>
      </div>
      <div class="panel" style="height:28px;flex:none;display:flex;align-items:center;gap:10px">
        <span class="page-note">STRUCTURE: SHOW → 4 BANKS × 8 SCENES → 32 SLOTS · STORE / RECALL / COPY / PASTE / DELETE · PER-GROUP SAFE</span>
      </div>
    </div>`;
  }

  function pageMeter() {
    const rows = inputIds().slice(0, 16).map(i => `<div class="mrow">
      <div class="mlabel">${z2(i + 1)}</div>
      <div class="mbar" data-meterh="${i}"><div class="lvl"></div></div>
      <div class="mval" data-mval="${i}">-∞</div>
    </div>`).join('');
    return `<div class="page">
      ${head(inputIds().length + '× INPUT · ' + outIds().length + '× XLR OUT · ' + groupIds().length + '× GROUP')}
      <div class="panel" style="flex:1">
        <div class="panel-title"><span>INPUT METERS</span><em>PEAK / RMS</em></div>
        <div class="metro">${rows}</div>
      </div>
      <div class="panel" style="height:44px;flex:none;display:flex;align-items:center;gap:20px">
        <span class="page-note">MASTER L</span><div class="mbar" style="flex:1" data-mastermeterh="L"><div class="lvl"></div></div>
        <span class="page-note">MASTER R</span><div class="mbar" style="flex:1" data-mastermeterh="R"><div class="lvl"></div></div>
      </div>
    </div>`;
  }

  function pageFX() {
    const slots = fxIds().map(i => {
      const f = state.fx[i];
      return `<div class="panel fx-slot">
        <div class="fx-head"><span class="fx-name">${f.name}</span><span class="tag ${f.on ? 'green' : ''}">${f.on ? 'ON' : 'OFF'}</span></div>
        <div class="fx-body">${f.params.map(([label, v], k) => knob(label, 'fx', String(k), i, v, 0, 100, 'pct')).join('')}</div>
        <div class="fx-foot">
          <div style="display:flex;gap:3px;align-items:center;margin-top:4px">
            <button class="btn ${f.on ? 'on' : ''}" data-act="fx" data-v="${i}" style="flex:1">${f.on ? 'ON' : 'BYPASS'}</button>
            <span class="tag">POS</span>
            ${['PRE FADER', 'POST FADER'].map(p => `<button class="btn ${f.pos === p ? 'on' : ''}" data-act="fxpos" data-i="${i}" data-v="${p}" style="font-size:7px">${p}</button>`).join('')}
          </div>
          <div style="display:flex;gap:3px;align-items:center;margin-top:4px">
            <span class="tag">OUT</span>
            ${['XLR', 'USB', 'CARD 1', 'CARD 2'].map(o => `<button class="btn ${f.out === o ? 'on' : ''}" data-act="fxout" data-i="${i}" data-v="${o}" style="font-size:7px;flex:1">${o}</button>`).join('')}
          </div>
        </div>
      </div>`;
    }).join('');
    return `<div class="page">
      ${head('DSP2 · ' + fxIds().length + ' EFFECT SLOT' + (fxIds().length === 1 ? '' : 'S'))}
      <div class="fx-grid">${slots}</div>
      <div class="panel" style="height:34px;flex:none;display:flex;align-items:center;gap:10px">
        <span class="page-note">FX RET → XLR / USB / CARD 1 / CARD 2 · insert PRE/POST FADER · DSP2 LOAD 42%</span><span class="spacer"></span>
        <span class="tag">5.1 UPMIX AVAILABLE</span>
      </div>
    </div>`;
  }

  function pageCard() {
    const tracks = [
      ['01', 'INTRO / CLICK', '00:32'], ['02', 'SETLIST / PT 1', '04:18'], ['03', 'SETLIST / PT 2', '05:44'],
      ['04', 'ENCORE', '03:09'], ['05', 'BACKING / AMBIENCE', '02:51'], ['06', 'RECORDING 22-11', '18:04']
    ].map((t, i) => `<div class="track ${state.card.track === i ? 'sel' : ''}" data-act="track" data-v="${i}">
      <span>${t[0]}</span><span>${t[1]}</span><span class="len">${t[2]}</span><span class="len">WAV 48k</span>
    </div>`).join('');
    return `<div class="page">
      ${head('X-LIVE · RECORD / PLAYBACK')}
      <div class="row" style="flex:1">
        <div class="panel" style="width:200px;display:flex;flex-direction:column">
          <div class="panel-title"><span>SOURCE</span><em>REVERSED CMDS</em></div>
          <div style="display:flex;gap:4px">
            <button class="btn ${state.card.medium === 'SD' ? 'on' : ''}" data-act="medium" data-v="SD" style="flex:1">SD CARD</button>
            <button class="btn ${state.card.medium === 'USB' ? 'on' : ''}" data-act="medium" data-v="USB" style="flex:1">USB</button>
          </div>
          <div class="counter" style="margin-top:auto">00:${z2(state.card.pos)}:12</div>
          <div class="transport">
            <button class="btn" data-act="transport" data-v="prev">◀◀</button>
            <button class="btn ${state.card.playing ? 'on' : ''}" data-act="transport" data-v="play">${state.card.playing ? '❚❚' : '▶'}</button>
            <button class="btn" data-act="transport" data-v="stop">■</button>
            <button class="btn ${state.card.rec ? 'mute on' : ''}" data-act="transport" data-v="rec">●</button>
          </div>
        </div>
        <div class="panel tracklist">
          <div class="panel-title"><span>${state.card.medium} / TRACKS</span><em>6 FILES</em></div>
          ${tracks}
        </div>
      </div>
    </div>`;
  }

  function pageAbout() {
    return `<div class="page">
      ${head('OPENX32 · M32 UI CONCEPT')}
      <div class="panel clock-panel" style="height:70px;flex:none;display:flex;flex-direction:column;justify-content:center;gap:4px">
        <div style="display:flex;align-items:center;gap:6px">
          <div class="panel-title" style="margin:0;min-width:52px"><span>CLOCK</span></div>
          <span class="tag">RATE</span>
          ${[44.1, 48].map(r => `<button class="btn ${state.clock.rate === r ? 'on' : ''}" data-act="clockrate" data-v="${r}">${r === 44.1 ? '44.1' : '48'} kHz</button>`).join('')}
          <span class="tag">FPS</span>
          ${[24, 25, 30, 50, 60].map(f => `<button class="btn ${state.clock.fps === f ? 'on' : ''}" data-act="clockfps" data-v="${f}" style="min-width:32px">${f}</button>`).join('')}
          <span class="tag green" style="margin-left:auto">● LOCKED</span>
        </div>
        <div style="display:flex;align-items:center;gap:6px">
          <span class="tag" style="min-width:52px;text-align:center">SOURCE</span>
          ${['INTERNAL', 'EXTERNAL', 'AES50 A', 'AES50 B', 'CARD 1', 'CARD 2'].map(s => `<button class="btn ${state.clock.source === s ? 'on' : ''}" data-act="clocksrc" data-v="${s}" style="font-size:8px">${s}</button>`).join('')}
        </div>
      </div>
      <div style="display:flex;gap:4px;flex:none">
        <button class="btn" data-act="page" data-v="setup" style="flex:1">SHOW CONFIG · INPUT / AUX / FX / GROUP / MATRIX</button>
        <button class="btn" data-act="page" data-v="assign" style="flex:1">FADER MAP · IN 2×4 / OUT 1×4</button>
      </div>
      <div class="sys-grid">
        <div class="panel">
          <div class="panel-title"><span>SYSTEM</span><em>BETA</em></div>
          <div class="kv"><span>KERNEL</span><span>Linux 6.18 LTS</span></div>
          <div class="kv"><span>DISPLAY</span><span>800×480 · 30 FPS</span></div>
          <div class="kv"><span>UI FRAMEWORK</span><span>LVGL 9.5.0</span></div>
          <div class="kv"><span>CPU</span><span>i.MX25 / ARM926EJ-S</span></div>
          <div class="kv"><span>DSP</span><span>2× ADSP-21371 SHARC</span></div>
          <div class="kv"><span>FPGA</span><span>Spartan-3A / ECP5</span></div>
          <div class="kv"><span>AUDIO</span><span>32 IN · 16 OUT · AES50</span></div>
          <div class="kv"><span>CLOCK</span><span>${state.clock.rate} kHz · ${state.clock.source}</span></div>
          <div class="kv"><span>VIDEO FPS</span><span>${state.clock.fps} fps · ${frameMs().toFixed(1)} ms/frame</span></div>
          <div class="kv"><span>SHOW SETUP</span><span>${state.setup.preset} · ${inputIds().length}IN / ${auxIds().length}AUX / ${groupIds().length}GRP / ${matrixIds().length}MTX / ${outIds().length}OUT</span></div>
          <div class="kv"><span>FADER MAP</span><span>${state.assign.enabled ? 'CUSTOM' : 'AUTO'} · BANK ${state.assign.banks[state.assign.bank].name}</span></div>
          <div class="kv"><span>BUILD</span><span>m32-ui / prototype 0.3</span></div>
        </div>
        <div class="panel">
          <div class="panel-title"><span>M32 SURFACE MAPPING</span><em>PLANNED</em></div>
          <div class="maplist">
            <div><span>25 FADERS</span><b>12 logic faders + banks</b></div>
            <div><span>ENCODER WHEEL</span><b>scroll / select channel</b></div>
            <div><span>SOFT KEYS 1–8</span><b>page + mute / solo</b></div>
            <div><span>CH SELECT</span><b>select / assign</b></div>
            <div><span>AUX SENDS</span><b>${auxIds().length} bus · pre / post</b></div>
            <div><span>TOUCHSCREEN</span><b>drag nodes / faders</b></div>
            <div><span>AES50 A / B</span><b>stagebox / third party</b></div>
          </div>
          <div class="legal" style="margin-top:10px">
            UI concept inspired by large-format digital consoles. No DiGiCo logos, artwork,
            trademarks or protected assets are included. Intended for OpenX32 / M32 hobby use.
          </div>
        </div>
      </div>
      <div class="panel" style="height:54px;flex:none">
        <div class="panel-title"><span>ROADMAP</span><em>NEXT</em></div>
        <div class="page-note">0 · UI SPEC + AUX SENDS (this prototype) → 1 · EEZ STUDIO SCREENS → 2 · LVGL BINDINGS → 3 · USB BOOT TEST → 4 · DSP / FPGA PARITY</div>
      </div>
    </div>`;
  }

  const PAGES = {
    overview: pageOverview, channel: pageChannel, eq: pageEQ, dyn: pageDyn, aux: pageAux,
    setup: pageSetup, assign: pageAssign, monitor: pageMonitor, routing: pageRouting, delay: pageDelay, meter: pageMeter,
    fx: pageFX, card: pageCard, macro: pageMacro, scenes: pageScenes, about: pageAbout
  };

  /* ---------------- render ---------------- */

  function bottomHTML() {
    const c = state.channels[state.selected];
    return `<div class="bb-group"><span class="bb-key">SEL</span><b>${z2(state.selected + 1)} ${c.name}</b></div>
      <div class="bb-group"><span class="bb-key">M/S</span><span>${c.mute ? 'MUTED' : c.solo ? 'SOLO' : 'ON'}</span></div>
      <div class="bb-group"><span class="bb-key">FADER</span><span data-fval="${state.selected}">${faderDb(c.fader)}</span></div>
      <div class="bb-group"><span class="bb-key">VIEW</span><span>${PAGE_TITLE[state.page]}</span></div>
      ${state.page === 'aux' ? `<div class="bb-group"><span class="bb-key">AUX</span><b>${z2(state.aux + 1)} ${state.auxMode[state.aux]}</b></div>` : ''}
      ${state.page === 'setup' ? `<div class="bb-group"><span class="bb-key">CFG</span><b>${inputIds().length}IN · ${auxIds().length}AUX · ${groupIds().length}GRP · ${matrixIds().length}MTX · ${outIds().length}OUT</b></div>` : ''}
      ${state.page === 'assign' ? `<div class="bb-group"><span class="bb-key">MAP</span><b>${state.assign.banks[state.assign.bank].name} · SLOT ${z2(state.assign.slot + 1)} · ${state.assign.enabled ? 'CUSTOM' : 'AUTO'}</b></div>` : ''}
      ${state.page === 'monitor' ? `<div class="bb-group"><span class="bb-key">MON</span><b>${state.monitor.source} · ${state.monitor.level}%</b></div>` : ''}
      ${state.page === 'macro' ? `<div class="bb-group"><span class="bb-key">MAC</span><b>A${z2(state.macro.idx + 1)} ${state.macro.recording !== null ? 'REC' : state.macro.playing !== null ? 'PLAY' : 'IDLE'}</b></div>` : ''}
      ${state.page === 'scenes' ? `<div class="bb-group"><span class="bb-key">SCENE</span><b>SLOT ${z2(state.scene.slot + 1)}</b></div>` : ''}
      ${state.page === 'about' ? `<div class="bb-group"><span class="bb-key">CLK</span><b>${state.clock.rate}k · ${state.clock.fps}fps · ${state.clock.source}</b></div>` : ''}
      <div class="bb-msg" id="bb-msg">${state.msg}</div>`;
  }

  function render() {
    syncSetup();
    $('#screen').innerHTML = PAGES[state.page]();
    $('#bottombar').innerHTML = bottomHTML();
    document.querySelectorAll('.tab').forEach(b => b.classList.toggle('active', b.dataset.v === state.page));
    document.querySelectorAll('.soft').forEach(b => {
      const v = b.dataset.v;
      b.classList.toggle('on', v === state.page || (v === 'eq' && state.page === 'eq') || (v === 'dyn' && state.page === 'dyn'));
    });
    const rate = $('#rate');
    if (rate) rate.textContent = state.clock.rate === 44.1 ? '44.1k' : '48k';
    paintMeters();
    paintDelayReadouts();
  }

  function setMsg(text) {
    state.msg = text;
    const el = $('#bb-msg');
    if (el) el.textContent = text;
  }

  /* ---------------- interactions ---------------- */

  /* Centralized parameter writes: shared by knobs and the macro player. */
  function writeValue(obj, key, i, value) {
    value = +value;
    if (obj === 'fx') state.fx[+i].params[+key][1] = value;
    else if (obj === 'aux') state.auxLevel[+i] = value;
    else if (obj === 'master') state.master.level = value;
    else if (obj === 'mon') state.monitor.level = value;
    else if (obj === 'delay') {
      if (state.delaySide === 'out') state.outputDelay[+i] = value;
      else state.channels[+i].delayMs = value;
    } else {
      const c = state.channels[state.selected];
      const target = obj === 'ch' ? c : obj === 'eq' ? c.eq[+i] : obj === 'gate' ? c.gate : obj === 'comp' ? c.comp : state.channels[+i];
      target[key] = value;
    }
  }

  function setKnob(t, value) {
    const d = t.dataset, min = +d.min, max = +d.max;
    value = clamp(value, min, max);
    writeValue(d.obj, d.key, d.i, value);
    if (d.obj === 'master') document.querySelectorAll('[data-fval="23"]').forEach(el => { el.textContent = faderDb(value); });
    if (d.obj === 'delay') paintDelayReadouts();
    if (d.obj === 'ch' && d.key === 'pan') paintPan(+d.i);
    t.dataset.val = value;
    const knobEl = $('.knob', t);
    if (knobEl) knobEl.style.setProperty('--rot', rot(value, min, max) + 'deg');
    const out = $('output', t);
    if (out) out.textContent = fmtVal(value, d.unit);
    setMsg(d.obj.toUpperCase() + ' · ' + d.key.toUpperCase() + ' ' + fmtVal(value, d.unit));
  }

  function paintDelayReadouts() {
    const valueFor = (side, i) => side === 'out' ? state.outputDelay[+i] : state.channels[+i].delayMs;
    document.querySelectorAll('[data-delaym]').forEach(el => { el.textContent = delayInfo(valueFor(el.dataset.side, el.dataset.i)).m + ' m'; });
    document.querySelectorAll('[data-delayms]').forEach(el => { el.textContent = delayInfo(valueFor(el.dataset.side, el.dataset.i)).ms + ' ms'; });
    document.querySelectorAll('[data-delayf]').forEach(el => { el.textContent = delayInfo(valueFor(el.dataset.side, el.dataset.i)).f + ' f'; });
  }

  function startKnob(e, t) {
    e.preventDefault();
    const startY = e.clientY;
    const initial = +t.dataset.val;
    const min = +t.dataset.min, max = +t.dataset.max;
    const move = ev => setKnob(t, initial + (startY - ev.clientY) / 90 * (max - min));
    const up = () => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', up);
      document.body.style.cursor = '';
      recordStep({ act: 'knob', obj: t.dataset.obj, key: t.dataset.key, i: t.dataset.i, value: +t.dataset.val });
    };
    document.body.style.cursor = 'ns-resize';
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
  }

  /* Update strip mini-pan and PAN knobs on detail views at the same time. */
  function paintPan(i) {
    const v = state.channels[i].pan;
    const pct = ((v + 6) / 12 * 100).toFixed(1) + '%';
    document.querySelectorAll('[data-pan-i="' + i + '"]').forEach(p => {
      const marker = $('.pan-track i', p);
      const label = $('b', p);
      if (marker) marker.style.left = pct;
      if (label) label.textContent = panLabel(v);
    });
    document.querySelectorAll('[data-act="knob"][data-obj="ch"][data-key="pan"][data-i="' + i + '"]').forEach(k => {
      const dial = $('.knob', k);
      const out = $('output', k);
      if (dial) dial.style.setProperty('--rot', rot(v, -6, 6) + 'deg');
      if (out) out.textContent = panLabel(v);
    });
  }

  function paintFader(track, value, i) {
    const fill = $('.ffill', track);
    const cap = $('.fcap', track);
    if (fill) fill.style.height = value + '%';
    if (cap) {
      const big = cap.offsetHeight > 16;
      cap.style.bottom = 'calc(' + value + '% - ' + (big ? 11 : 7) + 'px)';
    }
    document.querySelectorAll('[data-fval="' + i + '"]').forEach(el => { el.textContent = faderDb(value); });
  }

  function startFader(e, t) {
    e.preventDefault();
    const i = +t.dataset.i, track = $('[data-ftrack="' + i + '"]', t.closest('.page') || document);
    if (!track) return;
    const rect = track.getBoundingClientRect();
    const move = ev => {
      const v = clamp((rect.bottom - ev.clientY) / rect.height * 100, 0, 100);
      state.channels[i].fader = v;
      paintFader(track, v, i);
    };
    const up = () => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', up);
      document.body.style.cursor = '';
      recordStep({ act: 'fader', i: String(i), value: state.channels[i].fader });
      setMsg('FADER ' + z2(i + 1) + ' · ' + faderDb(state.channels[i].fader) + ' dB');
    };
    document.body.style.cursor = 'ns-resize';
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
  }

  function startPan(e, t) {
    e.preventDefault();
    const i = +t.dataset.i, track = $('.pan-track', t);
    if (!track) return;
    const rect = track.getBoundingClientRect();
    const setFromX = x => {
      const raw = clamp((x - rect.left) / rect.width * 12 - 6, -6, 6);
      const v = Math.round(raw * 2) / 2;
      state.channels[i].pan = Math.abs(v) < .25 ? 0 : v;
      paintPan(i);
      setMsg('PAN CH ' + z2(i + 1) + ' · ' + panLabel(state.channels[i].pan));
    };
    setFromX(e.clientX);
    const move = ev => setFromX(ev.clientX);
    const up = () => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', up);
      document.body.style.cursor = '';
      recordStep({ act: 'pan', i: String(i), value: state.channels[i].pan });
    };
    document.body.style.cursor = 'ew-resize';
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
  }

  function paintSendFader(track, value, i) {
    const fill = $('.ffill', track);
    const cap = $('.fcap', track);
    if (fill) fill.style.height = value + '%';
    if (cap) {
      const big = cap.offsetHeight > 16;
      cap.style.bottom = 'calc(' + value + '% - ' + (big ? 11 : 7) + 'px)';
    }
    document.querySelectorAll('[data-sendval="' + i + '"]').forEach(el => { el.textContent = faderDb(value); });
  }

  function startSendFader(e, t) {
    e.preventDefault();
    const i = +t.dataset.i, track = $('[data-sendtrack="' + state.aux + '-' + i + '"]', t.closest('.page') || document);
    if (!track) return;
    const rect = track.getBoundingClientRect();
    const move = ev => {
      const v = clamp((rect.bottom - ev.clientY) / rect.height * 100, 0, 100);
      state.sends[i][state.aux] = v;
      paintSendFader(track, v, i);
    };
    const up = () => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', up);
      document.body.style.cursor = '';
      recordStep({ act: 'sendfader', i: String(i), aux: String(state.aux), value: state.sends[i][state.aux] });
      setMsg('SEND ' + z2(i + 1) + ' → AUX ' + z2(state.aux + 1) + ' · ' + faderDb(state.sends[i][state.aux]) + ' dB');
    };
    document.body.style.cursor = 'ns-resize';
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
  }

  function startAuxMasterFader(e, t) {
    e.preventDefault();
    const a = +t.dataset.i, track = $('[data-auxmastertrack="' + a + '"]', t.closest('.page') || document);
    if (!track) return;
    const rect = track.getBoundingClientRect();
    const move = ev => {
      const v = clamp((rect.bottom - ev.clientY) / rect.height * 100, 0, 100);
      state.auxLevel[a] = v;
      const fill = $('.ffill', track);
      if (fill) fill.style.height = v + '%';
      t.style.bottom = 'calc(' + v + '% - 7px)';
      document.querySelectorAll('[data-auxmasterval]').forEach(el => { el.textContent = faderDb(v); });
      const out = $('[data-act="knob"][data-obj="aux"][data-i="' + a + '"] output');
      if (out) out.textContent = fmtVal(v, 'pct');
    };
    const up = () => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', up);
      document.body.style.cursor = '';
      setMsg('AUX ' + z2(a + 1) + ' BUS MASTER · ' + faderDb(state.auxLevel[a]));
    };
    document.body.style.cursor = 'ns-resize';
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
  }

  function startEqNode(e, t) {
    e.preventDefault();
    const k = +t.dataset.i, svg = t.closest('svg');
    const rect = svg.getBoundingClientRect();
    const c = state.channels[state.selected];
    const move = ev => {
      const x = clamp((ev.clientX - rect.left) / rect.width, 0, 1);
      const y = clamp((ev.clientY - rect.top) / rect.height, 0, 1);
      c.eq[k].f = Math.round(20 * Math.pow(1000, x));
      c.eq[k].g = Math.round((12 - y * 24) * 2) / 2;
      state.eqBand = k;
      const path = $('#eqcurve', svg);
      if (path) path.setAttribute('d', eqCurve());
      svg.querySelectorAll('.node').forEach((n, idx) => {
        const b = c.eq[idx];
        n.setAttribute('cx', (Math.log(b.f / 20) / Math.log(1000) * 640).toFixed(1));
        n.setAttribute('cy', clamp(100 - eqBandResponse(b, b.f) * 4, 3, 197).toFixed(1));
        n.classList.toggle('sel', idx === k);
      });
    };
    const up = () => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', up);
      document.body.style.cursor = '';
      recordStep({ act: 'eqnode', i: String(k), f: c.eq[k].f, g: c.eq[k].g });
      setMsg('EQ BAND ' + (k + 1) + ' · ' + fmtFreq(c.eq[k].f) + ' / ' + fmtVal(c.eq[k].g, 'db') + 'dB');
      render();
    };
    document.body.style.cursor = 'grab';
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
  }

  document.addEventListener('pointerdown', e => {
    const t = e.target.closest('[data-act]');
    if (!t) return;
    if (t.dataset.act === 'fader') startFader(e, t);
    else if (t.dataset.act === 'pan') startPan(e, t);
    else if (t.dataset.act === 'sendfader') startSendFader(e, t);
    else if (t.dataset.act === 'auxmasterfader') startAuxMasterFader(e, t);
    else if (t.dataset.act === 'knob') startKnob(e, t);
    else if (t.dataset.act === 'eqnode') startEqNode(e, t);
  });

  /* ---------------- macro recorder + scene memory ---------------- */

  const STORE_KEY = 'openx32-m32ui-v1';
  const MACRO_SKIP = new Set(['pan', 'setupcount', 'setuphide', 'setuppreset', 'setupinput',
    'setupauxfmt', 'setupgroupfmt', 'setupgroupdest', 'setupfxmode',
    'assignbank', 'assignslot', 'assignlist', 'assignitem', 'assignfree', 'assignstraight', 'assignenable',
    'auxextgroup', 'auxextpage', 'auxpatch', 'auxextclear', 'auxmapsend', 'auxmasterfader',
    'macroidx', 'macrorec', 'macroplay', 'macroclear', 'macrosave',
    'sceneuse', 'scenebank', 'scenestore', 'sceneload', 'scenedelete', 'scenecopy',
    'scenepaste', 'sceneprev', 'scenenext', 'sceneauto', 'scenesafe',
    'clockrate', 'clockfps', 'clocksrc', 'msg']);

  function recordStep(step) {
    if (state.macro.recording === null || state.macro.playing !== null) return;
    const m = state.macros[state.macro.recording];
    m.steps.push(step);
    if (m.steps.length > 120) m.steps.shift();
    setMsg('REC A' + z2(state.macro.recording + 1) + ' · ' + m.steps.length + ' STEPS');
  }

  function saveStore() {
    try {
      localStorage.setItem(STORE_KEY, JSON.stringify({
        scenes: state.scenes,
        setup: state.setup,
        assign: state.assign,
        macros: state.macros.map(m => ({ name: m.name, steps: m.steps }))
      }));
    } catch (e) { /* file:// may block localStorage: the prototype stays usable */ }
  }

  function snapshot() {
    return JSON.parse(JSON.stringify({
      channels: state.channels.map(c => ({
        fader: c.fader, gain: c.gain, pan: c.pan, mute: c.mute, solo: c.solo, phase: c.phase,
        hpf: c.hpf, hpfFreq: c.hpfFreq, mode: c.mode, link: c.link, delayMs: c.delayMs,
        insertSlot: c.insertSlot, insertPos: c.insertPos, eq: c.eq, gate: c.gate, comp: c.comp
      })),
      aux: state.aux, sends: state.sends, sendMute: state.sendMute, auxMode: state.auxMode,
      auxMute: state.auxMute, auxLevel: state.auxLevel,
      auxExtGroup: state.auxExtGroup, auxExtPage: state.auxExtPage, patch: state.patch,
      outputDelay: state.outputDelay, fx: state.fx, card: state.card,
      monitor: state.monitor, clock: state.clock, setup: state.setup, assign: state.assign, master: state.master,
      macros: state.macros.map(m => ({ name: m.name, steps: m.steps }))
    }));
  }

  function restoreScene(data, safe = {}) {
    if (!data || !Array.isArray(data.channels)) return;
    if (!safe.input) data.channels.forEach((c, k) => { if (state.channels[k]) Object.assign(state.channels[k], c); });
    const apply = (group, keys) => {
      if (safe[group]) return;
      keys.forEach(k => { if (data[k] !== undefined) state[k] = JSON.parse(JSON.stringify(data[k])); });
    };
    apply('setup', ['setup']);
    apply('assign', ['assign']);
    apply('aux', ['aux', 'sends', 'sendMute', 'auxMode', 'auxMute', 'auxLevel', 'auxExtGroup', 'auxExtPage']);
    apply('routing', ['patch']);
    apply('output', ['outputDelay', 'fx', 'card', 'clock', 'master']);
    apply('monitor', ['monitor']);
    apply('macro', ['macros']);
    syncSetup();
    stopMacro();
  }

  function storeScene(slot) {
    const input = $('#scene-name');
    const name = (input && input.value.trim()) || 'SCENE ' + z2(slot + 1);
    state.scenes[slot] = {
      name, time: new Date().toLocaleTimeString('en-GB', { hour: '2-digit', minute: '2-digit' }),
      safe: JSON.parse(JSON.stringify(state.scene.safe)),
      data: snapshot()
    };
    state.scene.slot = slot;
    saveStore();
    setMsg('SCENE ' + z2(slot + 1) + ' STORED · ' + name);
  }

  function loadScene(slot) {
    const s = state.scenes[slot];
    if (!s) { setMsg('SLOT ' + z2(slot + 1) + ' EMPTY'); return; }
    restoreScene(s.data, s.safe || {});
    state.scene.slot = slot;
    if (s.safe) state.scene.safe = JSON.parse(JSON.stringify(s.safe));
    setMsg('RECALL · ' + s.name);
  }

  let macroTimer = null;
  function stopMacro() {
    if (macroTimer) { clearTimeout(macroTimer); macroTimer = null; }
    state.macro.playing = null;
    state.macro.step = 0;
  }

  function playMacro(idx) {
    const m = state.macros[idx];
    if (!m || !m.steps.length) { setMsg('MACRO A' + z2(idx + 1) + ' EMPTY'); return; }
    stopMacro();
    state.macro.playing = idx;
    state.macro.step = 0;
    const next = () => {
      if (state.macro.playing !== idx) return;
      const steps = m.steps;
      if (state.macro.step >= steps.length) {
        state.macro.playing = null; state.macro.step = 0; macroTimer = null;
        setMsg('MACRO A' + z2(idx + 1) + ' · COMPLETE'); render(); return;
      }
      const s = steps[state.macro.step++];
      if (s.act === 'fader' && s.value !== undefined) state.channels[+s.i].fader = +s.value;
      else if (s.act === 'pan' && s.value !== undefined) state.channels[+s.i].pan = +s.value;
      else if (s.act === 'sendfader' && s.value !== undefined) state.sends[+s.i][s.aux !== undefined ? +s.aux : state.aux] = +s.value;
      else if (s.act === 'knob' && s.value !== undefined) writeValue(s.obj, s.key, s.i, +s.value);
      else if (s.act === 'eqnode' && s.f !== undefined) {
        const c = state.channels[state.selected];
        c.eq[+s.i].f = +s.f; c.eq[+s.i].g = +s.g;
      } else applyAction(s, +s.i, s.v, true);
      render();
      const label = $('#macro-step');
      if (label) label.textContent = state.macro.step + '/' + steps.length;
      macroTimer = setTimeout(next, 130);
    };
    next();
  }

  function applyAction(d, i, v, skipRender = false) {
    switch (d.act) {
      case 'page': state.page = v; break;
      case 'select': state.selected = i; break;
      case 'layer': state.layer = +v; state.assign.bank = +v; break;
      case 'assignbank': state.assign.bank = +v; if (+v >= INPUT_BANKS) state.assign.list = 3; state.layer = +v; state.assign.slot = 0; break;
      case 'assignslot': state.assign.slot = +v; break;
      case 'assignlist': state.assign.list = +v; break;
      case 'assignitem': {
        const id = +v, a = state.assign, b = a.banks[a.bank];
        b.slots[a.slot] = id;
        a.enabled = true;
        a.slot = (a.slot + 1) % SLOT_COUNT;
        setMsg('FADER ' + z2((a.slot + SLOT_COUNT - 1) % SLOT_COUNT + 1) + ' ← ' + state.channels[id].name);
        break;
      }
      case 'assignfree':
        state.assign.banks.forEach(b => b.slots.fill(null));
        state.assign.enabled = true;
        setMsg('ALL BANKS CLEARED');
        break;
      case 'assignstraight':
        state.assign.banks = straightBanks();
        state.assign.enabled = true;
        state.assign.bank = 0; state.layer = 0; state.assign.slot = 0;
        setMsg('STRAIGHT BANKS RESTORED');
        break;
      case 'assignenable': state.assign.enabled = !state.assign.enabled; setMsg('CUSTOM BANK · ' + (state.assign.enabled ? 'ON' : 'OFF')); break;
      case 'pan':
        if (d.value !== undefined && state.channels[+d.i]) state.channels[+d.i].pan = +d.value;
        break;
      case 'mute': state.channels[i].mute = !state.channels[i].mute; setMsg('CH ' + z2(i + 1) + ' ' + (state.channels[i].mute ? 'MUTED' : 'UNMUTED')); break;
      case 'solo': state.channels[i].solo = !state.channels[i].solo; setMsg('CH ' + z2(i + 1) + ' ' + (state.channels[i].solo ? 'SOLO' : 'SOLO OFF')); break;
      case 'toggle': {
        const c = state.channels[state.selected];
        const target = d.obj === 'ch' ? c : d.obj === 'gate' ? c.gate : d.obj === 'comp' ? c.comp : d.obj === 'auxmute' ? state.auxMute : d.obj === 'mon' ? state.monitor : d.obj === 'master' ? state.master : state;
        target[d.key] = !target[d.key];
        setMsg(d.obj.toUpperCase() + ' · ' + d.key.toUpperCase() + ' ' + (target[d.key] ? 'ON' : 'OFF'));
        break;
      }
      case 'auxsel': state.aux = +v; setMsg('AUX BUS ' + z2(state.aux + 1) + ' SELECTED'); break;
      case 'sendmute': state.sendMute[i][state.aux] = !state.sendMute[i][state.aux]; setMsg('SEND CH ' + z2(i + 1) + ' → AUX ' + z2(state.aux + 1) + ' ' + (state.sendMute[i][state.aux] ? 'MUTED' : 'ON')); break;
      case 'sendmode': state.auxMode[state.aux] = state.auxMode[state.aux] === 'PRE' ? 'POST' : 'PRE'; setMsg('AUX ' + z2(state.aux + 1) + ' MODE ' + state.auxMode[state.aux]); break;
      case 'sendclear': state.sends = state.sends.map(row => { row[state.aux] = 0; return row; }); setMsg('AUX ' + z2(state.aux + 1) + ' SENDS CLEARED'); break;
      case 'auxextgroup': state.auxExtGroup = +v; state.auxExtPage = 0; break;
      case 'auxextpage': state.auxExtPage = +v; break;
      case 'auxpatch': {
        const si = auxSourceIndex(state.aux), g = AUX_EXT_GROUPS[state.auxExtGroup];
        const di = +d.j, key = patchKey(1, si, g.i, di);
        const label = destGroups()[g.i].items[di] || 'DST ' + z2(di + 1);
        if (state.patch[key]) { delete state.patch[key]; setMsg('AUX ' + z2(state.aux + 1) + ' → ' + label + ' · UNPATCHED'); }
        else { state.patch[key] = true; setMsg('AUX ' + z2(state.aux + 1) + ' → ' + label + ' · PATCHED'); }
        break;
      }
      case 'auxextclear': {
        const prefix = '1/' + auxSourceIndex(state.aux) + '|';
        Object.keys(state.patch).forEach(k => { if (k.startsWith(prefix)) delete state.patch[k]; });
        setMsg('AUX ' + z2(state.aux + 1) + ' EXTERNAL MAP CLEARED');
        break;
      }
      case 'auxmapsend': {
        if (state.sendMute[i][state.aux]) {
          state.sendMute[i][state.aux] = false;
          if (state.sends[i][state.aux] <= 0) state.sends[i][state.aux] = 40;
        } else state.sendMute[i][state.aux] = true;
        setMsg('INTERNAL MAP CH ' + z2(i + 1) + ' → AUX ' + z2(state.aux + 1) + ' · ' + (state.sendMute[i][state.aux] ? 'OFF' : 'ON'));
        break;
      }
      case 'monsrc': state.monitor.source = v; setMsg('MONITOR SOURCE · ' + v); break;
      case 'montoggle': state.monitor[v] = !state.monitor[v]; setMsg('MONITOR ' + v.toUpperCase() + ' · ' + (state.monitor[v] ? 'ON' : 'OFF')); break;
      case 'monphones': state.monitor.phones = v; setMsg('MONITOR OUT · ' + v); break;
      case 'clockrate': state.clock.rate = +v; setMsg('CLOCK RATE · ' + state.clock.rate + ' kHz'); break;
      case 'clockfps': state.clock.fps = +v; paintDelayReadouts(); setMsg('VIDEO FRAME RATE · ' + state.clock.fps + ' fps · ' + frameMs().toFixed(1) + ' ms/frame'); break;
      case 'clocksrc': state.clock.source = v; setMsg('CLOCK SOURCE · ' + v); break;
      case 'chanmode': state.channels[state.selected].mode = v; if (v === 'MONO') state.channels[state.selected].link = null; setMsg('CH ' + z2(state.selected + 1) + ' MODE ' + v); break;
      case 'chanlink': {
        const c = state.channels[state.selected];
        if (c.link !== null) {
          const old = c.link; c.link = null;
          if (state.channels[old].link === state.selected) state.channels[old].link = null;
          setMsg('CH ' + z2(state.selected + 1) + ' UNLINKED');
        } else {
          const ids = selectableIds(), at = ids.indexOf(state.selected);
          c.link = ids.length > 1 ? ids[(at + 1) % ids.length] : null;
          if (c.link === null) { setMsg('LINK NEEDS TWO ACTIVE CHANNELS'); break; }
          state.channels[c.link].link = state.selected;
          state.channels[c.link].mode = c.mode = 'STEREO';
          setMsg('LINK ' + z2(state.selected + 1) + ' + ' + z2(c.link + 1));
        }
        break;
      }
      case 'insertslot': state.channels[state.selected].insertSlot = v; setMsg('INSERT CH ' + z2(state.selected + 1) + ' → ' + v); break;
      case 'insertpos': state.channels[state.selected].insertPos = v; setMsg('INSERT POS ' + v); break;
      case 'gatetype': state.channels[state.selected].gate.type = v; setMsg('GATE TYPE · ' + v); break;
      case 'comptype': state.channels[state.selected].comp.type = v; setMsg('COMP TYPE · ' + v); break;
      case 'fxpos': state.fx[i].pos = v; setMsg(state.fx[i].name + ' INSERT ' + v); break;
      case 'fxout': state.fx[i].out = v; setMsg(state.fx[i].name + ' OUT → ' + v); break;
      case 'eqband': state.eqBand = +v; break;
      case 'eqtype': {
        const b = state.channels[state.selected].eq[i];
        if (b && EQ_MODES.includes(v)) {
          b.type = v;
          state.eqBand = i;
          if ((v === 'LOW CUT' || v === 'HIGH CUT') && b.g > 0) b.g = -12;
          setMsg('EQ BAND ' + (i + 1) + ' · ' + v);
        }
        break;
      }
      case 'srcgroup': state.srcGroup = +v; break;
      case 'dstgroup': state.dstGroup = +v; break;
      case 'patch': {
        const key = state.srcGroup + '/' + i + '|' + state.dstGroup + '/' + d.j;
        if (state.patch[key]) delete state.patch[key]; else state.patch[key] = true;
        setMsg('PATCH ' + sourceGroups()[state.srcGroup].items[i] + ' → ' + destGroups()[state.dstGroup].items[+d.j]);
        break;
      }
      case 'delayside': state.delaySide = v; break;
      case 'delayrow': state.delaySide = d.side; if (d.side === 'out') state.delayOut = i; else state.selected = i; break;
      case 'delaypreset': {
        const value = +v;
        if (state.delaySide === 'out') state.outputDelay[state.delayOut] = value;
        else state.channels[state.selected].delayMs = value;
        setMsg('DELAY ' + value + ' ms · ' + delayInfo(value).m + ' m · ' + delayInfo(value).f + ' frame');
        break;
      }
      case 'fx': state.fx[+v].on = !state.fx[+v].on; setMsg(state.fx[+v].name + ' ' + (state.fx[+v].on ? 'ON' : 'BYPASS')); break;
      case 'medium': state.card.medium = v; break;
      case 'track': state.card.track = +v; break;
      case 'transport': {
        if (v === 'play') { state.card.playing = !state.card.playing; setMsg('TRANSPORT ' + (state.card.playing ? 'PLAY' : 'PAUSE')); }
        else if (v === 'rec') { state.card.rec = !state.card.rec; setMsg('RECORD ' + (state.card.rec ? 'ARMED' : 'OFF')); }
        else if (v === 'stop') { state.card.playing = false; setMsg('TRANSPORT STOP'); }
        else { state.card.track = Math.max(0, state.card.track - 1); }
        break;
      }
      case 'routeclear': state.patch = {}; setMsg('ROUTING CLEARED'); break;
      case 'routepreset': {
        state.patch = {};
        const n = Math.min(sourceGroups()[state.srcGroup].items.length, destGroups()[state.dstGroup].items.length);
        for (let k = 0; k < n; k++) state.patch[state.srcGroup + '/' + k + '|' + state.dstGroup + '/' + k] = true;
        setMsg('ROUTING PRESET LOADED · ' + n + ' PATCH');
        break;
      }
      case 'setupcount': {
        const arr = state.setup[v];
        if (Array.isArray(arr)) setFlagCount(v, idsFrom(arr).length + (+d.d || 0));
        setMsg('SETUP · ' + v.toUpperCase() + ' = ' + idsFrom(arr).length);
        syncSetup();
        break;
      }
      case 'setuphide': state.setup.hideUnused = !state.setup.hideUnused; setMsg('HIDE UNUSED · ' + (state.setup.hideUnused ? 'ON' : 'OFF')); syncSetup(); break;
      case 'setuppreset': applyPreset(v); break;
      case 'setupinput': {
        const k = +d.i, on = v !== 'OFF';
        state.setup.inputOn[k] = on;
        if (on) state.channels[k].mode = v === 'STEREO' ? 'STEREO' : 'MONO';
        if (v === 'MONO') state.channels[k].link = null;
        state.setup.preset = 'CUSTOM';
        setMsg('INPUT ' + z2(k + 1) + ' · ' + (on ? v : 'DISABLED'));
        syncSetup();
        break;
      }
      case 'setupauxfmt': state.setup.auxFormat[+d.i] = v; state.setup.preset = 'CUSTOM'; setMsg('AUX ' + z2(+d.i + 1) + ' · ' + v); break;
      case 'setupgroupfmt': state.setup.groupFormat[+d.i] = v; state.setup.preset = 'CUSTOM'; setMsg('GROUP ' + z2(+d.i + 1) + ' · ' + v); syncSetup(); break;
      case 'setupgroupdest': state.setup.groupDest[+d.i] = v; state.setup.preset = 'CUSTOM'; setMsg('GROUP ' + z2(+d.i + 1) + ' → ' + v); break;
      case 'setupfxmode': state.setup.fxMix[+d.i] = v; state.setup.preset = 'CUSTOM'; state.fx[+d.i].pos = v === 'PRE' ? 'PRE FADER' : 'POST FADER'; setMsg('FX ' + (+d.i + 1) + ' MIX · ' + v); break;
      case 'macroidx':
        state.macro.idx = +v;
        if (state.macro.recording !== null) state.macro.recording = +v;
        stopMacro();
        break;
      case 'macrorec':
        if (state.macro.recording !== null) { state.macro.recording = null; setMsg('REC STOP'); }
        else { stopMacro(); state.macro.recording = state.macro.idx; setMsg('REC A' + z2(state.macro.idx + 1) + ' · RECORDING'); }
        break;
      case 'macroplay':
        if (state.macro.playing !== null) { stopMacro(); setMsg('PLAY STOP'); }
        else playMacro(state.macro.idx);
        break;
      case 'macroclear':
        state.macros[state.macro.idx].steps = [];
        state.macro.step = 0;
        saveStore();
        setMsg('MACRO A' + z2(state.macro.idx + 1) + ' CLEARED');
        break;
      case 'macrosave': saveStore(); setMsg('MACROS SAVED IN BROWSER'); break;
      case 'sceneuse': {
        state.scene.slot = +v;
        const s = state.scenes[+v];
        if (s && s.safe) state.scene.safe = JSON.parse(JSON.stringify(s.safe));
        break;
      }
      case 'scenebank': state.scene.slot = Math.floor(+v) * 8 + (state.scene.slot % 8); break;
      case 'scenestore': storeScene(+v); break;
      case 'sceneload': loadScene(+v); break;
      case 'scenedelete': state.scenes[+v] = null; saveStore(); setMsg('SCENE ' + z2(+v + 1) + ' DELETED'); break;
      case 'scenecopy':
        state.scene.clipboard = state.scenes[state.scene.slot] ? JSON.parse(JSON.stringify(state.scenes[state.scene.slot])) : null;
        setMsg(state.scene.clipboard ? 'SCENE COPIED' : 'EMPTY SLOT · NOTHING TO COPY');
        break;
      case 'scenepaste':
        if (!state.scene.clipboard) { setMsg('CLIPBOARD EMPTY'); break; }
        state.scenes[state.scene.slot] = JSON.parse(JSON.stringify(state.scene.clipboard));
        state.scene.safe = JSON.parse(JSON.stringify(state.scene.clipboard.safe || {}));
        saveStore(); setMsg('SCENE PASTED INTO ' + z2(state.scene.slot + 1));
        break;
      case 'sceneprev': state.scene.slot = (state.scene.slot + 31) % 32; break;
      case 'scenenext': state.scene.slot = (state.scene.slot + 1) % 32; break;
      case 'sceneauto': state.scene.autoSave = !state.scene.autoSave; setMsg('AUTO SAVE · ' + (state.scene.autoSave ? 'ON' : 'OFF')); break;
      case 'scenesafe': {
        state.scene.safe[v] = !state.scene.safe[v];
        const s = state.scenes[state.scene.slot];
        if (s) { s.safe = JSON.parse(JSON.stringify(state.scene.safe)); saveStore(); }
        setMsg('SAFE ' + v.toUpperCase() + ' · ' + (state.scene.safe[v] ? 'EXCLUDED FROM RECALL' : 'INCLUDED'));
        break;
      }
      case 'msg': setMsg(v); break;
      case 'soft': {
        const map = { sel: 'channel', eq: 'eq', dyn: 'dyn', aux: 'aux', setup: 'setup', assign: 'assign', delay: 'delay', monitor: 'monitor', macro: 'macro', overview: 'overview' };
        if (map[v]) state.page = map[v];
        else if (v === 'mute') { state.channels[state.selected].mute = !state.channels[state.selected].mute; setMsg('MUTE ' + z2(state.selected + 1)); }
        else if (v === 'solo') { state.channels[state.selected].solo = !state.channels[state.selected].solo; setMsg('SOLO ' + z2(state.selected + 1)); }
        else if (v === 'prev' || v === 'next') state.layer = (state.layer + (v === 'next' ? 1 : activeLayers().length - 1)) % activeLayers().length;
        break;
      }
      default: return;
    }
    if (!skipRender) render();
  }

  document.addEventListener('click', e => {
    const t = e.target.closest('[data-act]');
    if (!t) return;
    const d = t.dataset, i = +d.i, v = d.v;
    if (state.macro.recording !== null && !MACRO_SKIP.has(d.act)) {
      recordStep({ act: d.act, i: d.i, j: d.j, v: d.v, obj: d.obj, key: d.key, side: d.side });
    }
    applyAction(d, i, v);
    if (state.scene.autoSave && state.scenes[state.scene.slot]) {
      state.scenes[state.scene.slot].data = snapshot();
      saveStore();
    }
  });

  document.addEventListener('keydown', e => {
    if (e.target && e.target.tagName === 'INPUT') return;
    if (e.key >= '1' && e.key <= '9') { state.page = PAGE_ORDER[+e.key - 1]; render(); }
    else if (e.key === '0') { state.page = 'about'; render(); }
    else if (e.key === '-') { state.page = 'macro'; render(); }
    else if (e.key === '=') { state.page = 'scenes'; render(); }
    else if (e.key.toLowerCase() === 'v') { state.page = 'monitor'; render(); }
    else if (e.key.toLowerCase() === 'p') { state.page = 'setup'; render(); }
    else if (e.key.toLowerCase() === 'g') { state.page = 'assign'; render(); }
    else if (e.key.toLowerCase() === 'r') { state.page = 'macro'; render(); }
    else if (e.key.toLowerCase() === 'c') { state.page = 'scenes'; render(); }
    else if (e.key.toLowerCase() === 'd') { state.page = 'delay'; render(); }
    else if (e.key === 'ArrowRight') { const ids = selectableIds(); if (ids.length) state.selected = ids[(Math.max(0, ids.indexOf(state.selected)) + 1) % ids.length]; render(); }
    else if (e.key === 'ArrowLeft') { const ids = selectableIds(); if (ids.length) state.selected = ids[(Math.max(0, ids.indexOf(state.selected)) + ids.length - 1) % ids.length]; render(); }
    else if (e.key.toLowerCase() === 'm') { state.channels[state.selected].mute = !state.channels[state.selected].mute; render(); }
    else if (e.key.toLowerCase() === 's') { state.channels[state.selected].solo = !state.channels[state.selected].solo; render(); }
  });

  /* ---------------- animation + scaling ---------------- */

  function paintMeters() {
    const active = inputIds();
    const avg = active.length ? active.reduce((s, k) => s + (state.levels[k] || 0), 0) / active.length : 0;
    state.masterMeter = { l: clamp(avg * 1.06, 0, 1), r: clamp(avg * .94, 0, 1) };
    document.querySelectorAll('[data-mastermeter]').forEach(m => {
      let v = m.dataset.mastermeter === 'L' ? state.masterMeter.l : state.masterMeter.r;
      if (m.dataset.monmeter) {
        if (state.monitor.mono) v = (state.masterMeter.l + state.masterMeter.r) / 2;
        if (state.monitor.dim) v *= .25;
        if (state.monitor.mute) v = 0;
      }
      if (state.master.mute) v = 0;
      const l = $('.lvl', m);
      if (!l) return;
      l.style.height = clamp(v * 100, 0, 100).toFixed(0) + '%';
      l.style.background = v > .92 ? 'var(--red)' : v > .74 ? 'var(--yellow)' : 'var(--green)';
    });
    document.querySelectorAll('[data-mastermeterh]').forEach(m => {
      const v = m.dataset.mastermeterh === 'L' ? state.masterMeter.l : state.masterMeter.r;
      const l = $('.lvl', m);
      if (!l) return;
      l.style.width = clamp(v * 100, 0, 100).toFixed(0) + '%';
      l.style.background = v > .92 ? 'var(--red)' : v > .74 ? 'var(--yellow)' : 'var(--green)';
    });
    document.querySelectorAll('[data-meter]').forEach(m => {
      const v = state.levels[+m.dataset.meter] || 0;
      const l = $('.lvl', m);
      if (!l) return;
      l.style.height = clamp(v * 100, 0, 100).toFixed(0) + '%';
      l.style.background = v > .92 ? 'var(--red)' : v > .74 ? 'var(--yellow)' : 'var(--green)';
    });
    document.querySelectorAll('[data-vumeter]').forEach(m => {
      const i = +m.dataset.vumeter, side = m.dataset.side;
      let v = side === 'L' ? state.levelsL[i] || 0 : side === 'R' ? state.levelsR[i] || 0 : state.levels[i] || 0;
      if (m.dataset.monmeter) {
        if (state.monitor.mono) v = ((state.levelsL[i] || 0) + (state.levelsR[i] || 0)) / 2;
        if (state.monitor.dim) v *= .25;
        if (state.monitor.mute) v = 0;
      }
      const l = $('.lvl', m);
      if (!l) return;
      l.style.height = clamp(v * 100, 0, 100).toFixed(0) + '%';
      l.style.background = v > .92 ? 'var(--red)' : v > .74 ? 'var(--yellow)' : 'var(--green)';
    });
    document.querySelectorAll('[data-meterh]').forEach(m => {
      const v = state.levels[+m.dataset.meterh] || 0;
      const l = $('.lvl', m);
      if (!l) return;
      l.style.width = clamp(v * 100, 0, 100).toFixed(0) + '%';
      l.style.background = v > .92 ? 'var(--red)' : v > .74 ? 'var(--yellow)' : 'var(--green)';
      const val = $('[data-mval="' + m.dataset.meterh + '"]');
      if (val) val.textContent = v < .01 ? '-∞' : (v * 40 - 40).toFixed(0);
    });
    document.querySelectorAll('[data-sendmeter]').forEach(m => {
      const idx = +m.dataset.sendmeter;
      const send = state.sends[idx][state.aux];
      const v = state.sendMute[idx][state.aux] ? 0 : (state.levels[idx] || 0) * (send / 100) * 1.4;
      const l = $('.lvl', m);
      if (!l) return;
      l.style.height = clamp(v * 100, 0, 100).toFixed(0) + '%';
      l.style.background = v > .92 ? 'var(--red)' : v > .74 ? 'var(--yellow)' : 'var(--green)';
    });
    document.querySelectorAll('[data-auxmeter]').forEach(m => {
      const v = state.auxMeter[+m.dataset.auxmeter] || 0;
      const l = $('.lvl', m);
      if (!l) return;
      l.style.width = clamp(v * 100, 0, 100).toFixed(0) + '%';
      l.style.background = v > .92 ? 'var(--red)' : v > .74 ? 'var(--yellow)' : 'var(--green)';
    });
    document.querySelectorAll('#spectrum i').forEach((bar, k) => {
      const v = clamp(.15 + state.levels[k % 16] * .8 + Math.random() * .12, 0, 1);
      bar.style.height = (v * 100).toFixed(0) + '%';
    });
  }

  let last = 0, tickCount = 0;
  function tick(now) {
    requestAnimationFrame(tick);
    if (now - last < 90) return;
    last = now;
    tickCount++;
    state.channels.forEach((c, i) => {
      let target = .16 + .1 * Math.sin(now / 900 + i * .7) + Math.random() * .18;
      if (c.mute) target = .01;
      if (c.solo && state.channels.some(x => x.solo) && !c.solo) target = .01;
      if (Math.random() < .018) target = .82 + Math.random() * .18;
      state.levels[i] += (target - state.levels[i]) * .28;
      state.levelsL[i] = clamp(state.levels[i] * (.86 + .14 * Math.sin(now / 700 + i * .6)), 0, 1);
      state.levelsR[i] = clamp(state.levels[i] * (.86 + .14 * Math.cos(now / 830 + i * .9)), 0, 1);
    });
    state.levels[22] = .52 + Math.random() * .18;
    state.levels[23] = .49 + Math.random() * .2;
    state.auxMeter.forEach((_, j) => {
      let sum = 0;
      for (let i = 0; i < 16; i++) {
        if (!state.sendMute[i][j]) sum += state.levels[i] * state.sends[i][j] / 100;
      }
      const target = state.auxMute[j] ? 0 : clamp(sum / 16 * 1.6 * state.auxLevel[j] / 100, 0, 1);
      state.auxMeter[j] += (target - state.auxMeter[j]) * .28;
    });
    paintMeters();
    document.querySelectorAll('[data-gr]').forEach(el => { el.textContent = (-1 - Math.random() * 5).toFixed(1); });
    if (tickCount % 10 === 0) {
      const d = new Date();
      $('#clock').textContent = z2(d.getHours()) + ':' + z2(d.getMinutes());
    }
  }

  function fit() {
    const s = Math.min(window.innerWidth / 800, window.innerHeight / 480);
    const stage = $('#stage');
    stage.style.transform = 'scale(' + s + ')';
    stage.style.left = ((window.innerWidth - 800 * s) / 2) + 'px';
    stage.style.top = ((window.innerHeight - 480 * s) / 2) + 'px';
  }

  try {
    const raw = localStorage.getItem(STORE_KEY);
    if (raw) {
      const saved = JSON.parse(raw);
      if (Array.isArray(saved.scenes)) saved.scenes.slice(0, state.scenes.length).forEach((s, k) => { if (s && s.data) state.scenes[k] = s; });
      if (saved.setup) Object.assign(state.setup, saved.setup);
      if (saved.assign) Object.assign(state.assign, saved.assign);
      if (Array.isArray(saved.macros)) saved.macros.slice(0, state.macros.length).forEach((m, k) => {
        if (m && Array.isArray(m.steps)) state.macros[k] = { name: m.name || state.macros[k].name, steps: m.steps };
      });
    }
  } catch (e) { /* storage unavailable */ }

  const initialPage = location.hash.replace('#', '');
  if (PAGE_ORDER.includes(initialPage)) state.page = initialPage;

  window.addEventListener('resize', fit);
  fit();
  render();
  requestAnimationFrame(tick);
})();
