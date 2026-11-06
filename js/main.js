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
import { TYRES, FORMATS, MODES, TANK, forecast, wxText, makePlans, lapsFor, fuelFor, boxToggle, radioAsk, radioOrder } from './strategy.js';
import { initAudio, updateAudio } from './audio.js';
import { initFx, updateFx } from './fx.js';
import { resetCareer } from './career.js';
import { resetTeam, saveTeam } from './team.js';
import { tickInfra, sponsorIncome, prizeBonus, resetInfra } from './infra.js';
import { openMarket, aiMercato, MID_ROUND, MID_MULT } from './market.js';
import { TIERS } from './career.js';

const $ = id => document.getElementById(id);
const params = new URLSearchParams(location.search);
const settings = Object.assign({ cam: 'chase', tilt: false, format: 'sprint', circuit: 'valmont', mode: 'free', sound: true }, load());
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
initFx(scene);
function clearRace() { if (!R) return; for (const c of R.cars) scene.remove(c.mesh); if (R.sc) scene.remove(R.sc.car.mesh); }

// ---------- circuit ----------
let def, track, sky, lights, env, world = null, R = null;
function disposeAll(obj) {
  obj.traverse(o => {
    if (!o.geometry) return;
    o.geometry.dispose();
    for (const m of [].concat(o.material)) { if (m.map) m.map.dispose(); m.dispose(); }
  });
}
function loadCircuit(id) {
  if (R) { clearRace(); R = null; }
  if (world) { scene.remove(world); disposeAll(world); scene.environment?.dispose(); }
  def = CIRCUITS.find(c => c.id === id) || CIRCUITS[0];
  track = buildTrack(def);
  world = new THREE.Group(); scene.add(world);
  env = buildEnvironment(scene, renderer, track, world); sky = env.sky; env.road = null;
  ({ lights } = buildTrackMeshes(track, world)); env.road = world.getObjectByName('road');
  initHud(track);
  // vue d'attente du menu : survol de la ligne de départ
  const i = 40 % track.N, h = track.H[i];
  camera.position.set(track.X[i] - Math.sin(h) * 70 + Math.cos(h) * 30, track.Y[i] + 25, track.Z[i] - Math.cos(h) * 70 - Math.sin(h) * 30);
  camera.lookAt(track.X[i], track.Y[i] + 2, track.Z[i]);
}

let paused = true, last = performance.now();
const camState = { h: 0, x: 0, y: 0, z: 0 };

// ---------- météo : pluie, ciel gris, piste mouillée ----------
const RAIN_N = 1800, rainPos = new Float32Array(RAIN_N * 6);
for (let k = 0; k < RAIN_N; k++) {
  const x = (Math.random() - 0.5) * 80, y = (Math.random() - 0.5) * 40, z = (Math.random() - 0.5) * 80;
  rainPos.set([x, y, z, x + 0.15, y + 1.1, z], k * 6);
}
const rainGeo = new THREE.BufferGeometry(); rainGeo.setAttribute('position', new THREE.BufferAttribute(rainPos, 3));
const rain = new THREE.LineSegments(rainGeo, new THREE.LineBasicMaterial({ color: 0xc8d4e0, transparent: true, opacity: 0, depthWrite: false }));
rain.frustumCulled = false; scene.add(rain);
const GREY = new THREE.Color(0x8d949c);
let rainI = 0;
function applyWeather(dt, wet, raining) {
  rainI += ((raining ? 1 : 0) - rainI) * Math.min(1, dt * 0.6);
  if (!env) return;
  const r = rainI;
  scene.fog.color.copy(env.fog.color).lerp(GREY, 0.75 * r); scene.background = scene.fog.color;
  scene.fog.near = env.fog.near * (1 - 0.7 * r); scene.fog.far = env.fog.far * (1 - 0.6 * r);
  env.hemi.intensity = 1.25 * (1 - 0.35 * Math.max(r, wet * 0.5)); env.sun.intensity = 2.6 * (1 - 0.75 * r);
  env.skyMat.color.setRGB(1, 1, 1).lerp(GREY, 0.8 * r); env.sunSprite.material.opacity = 1 - r;
  if (env.road) env.road.material.color.setScalar(1 - 0.38 * wet);
  rain.visible = r > 0.02; rain.material.opacity = 0.45 * r;
  if (rain.visible) {
    rain.position.copy(camera.position);
    for (let k = 0; k < RAIN_N; k++) {
      const o = k * 6; let y = rainPos[o + 1] - 32 * dt;
      if (y < -20) y += 40;
      rainPos[o + 1] = y; rainPos[o + 4] = y + 1.1;
    }
    rainGeo.attributes.position.needsUpdate = true;
  }
}
const wxParam = () => ({ pluie: { rain: true, change: null }, averse: { rain: false, change: { at: 0.3, rain: true } }, eclaircie: { rain: true, change: { at: 0.35, rain: false } } })[params.get('meteo')] || { rain: false, change: null };

