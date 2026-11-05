// Point d'entrée : rendu Three.js, caméras, boucle de jeu, écrans.
import * as THREE from '../lib/three.module.min.js';
import { CIRCUITS } from './data.js';
import { buildTrack } from './track.js';
import { buildTrackMeshes, buildEnvironment } from './scenery.js';
import { setAniso } from './textures.js';
import { createRace, updateRace, results } from './race.js';
import { initInput, enableTilt, resetInput } from './input.js';
import { initHud, updateHud, fmt } from './hud.js';
import { garage, openGarage, tickDevelopment, resetGarage } from './garage.js';
import { openSetup, growMate, career } from './career.js';
import { team, racePrize, repFactor, consumeBoost } from './team.js';
import { buildQuestions, runPress, teamLine } from './press.js';
import { TEAMS } from './data.js';
import { carUpgrades } from './garage.js';
import * as SZ from './season.js';

const $ = id => document.getElementById(id);
const params = new URLSearchParams(location.search);
const settings = Object.assign({ cam: 'chase', tilt: false, laps: 3, circuit: 'valmont', mode: 'free' }, load());
if (params.get('circuit')) settings.circuit = params.get('circuit');
function load() { try { return JSON.parse(localStorage.getItem('gpm-settings')) || {}; } catch { return {}; } }
function save() { try { localStorage.setItem('gpm-settings', JSON.stringify(settings)); } catch {} }

// ---------- rendu ----------
const canvas = $('game');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: devicePixelRatio < 2, powerPreference: 'high-performance' });
let pixelRatio = Math.min(devicePixelRatio, 1.5);
renderer.setPixelRatio(pixelRatio);
renderer.toneMapping = THREE.ACESFilmicToneMapping; renderer.toneMappingExposure = 1.05;
setAniso(renderer.capabilities.getMaxAnisotropy());
const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(65, 1, 0.15, 3000);

function resize() {
  const w = innerWidth, h = innerHeight;
  renderer.setSize(w, h, false); camera.aspect = w / h;
  setFov(); camera.updateProjectionMatrix();
}
function setFov() { const portrait = camera.aspect < 1; camera.fov = settings.cam === 'cockpit' ? (portrait ? 95 : 72) : (portrait ? 88 : 62); camera.updateProjectionMatrix(); }
addEventListener('resize', resize); resize();

// ---------- circuit ----------
let def, track, sky, lights, world = null, R = null;
function disposeAll(obj) {
  obj.traverse(o => {
    if (!o.geometry) return;
    o.geometry.dispose();
    for (const m of [].concat(o.material)) { if (m.map) m.map.dispose(); m.dispose(); }
  });
}
function loadCircuit(id) {
  if (R) { for (const c of R.cars) scene.remove(c.mesh); R = null; }
  if (world) { scene.remove(world); disposeAll(world); scene.environment?.dispose(); }
  def = CIRCUITS.find(c => c.id === id) || CIRCUITS[0];
  track = buildTrack(def);
  world = new THREE.Group(); scene.add(world);
  ({ sky } = buildEnvironment(scene, renderer, track, world));
  ({ lights } = buildTrackMeshes(track, world));
  initHud(track);
  // vue d'attente du menu : survol de la ligne de départ
  const i = 40 % track.N, h = track.H[i];
  camera.position.set(track.X[i] - Math.sin(h) * 70 + Math.cos(h) * 30, track.Y[i] + 25, track.Z[i] - Math.cos(h) * 70 - Math.sin(h) * 30);
  camera.lookAt(track.X[i], track.Y[i] + 2, track.Z[i]);
}

let paused = true, last = performance.now();
const camState = { h: 0, x: 0, y: 0, z: 0 };

let raceOpts = {};
function startRace(opts = raceOpts) {
  raceOpts = opts;
  if (R) for (const c of R.cars) scene.remove(c.mesh);
  R = createRace(track, scene, +params.get('tours') || settings.laps, { autopilot: params.has('demo'), grid: +params.get('grille') || 10, ...opts });
  R.season = !!opts.season;
  resetInput();
  const p = R.player; camState.h = p.heading; camState.x = p.x; camState.y = p.y; camState.z = p.z;
  show('hud'); paused = false; last = performance.now();
}

