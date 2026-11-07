// The page: your boards over USB. Connect, flash, and each board's own screen beside its settings.
// A Screen is the radar's own code built for that display; with a board on the cable it shows that
// board's planes, and a touch on either side reaches the other.

import { startScene } from './scene.js';
import { loadRelief } from './relief.js';
import { reliefAround } from './relief-live.js';
import { Link } from './link.js';
import { detect, flash as writeFirmware } from './flasher.js';
import { Shell } from './console/shell.js';
import { CORE_COMMANDS, CORE_GROUPS } from './console/commands.js';
import { Terminal } from './term.js';
import { BoardPaths, BOARD_GROUPS, boardCommands } from './board-console.js';

const $ = (id) => document.getElementById(id);
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const esc = (t) => String(t).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
const fmt = (n) => n.toLocaleString('en-US');

const BOARDS = {
  ili9341: { name: '2.8" CYD', spec: 'ESP32, 320x240, resistive touch', chip: 'esp32', w: 320, h: 240, inch: 2.8, touch: 'resistive', twin: 'Twin1', shot: '02-live.png', yellow: true },
  st7796: { name: '3.5" CYD', spec: 'ESP32, 480x320, touch', chip: 'esp32', w: 480, h: 320, inch: 3.5, touch: 'resistive or capacitive', twin: 'Twin2', shot: 'st7796/02-live.png', yellow: true },
  ili9486: { name: '3.5" RPi LCD', spec: 'on OWLOSAirQuality, no touch', chip: 'esp32', w: 480, h: 320, inch: 3.5, touch: 'none', twin: 'Twin2', shot: 'ili9486/03-picked-heavy.png' },
  gc9a01: { name: '1.28" round', spec: 'Elecrow CrowPanel, ESP32-C3', chip: 'c3', w: 240, h: 240, inch: 1.28, touch: 'capacitive', round: true, twin: 'Twin4', shot: 'gc9a01/03-picked-heavy.png' },
  nmcydc5: { name: '2.8" NM-CYD-C5', spec: 'ESP32-C5, 320x240, resistive touch', chip: 'c5', w: 320, h: 240, inch: 2.8, touch: 'resistive', twin: 'Twin1', shot: '02-live.png' },
  es3c28p: { name: '2.8" LCDWIKI ES3C28P', spec: 'ESP32-S3, 320x240, capacitive touch', chip: 's3', w: 320, h: 240, inch: 2.8, touch: 'capacitive', twin: 'Twin1', shot: '02-live.png' },
  crowadv28: { name: '2.8" CrowPanel Advance', spec: 'ESP32-S3, 320x240 IPS, capacitive touch', chip: 's3', w: 320, h: 240, inch: 2.8, touch: 'capacitive', twin: 'Twin1', shot: '02-live.png' },
  crowadv50: { name: '5" CrowPanel Advance', spec: 'ESP32-S3, 800x480 IPS, capacitive touch', chip: 's3', w: 800, h: 480, inch: 5, touch: 'capacitive', twin: 'Twin', shot: 'crowadv50/02-live.png' },
  jc8048w550: { name: '5" Guition JC8048W550', spec: 'ESP32-S3, 800x480, capacitive touch', chip: 's3', w: 800, h: 480, inch: 5, touch: 'capacitive', twin: 'Twin', shot: 'crowadv50/02-live.png' },
};
const CHIP_NAME = { esp32: 'ESP32', s3: 'ESP32-S3', c3: 'ESP32-C3', c5: 'ESP32-C5' };
const THEMES = ['arctic', 'phosphor', 'amber', 'crimson', 'synth'];
const LOOKS = ['radar', 'map', 'classic'];
const RANGES = [2, 5, 10, 25, 50, 100, 250];
const SWITCHES = { vectors: 'Vectors', clock: 'UTC clock', block: 'Data block', conflict: 'Conflict' };
const KEPT = 'owlos.boards';                           // the boards this browser knows, by MAC. it stays here

let newest = '0';

const older = (a, b) => a.localeCompare(b, 'en', { numeric: true }) < 0;

/* --- a screen: one running radar --- */

class Screen {
  constructor(factory, first = {}) {
    this.factory = factory;
    this.first = first;
    this.raw = document.createElement('canvas');       // the radar's own pixels
    this.k = 1;                                        // screen pixels for one of them
    this.zoom = 1;                                     // what + and - set: 1 is one to one
    this.canvas = document.createElement('canvas');    // what is seen: the same, drawn up with no smoothing
    this.canvas.className = 'screen';
    this.canvas.setAttribute('aria-label', "the radar's screen");
    this.finger = { down: 0, pressed: 0, x: 0, y: 0 };
    this.opts = { vectors: 0, beam_step: 0, clock: 0, block: 0, autopilot: 0, lost: 0, conflict: 0, fl_lo: 0, fl_hi: 0 };
    this.m = null;
    this.drawn = false;
    this.last = 0;
    this.told = null;
    this.home = null;                                  // a board's: lat, lon, place, country
    this.onChange = null;                              // a touch changed something: (settings changed, plane picked)
    this.listen();
  }

  listen() {
    const c = this.canvas;
    const follow = (e) => {
      const box = c.getBoundingClientRect();
      this.finger.x = Math.round((e.clientX - box.left) * this.w / box.width);
      this.finger.y = Math.round((e.clientY - box.top) * this.h / box.height);
    };
    c.addEventListener('pointerdown', (e) => {
      follow(e);
      this.finger.down = 1;
      this.finger.pressed = 1;
      c.setPointerCapture(e.pointerId);
    });
    c.addEventListener('pointermove', (e) => {
      if (this.finger.down) {
        follow(e);
      }
    });
    for (const name of ['pointerup', 'pointercancel']) {
      c.addEventListener(name, () => {
        this.finger.down = 0;
      });
    }
  }

  async start() {
    const m = await window[this.factory]();
    let started = performance.now();
    this.now = () => Math.round(performance.now() - started) + 1;
    if (!m._twin_init(this.now())) {
      return;
    }
    this.m = m;
    this.w = m._twin_width();
    this.h = m._twin_height();
    this.raw.width = this.w;
    this.raw.height = this.h;
    this.rawPen = this.raw.getContext('2d');
    this.canvas.width = this.w;
    this.canvas.height = this.h;
    this.pen = this.canvas.getContext('2d');
    this.apply({ range: 50, ...this.first });
    started -= 60000;                                  // a new sky is empty till its next answer: that is now
  }

  cfg() {
    return JSON.parse(this.m.UTF8ToString(this.m._twin_set_json()));
  }

  apply(change) {
    const m = this.m;
    const c = { ...this.cfg(), ...change };
    const at = this.home ?? { lat: 50.9, lon: 4.48, place: 'DEMO SKY', country: '' };
    const place = m.stringToNewUTF8(at.place || 'HOME');
    const country = m.stringToNewUTF8(at.country || '');
    m._twin_cfg(at.lat, at.lon, place, country, c.range, c.up, c.theme, c.look, c.map, c.ground, c.dist_unit, c.alt_unit, c.labels);
    m._free(place);
    m._free(country);
    this.told = this.cfg();
  }