// format de course : nombre de tours d'après le temps au tour estimé du circuit
function raceFormat() {
  const lapTime = SZ.lapEstimate(track, 0.95, TEAMS[0].perf) * 0.985;
  return { lapTime, laps: +params.get('tours') || lapsFor(lapTime, settings.format) };
}
let raceOpts = {};
function startRace(opts = raceOpts) {
  raceOpts = opts;
  initAudio(settings.sound);
  if (R) clearRace();
  const rf = raceFormat();
  R = createRace(track, scene, opts.solo ? 1 : rf.laps, { autopilot: params.has('demo'), grid: +params.get('grille') || 10, lapTime: rf.lapTime, format: settings.format, ...opts });
  R.season = !!opts.season;
  resetInput();
  const p = R.player; camState.h = p.heading; camState.x = p.x; camState.y = p.y; camState.z = p.z;
  show('hud'); paused = false; last = performance.now();
}

// ---------- caméras ----------
const tmp = new THREE.Vector3();
// petite secousse lors d'un choc
function shake(p) {
  const k = Math.max(p.hitWall > 0.2 ? p.hitWall : 0, p.bump > 0.3 ? p.bump * 0.5 : 0);
  if (k > 0) camera.position.add(tmp.set(Math.random() - 0.5, Math.random() - 0.5, Math.random() - 0.5).multiplyScalar(k * 0.35));
}
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
  shake(p);
}
function toggleCam() {
  settings.cam = settings.cam === 'cockpit' ? 'chase' : 'cockpit'; save(); setFov();
  $('camBtn').textContent = settings.cam === 'cockpit' ? '🎥 1re' : '🎥 3e';
}

// ---------- boucle ----------
let fpsAcc = 0, fpsN = 0, fps = 0, slowT = 0;
const timeScale = +params.get('vitesse') || 1;
// derrière les menus la scène est fixe : on la redessine 4 fois par seconde seulement (menus plus fluides, moins de batterie)
let menuDraw = 0, upT = 0;
const maxRatio = Math.min(devicePixelRatio, 1.5);
function loop(now) {
  requestAnimationFrame(loop);
  let dt = Math.min(0.05, (now - last) / 1000); last = now;
  if (!R || paused) {
    if (now - menuDraw < 250) return;
    dt = Math.min(0.25, (now - menuDraw) / 1000); menuDraw = now;
  }
  applyWeather(dt, R ? R.wet : 0, R ? R.raining : false);
  if (!R) { updateAudio(null, camera.position, false); updateFx(null); sky.position.copy(camera.position); renderer.render(scene, camera); return; }
  if (!paused) {
    let t = dt * timeScale;
    while (t > 0) { const s = Math.min(t, 1 / 60); updateRace(R, s); t -= s; }
    lights.forEach((l, k) => l.material.color.setHex(R.phase === 'lights' && k < R.lightsOn ? 0xff1a1a : 0x330000));
    if (R.done && R.t - R.endT > (R.solo ? 1.2 : 2.5)) R.solo ? showQuali() : showResults();
    if (!R) return;
  }
  updateCamera(dt);
  if (params.has('sc') && !R.solo && R.phase === 'race' && R.t - R.raceStart > 12 && !R.scUsed) R.scWhy = 'test';
  updateFx(R, camera.position, paused ? 0 : dt * timeScale);
  updateAudio(R, camera.position, !paused);
  sky.position.copy(camera.position);
  renderer.render(scene, camera);
  // fps + qualité adaptative
  // résolution adaptative : baisse vite si ça rame, remonte doucement si c'est fluide
  if (paused) { fpsAcc = 0; fpsN = 0; } else { fpsAcc += dt; fpsN++; }
  if (fpsAcc > 1) {
    fps = Math.round(fpsN / fpsAcc); fpsAcc = 0; fpsN = 0;
    const setRatio = r => { pixelRatio = Math.round(r * 100) / 100; renderer.setPixelRatio(pixelRatio); resize(); };
    if (fps < 50 && pixelRatio > 0.6) { if (++slowT >= 2) { setRatio(Math.max(0.6, pixelRatio - (fps < 35 ? 0.3 : 0.15))); slowT = 0; } upT = 0; }
    else { slowT = 0; if (fps >= 58 && pixelRatio < maxRatio && ++upT >= 6) { setRatio(Math.min(maxRatio, pixelRatio + 0.1)); upT = 0; } }
  }
  if (!paused) updateHud(R, params.has('fps') ? fps + ' i/s · ' + renderer.info.render.calls + ' appels · ' + Math.round(renderer.info.render.triangles / 1000) + 'k tri' : 0);
}