// ---------- caméras ----------
const tmp = new THREE.Vector3();
function updateCamera(dt) {
  const p = R.player;
  if (settings.cam === 'cockpit') {
    p.mesh.updateMatrixWorld();
    camera.position.copy(tmp.set(0, 1.3, -0.2).applyMatrix4(p.mesh.matrixWorld));
    camera.lookAt(tmp.set(0, 0.2, 10).applyMatrix4(p.mesh.matrixWorld));
    return;
  }
  const k = 1 - Math.exp(-dt * 7);
  let d = p.heading - camState.h; d = Math.atan2(Math.sin(d), Math.cos(d));
  camState.h += d * k;
  camState.x += (p.x - camState.x) * Math.min(1, dt * 25); camState.z += (p.z - camState.z) * Math.min(1, dt * 25);
  camState.y += (p.y - camState.y) * Math.min(1, dt * 6);
  const fx = Math.sin(camState.h), fz = Math.cos(camState.h);
  const back = 6.8 + Math.max(0, p.v) * 0.012;
  camera.position.set(camState.x - fx * back, camState.y + 2.3, camState.z - fz * back);
  camera.lookAt(camState.x + fx * 6, camState.y + 0.9, camState.z + fz * 6);
}
function toggleCam() {
  settings.cam = settings.cam === 'cockpit' ? 'chase' : 'cockpit'; save(); setFov();
  $('camBtn').textContent = settings.cam === 'cockpit' ? '🎥 1re' : '🎥 3e';
}

// ---------- boucle ----------
let fpsAcc = 0, fpsN = 0, fps = 0, slowT = 0;
const timeScale = +params.get('vitesse') || 1;
function loop(now) {
  requestAnimationFrame(loop);
  let dt = Math.min(0.05, (now - last) / 1000); last = now;
  if (!R) { sky.position.copy(camera.position); renderer.render(scene, camera); return; }
  if (!paused) {
    let t = dt * timeScale;
    while (t > 0) { const s = Math.min(t, 1 / 60); updateRace(R, s); t -= s; }
    lights.forEach((l, k) => l.material.color.setHex(R.phase === 'lights' && k < R.lightsOn ? 0xff1a1a : 0x330000));
    if (R.done && R.t - R.endT > (R.solo ? 1.2 : 2.5)) R.solo ? showQuali() : showResults();
    if (!R) return;
  }
  updateCamera(dt);
  sky.position.copy(camera.position);
  renderer.render(scene, camera);
  // fps + qualité adaptative
  fpsAcc += dt; fpsN++;
  if (fpsAcc > 1) {
    fps = Math.round(fpsN / fpsAcc); fpsAcc = 0; fpsN = 0;
    if (!paused && fps < 45 && pixelRatio > 0.75) { if (++slowT >= 2) { pixelRatio = Math.max(0.75, pixelRatio - 0.25); renderer.setPixelRatio(pixelRatio); resize(); slowT = 0; } }
    else slowT = 0;
  }
  if (!paused) updateHud(R, params.has('fps') ? fps + ' i/s · ' + renderer.info.render.calls + ' appels · ' + Math.round(renderer.info.render.triangles / 1000) + 'k tri' : 0);
}