  setOpts() {
    const o = this.opts;
    this.m._twin_opt(o.vectors, o.beam_step, o.clock, o.block, o.autopilot, o.lost, o.conflict, o.fl_lo, o.fl_hi);
  }

  /* one setting by its console word. false: no such thing */
  set(name, value) {
    if (name === 'theme' && THEMES.includes(value)) {
      this.apply({ theme: THEMES.indexOf(value) });
    } else if (name === 'look' && LOOKS.includes(value)) {
      this.apply({ look: LOOKS.indexOf(value) });
    } else if (name === 'range' && Number(value) >= 2 && Number(value) <= 250) {
      this.apply({ range: Number(value) });
    } else if (name in SWITCHES && ['on', 'off'].includes(value)) {
      this.opts[name] = value === 'on' ? 1 : 0;
      this.setOpts();
    } else {
      return false;
    }
    return true;
  }

  setJson() {
    return this.m.UTF8ToString(this.m._twin_set_json());
  }

  /* what a board says of itself: how this screen shows it from now on */
  fromBoard(c) {
    this.home = { lat: c.lat, lon: c.lon, place: c.place, country: c.country };
    this.m._twin_board(1);
    this.apply({ range: c.range, up: c.up, theme: c.theme, look: c.look, map: c.map, ground: c.ground, dist_unit: c.dist_unit, alt_unit: c.alt_unit, labels: c.labels });
    Object.assign(this.opts, { vectors: c.vectors, beam_step: c.beam_step, clock: c.clock, block: c.block, autopilot: c.autopilot, lost: c.lost, conflict: c.conflict, fl_lo: c.fl_lo, fl_hi: c.fl_hi });
    this.setOpts();
  }

  /* a board's planes, line by line as it sends them */
  take(line) {
    const m = this.m;
    const held = (text, fn) => {
      const at = m.stringToNewUTF8(text);
      fn(at);
      m._free(at);
    };
    if (line.startsWith('OWLOS SKY BEGIN ')) {
      held(line.slice(16), (at) => m._twin_sky_begin(this.now(), at));
      this.inAnswer = true;
    } else if (line.startsWith('OWLOS PLANE ') && this.inAnswer) {
      held(line.slice(12), (at) => m._twin_sky_plane(at));
    } else if (line.startsWith('OWLOS SKY END ') && this.inAnswer) {
      m._twin_sky_end(this.now(), parseInt(line.slice(14), 10) || 0);
      this.inAnswer = false;
      return true;
    } else if (line.startsWith('OWLOS SEL ')) {
      m._twin_pick(parseInt(line.slice(10), 16) || 0);
    }
    return false;
  }

  async tile(k) {
    const m = this.m;
    const url = m.UTF8ToString(m._twin_map_url(k));
    if (!url) {
      return;
    }
    const answer = await fetch(url);
    if (!answer.ok) {
      return;
    }
    const bytes = new Uint8Array(await answer.arrayBuffer());
    const at = m._malloc(bytes.length);
    m.HEAPU8.set(bytes, at);
    m._twin_map_tile(k, at, bytes.length);
    m._free(at);
  }

  async map(tiles) {
    let next = 0;
    const worker = async () => {
      while (next < tiles) {
        const k = next++;
        try {
          await this.tile(k);
        } catch (e) {
          console.warn('tile', k, e);
        }
      }
    };
    await Promise.all([worker(), worker(), worker(), worker()]);
    this.m._twin_map_end();
  }

  /* a finger on the screen changed something: the console says which command that was */
  noticeFinger() {
    const what = this.m._twin_changed();
    if (!what) {
      return;
    }
    if (what & 1) {
      const now = this.cfg();
      for (const [key, word] of [['theme', THEMES], ['look', LOOKS]]) {
        if (this.told && now[key] !== this.told[key]) {
          say(`set ${key}=${word[now[key]]}`, 'cmd');
        }
      }
      if (this.told && now.range !== this.told.range) {
        say(`set range=${Math.round(now.range)}`, 'cmd');
      }
      this.told = now;
      drawPane();
    }
    this.onChange?.(what);
  }

  /* the browser gets a picture already at its size, so it has nothing left to scale or to smooth */
  show() {
    if (this.canvas.isConnected) {
      this.pen.imageSmoothingEnabled = this.k < 1;
      this.pen.drawImage(this.raw, 0, 0, this.canvas.width, this.canvas.height);
    }
  }

  frame(time) {
    if (time - this.last < 66) {
      return;
    }
    this.last = time;
    const m = this.m;
    const f = this.finger;
    const at = m._twin_frame(this.now(), Math.floor(Date.now() / 1000), f.down || f.pressed, f.x, f.y);
    f.pressed = 0;
    this.rawPen.putImageData(new ImageData(new Uint8ClampedArray(m.HEAPU8.buffer, at, this.w * this.h * 4), this.w, this.h), 0, 0);
    this.drawn = true;
    this.show();
    this.noticeFinger();
    const tiles = m._twin_map_wanted();
    if (tiles) {
      this.map(tiles);
    }
  }
}

/* --- what the page knows --- */

const hero = new Screen('Twin');
const app = {
  units: [],             // your boards
  cur: null,             // the one on show
  view: null,            // 'all', 'unit', 'demo' or null
  tab: 'view',
  port: null,            // the port of the board being connected
  chip: null,            // what its bootloader said
  kind: null,            // the board picked for it
  ours: false,           // it runs an older Flight Radar: its settings stay
  erase: false,          // ticked by the person, never by the page
  busy: null,            // { name, pct } while one is being flashed
};
let scene = null;

function remember() {
  try {
    localStorage.setItem(KEPT, JSON.stringify(app.units.map((u) => ({ mac: u.mac, name: u.name, build: u.build, zoom: u.screen.zoom }))));
  } catch (e) {
    console.warn(e);
  }
}

function addUnit({ mac, name, build, online = false, zoom = 1 }) {
  const screen = new Screen(BOARDS[build].twin);
  screen.zoom = zoom;
  const u = { mac, name, build, online, version: '', wifi: null, nets: [], port: null, link: null, planes: 0, sky: [], screen, ready: screen.start() };
  app.units.push(u);
  return u;
}

const screenNow = () => (app.cur ? app.cur.screen : hero);
const here = () => app.units.filter((u) => u.online);
const byMac = (mac) => app.units.find((u) => u.mac === mac);
const short = (u) => u.name.toLowerCase().replace(/\s+/g, '-').replace(/[^\p{L}\p{N}.-]/gu, '');