// ---------- écrans ----------
function show(id) {
  for (const s of ['menu', 'hud', 'pause', 'results', 'garage', 'press', 'season', 'standings', 'quali', 'setup', 'prerace']) $(s).classList.toggle('hidden', s !== id);
  document.body.classList.toggle('racing', id === 'hud');
}
function showResults() {
  paused = true;
  const rows = results(R);
  const win = rows[0].time;
  $('resultsTable').innerHTML = rows.map(r =>
    `<tr class="${r.player ? 'me' : ''}"><td>${r.pos}</td><td><i style="background:#${r.color.toString(16).padStart(6, '0')}"></i>${r.name}<small>${r.team}${r.stops ? ` · ${r.stops} arrêt${r.stops > 1 ? 's' : ''}` : ''}${r.pen ? ` · pénalité +${r.pen} s` : ''}</small></td>` +
    `<td>${r.dnf ? 'Abandon' : r.pos === 1 ? fmt(r.time) : '+' + (r.time - win).toFixed(3) + (r.finished ? '' : '*')}</td><td>${fmt(r.best)}</td></tr>`).join('');
  const me = rows.find(r => r.player);
  if (!R.awarded) {
    const fastest = me.best && rows.every(r => !r.best || r.best >= me.best);
    R.mult = repFactor() * prizeBonus();
    R.prize = racePrize(me.pos, FORMATS[R.format]?.prize || 1, R.mult);
    R.sponsor = sponsorIncome(); team.budget = Math.round((team.budget + R.sponsor) * 10) / 10; saveTeam();
    R.parts = tickDevelopment();
    R.built = tickInfra();
    consumeBoost();
    // contexte pour la presse
    const mate = rows.find(r => !r.player && r.team === me.team);
    R.pressCtx = { pos: me.pos, grid: me.grid, gained: me.grid - me.pos, circuit: def.name, winner: rows[0].name,
      rival: me.pos > 1 && Math.random() < 0.6 ? rows[me.pos - 2].name : null, mate: mate?.name, mateAhead: mate && mate.pos < me.pos, fastest };
    R.pressDone = false;
    if (R.season) {
      SZ.recordRace(rows); R.grow = growMate();
      // ouverture du mercato : les écuries IA font leurs transferts
      const S = SZ.season;
      if (career && (S.round === MID_ROUND || S.done)) { S.news = aiMercato(S.done ? 'off' : 'mid'); SZ.saveSeason(); R.news = S.news.length; }
    }
  }
  $('againBtn').textContent = R.season ? 'Saison →' : 'Rejouer';
  $('rdGain').innerHTML = `Prime : <b>+${R.prize.toFixed(1)} M€</b> · Sponsors : <b>+${R.sponsor.toFixed(1)} M€</b> <small>(×${R.mult.toFixed(2)} · budget ${team.budget.toFixed(1)} M€)</small>` +
    (R.built ? `<br>🏗️ Construction terminée : <b>${R.built}</b>` : '') +
    (R.news ? `<br>📰 Mercato ouvert : <b>${R.news} transfert${R.news > 1 ? 's' : ''}</b> chez les rivaux` : '') +
    (R.parts.length ? `<br>🔧 Pièce${R.parts.length > 1 ? 's' : ''} livrée${R.parts.length > 1 ? 's' : ''} : <b>${R.parts.join(', ')}</b>` : '') +
    (R.grow ? `<br>📈 ${career.mate.name} progresse : vitesse ${Math.round(career.mate.spd)} (+${R.grow})` : '');
  $('pressBtn').classList.toggle('hidden', R.pressDone);
  $('resultTitle').textContent = me.dnf ? 'Abandon' : me.pos === 1 ? 'Victoire !' : me.pos <= 3 ? `Podium : P${me.pos}` : `Arrivée : P${me.pos}`;
  show('results');
}