// ---------- écrans ----------
function show(id) {
  for (const s of ['menu', 'hud', 'pause', 'results', 'garage', 'press', 'season', 'standings', 'quali', 'setup']) $(s).classList.toggle('hidden', s !== id);
  document.body.classList.toggle('racing', id === 'hud');
}
function showResults() {
  paused = true;
  const rows = results(R);
  const win = rows[0].time;
  $('resultsTable').innerHTML = rows.map(r =>
    `<tr class="${r.player ? 'me' : ''}"><td>${r.pos}</td><td><i style="background:#${r.color.toString(16).padStart(6, '0')}"></i>${r.name}<small>${r.team}</small></td>` +
    `<td>${r.pos === 1 ? fmt(r.time) : '+' + (r.time - win).toFixed(3) + (r.finished ? '' : '*')}</td><td>${fmt(r.best)}</td></tr>`).join('');
  const me = rows.find(r => r.player);
  if (!R.awarded) {
    const fastest = me.best && rows.every(r => !r.best || r.best >= me.best);
    R.mult = repFactor();
    R.prize = racePrize(me.pos, R.laps, R.mult);
    R.parts = tickDevelopment();
    consumeBoost();
    // contexte pour la presse
    const mate = rows.find(r => !r.player && r.team === me.team);
    R.pressCtx = { pos: me.pos, grid: me.grid, gained: me.grid - me.pos, circuit: def.name, winner: rows[0].name,
      rival: me.pos > 1 && Math.random() < 0.6 ? rows[me.pos - 2].name : null, mate: mate?.name, mateAhead: mate && mate.pos < me.pos, fastest };
    R.pressDone = false;
    if (R.season) { SZ.recordRace(rows); R.grow = growMate(); }
  }
  $('againBtn').textContent = R.season ? 'Saison →' : 'Rejouer';
  $('rdGain').innerHTML = `Prime : <b>+${R.prize.toFixed(1)} M€</b> <small>(réputation ×${R.mult.toFixed(2)} · budget ${team.budget.toFixed(1)} M€)</small>` +
    (R.parts.length ? `<br>🔧 Pièce${R.parts.length > 1 ? 's' : ''} livrée${R.parts.length > 1 ? 's' : ''} : <b>${R.parts.join(', ')}</b>` : '') +
    (R.grow ? `<br>📈 ${career.mate.name} progresse : vitesse ${Math.round(career.mate.spd)} (+${R.grow})` : '');
  $('pressBtn').classList.toggle('hidden', R.pressDone);
  $('resultTitle').textContent = me.pos === 1 ? 'Victoire !' : me.pos <= 3 ? `Podium : P${me.pos}` : `Arrivée : P${me.pos}`;
  show('results');
}