const usbName = (port) => ({ 0x1a86: 'CH340', 0x10c4: 'CP210X', 0x303a: 'USB' }[port?.getInfo?.().usbVendorId] ?? 'USB');

/* --- the amber bar: what is going on now, and which board --- */

function barState() {
  if (app.busy) {
    return `FLASHING ${esc(app.busy.name.toUpperCase())} ${app.busy.pct}%`;
  }
  const old = here().find((u) => older(u.version, newest));
  let text = '';
  if (!app.units.length) {
    text = 'NO BOARD: A DEMO SKY';
  } else if (app.view === 'unit' && app.cur?.online) {
    const u = app.cur;
    text = `${usbName(u.port)} / ${esc(BOARDS[u.build].name)} / ${u.version} / ${u.wifi ? 'WIFI' : 'NO WIFI'} / ${u.planes} PLANES`;
  } else if (app.view === 'unit' && app.cur) {
    text = 'NOT CONNECTED';
  } else {
    text = `${here().length} OF ${app.units.length} CONNECTED`;
  }
  if (old) {
    text += ` <button class="baract" data-update="${old.mac}">${esc(old.name.toUpperCase())} RUNS ${old.version}: UPDATE</button>`;
  }
  const waits = here().find((u) => u.aligning);
  if (waits) {
    text += ` / ${esc(waits.name.toUpperCase())}: TOUCH THE CROSSES ON ITS SCREEN <button class="baract" data-skip="${waits.mac}">SKIP</button>`;
  }
  return text;
}

/* --- the picture on top: with a board on a cable its planes fly over its own ground --- */

const KM_PER_UNIT = 0.12;
const UNITS_PER_METRE = 0.12;
let skyUnit = null;                                    // the board the picture shows
let demoGround = null;                                 // the ground of the demo sky

function toScene(a, home) {
  const east = 111.32 * Math.cos(home.lat * Math.PI / 180);
  const way = (a.track ?? 0) * Math.PI / 180;
  const speed = (a.gs ?? 0) * 1.852 / 3600 / KM_PER_UNIT;
  const feet = a.alt_baro;
  const late = a.seen_pos ?? 0;                        // seconds since this position was heard
  return {
    id: a.hex, name: (a.flight ?? '').trim() || a.r || a.hex, type: a.t || 'Unknown type', level: feet >= 6000 ? `FL${Math.round(feet / 100)}` : `${fmt(feet)} ft`,
    knots: Math.round(a.gs ?? 0), feet: `${fmt(feet)} ft`, course: String(Math.round(a.track ?? 0)).padStart(3, '0'), squawk: a.squawk ?? 'none',
    x: (a.lon - home.lon) * east / KM_PER_UNIT + Math.sin(way) * speed * late, y: -(a.lat - home.lat) * 111.32 / KM_PER_UNIT - Math.cos(way) * speed * late,
    z: 30 + Math.min(feet, 45000) / 45000 * 150,
    vx: Math.sin(way) * speed, vy: -Math.cos(way) * speed, heading: Math.atan2(-Math.cos(way), Math.sin(way)), rotor: a.category === 'A7',
  };
}

/* which board: the one on show, or the first on a cable. none: the demo again */
function pickSky() {
  const u = (app.cur?.online ? app.cur : here()[0]) ?? null;
  if (u === skyUnit) {
    return;
  }
  skyUnit = u;
  if (u) {
    const { lat, lon } = u.cfg;
    reliefAround(lat, lon, KM_PER_UNIT, UNITS_PER_METRE).then((fn) => {
      if (skyUnit === u) {
        scene.setRelief(fn);
      }
    }).catch((e) => console.warn(e));
  } else {
    scene.setSky(null);
    scene.setRelief(demoGround);
  }
}

/* a whole answer of a board's planes */
function planesFrom(u) {
  if (u === skyUnit) {
    const home = { lat: u.cfg.lat, lon: u.cfg.lon };
    scene.setSky(u.sky.filter((p) => typeof p.lat === 'number' && typeof p.alt_baro === 'number').map((p) => toScene(p, home)));
  }
}

function renderBar() {
  pickSky();
  const chips = [];
  if (app.units.length > 1) {
    chips.push(`<button class="uchip" data-all aria-pressed="${app.view === 'all'}">ALL</button>`);
  }
  for (const u of app.units) {
    chips.push(`<button class="uchip${u.online ? '' : ' off'}" data-u="${u.mac}" aria-pressed="${app.view === 'unit' && app.cur === u}"><i></i>${esc(u.name.toUpperCase())}</button>`);
  }
  if ('serial' in navigator) {
    chips.push(`<button class="uchip add" data-add>${app.units.length ? '+ ADD A BOARD' : 'CONNECT A DEVICE'}</button>`);
  }
  $('barchips').innerHTML = chips.join('');
  $('barstate').innerHTML = barState();
  const cwd = app.cur ? `/boards/${short(app.cur)}` : '/boards';
  if (term && term.shell.cwd !== cwd && !term.shell.cwd.startsWith(`${cwd}/`)) {
    term.shell.cwd = cwd;
    term.refresh();
  }
}

$('bar').addEventListener('click', (e) => {
  const b = e.target.closest('button');
  if (!b) {
    return;
  }
  if (b.dataset.all !== undefined) {
    showAll();
  } else if (b.dataset.add !== undefined) {
    addBoard();
  } else if (b.dataset.u) {
    showLive(byMac(b.dataset.u));
  } else if (b.dataset.update) {
    updateThen(byMac(b.dataset.update));
  } else if (b.dataset.skip) {
    term.run(`skip --board ${short(byMac(b.dataset.skip))}`);
  }
});

/* --- the console --- */

const boardLog = [];
let following = false;
let term = null;

/* what the page does, in the console: a button's command as if typed, its answer, a board's log */
function say(text, kind = '') {
  if (kind === 'cmd') {
    term.echo(text);
  } else if (kind === 'board') {
    boardLog.push(text);
    if (boardLog.length > 1000) {
      boardLog.shift();
    }
    if (following) {
      term.write(`\x1b[90m${text}\x1b[0m\r\n`);
    }
  } else {
    term.write(`\x1b[90m${text}\x1b[0m\r\n`);
    term.note(text);
  }
}

/* a setting on one board: the screen here, and the board itself */
function setOne(u, name, value) {
  const s = u ? u.screen : hero;
  if (!s.m || !s.set(name, value)) {
    return false;
  }
  if (u?.link) {
    u.link.send(`OWLOS SET ${s.setJson()}`).catch((e) => console.warn(e));
  }
  drawPane();
  return true;
}

const FILES = {
  info: (u) => unitFacts(u).map(([k, v]) => `${k}: ${v}`),
  config: (u) => {
    const c = u.screen.cfg();
    return [`theme=${THEMES[c.theme]}`, `look=${LOOKS[c.look]}`, `range=${Math.round(c.range)}`,
      ...Object.keys(SWITCHES).map((k) => `${k}=${u.screen.opts[k] ? 'on' : 'off'}`)];
  },
  wifi: (u) => (u.nets?.length ? [...u.nets, '(passwords go in, never out)'] : ['no network set']),
  log: () => boardLog.slice(-200),
};