function refreshMenu() {
  document.querySelectorAll('[data-fmt]').forEach(b => b.classList.toggle('sel', b.dataset.fmt === settings.format));
  $('tiltBtn').textContent = settings.tilt ? 'Inclinaison' : 'Boutons';
  $('soundBtn').textContent = settings.sound ? '🔊 Activé' : '🔇 Coupé';
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
  openSetup($('setup'), () => { resetGarage(); resetInfra(); SZ.newSeason(SZ.season ? SZ.season.year + 1 : 2027); openSeason(); }, () => { refreshMenu(); show('menu'); });
}
function openSeason() {
  const S = SZ.season;
  if (!S) return show('menu');
  if (R) { clearRace(); R = null; }
  paused = true;
  $('szTitle').textContent = `Championnat ${S.year}` + (S.done ? ' · terminé' : ` · manche ${S.round + 1}/10`);
  $('szCal').innerHTML = SZ.calendarHTML();
  $('szState').innerHTML = teamLine();
  const me = SZ.driverStandings().findIndex(d => d.player) + 1, myPts = S.pts['Toi'] || 0;
  if (S.done) {
    const ch = SZ.driverStandings()[0], tc = SZ.teamStandings()[0];
    const tp = SZ.teamStandings().findIndex(t => t.player) + 1;
    $('szNext').innerHTML = `<h3>${ch.player ? '🏆 Tu es champion du monde !' : `Champion : ${ch.name}`}</h3><p>Tu termines P${me} avec ${myPts} pts. Écuries : ${tc.name} championne, ton écurie P${tp}.</p><p>Intersaison : dotation, contrat moteur et transferts à prix normaux.</p>` + newsHTML(S);
    $('szActions').innerHTML = `<button class="main" id="szNew">Intersaison : moteur et transferts →</button>`;
    $('szNew').onclick = () => offSeason(tp);
  } else {
    const c = SZ.roundCircuit();
    if (def.id !== c.id) loadCircuit(c.id);
    if (!S.wx) { S.wx = forecast(c.theme); SZ.saveSeason(); }
    $('szNext').innerHTML = `<h3>${c.name}</h3><p>${c.type} · ${(track.length / 1000).toFixed(2)} km · ${FORMATS[settings.format].name} : ${raceFormat().laps} tours</p><p>Météo : ${wxText(S.wx)}</p><p>Toi : P${me} au championnat, ${myPts} pts</p>` + (S.round === MID_ROUND ? newsHTML(S) : '');
    const mid = S.round === MID_ROUND && career;
    $('szActions').innerHTML = (S.grid
      ? `<button class="main" id="szRace">Départ de la course (P${S.grid.indexOf('Toi') + 1} sur la grille)</button>`
      : `<button class="main" id="szQuali">Qualifications : tour chrono</button><button id="szSim">Simuler les qualifications</button>`) +
      (mid ? `<button id="szMarket">🔁 Mercato de mi-saison <small>(transferts ×${String(MID_MULT).replace(".", ",")}, ferme au départ)</small></button>` : '');
    const mk = $('szMarket'); if (mk) mk.onclick = () => {
      show('setup');
      openMarket($('setup'), 'mid', name => { S.midSign = name || S.midSign; SZ.saveSeason(); openSeason(); }, () => openSeason());
    };
    const q = $('szQuali'); if (q) q.onclick = async () => { await enableTilt(settings.tilt); startRace({ solo: true, wx: S.wx }); };
    const sm = $('szSim'); if (sm) sm.onclick = () => {
      const up = carUpgrades(), perf = TEAMS[0].perf * up.vtop;
      showQuali(SZ.lapEstimate(track, 0.95, perf, up.acc) * (1 + 0.006 * Math.random()) * 0.985 * (S.wx.rain ? 1.12 : 1));
    };
    const rc = $('szRace'); if (rc) rc.onclick = async () => { await enableTilt(settings.tilt); showPrerace({ season: true, gridOrder: S.grid, perfBonus: S.dev, wx: S.wx }); };
  }
  show('season');
}
const newsHTML = S => S.news?.length ? `<div class="news"><b>📰 Mercato</b>${S.news.map(n => `<p>${n}</p>`).join('')}</div>` : '';
// intersaison : dotation (niveau de l'écurie + place au championnat), puis moteur et transferts
function offSeason(tp) {
  const S = SZ.season;
  if (!career) return setupSeason();
  if (S.bonus == null) {
    const tier = TIERS.find(t => t.id === career.tier);
    S.bonus = Math.round((tier.budget * 0.6 + (11 - tp) * 2.5) * 10) / 10;
    team.budget = Math.round((team.budget + S.bonus) * 10) / 10; saveTeam(); SZ.saveSeason();
  }
  show('setup');
  openMarket($('setup'), 'off', () => { SZ.newSeason(S.year + 1, true); openSeason(); }, () => openSeason());
}
function showQuali(simTime) {
  paused = true;
  const time = simTime ?? (R && R.player.finished ? R.player.lastLap : null);
  if (R) { clearRace(); R = null; }
  const rows = SZ.qualifying(track, time, SZ.season.wx?.rain);
  SZ.season.quali = rows.map(r => ({ name: r.name, time: r.time }));
  SZ.season.grid = rows.map(r => r.name); SZ.saveSeason();
  const pos = rows.findIndex(r => r.player) + 1;
  $('qTitle').textContent = `Qualifications · ${def.name} · ${pos === 1 ? 'Pole position !' : 'P' + pos}`;
  $('qTable').innerHTML = SZ.qualiHTML(rows);
  show('quali');
}
$('qRace').onclick = () => openSeason();
$('qRetry').onclick = async () => { SZ.season.grid = null; startRace({ solo: true, wx: SZ.season.wx }); };
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
// confirmation en deux touches (les fenêtres confirm() sont bloquées en plein écran sur certains téléphones)
function twoTap(btn, action) {
  const label = btn.textContent;
  btn.onclick = () => {
    if (btn.dataset.armed) { clearTimeout(+btn.dataset.armed); delete btn.dataset.armed; btn.textContent = label; btn.classList.remove('warn'); action(); return; }
    btn.textContent = 'Touche encore pour confirmer'; btn.classList.add('warn');
    btn.dataset.armed = setTimeout(() => { delete btn.dataset.armed; btn.textContent = label; btn.classList.remove('warn'); }, 6000);
  };
}
twoTap($('szQuit'), () => { SZ.quitSeason(); refreshMenu(); show('menu'); });
document.querySelectorAll('[data-mode]').forEach(b => b.onclick = () => { settings.mode = b.dataset.mode; save(); refreshMenu(); });