function refreshMenu() {
  document.querySelectorAll('[data-laps]').forEach(b => b.classList.toggle('sel', +b.dataset.laps === settings.laps));
  $('tiltBtn').textContent = settings.tilt ? 'Inclinaison' : 'Boutons';
  $('camBtn').textContent = settings.cam === 'cockpit' ? '🎥 1re' : '🎥 3e';
  $('camSel').textContent = settings.cam === 'cockpit' ? 'Cockpit (1re pers.)' : 'Arrière (3e pers.)';
  $('menuPts').textContent = team.budget.toFixed(1) + ' M€';
  document.querySelectorAll('[data-mode]').forEach(b => b.classList.toggle('sel', b.dataset.mode === settings.mode));
  const S = SZ.season;
  $('startBtn').textContent = settings.mode === 'season' ? (S ? (S.done ? 'Saison terminée →' : `Saison ${S.year} · manche ${S.round + 1}/10 →`) : 'Commencer une saison') : 'Départ';
  document.querySelector('.circ').classList.toggle('dim', settings.mode === 'season');
  $('teamState').innerHTML = teamLine() + (team.boostTxt ? `<span class="boost">Prochaine course : ${team.boostTxt}</span>` : '');
}
$('pressBtn').onclick = () => {
  if (!R || R.pressDone) return;
  show('press');
  runPress($('press'), buildQuestions(R.pressCtx), () => { R.pressDone = true; $('pressBtn').classList.add('hidden'); show('results'); });
};
// ---------- saison ----------
function setupSeason() {
  show('setup');
  openSetup($('setup'), () => { resetGarage(); SZ.newSeason(SZ.season ? SZ.season.year + 1 : 2027); openSeason(); }, () => { refreshMenu(); show('menu'); });
}
function openSeason() {
  const S = SZ.season;
  if (!S) return show('menu');
  if (R) { for (const c of R.cars) scene.remove(c.mesh); R = null; }
  paused = true;
  $('szTitle').textContent = `Championnat ${S.year}` + (S.done ? ' · terminé' : ` · manche ${S.round + 1}/10`);
  $('szCal').innerHTML = SZ.calendarHTML();
  $('szState').innerHTML = teamLine();
  const me = SZ.driverStandings().findIndex(d => d.player) + 1, myPts = S.pts['Toi'] || 0;
  if (S.done) {
    const ch = SZ.driverStandings()[0], tc = SZ.teamStandings()[0];
    $('szNext').innerHTML = `<h3>${ch.player ? '🏆 Tu es champion du monde !' : `Champion : ${ch.name}`}</h3><p>Tu termines P${me} avec ${myPts} pts. Écuries : ${tc.name} championne.</p>`;
    $('szActions').innerHTML = `<button class="main" id="szNew">Nouvelle saison</button>`;
    $('szNew').onclick = () => setupSeason();
  } else {
    const c = SZ.roundCircuit();
    if (def.id !== c.id) loadCircuit(c.id);
    $('szNext').innerHTML = `<h3>${c.name}</h3><p>${c.type} · ${(track.length / 1000).toFixed(2)} km · ${settings.laps} tour${settings.laps > 1 ? 's' : ''}</p><p>Toi : P${me} au championnat, ${myPts} pts</p>`;
    $('szActions').innerHTML = S.grid
      ? `<button class="main" id="szRace">Départ de la course (P${S.grid.indexOf('Toi') + 1} sur la grille)</button>`
      : `<button class="main" id="szQuali">Qualifications : tour chrono</button><button id="szSim">Simuler les qualifications</button>`;
    const q = $('szQuali'); if (q) q.onclick = async () => { await enableTilt(settings.tilt); startRace({ solo: true }); };
    const sm = $('szSim'); if (sm) sm.onclick = () => {
      const up = carUpgrades(), perf = TEAMS[0].perf * up.vtop;
      showQuali(SZ.lapEstimate(track, 0.95, perf, up.acc) * (1 + 0.006 * Math.random()) * 0.985);
    };
    const rc = $('szRace'); if (rc) rc.onclick = async () => { await enableTilt(settings.tilt); startRace({ season: true, gridOrder: S.grid, perfBonus: S.dev }); };
  }
  show('season');
}
function showQuali(simTime) {
  paused = true;
  const time = simTime ?? (R && R.player.finished ? R.player.lastLap : null);
  if (R) { for (const c of R.cars) scene.remove(c.mesh); R = null; }
  const rows = SZ.qualifying(track, time);
  SZ.season.quali = rows.map(r => ({ name: r.name, time: r.time }));
  SZ.season.grid = rows.map(r => r.name); SZ.saveSeason();
  const pos = rows.findIndex(r => r.player) + 1;
  $('qTitle').textContent = `Qualifications · ${def.name} · ${pos === 1 ? 'Pole position !' : 'P' + pos}`;
  $('qTable').innerHTML = SZ.qualiHTML(rows);
  show('quali');
}
$('qRace').onclick = () => openSeason();
$('qRetry').onclick = async () => { SZ.season.grid = null; startRace({ solo: true }); };
let stTab = 'drivers', stFrom = 'season';
function showStandings(from) {
  stFrom = from || stFrom;
  $('stBody').innerHTML = SZ.standingsHTML(stTab);
  document.querySelectorAll('[data-st]').forEach(b => b.classList.toggle('sel', b.dataset.st === stTab));
  show('standings');
}
document.querySelectorAll('[data-st]').forEach(b => b.onclick = () => { stTab = b.dataset.st; showStandings(); });
$('stBack').onclick = () => show(stFrom);
$('szStand').onclick = () => showStandings('season');
$('szGarage').onclick = () => showGarage('season');
$('szMenu').onclick = () => { refreshMenu(); show('menu'); };
$('szQuit').onclick = () => { if (confirm('Abandonner la saison en cours ? Le garage et le budget sont conservés.')) { SZ.quitSeason(); refreshMenu(); show('menu'); } };
document.querySelectorAll('[data-mode]').forEach(b => b.onclick = () => { settings.mode = b.dataset.mode; save(); refreshMenu(); });