const boardApi = {
  units: () => app.units,
  byName: (name) => app.units.find((u) => short(u) === name),
  short: (u) => short(u),
  kind: (u) => BOARDS[u.build].name,
  current: () => app.cur,
  showAll: () => showAll(),
  showLive: (u) => showLive(u),
  files: FILES,
  settings: [...THEMES.map((v) => `theme=${v}`), ...LOOKS.map((v) => `look=${v}`), ...RANGES.map((v) => `range=${v}`), ...Object.keys(SWITCHES).flatMap((k) => [`${k}=on`, `${k}=off`])],
  set: setOne,
  async send(u, line) {
    if (!u.link) {
      throw new Error(`${short(u)} is not connected`);
    }
    await u.link.send(line);
  },
  findHome: (text) => findHome(text),
  flash: (u) => updateThen(u),
  follow(on) {
    following = on;
  },
  logLines: () => boardLog.slice(-200),
};

term = new Terminal({
  makeShell: (io) => new Shell({
    ...io,
    commands: [...CORE_COMMANDS, ...boardCommands(boardApi)],
    groups: [...BOARD_GROUPS, ...CORE_GROUPS],
    paths: (shell) => new BoardPaths(shell, boardApi),
    home: '/boards',
    host: 'radar',
    user: 'pilot',
  }),
  onToggle(open) {
    document.body.classList.toggle('termopen', open);
    if (open) {
      document.querySelector('#stage .glass')?.scrollIntoView({ block: 'start' });   // what a command does stays in sight
    }
  },
});

/* --- a board on a cable --- */

function step(n) {
  $('flow').hidden = false;
  $('steps').hidden = false;
  [...$('steps').children].forEach((li, i) => {
    li.className = i + 1 === n ? 'now' : i + 1 < n ? 'done' : '';
  });
}

function manage() {
  $('flow').hidden = false;
  $('steps').hidden = true;
}

function stage(html) {
  const s = $('stage');
  s.innerHTML = `<div class="stage">${html}</div>`;
  return s;
}

function trouble(title, text) {
  stage(`<h2>${esc(title)}</h2><p>${esc(text)}</p><div class="row"><button class="btn" id="again">TRY AGAIN</button></div>`);
  $('again').addEventListener('click', addBoard);
}

/* a Flight Radar answered: it is one of your boards from now on */
async function attach(link, port, who) {
  let u = byMac(who.mac);
  if (!u) {
    u = addUnit({ mac: who.mac, name: BOARDS[who.build].name, build: who.build });
    remember();
  }
  await u.ready;
  const c = await link.config();
  if (!c) {
    throw new Error('the board did not say what it has');
  }
  Object.assign(u, { link, port, online: true, version: who.version, wifi: c.ssid || null, nets: c.nets ?? [], cfg: c });
  u.screen.fromBoard(c);
  u.screen.onChange = (what) => {
    if (what & 1) {
      link.send(`OWLOS SET ${u.screen.setJson()}`).catch((e) => console.warn(e));
    }
    if (what & 2) {
      link.send(`OWLOS PICK ${(u.screen.m._twin_sel() >>> 0).toString(16)}`).catch((e) => console.warn(e));
    }
  };
  link.onLine = (line) => {
    if (line.startsWith('OWLOS CFG ')) {
      const now = JSON.parse(line.slice(10));
      const moved = now.lat !== u.cfg?.lat || now.lon !== u.cfg?.lon;
      u.wifi = now.ssid || null;
      u.nets = now.nets ?? [];
      u.cfg = now;
      u.screen.fromBoard(now);
      if (moved && u === skyUnit) {
        skyUnit = null;                                // a new home: the ground under it again
        pickSky();
      }
      drawPane();
    } else if (line.startsWith('OWLOS ALIGN ')) {
      u.aligning = line.endsWith('1');
      renderBar();
    } else if (line.startsWith('OWLOS PLANE ')) {
      u.screen.take(line);
      try {
        u.sky.push(JSON.parse(line.slice(12)));
      } catch (e) {
        console.warn(e);
      }
    } else if (line.startsWith('OWLOS SKY BEGIN ')) {
      u.screen.take(line);
      u.sky = [];
    } else if (u.screen.take(line)) {
      u.planes = parseInt(line.slice(14), 10) || 0;
      planesFrom(u);
      renderBar();
    }
    if (!line.startsWith('OWLOS PLANE') && !line.startsWith('OWLOS SKY')) {
      say(line, 'board');
    }
  };
  link.onLost = () => {
    u.online = false;
    u.link = null;
    u.screen.m._twin_board(0);
    say(`${u.name} is gone from its cable`, 'dim');
    if (app.view === 'all') {
      showAll();
    } else if (app.cur === u) {
      showLive(u);
    } else {
      renderBar();
    }
  };
  await link.send('OWLOS SKY ON');
  link.keep = setInterval(() => {
    link.send('OWLOS SKY ON').catch((e) => console.warn(e));
  }, 30000);
  return u;
}

/* one port, whatever is on it. an answer of ours: attached. anything else: the caller goes on */
async function tryPort(port) {
  const link = new Link(port);
  await link.open();
  const who = await link.hello();
  if (who?.mac && BOARDS[who.build]) {
    return { unit: await attach(link, port, who) };
  }
  await link.close();
  return { who };
}

async function addBoard() {
  let port;
  try {
    port = await navigator.serial.requestPort();
  } catch (e) {
    if (e.name !== 'NotFoundError') {
      trouble('No port', e.message);
    }
    return;
  }
  step(1);
  stage('<h2>Asking the board</h2><p>One moment.</p>');
  $('flow').scrollIntoView();
  say('connect', 'cmd');
  app.port = port;
  app.kind = null;
  app.ours = false;
  app.chip = null;
  try {
    const { unit, who } = await tryPort(port);
    if (unit) {
      say(`${unit.name}: Flight Radar ${unit.version}`, 'dim');
      showLive(unit);
      return;
    }
    if (who) {                                         // an older Flight Radar: it can't name itself
      app.ours = true;
      say(`Flight Radar ${who.version}`, 'dim');
      showBoards(true, who.version);
      return;
    }
    say('no Flight Radar on it, asking the chip', 'dim');
    const found = await detect(port, (t) => say(t, 'board'));
    app.chip = found.chip;
    say(`chip ${found.name}`, 'dim');
    showBoards(!found.chip);
  } catch (e) {
    console.warn(e);
    trouble('The board did not answer', `${e.message}. Is the port free, and is it the right one? Close any other page that uses it.`);
  }
}