let garageFrom = 'menu';
function showGarage(from) { garageFrom = from; show('garage'); openGarage($('garage'), () => { refreshMenu(); show(garageFrom); }); }
$('garageBtn').onclick = () => showGarage('menu');
$('resGarageBtn').onclick = () => showGarage('results');
document.querySelectorAll('[data-fmt]').forEach(b => b.onclick = () => { settings.format = b.dataset.fmt; save(); refreshMenu(); });
$('soundBtn').onclick = () => { settings.sound = !settings.sound; save(); initAudio(settings.sound); refreshMenu(); };
// remise à zéro complète : saison, écurie, garage, budget
function resetAll() {
  SZ.quitSeason(); resetCareer(); resetGarage(); resetInfra(); resetTeam(); refreshMenu(); show('menu');
}
twoTap($('resetBtn'), resetAll); twoTap($('szReset'), resetAll);
$('tiltBtn').onclick = async () => { settings.tilt = !settings.tilt; save(); refreshMenu(); };
$('camSel').onclick = () => { toggleCam(); refreshMenu(); };
$('startBtn').onclick = async () => {
  if (def.id !== settings.circuit) { clearTimeout(loadTimer); loadCircuit(settings.circuit); }
  await enableTilt(settings.tilt);
  try { await document.documentElement.requestFullscreen?.(); await screen.orientation?.lock?.('landscape'); } catch {}
  if (settings.mode === 'season') { SZ.season ? openSeason() : setupSeason(); return; }
  showPrerace({});
};
$('camBtn').onclick = toggleCam;