let garageFrom = 'menu';
function showGarage(from) { garageFrom = from; show('garage'); openGarage($('garage'), () => { refreshMenu(); show(garageFrom); }); }
$('garageBtn').onclick = () => showGarage('menu');
$('resGarageBtn').onclick = () => showGarage('results');
document.querySelectorAll('[data-laps]').forEach(b => b.onclick = () => { settings.laps = +b.dataset.laps; save(); refreshMenu(); });
$('tiltBtn').onclick = async () => { settings.tilt = !settings.tilt; save(); refreshMenu(); };
$('camSel').onclick = () => { toggleCam(); refreshMenu(); };
$('startBtn').onclick = async () => {
  if (def.id !== settings.circuit) { clearTimeout(loadTimer); loadCircuit(settings.circuit); }
  await enableTilt(settings.tilt);
  try { await document.documentElement.requestFullscreen?.(); await screen.orientation?.lock?.('landscape'); } catch {}
  if (settings.mode === 'season') { SZ.season ? openSeason() : setupSeason(); return; }
  startRace({});
};
$('camBtn').onclick = toggleCam;
$('pauseBtn').onclick = () => pause();
$('resumeBtn').onclick = () => { paused = false; last = performance.now(); show('hud'); };
$('restartBtn').onclick = () => startRace();
$('quitBtn').onclick = $('menuBtn').onclick = () => { paused = true; refreshMenu(); show('menu'); };
$('againBtn').onclick = () => R?.season ? openSeason() : startRace();
function pause() { if (!R || R.done || paused) return; paused = true; show('pause'); }
document.addEventListener('visibilitychange', () => { if (document.hidden) pause(); });

// choix du circuit : aperçu immédiat, construction 3D après une courte pause
let loadTimer = 0;
function showCircuitInfo() {
  const k = CIRCUITS.findIndex(c => c.id === settings.circuit);
  const d = CIRCUITS[Math.max(0, k)], t = buildTrack(d);
  $('circName').textContent = `${Math.max(0, k) + 1}. ${d.name}`;
  let dz = 0; for (let i = 0; i < t.N; i++) dz = Math.max(dz, t.Y[i]);
  $('circInfo').textContent = `${d.type} · ${(t.length / 1000).toFixed(2)} km` + (dz > 15 ? ` · dénivelé ${Math.round(dz)} m` : '');
  const cv = $('circPrev'), c = cv.getContext('2d'), W = cv.width, H = cv.height;
  let a = Infinity, b = -Infinity, e = Infinity, f = -Infinity;
  for (let i = 0; i < t.N; i++) { a = Math.min(a, t.X[i]); b = Math.max(b, t.X[i]); e = Math.min(e, t.Z[i]); f = Math.max(f, t.Z[i]); }
  const sc = Math.min((W - 12) / (b - a), (H - 12) / (f - e));
  const P = i => [W / 2 - (t.X[i] - (a + b) / 2) * sc, H / 2 - (t.Z[i] - (e + f) / 2) * sc];
  c.clearRect(0, 0, W, H); c.lineWidth = 3; c.lineJoin = 'round'; c.strokeStyle = '#fff'; c.beginPath();
  for (let i = 0; i < t.N; i += 3) { const [x, y] = P(i); i ? c.lineTo(x, y) : c.moveTo(x, y); }
  c.closePath(); c.stroke();
  const [x, y] = P(0); c.fillStyle = '#e10600'; c.beginPath(); c.arc(x, y, 4, 0, 7); c.fill();
}
function pickCircuit(step) {
  const k = CIRCUITS.findIndex(c => c.id === settings.circuit);
  settings.circuit = CIRCUITS[(Math.max(0, k) + step + CIRCUITS.length) % CIRCUITS.length].id; save();
  showCircuitInfo();
  clearTimeout(loadTimer); loadTimer = setTimeout(() => loadCircuit(settings.circuit), 350);
}
$('prevC').onclick = () => pickCircuit(-1);
$('nextC').onclick = () => pickCircuit(1);

loadCircuit(settings.circuit);
showCircuitInfo();
initInput(toggleCam, pause);
refreshMenu();
setFov();
show('menu');
if (params.has('demo')) startRace({});
requestAnimationFrame(loop);