function card(id) {
  const b = BOARDS[id];
  return `<button class="board" data-board="${id}" aria-pressed="${app.kind === id}"><img src="shots/${b.shot}" alt=""><b>${b.name}</b><span>${b.spec}</span></button>`;
}

function showBoards(all = false, oldVersion = null) {
  step(2);
  const fits = Object.keys(BOARDS).filter((id) => BOARDS[id].chip === app.chip);
  const list = all ? Object.keys(BOARDS) : fits;
  if (!all && fits.length === 1) {
    app.kind = fits[0];
  }
  const sure = !all && fits.length === 1;
  const words = oldVersion ? `It runs Flight Radar ${esc(oldVersion)}, which can't say which board it is. Pick yours: your settings stay.`
    : all ? 'All the boards we have firmware for. Pick yours.'
      : sure ? 'This chip goes on one board of ours.'
        : `It has an ${CHIP_NAME[app.chip]}. The chip does not say which screen is on it, so pick yours.`;
  const name = app.kind ? BOARDS[app.kind].name : null;
  const s = stage(`<h2>${sure ? `We found your board: ${name}` : 'Which one is yours?'}</h2><p>${words}</p>
    <div class="boards">${list.map(card).join('')}</div>
    <div class="row"><button class="btn" id="go" ${app.kind ? '' : 'disabled'}>${app.kind ? `FLASH ${newest} TO THE ${name.toUpperCase()}` : 'PICK YOUR BOARD'}</button>
    ${all ? '' : '<button class="plain" id="all">Not this one? Show all boards</button>'}</div>
    <label class="erase"><input type="checkbox" id="erase" ${app.erase ? 'checked' : ''}> Erase the board first: its WiFi, settings and saved maps go too</label>`);
  $('erase').addEventListener('change', (e) => {
    app.erase = e.target.checked;
  });
  s.querySelectorAll('.board').forEach((el) => {
    el.addEventListener('click', () => {
      app.kind = el.dataset.board;
      showBoards(all, oldVersion);
    });
  });
  s.querySelector('#all')?.addEventListener('click', () => showBoards(true, oldVersion));
  s.querySelector('#go').addEventListener('click', flashNew);
}

/* --- flashing --- */

async function flashing(port, build, name, erase, others) {
  app.busy = { name, pct: 0 };
  renderBar();
  stage(`<h2>Flashing ${esc(name)}</h2><p>Leave the cable in, it takes about a minute.${others ? ' The other boards go on running.' : ''}</p>
    <div class="flashing"><span id="what">Starting</span><b id="pct">0%</b></div><div class="bar"><i id="fill"></i></div>`);
  say(`flash ${newest}`, 'cmd');
  try {
    await writeFirmware(port, build, erase, (pct) => {
      app.busy.pct = pct;
      if ($('pct')) {
        $('what').textContent = pct < 100 ? 'Writing' : 'Restarting';
        $('pct').textContent = `${pct}%`;
        $('fill').style.width = `${pct}%`;
      }
      $('barstate').textContent = `FLASHING ${name.toUpperCase()} ${pct}%`;
    }, (t) => say(t, 'board'));
  } finally {
    app.busy = null;
  }
}

/* the board restarts after flashing, and a board with its own USB comes back as a new port */
async function afterFlash(port) {
  await wait(1500);
  for (let t = 0; t < 4; t++) {
    try {
      const { unit } = await tryPort(port);
      if (unit) {
        return unit;
      }
    } catch (e) {
      console.warn(e);
    }
    await wait(1500);
  }
  return null;
}

async function flashNew() {
  step(3);
  const build = app.kind;
  const port = app.port;
  try {
    await flashing(port, build, BOARDS[build].name, app.erase, here().length);
  } catch (e) {
    console.warn(e);
    trouble('Flashing stopped', `${e.message}. Nothing was left half way that a new try can't fix.`);
    return;
  }
  const u = await afterFlash(port);
  if (u) {
    showLive(u, true);
  } else {
    stage('<h2>Flashed</h2><p>The board restarted and came back on another port, as a board with its own USB does. Connect it once more.</p><div class="row"><button class="btn" id="again">CONNECT</button></div>');
    $('again').addEventListener('click', addBoard);
  }
}

async function update(u) {
  manage();
  const port = u.port;
  const link = u.link;
  u.link = null;
  u.online = false;
  await link.close();
  await flashing(port, u.build, u.name, false, here().length);
  return (await afterFlash(port)) ?? u;
}

async function updateThen(u) {
  try {
    showLive(await update(u));
  } catch (e) {
    console.warn(e);
    trouble('Flashing stopped', e.message);
  }
}

async function updateAll() {
  for (const u of here().filter((v) => older(v.version, newest))) {
    await update(u);
  }
  showAll();
}

/* --- all your boards at once --- */

/* a board's screen one to one: its pixel is one pixel of the monitor, the picture exactly as the board
 * has it. + and - beside it make that a whole square of two, three, as the person likes. counted in the
 * monitor's pixels, not the page's, or a Windows scale of 125% stretches and blurs it. only a screen
 * too big for its place is shrunk, and smoothed */
function fit(el, screen) {
  const photo = el.classList.contains('yellow');
  const dpr = devicePixelRatio || 1;
  const wide = $('stage').clientWidth - 70;            // a big one takes the whole width, the settings go under it
  const aroundX = photo ? (PHOTO.left + PHOTO.right) / PHOTO.w : 0;
  const aroundY = photo ? (PHOTO.top + PHOTO.bottom) / PHOTO.w * (screen.w / screen.h) : 0;
  const room = wide * dpr / (1 + aroundX);
  const high = Math.max(240, innerHeight * 0.7) * dpr / (1 + aroundY);
  const most = Math.min(room / screen.w, high / screen.h);
  const base = Math.max(1, Math.round(dpr));
  screen.zoomMost = Math.max(1, Math.floor(most / base));
  const k = Math.min(most, base * (app.view === 'all' ? 1 : Math.min(screen.zoom, screen.zoomMost)));
  const w = Math.round(screen.w * k);
  const h = Math.round(screen.h * k);
  screen.k = k;
  screen.canvas.width = w;
  screen.canvas.height = h;
  screen.canvas.style.width = `${w / dpr}px`;
  screen.canvas.style.height = `${h / dpr}px`;
  screen.canvas.style.imageRendering = k >= 1 ? 'pixelated' : 'auto';
  if (photo) {
    const s = w / dpr / PHOTO.w;
    el.style.borderWidth = `${PHOTO.top * s}px ${PHOTO.right * s}px ${PHOTO.bottom * s}px ${PHOTO.left * s}px`;
  }
  el.closest('.live')?.classList.toggle('stack', w / dpr * (1 + aroundX) > wide - 390);
  screen.show();
}

/* the photo of a CYD round the screen: where the glass lies in img/board-35.png, its index.html border-image */
const PHOTO = { top: 34, right: 78, bottom: 24, left: 100, w: 522 };