// ---------- avant la course : météo, 3 stratégies proposées, réglage des pneus et du carburant ----------
const chip = t => `<b class="tchip" style="--c:${TYRES[t].color}">${t}</b>`;
const mmss = s => `${Math.floor(s / 60)} min ${String(Math.round(s % 60)).padStart(2, '0')}`;
function showPrerace(opts) {
  const wx = opts.wx || (params.get('meteo') ? wxParam() : forecast(def.theme));
  const { laps, lapTime } = raceFormat(), lapKm = track.length / 1000;
  const plans = makePlans(laps, lapKm, lapTime, wx.rain);
  let sel = 1, plan = null;
  const choose = k => { sel = k; plan = { ...plans[k], stints: plans[k].stints.map(s => ({ ...s })) }; };
  choose(Math.max(0, plans.findIndex(p => p.best)));
  const render = () => {
    $('prTitle').textContent = `${FORMATS[settings.format].name} · ${laps} tours · ~${Math.round(laps * lapTime * 1.12 / 60)} min`;
    $('prWx').textContent = 'Météo : ' + wxText(wx);
    $('prPlans').innerHTML = plans.map((p, k) => `<button data-plan="${k}" class="${k === sel ? 'pick' : ''}"><b>${p.name}${p.best ? ' <i class="best">★ la plus rapide</i>' : ''}</b>
      <span class="stints">${p.stints.map(s => chip(s.tyre) + s.laps).join(' → ')}</span>
      <small>${p.stops} arrêt${p.stops > 1 ? 's' : ''} · ${p.fuel} kg au départ · ≈ ${mmss(p.time)}</small></button>`).join('');
    const s0 = plan.stints[0], need = fuelFor(s0.laps, lapKm);
    $('prAdjust').innerHTML = `<span>Départ :</span><button id="prTyre">${chip(s0.tyre)} ${TYRES[s0.tyre].name}</button>
      <button id="prF-">−</button><b>${plan.fuel} kg</b><button id="prF+">+</button>`;
    const wetBad = (s0.tyre === 'W') !== wx.rain;
    $('prHint').textContent = plan.fuel < need ? `⚠ Pas assez de carburant pour ${s0.laps} tours (il faut ${need} kg) : arrêt plus tôt ou panne sèche !`
      : wetBad ? (wx.rain ? '⚠ Pneus slicks sur piste mouillée : très peu d’adhérence !' : '⚠ Pneus pluie sur le sec : lents et usure rapide.')
      : `${plans[sel].desc}${plans[sel].best ? ' C’est le choix conseillé par le muret.' : ''} Tu pourras changer d’avis en course avec la radio.`;
    document.querySelectorAll('[data-plan]').forEach(b => b.onclick = () => { choose(+b.dataset.plan); render(); });
    $('prTyre').onclick = () => { const k = Object.keys(TYRES); s0.tyre = k[(k.indexOf(s0.tyre) + 1) % k.length]; render(); };
    $('prF-').onclick = () => { plan.fuel = Math.max(10, plan.fuel - 5); render(); };
    $('prF+').onclick = () => { plan.fuel = Math.min(TANK, plan.fuel + 5); render(); };
  };
  render();
  $('prGo').onclick = () => startRace({ ...opts, wx: { ...wx }, plan });
  $('prBack').onclick = () => opts.season ? openSeason() : (refreshMenu(), show('menu'));
  show('prerace');
}
// stand et radio
const toggleBox = () => { if (R && !R.solo) boxToggle(R, R.player); };
$('boxBtn').onclick = toggleBox;
$('tyreChip').onclick = () => { if (R && !R.solo) radioOrder(R, 'tyre'); };
addEventListener('keydown', e => { if (e.code === 'KeyB') toggleBox(); if (e.code === 'KeyR') toggleRadio(); });
function toggleRadio() { if (!R || R.solo) return; $('radioPanel').classList.toggle('hidden'); radioLabels(); }
function radioLabels() {
  if (!R) return;
  const p = R.player, nx = p.box?.tyre || p.plan?.[p.stint + 1]?.tyre;
  $('rpMode').textContent = 'Rythme : ' + MODES[p.mode].name;
  $('rpTyre').innerHTML = 'Pneus suivants : ' + (nx ? chip(nx) : '—');
}
$('radioBtn').onclick = toggleRadio;
$('rpClose').onclick = () => $('radioPanel').classList.add('hidden');
document.querySelectorAll('[data-ask]').forEach(b => b.onclick = () => R && radioAsk(R, b.dataset.ask));
document.querySelectorAll('[data-order]').forEach(b => b.onclick = () => { if (R) { radioOrder(R, b.dataset.order); radioLabels(); } });
$('pauseBtn').onclick = () => pause();
$('resumeBtn').onclick = () => { paused = false; last = performance.now(); show('hud'); };
$('restartBtn').onclick = () => startRace();
$('quitBtn').onclick = $('menuBtn').onclick = () => { paused = true; refreshMenu(); show('menu'); };
$('againBtn').onclick = () => R?.season ? openSeason() : showPrerace({});
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
if (params.has('demo')) startRace({ wx: wxParam() });
requestAnimationFrame(loop);