let mounted = [];

function mount(el, screen, build) {
  el.classList.add('glass');
  el.classList.toggle('round', !!BOARDS[build]?.round);
  el.classList.toggle('yellow', !!BOARDS[build]?.yellow);
  el.replaceChildren(screen.canvas);
  mounted = mounted.filter(([e]) => e.isConnected);
  mounted.push([el, screen]);
  fit(el, screen);
}

function zoomWords() {
  const s = screenNow();
  if ($('zoomword')) {
    const z = Math.min(s.zoom, s.zoomMost);
    $('zoomword').textContent = `${z}x`;
    $('zoomout').disabled = z <= 1;
    $('zoomin').disabled = z >= s.zoomMost;
  }
}

function zoomBy(step) {
  const s = screenNow();
  s.zoom = Math.max(1, Math.min(s.zoomMost, Math.min(s.zoom, s.zoomMost) + step));
  fit($('glass'), s);
  zoomWords();
  remember();
}

addEventListener('resize', () => {
  for (const [el, screen] of mounted) {
    if (el.isConnected) {
      fit(el, screen);
    }
  }
  zoomWords();
});

function tile(u) {
  const b = BOARDS[u.build];
  const body = u.online ? `<div class="slot"><div data-glass="${u.mac}"></div></div>`
    : '<div class="slot"><p class="gone">Not connected. Plug it in and it comes back by itself.</p></div>';
  const line = u.online ? `${esc(b.name)}, ${u.version}` : esc(b.name);
  const action = u.online ? `<button class="btn quiet" data-open="${u.mac}">OPEN</button><button class="btn quiet" data-one="${u.mac}">${older(u.version, newest) ? 'UPDATE' : 'FLASH AGAIN'}</button>`
    : `<button class="btn quiet" data-open="${u.mac}">DETAILS</button>`;
  return `<article class="tile${u.online ? '' : ' off'}"><h3>${esc(u.name)}</h3><span>${line}</span>${body}<div class="row">${action}</div></article>`;
}

function showAll() {
  app.view = 'all';
  app.cur = null;
  manage();
  const old = here().filter((u) => older(u.version, newest)).length;
  const s = stage(`<h2>Your boards</h2><p>Every connected board, live. Touch a screen here and it moves there. Next time they are here without asking.</p>
    <div class="tiles">${app.units.map(tile).join('')}</div>
    <div class="pane common"><div class="field"><span>Look, on every board</span><div class="chips">${LOOKS.map((l) => `<button class="chip" data-every="look=${l}">${l.toUpperCase()}</button>`).join('')}</div></div>
    <div class="field"><span>Colours, on every board</span><div class="chips">${THEMES.map((t) => `<button class="chip" data-every="theme=${t}">${t.toUpperCase()}</button>`).join('')}</div></div>
    ${old ? `<button class="btn quiet" id="updateall">UPDATE ${old === 1 ? 'THE OLD ONE' : `ALL ${old}`} TO ${newest}</button>` : ''}</div>`);
  for (const u of here()) {
    mount(s.querySelector(`[data-glass="${u.mac}"]`), u.screen, u.build);
  }
  s.querySelectorAll('[data-open]').forEach((el) => {
    el.addEventListener('click', () => showLive(byMac(el.dataset.open)));
  });
  s.querySelectorAll('[data-one]').forEach((el) => {
    el.addEventListener('click', () => updateThen(byMac(el.dataset.one)));
  });
  s.querySelectorAll('[data-every]').forEach((el) => {
    el.addEventListener('click', () => {
      term.run(`set ${el.dataset.every} --all`);
    });
  });
  s.querySelector('#updateall')?.addEventListener('click', updateAll);
  renderBar();
}

/* --- one board --- */

function facts() {
  return app.cur ? unitFacts(app.cur) : [['Board', 'none, a demo sky in this page']];
}

function unitFacts(u) {
  const b = BOARDS[u.build];
  return [['Board', b.name], ['Chip', CHIP_NAME[b.chip]], ['Screen', `${b.w} x ${b.h}, ${b.inch}"`], ['Touch', b.touch], ['Firmware', u.version || 'not known yet'], ['Cable', u.online ? usbName(u.port) : 'not connected']];
}

/* what a board just flashed may ask of you, on its own screen */
function firstWords(u) {
  const list = [];
  if (u.build === 'ili9341') {
    list.push('Colours wrong, or the picture upside down or mirrored? This board is sold with three screens: press BOOT right after a restart, each press tries the next one.');
  }
  return list.length ? `<p class="note wait">${list.join(' ')}</p>` : '';
}

function liveNote() {
  const u = app.cur;
  if (!u) {
    return '<p class="note wait"><b>A demo sky.</b> Tap a plane, drag the rim to turn the map, drag inside to zoom.</p>';
  }
  if (!u.wifi) {
    return '<p class="note wait"><b>No WiFi yet.</b> This is the board\'s screen: touch it here, it moves there.</p>';
  }
  return `<p class="note"><b>On "${esc(u.wifi)}".</b> This is the board's screen: touch it here, it moves there.</p>`;
}

async function findHome(text) {
  const url = `https://nominatim.openstreetmap.org/search?format=jsonv2&limit=1&accept-language=en&q=${encodeURIComponent(text)}`;
  const list = await (await fetch(url)).json();
  return list[0] ? { lat: Number(list[0].lat), lon: Number(list[0].lon) } : null;
}

function drawPane() {
  const pane = $('pane');
  if (!pane) {
    return;
  }
  const u = app.cur;
  const s = screenNow();
  document.querySelectorAll('.tabs button').forEach((t) => t.setAttribute('aria-selected', String(t.dataset.tab === app.tab)));
  if (app.tab === 'setup') {
    if (!u) {
      pane.innerHTML = '<small>The WiFi and your home go to a board. Connect one to set them.</small><button class="btn quiet" id="save">CONNECT A DEVICE</button>';
      $('save').addEventListener('click', addBoard);
      return;
    }
    if (document.activeElement?.closest?.('#pane')) {
      return;                                          // a board's answer must not wipe what is being typed
    }
    const nets = (u.nets ?? []).map((n) => `<div class="net"><span>${esc(n)}</span><button class="plain" data-forget="${esc(n)}">Forget</button></div>`).join('');
    pane.innerHTML = `<div><label for="ssid">WiFi network</label><input type="text" id="ssid" autocomplete="off"></div>
      <div><label for="pass">Password</label><input type="password" id="pass" autocomplete="off"></div>
      ${nets ? `<div class="nets"><span>Kept on the board</span>${nets}</div>` : ''}
      <div><label for="home">Your town</label><input type="text" id="home" autocomplete="off" placeholder="a town, a postcode or a point"></div>
      <button class="btn quiet" id="save">SEND TO THE BOARD</button>
      <small id="saved">They go to the board over the cable, never to a server of ours. A town is looked up on OpenStreetMap.</small>`;
    pane.querySelectorAll('[data-forget]').forEach((el) => {
      el.addEventListener('click', () => {
        say(`wifi forget "${el.dataset.forget}"`, 'cmd');
        u.link?.send(`OWLOS SET ${JSON.stringify({ forget: el.dataset.forget })}`).then(() => u.link.send('OWLOS GET'));
      });
    });
    $('save').addEventListener('click', async () => {
      const set = {};
      const ssid = $('ssid').value.trim();
      const town = $('home').value.trim();
      if (ssid) {
        set.ssid = ssid;
        set.pass = $('pass').value;
        say(`wifi join "${ssid}"`, 'cmd');
      }
      if (town) {
        const found = await findHome(town);
        if (!found) {
          $('saved').textContent = `OpenStreetMap does not know "${town}".`;
          return;
        }
        Object.assign(set, found);
        say(`home "${town}"`, 'cmd');
      }
      if (Object.keys(set).length && u.link) {
        await u.link.send(`OWLOS SET ${JSON.stringify(set)}`);
        await u.link.send('OWLOS GET');
        $('saved').textContent = 'Sent. The console below says how it goes.';
      }
    });
  } else if (app.tab === 'view') {
    const c = s.cfg();
    const range = RANGES.reduce((best, r, i) => (Math.abs(r - c.range) < Math.abs(RANGES[best] - c.range) ? i : best), 0);
    const round = u && BOARDS[u.build].round;
    pane.innerHTML = `${round ? '' : `<div class="field"><span>Look</span><div class="chips">${LOOKS.map((l, i) => `<button class="chip" data-set="look=${l}" aria-pressed="${c.look === i}">${l.toUpperCase()}</button>`).join('')}</div></div>`}
      <div class="field"><span>Colours</span><div class="chips">${THEMES.map((t, i) => `<button class="chip" data-set="theme=${t}" aria-pressed="${c.theme === i}">${t.toUpperCase()}</button>`).join('')}</div></div>
      <div><label for="range">Range: <b id="rangeword">${Math.round(c.range)}</b> km</label><input type="range" id="range" min="0" max="${RANGES.length - 1}" value="${range}"></div>
      <div class="field"><span>On the radar</span><div class="chips">${Object.entries(SWITCHES).map(([k, v]) => `<button class="chip" data-set="${k}=${s.opts[k] ? 'off' : 'on'}" aria-pressed="${!!s.opts[k]}">${v.toUpperCase()}</button>`).join('')}</div></div>`;
    pane.querySelectorAll('[data-set]').forEach((el) => {
      el.addEventListener('click', () => {
        if (u) {
          term.run(`set ${el.dataset.set}`);
        } else {
          const [name, value] = el.dataset.set.split('=');
          setOne(null, name, value);
        }
      });
    });
    $('range').addEventListener('input', (e) => {
      $('rangeword').textContent = RANGES[e.target.value];
    });
    $('range').addEventListener('change', (e) => {
      const value = RANGES[e.target.value];
      if (u) {
        term.run(`set range=${value}`);
      } else {
        setOne(null, 'range', String(value));
      }
    });
  } else {
    pane.innerHTML = `${u ? `<div><label for="rename">Name</label><input type="text" id="rename" autocomplete="off" value="${esc(u.name)}"></div>` : ''}
      <dl class="facts">${facts().map(([k, v]) => `<dt>${k}</dt><dd>${esc(v)}</dd>`).join('')}</dl>
      <div class="row">${u ? '<button class="btn quiet" id="again">FLASH AGAIN</button><button class="plain" id="forget">Forget this board</button>' : '<button class="btn quiet" id="again">CONNECT A DEVICE</button>'}</div>`;
    $('again').addEventListener('click', () => {
      if (u) {
        updateThen(u);
      } else {
        addBoard();
      }
    });
    $('rename')?.addEventListener('change', (e) => {
      u.name = e.target.value.trim() || u.name;
      remember();
      renderBar();
    });
    $('forget')?.addEventListener('click', async () => {
      if (u.link) {
        await u.link.close();
      }
      app.units = app.units.filter((v) => v !== u);
      remember();
      say(`board forget "${u.name}"`, 'cmd');
      if (app.units.length) {
        showAll();
      } else {
        app.cur = null;
        app.view = null;
        $('flow').hidden = true;
        renderBar();
      }
    });
  }
}

async function showLive(u, justFlashed = false) {
  if (u) {
    await u.ready;
  }
  app.cur = u ?? null;
  app.view = u ? 'unit' : 'demo';
  if (justFlashed) {
    step(4);
  } else {
    manage();
  }
  if (u && !u.online) {
    stage(`<h2>${esc(u.name)} is not connected</h2><p>Plug it in and it comes back by itself, no questions asked.</p>
      <dl class="facts">${facts().map(([k, v]) => `<dt>${k}</dt><dd>${esc(v)}</dd>`).join('')}</dl>`);
    renderBar();
    return;
  }
  app.tab = u && !u.wifi ? 'setup' : 'view';
  const title = u ? (u.name === BOARDS[u.build].name ? `Your ${esc(u.name)} is running` : `${esc(u.name)}: ${esc(BOARDS[u.build].name)}`) : 'The radar, running in this page';
  const s = stage(`<h2>${title}</h2><p>${u && BOARDS[u.build].round ? 'The same round screen as on the board: tap it for its menu.' : 'The same code as on the board.'}</p>
    <div class="live"><div><div id="glass"></div>
    <div class="zoom"><button id="zoomout" aria-label="smaller">-</button><span id="zoomword"></span><button id="zoomin" aria-label="bigger">+</button>
    ${u ? '<button class="again" id="reflash">FLASH AGAIN</button>' : ''}</div>
    <div id="livenote">${liveNote()}</div>${justFlashed && u ? firstWords(u) : ''}</div>
    <div><div class="tabs" role="tablist"><button role="tab" data-tab="setup">SETUP</button><button role="tab" data-tab="view">VIEW</button><button role="tab" data-tab="board">BOARD</button></div>
    <div class="pane" id="pane"></div></div></div>`);
  mount($('glass'), screenNow(), u?.build);
  $('zoomout').addEventListener('click', () => zoomBy(-1));
  $('zoomin').addEventListener('click', () => zoomBy(1));
  $('reflash')?.addEventListener('click', () => updateThen(u));
  zoomWords();
  s.querySelectorAll('.tabs button').forEach((t) => {
    t.addEventListener('click', () => {
      app.tab = t.dataset.tab;
      drawPane();
    });
  });
  drawPane();
  renderBar();
  $('flow').scrollIntoView();
}

/* --- what a click on the picture opens --- */

function describe(id) {
  const { kind, thing, count } = scene.about(id);
  if (kind === 'plane' && !thing) {
    return { title: 'Gone', what: 'It has left the sky here', facts: [], text: 'Planes come and go: this one flew out of range.' };
  }
  if (kind === 'plane' || kind === 'heli') {
    return {
      title: thing.name, what: thing.live ? `${thing.type}, in the air right now` : `${thing.type}, in the demo sky`,
      facts: [['Height', thing.feet], ['Speed', `${thing.knots} kt`], ['Course', thing.course], ['Squawk', thing.squawk]],
      text: kind === 'heli'
        ? 'A helicopter with ADS-B sends the same signal as a plane.'
        : 'It sends this by itself, about twice a second, on 1090 MHz. Nobody asks it, and anyone with an antenna can hear it.',
    };
  }
  if (kind === 'receiver') {
    return {
      title: 'A volunteer\'s antenna', what: 'On a roof, somewhere near you',
      facts: [['Heard here', `${fmt(thing.heard)} messages`], ['Reach', 'line of sight, often hundreds of km'], ['Made of', 'an antenna, a USB receiver, a small computer']],
      text: 'It passes on what it hears, and the network puts it together with all the other antennas. You can build one too.',
    };
  }
  if (id === 'server') {
    return {
      title: 'The network', what: 'adsb.lol, adsb.fi, or Flightradar24 with a key',
      facts: [['In', `${fmt(count.into)} from the antennas`], ['Out', `${fmt(count.out)} answers`]],
      text: 'IN: what every antenna heard. The network makes one picture of the sky out of it. OUT: only what someone asked for, the planes around one point.',
    };
  }
  if (id === 'router') {
    return {
      title: 'Your router', what: 'The one you already have',
      facts: [['Asks', 'every few seconds'], ['For', 'the planes around your home']],
      text: 'Nothing to set up on it. The board joins your WiFi the way a phone does.',
    };
  }
  if (id === 'home') {
    return {
      title: 'Your home', what: 'The middle of your radar',
      facts: [['Set on', 'the touch screen, or from this page']],
      text: 'Tell the board where you live, by a town, a postcode or a point, and it shows the planes around it.',
    };
  }
  if (id === 'board') {
    return {
      title: 'Your board', what: 'A small ESP32 with a touch screen',
      facts: [['Boards', `${Object.keys(BOARDS).length} to pick from`], ['Screens', '1.28" round to 5"'], ['Needs', 'a USB cable, once']],
      text: 'It draws the planes itself, on its own screen. Your computer can go to sleep.',
      action: 'serial' in navigator,
    };
  }
  return {
    title: 'Another board', what: 'At a neighbour\'s, or in your kitchen',
    facts: [],
    text: 'Every board asks for itself, so you can have as many as you like.',
  };
}

function infoBody(i) {
  return `<h3>${esc(i.title)}</h3><p class="what">${esc(i.what)}</p>
    ${i.facts.length ? `<dl class="facts" id="infofacts">${i.facts.map(([k, v]) => `<dt>${k}</dt><dd>${esc(v)}</dd>`).join('')}</dl>` : ''}
    <p>${esc(i.text)}</p>`;
}

let infoId = null;

function closeInfo() {
  infoId = null;
  $('info').hidden = true;
  scene.clear();
}

function showInfo(id, at) {
  $('hint').hidden = true;
  if (!id) {
    closeInfo();
    return;
  }
  infoId = id;
  const box = $('info');
  const i = describe(id);
  box.innerHTML = `<button class="x" aria-label="close">x</button>${infoBody(i)}${i.action ? '<button class="btn" id="infogo">CONNECT A DEVICE</button>' : ''}`;
  box.hidden = false;
  box.querySelector('.x').addEventListener('click', closeInfo);
  box.querySelector('#infogo')?.addEventListener('click', () => {
    closeInfo();
    addBoard();
  });
  const wrap = box.parentElement;
  const left = at.x + 330 > wrap.clientWidth ? at.x - 326 : at.x + 16;
  box.style.left = `${Math.max(8, left)}px`;
  box.style.top = `${Math.max(8, Math.min(at.y - 30, wrap.clientHeight - box.offsetHeight - 8))}px`;
}

setInterval(() => {
  const list = $('infofacts');
  if (infoId && list) {
    list.innerHTML = describe(infoId).facts.map(([k, v]) => `<dt>${k}</dt><dd>${esc(v)}</dd>`).join('');
  }
}, 500);

addEventListener('keydown', (e) => {
  if (e.key === 'Escape' && infoId) {
    closeInfo();
  }
});

/* --- start --- */

function tickAll(now) {
  requestAnimationFrame(tickAll);
  if (hero.m && (scene.isShown() || hero.canvas.isConnected)) {
    hero.frame(now);
  }
  for (const u of app.units) {
    if (u.screen.m && (u.screen.canvas.isConnected || (u === skyUnit && scene.isShown()))) {
      u.screen.frame(now);
    }
  }
}

/* the boards this browser knows come back as they were, and those on a cable it may use connect by
 * themselves, with no window: the browser remembers the ports it was allowed */
async function comeBack() {
  let kept = [];
  try {
    kept = JSON.parse(localStorage.getItem(KEPT) ?? '[]');
  } catch (e) {
    console.warn(e);
  }
  for (const k of kept) {
    if (BOARDS[k.build]) {
      addUnit(k);
    }
  }
  renderBar();
  if (!('serial' in navigator)) {
    return;
  }
  const ports = await navigator.serial.getPorts();
  for (const port of ports) {
    try {
      await tryPort(port);
    } catch (e) {
      console.warn('a remembered port', e);
    }
  }
  navigator.serial.addEventListener('connect', async (e) => {
    await wait(800);
    try {
      const { unit } = await tryPort(e.target);
      if (unit && app.view === 'all') {
        showAll();
      } else if (unit && app.cur === unit) {
        showLive(unit);
      } else {
        renderBar();
      }
    } catch (err) {
      console.warn(err);
    }
  });
  if (here().length) {
    if (app.units.length > 1) {
      showAll();
    } else {
      showLive(app.units[0]);
    }
  }
}

$('try').addEventListener('click', () => {
  showLive(null);
});
if ('serial' in navigator) {
  $('connect').addEventListener('click', addBoard);
} else {
  $('connect').hidden = true;
  $('cablenote').textContent = 'Connecting a board needs Chrome or Edge on a computer. Here you can look around.';
}

scene = startScene($('scene'), () => (skyUnit?.screen.drawn ? skyUnit.screen.raw : hero.drawn ? hero.raw : null), showInfo);
loadRelief().then((fn) => {
  demoGround = fn;
  if (!skyUnit) {
    scene.setRelief(fn);
  }
}).catch((e) => console.warn(e));
fetch('version.json').then((r) => r.json()).then((v) => {
  newest = v.version;
  renderBar();
}).catch((e) => console.warn(e));
hero.start().then(() => {
  requestAnimationFrame(tickAll);
  return comeBack();
}).catch((e) => {
  console.warn('the radar did not start', e);
});
