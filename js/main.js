// Point d'entrée : rendu Three.js, caméras, boucle de jeu, écrans.
import * as THREE from '../lib/three.module.min.js';
import { CIRCUITS } from './data.js';
import { buildTrack, pitLane } from './track.js';
import { buildTrackMeshes, buildEnvironment } from './scenery.js';
import { setAniso } from './textures.js';
import { createRace, updateRace, results } from './race.js';
import { initInput, enableTilt, resetInput } from './input.js';
import { initHud, updateHud, fmt } from './hud.js';
import { garage, openGarage, tickDevelopment, resetGarage, grantParts, drawRegulation, applyRegulation, treeName, perfFactor } from './garage.js';
import { openSetup, growMate, career } from './career.js';
import { team, racePrize, repFactor, consumeBoost } from './team.js';
import { buildQuestions, runPress, teamLine } from './press.js';
import { TEAMS } from './data.js';
import { carUpgrades } from './garage.js';
import * as SZ from './season.js';
import { TYRES, FORMATS, MODES, forecast, wxText, lapsFor, boxToggle, radioAsk, radioOrder } from './strategy.js';
import { showPreraceScreen } from './prerace.js';
import { startIntro, startOutro, cineCamera, cineKind, end as endCine, champCard } from './cine.js';
import { renderRoom } from './showroom.js';
import { showTuning, bestSetup, setupLap, weekendTemps, SETUPS } from './tuning.js';
import { makeQuali } from './qsession.js';
import { eventCtx, pickEvent, showEvent, boardAfterRace, seasonReview, isFired, boardHTML, makeMission, checkMission, missionHTML, payDeals } from './events.js';
// livraisons de l'usine et des chantiers (résultats d'une course, étapes de la pré-saison)
const devNews = (p, built) => (built ? `<br>🏗️ Construction terminée : <b>${built}</b>` : '') +
  (p.done.length ? `<br>🔧 Pièce${p.done.length > 1 ? 's' : ''} livrée${p.done.length > 1 ? 's' : ''} : <b>${p.done.join(', ')}</b>` : '') +
  (p.late.length ? `<br>⏱ Retard aux essais : <b>${p.late.join(', ')}</b>` : '') +
  (p.fail.length ? `<br>❌ Recherche ratée (argent perdu) : <b class="bad">${p.fail.join(', ')}</b>` : '');
import { initAudio, updateAudio } from './audio.js';
import { initFx, updateFx } from './fx.js';
import { initDebris, updateDebris, clearDebris } from './debris.js';
import { resetCareer } from './career.js';
import { resetTeam, saveTeam } from './team.js';
import { resetStaff, renewStaff } from './staff.js';
import { tickInfra, sponsorIncome, prizeBonus, resetInfra, grantInfra } from './infra.js';
const playerPerf = () => TEAMS[0].perf * perfFactor();
import { openMarket, aiMercato, MID_MULT } from './market.js';
import { TIERS } from './career.js';

const $ = id => document.getElementById(id);
const params = new URLSearchParams(location.search);
const settings = Object.assign({ cam: 'chase', tilt: false, format: 'sprint', circuit: 'valmont', sound: true, soloFmt: 'sprint', soloQuali: 'play', soloWx: 'auto', abs: true, tc: true, line: 0 }, load());
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
initFx(scene); initDebris(scene);
function clearRace() { clearDebris(); if (!R) return; for (const c of R.cars) scene.remove(c.mesh); if (R.sc) scene.remove(R.sc.car.mesh); }

// ---------- circuit ----------
let def, track, sky, lights, env, world = null, R = null;
function disposeAll(obj) {
  obj.traverse(o => {
    if (!o.geometry) return;
    o.geometry.dispose();
    for (const m of [].concat(o.material)) { for (const k of ['map', 'normalMap', 'alphaMap']) if (m[k]) m[k].dispose(); m.dispose(); }
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
  env.skyMat.color.setRGB(1, 1, 1).lerp(GREY, 0.8 * r); if (env.cloudMat) env.cloudMat.color.setRGB(1, 1, 1).lerp(GREY, 0.6 * r); env.sunSprite.material.opacity = 1 - r;
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

// contexte de jeu : 'career' (saison) ou 'solo' (course seule, sans effet sur la carrière)
let ctx = 'solo';
const fmtKey = () => ctx === 'career' ? settings.format : (FORMATS[settings.soloFmt] ? settings.soloFmt : 'sprint');
// format de course : nombre de tours d'après le temps au tour estimé du circuit
function raceFormat() {
  const lapTime = SZ.lapEstimate(track, 0.95, TEAMS[0].perf) * 0.985;
  return { lapTime, laps: +params.get('tours') || lapsFor(lapTime, fmtKey()) };
}
let raceOpts = {}, lastIntro = '';
if (params.has('debug')) { window.gpm = () => R; window.gpmCam = camera; window.gpmTrack = () => track; }   // tests : accès à la course, caméra et circuit
function startRace(opts = raceOpts) {
  const S = SZ.season;
  if (opts.season && S?.gridPen && opts.gridOrder) {   // pénalité de grille : on recule de n places
    const g = opts.gridOrder.filter(n => n !== 'Toi'), i = opts.gridOrder.indexOf('Toi');
    g.splice(Math.min(g.length, i + S.gridPen), 0, 'Toi');
    opts = { ...opts, gridOrder: g }; S.gridPen = 0; SZ.saveSeason();
  }
  raceOpts = opts;
  initAudio(settings.sound);
  if (R) clearRace();
  const rf = raceFormat();
  R = createRace(track, scene, opts.solo ? 1 : rf.laps, { autopilot: params.has('demo'), grid: +params.get('grille') || 10, lapTime: rf.lapTime, format: fmtKey(), assist: { abs: settings.abs, tc: settings.tc, line: settings.line }, ...opts });
  R.season = !!opts.season; R.free = ctx === 'solo';
  if (opts.solo) makeQuali(R, () => SZ.qualifying(track, null, !!opts.wx?.rain), `${def.id}:${ctx === 'career' && S ? S.year + '-' + S.round : 'solo'}`, rf.lapTime);
  resetInput();
  const p = R.player; camState.h = p.heading; camState.x = p.x; camState.y = p.y; camState.z = p.z;
  if (opts.sim) { R.sim = true; return simulate(); }
  show('hud'); paused = false; last = performance.now();
  // cinématique : survol du circuit et titre du Grand Prix avant les feux (pas quand on recommence la même course)
  const ik = `${def.id}:${opts.season && S ? S.year + '-' + S.round : ctx}`;
  if (!opts.solo && ik !== lastIntro) lastIntro = ik, startIntro(R, track, {
    top: opts.season && S ? `Manche ${S.round + 1}/${SZ.nRounds()} · ${SZ.fmtDate(SZ.raceDate(S.round))} ${S.year}` : 'Course solo',
    name: def.name, sub: `${def.type} · ${(track.length / 1000).toFixed(2).replace('.', ',')} km · ${R.laps} tour${R.laps > 1 ? 's' : ''} · ${wxText(R.wx)}` }, () => {});
}
// course simulée : le pilote automatique court à ta place, la course tourne en accéléré sans affichage
function simulate() {
  const btns = ['prGo', 'prSim', 'prBack'].map($), out = $('prerace').classList.contains('hidden') ? $('szSimProg') : $('prHint');
  btns.forEach(b => b.disabled = true);
  const step = () => {
    if (!R || !R.sim) return;
    const t0 = performance.now();
    while (!R.done && performance.now() - t0 < 30) for (let k = 0; k < 30 && !R.done; k++) updateRace(R, 1 / 30);
    const lead = R.order[0], pct = Math.min(99, Math.round(100 * ((lead.lap - 1) + lead.idx / track.N) / R.laps));
    out.textContent = `⏩ Simulation de la course… ${R.done ? 100 : Math.max(0, pct)} %  ·  P${R.order.indexOf(R.player) + 1} · tour ${Math.max(1, Math.min(R.player.lap, R.laps))}/${R.laps}`;
    if (R.done) { btns.forEach(b => b.disabled = false); showResults(); } else setTimeout(step, 0);
  };
  step();
}

// ---------- caméras ----------
const tmp = new THREE.Vector3();
// petite secousse lors d'un choc
function shake(p) {
  const k = Math.max(p.hitWall > 0.2 ? p.hitWall : 0, p.bump > 0.3 ? p.bump * 0.5 : 0, p.kerb && p.v > 20 ? 0.06 : 0, p.spinT > 0 ? 0.08 : 0);
  if (k > 0) camera.position.add(tmp.set(Math.random() - 0.5, Math.random() - 0.5, Math.random() - 0.5).multiplyScalar(k * 0.35));
}
function updateCamera(dt) {
  const p = R.player;
  if (settings.cam === 'cockpit') {
    p.mesh.updateMatrixWorld();
    camera.position.copy(tmp.set(0, 0.9, 0.32).applyMatrix4(p.mesh.matrixWorld));
    camera.lookAt(tmp.set(0, 0.62, 10).applyMatrix4(p.mesh.matrixWorld));
    shake(p);
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
let menuDraw = 0, upT = 0, roomOn = false;   // roomOn : atelier 3D derrière le QG et les réglages (30 i/s)
const maxRatio = Math.min(devicePixelRatio, 1.5);
function loop(now) {
  requestAnimationFrame(loop);
  let dt = Math.min(0.05, (now - last) / 1000); last = now;
  if (roomOn) {
    if (now - menuDraw < 32) return;
    renderRoom(renderer, Math.min(0.1, (now - menuDraw) / 1000)); menuDraw = now; return;
  }
  if (!R || paused) {
    if (now - menuDraw < 250) return;
    dt = Math.min(0.25, (now - menuDraw) / 1000); menuDraw = now;
  }
  applyWeather(dt, R ? R.wet : 0, R ? R.raining : false);
  if (!R) { updateAudio(null, camera.position, false); updateFx(null); sky.position.copy(camera.position); renderer.render(scene, camera); return; }
  if (!paused) {
    let t = cineKind() === 'intro' ? 0 : dt * timeScale;   // la course attend la fin du survol
    while (t > 0) { const s = Math.min(t, 1 / 60); updateRace(R, s); t -= s; }
    lights.forEach((l, k) => l.material.color.setHex(R.phase === 'lights' && k < R.lightsOn ? 0xff1a1a : 0x330000));
    // podium : tour d'honneur avant les résultats
    const pos = R.order.indexOf(R.player) + 1;
    if (R.done && !R.outro && !R.solo && R.player.finished && pos <= 3) {
      R.outro = true;
      startOutro(R, { top: def.name, name: pos === 1 ? 'Victoire !' : `Podium · P${pos}`, sub: pos === 1 ? 'Tu remportes le Grand Prix' : 'Un nouveau podium pour l’écurie', confetti: pos === 1 }, () => showResults());
    }
    if (R.done && cineKind() !== 'outro' && R.t - R.endT > (R.solo ? 3 : 2.5)) R.solo ? showQuali() : showResults();
    if (!R) return;
  }
  if (!cineCamera(camera)) updateCamera(dt);
  if (params.has('sc') && !R.solo && R.phase === 'race' && R.t - R.raceStart > 12 && !R.scUsed) R.scWhy = 'test';
  updateFx(R, camera.position, paused ? 0 : dt * timeScale);
  updateDebris(paused ? 0 : dt * timeScale);
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
  if (id !== 'hud') endCine(false);
  for (const s of ['menu', 'solo', 'settings', 'hud', 'pause', 'results', 'garage', 'press', 'season', 'standings', 'quali', 'setup', 'prerace', 'tuning']) $(s).classList.toggle('hidden', s !== id);
  roomOn = id === 'garage' || id === 'tuning';
  document.body.classList.toggle('racing', id === 'hud');
  document.body.classList.toggle('quali', id === 'hud' && !!R?.solo);
}
function showResults() {
  paused = true;
  const rows = results(R);
  const win = rows[0].time;
  $('resultsTable').innerHTML = rows.map(r =>
    `<tr class="${r.player ? 'me' : ''}"><td>${r.pos}</td><td><i style="background:#${r.color.toString(16).padStart(6, '0')}"></i>${r.name}<small>${r.team}${r.stops ? ` · ${r.stops} arrêt${r.stops > 1 ? 's' : ''}` : ''}${r.pen ? ` · pénalité +${r.pen} s` : ''}</small></td>` +
    `<td>${r.dnf ? 'Abandon' : r.pos === 1 ? fmt(r.time) : '+' + (r.time - win).toFixed(3) + (r.finished ? '' : '*')}</td><td>${fmt(r.best)}</td></tr>`).join('');
  const me = rows.find(r => r.player);
  if (R.free && !R.awarded) {
    R.awarded = true; R.pressDone = true; R.prize = R.sponsor = 0; R.mult = 1; R.parts = { done: [], fail: [], late: [] }; R.built = R.news = R.grow = 0;
  }
  if (!R.awarded) {
    const fastest = me.best && rows.every(r => !r.best || r.best >= me.best);
    R.mult = repFactor() * prizeBonus();
    R.prize = racePrize(me.pos, FORMATS[R.format]?.prize || 1, R.mult);
    R.sponsor = sponsorIncome(); team.budget = Math.round((team.budget + R.sponsor) * 10) / 10; saveTeam();
    // le temps passe jusqu'au prochain GP (calendrier) : recherches et chantiers avancent
    R.days = R.season && SZ.season ? SZ.daysAfter(SZ.season.round) : 14;
    R.parts = tickDevelopment(R.days);
    R.built = tickInfra(R.days);
    consumeBoost();
    // contexte pour la presse
    const mate = rows.find(r => !r.player && r.team === me.team);
    R.pressCtx = { pos: me.pos, grid: me.grid, gained: me.grid - me.pos, circuit: def.name, winner: rows[0].name,
      rival: me.pos > 1 && Math.random() < 0.6 ? rows[me.pos - 2].name : null, mate: mate?.name, mateAhead: mate && mate.pos < me.pos, fastest,
      dnf: me.dnf, wet: R.wet > 0.3, stops: me.stops, flat: (R.player.flats || 0) > 0, sc: !!R.scUsed, pen: me.pen };
    R.pressDone = false;
    if (R.season) {
      SZ.recordRace(rows); R.grow = growMate();
      // ouverture du mercato : les écuries IA font leurs transferts
      const S = SZ.season;
      if (career && (S.round === SZ.midRound() || S.done)) { S.news = aiMercato(S.done ? 'off' : 'mid'); SZ.saveSeason(); R.news = S.news.length; }
      // annonce du règlement de la saison suivante à la mi-saison (ou en fin de saison pour les anciennes sauvegardes)
      if ((S.round === SZ.midRound() || S.done) && !S.reg) { S.reg = drawRegulation(); SZ.saveSeason(); R.regNews = S.reg.map(treeName).join(' et '); R.regN = S.reg.length; }
      SZ.recordPerf(playerPerf());
      // direction, mission du sponsor, sponsors ponctuels, événement d'ici le prochain GP
      R.conf = boardAfterRace(me.pos, me.dnf);
      const m = S.mission, ahead = SZ.driverStandings();
      if (m?.acc) {
        const rp = rows.find(r => r.name === m.who);
        R.mis = { ...m, ok: checkMission(m, { pos: me.pos, dnf: me.dnf, gained: me.grid - me.pos, fastest, matePos: mate ? mate.pos : 99, rivalPos: rp ? (rp.dnf ? 99 : rp.pos) : 99 }) };
        if (R.mis.ok) team.budget = Math.round((team.budget + m.gain) * 10) / 10; else team.rep = Math.max(0, team.rep - 3);
        saveTeam();
      }
      S.mission = null;
      R.deals = payDeals();
      if (S.done) R.review = seasonReview();
      else S.event = pickEvent(eventCtx({ pos: me.pos, dnf: me.dnf, rival: ahead[Math.max(0, ahead.findIndex(d => d.player) - 1)]?.name }));
      SZ.saveSeason();
      Object.assign(R.pressCtx, { champ: ahead.findIndex(d => d.player) + 1, round: S.round, n: SZ.nRounds(), mid: S.round === SZ.midRound(), last: S.done,
        role: career?.contract?.role, conf: team.conf, tier: career?.tier, reg: !!S.reg?.length });
    }
  }
  $('againBtn').textContent = R.season ? 'Saison →' : 'Nouvelle course';
  $('resGarageBtn').classList.toggle('hidden', R.free);
  $('rdGain').innerHTML = R.free ? `Course solo · ${def.name} · parti P${me.grid}, ${me.dnf ? 'abandon' : 'arrivé P' + me.pos}${me.best ? ' · meilleur tour ' + fmt(me.best) : ''}` : `Prime : <b>+${R.prize.toFixed(1)} M€</b> · Sponsors : <b>+${R.sponsor.toFixed(1)} M€</b> <small>(×${R.mult.toFixed(2)} · budget ${team.budget.toFixed(1)} M€)</small>` +
    (R.regNews ? `<br>📜 <b>Annonce officielle</b> : nouveau règlement ${SZ.season.year + 1} pour <b>${R.regNews}</b>. Tes pièces ${R.regN > 1 ? 'de ces domaines' : 'de ce domaine'} repartiront de zéro l’an prochain.` : '') +
    (R.news ? `<br>📰 Mercato ouvert : <b>${R.news} transfert${R.news > 1 ? 's' : ''}</b> chez les rivaux` : '') +
    (R.season ? `<br>📅 ${SZ.season?.done ? `Fin de saison dans <b>${R.days} jours</b> (31 déc.), puis pré-saison` : `Prochain GP dans <b>${R.days} jours</b>`} : l’usine travaille pendant ce temps.` : '') +
    devNews(R.parts, R.built) +
    (R.grow ? `<br>📈 ${career.mate.name} progresse : vitesse ${Math.round(career.mate.spd)} (+${R.grow})` : '') +
    (R.mis ? `<br>🤝 Mission ${R.mis.sp} (${R.mis.txt.toLowerCase()}) : ${R.mis.ok ? `<b>réussie, +${R.mis.gain.toFixed(1)} M€</b>` : '<b class="bad">ratée, réputation −3</b>'}` : '') +
    (R.deals ? `<br>💼 Sponsors ponctuels : <b>+${R.deals.toFixed(1)} M€</b>` : '') +
    (R.conf != null && !R.free ? `<br>🏛️ Direction : confiance ${R.conf > 0 ? '+' : ''}${R.conf} → <b>${Math.round(team.conf)} %</b>` : '') +
    (R.review ? `<br>🏁 Bilan de la direction : ${R.review.met}/${R.review.n} objectifs atteints (${R.review.d > 0 ? '+' : ''}${R.review.d})${R.review.bonus ? ` · bonus <b>+${R.review.bonus} M€</b>` : ''}${R.review.fired ? ' · <b class="bad">la direction veut te remplacer…</b>' : ''}` : '');
  $('pressBtn').classList.toggle('hidden', R.pressDone);
  $('resultTitle').textContent = (me.dnf ? 'Abandon' : me.pos === 1 ? 'Victoire !' : me.pos <= 3 ? `Podium : P${me.pos}` : `Arrivée : P${me.pos}`) + (R.sim ? ' · course simulée' : '');
  show('results');
}

function refreshMenu() {
  document.querySelectorAll('[data-sfmt]').forEach(b => b.classList.toggle('sel', b.dataset.sfmt === settings.soloFmt));
  document.querySelectorAll('[data-squali]').forEach(b => b.classList.toggle('sel', b.dataset.squali === settings.soloQuali));
  document.querySelectorAll('[data-swx]').forEach(b => b.classList.toggle('sel', b.dataset.swx === settings.soloWx));
  $('soloGo').textContent = settings.soloQuali === 'play' ? 'Tour de qualification →' : 'Simuler les qualifs →';
  $('tiltBtn').textContent = settings.tilt ? 'Inclinaison' : 'Boutons';
  $('soundBtn').textContent = settings.sound ? '🔊 Activé' : '🔇 Coupé';
  $('absBtn').textContent = settings.abs ? 'Activé' : 'Désactivé';
  $('tcBtn').textContent = settings.tc ? 'Activé' : 'Désactivé';
  $('lineBtn').textContent = ['Aucune', 'Freinage', 'Freinage + direction'][settings.line || 0];
  $('camBtn').textContent = settings.cam === 'cockpit' ? '🎥 1re' : '🎥 3e';
  $('camSel').textContent = settings.cam === 'cockpit' ? 'Cockpit (1re pers.)' : 'Arrière (3e pers.)';
  $('menuPts').textContent = team.budget.toFixed(1) + ' M€';
  const S = SZ.season;
  $('homeYear').textContent = S ? S.year : 2027;
  const me = S ? SZ.driverStandings().findIndex(d => d.player) + 1 : 0;
  $('careerInfo').innerHTML = !S ? 'Nouvelle carrière' : S.done ? `Saison ${S.year} terminée · P${me}` : career && SZ.preStep() < 3 ? `${TEAMS[0].name} · saison ${S.year} · pré-saison (${SZ.fmtDate(SZ.today())})` : `Saison ${S.year} · manche <b>${S.round + 1}</b>/${SZ.nRounds()} · P${me} · ${S.pts['Toi'] || 0} pts`;
  $('careerBtn').querySelector('.hgo').firstChild.textContent = S ? 'Continuer ' : 'Commencer ';
  const c = CIRCUITS.find(c => c.id === settings.circuit) || CIRCUITS[0];
  $('soloInfo').textContent = `${c.name} · ${FORMATS[settings.soloFmt]?.name || 'Sprint'}`;
  $('garageBtn').classList.toggle('hidden', !S);
  $('teamState').innerHTML = S ? teamLine() : '';
}
$('pressBtn').onclick = () => {
  if (!R || R.pressDone) return;
  show('press');
  runPress($('press'), buildQuestions(R.pressCtx), () => { R.pressDone = true; $('pressBtn').classList.add('hidden'); show('results'); });
};
// ---------- saison ----------
function setupSeason() {
  show('setup');
  openSetup($('setup'), () => {
    // départ selon le niveau : rien pour une nouvelle écurie, des pièces et des bâtiments pour les autres
    resetGarage(); resetInfra(); resetStaff();
    const tier = TIERS.find(t => t.id === career.tier);
    if (tier.parts) grantParts(tier.parts);
    if (tier.infra) grantInfra(tier.infra);
    SZ.newSeason(SZ.season ? SZ.season.year + 1 : 2027); SZ.recordPerf(playerPerf()); openSeason();
  }, () => { refreshMenu(); show('menu'); });
}
// pré-saison : du 1er janvier au premier GP, le temps avance par étapes (présentation de la voiture, essais
// hivernaux) ; l'usine, les chantiers et les écuries rivales travaillent pendant ce temps
const PRE = [
  ['🎆 Pré-saison', 'La saison commence : lance tes premières pièces au QG, construis, recrute tes chefs de pôle et, si tu veux, un nouvel équipier au mercato d’hiver.'],
  ['🚗 Présentation de la voiture', 'La voiture est dévoilée. Relance l’usine si un pôle est libre : chaque jour compte avant le premier GP.'],
  ['🧪 Essais hivernaux', 'Dernière ligne droite avant le premier Grand Prix.'],
];
const PRE_NEXT = ['présentation de la voiture', 'essais hivernaux', 'semaine du 1er GP'];
function preSeason(S, pre) {
  const c = SZ.roundCircuit(), D = SZ.preDates(), days = Math.round((D[pre + 1] - D[pre]) / 864e5);
  if (def.id !== c.id) loadCircuit(c.id);
  $('szNext').innerHTML = `<h3>${PRE[pre][0]}</h3><p>📅 Aujourd’hui : <b>${SZ.fmtDate(D[pre])} ${S.year}</b> · 1er GP le ${SZ.fmtDate(D[3])} (${c.name})</p>` +
    `<p>${PRE[pre][1]} Les pièces demandent 20 à 56 jours ; les écuries rivales travaillent aussi pendant l’hiver.</p>` + (S.preLog || '') + boardHTML() + regHTML(S) + newsHTML(S);
  $('szActions').innerHTML = `<button class="main" id="szPre">⏩ ${pre < 2 ? `Avancer au ${SZ.fmtDate(D[pre + 1])}` : 'Partir pour le 1er GP'}<small>${days} jours · ${PRE_NEXT[pre]}</small></button>` +
    (career.talks?.signed === `${S.year}-pre` ? '' : '<button id="szWinter">🤝 Mercato d’hiver : recruter un pilote</button>');
  $('szPre').onclick = () => {
    const parts = tickDevelopment(days), built = tickInfra(days);
    SZ.devRivals(days);
    S.pre = pre + 1;
    S.preLog = `<p class="prelog">🏭 <b>${days} jours d’usine</b> (${SZ.fmtDate(D[pre])} → ${SZ.fmtDate(D[pre + 1])})${devNews(parts, built) || ' : rien de livré.'}</p>`;
    if (S.pre < 3) S.event = { id: S.pre === 1 ? 'launch' : 'tests', ctx: eventCtx({}) };   // choix à faire
    SZ.saveSeason(); openSeason();
  };
  const w = $('szWinter'); if (w) w.onclick = () => { show('setup'); openMarket($('setup'), 'pre', () => openSeason(), () => openSeason()); };
}
function openSeason() {
  const S = SZ.season;
  if (!S) return show('menu');
  if (R) { clearRace(); R = null; }
  paused = true;
  const pre = career ? SZ.preStep() : 3;   // pré-saison : 0 = 1er janvier, 1 = présentation, 2 = essais, 3 = semaine du 1er GP
  $('szTitle').textContent = `Championnat ${S.year}` + (S.done ? ' · terminé' : pre < 3 ? ' · pré-saison' : ` · manche ${S.round + 1}/${SZ.nRounds()}`);
  if (S.done && career) seasonReview();
  if (isFired()) {   // la direction a perdu confiance : fin de la carrière
    $('szTitle').textContent = 'Licencié !';
    $('szCal').innerHTML = SZ.calendarHTML(); $('szState').innerHTML = teamLine();
    $('szNext').innerHTML = `<h3>🏛️ La direction te remercie</h3><p>Confiance rompue (${Math.round(team.conf)} %). ${S.review?.fired ? `Objectifs atteints : ${S.review.met}/${S.review.n}.` : 'Les résultats sont trop loin des attentes.'} Une autre écurie te donnera peut-être ta chance…</p>`;
    $('szActions').innerHTML = `<button class="main" id="szFired">Nouvelle carrière</button>`;
    $('szFired').onclick = resetAll;
    return show('season');
  }
  $('szCal').innerHTML = SZ.calendarHTML();
  $('szState').innerHTML = teamLine();
  const me = SZ.driverStandings().findIndex(d => d.player) + 1, myPts = S.pts['Toi'] || 0;
  if (S.done) {
    const ch = SZ.driverStandings()[0], tc = SZ.teamStandings()[0];
    const tp = SZ.teamStandings().findIndex(t => t.player) + 1;
    $('szNext').innerHTML = `<h3>${ch.player ? '🏆 Tu es champion du monde !' : `Champion : ${ch.name}`}</h3><p>Tu termines P${me} avec ${myPts} pts. Écuries : ${tc.name} championne, ton écurie P${tp}.</p><p>Intersaison : dotation, salaires des chefs de département, contrat moteur, contrat de ton équipier et transferts sans surcoût.</p>` + (career ? boardHTML() : '') + regHTML(S) + newsHTML(S);
    $('szActions').innerHTML = `<button class="main" id="szNew">Intersaison : moteur et transferts →</button>`;
    $('szNew').onclick = () => offSeason(tp);
  } else if (pre < 3) preSeason(S, pre);
  else {
    const c = SZ.roundCircuit();
    if (def.id !== c.id) loadCircuit(c.id);
    if (!S.wx) { S.wx = forecast(c.theme); SZ.saveSeason(); }
    if (career && !S.mission && !S.grid) { const ds = SZ.driverStandings(), k = ds.findIndex(d => d.player); S.mission = makeMission(ds[k ? k - 1 : 1]?.name); SZ.saveSeason(); }
    $('szNext').innerHTML = `<h3>${c.name}</h3><p>${c.type} · ${(track.length / 1000).toFixed(2)} km · ${FORMATS[fmtKey()].name} : ${raceFormat().laps} tours · arrêt au stand ≈ ${Math.round(pitLane(track).loss)} s</p><p>📅 Aujourd’hui : ${SZ.fmtDate(SZ.today())} ${S.year} · GP le <b>${SZ.fmtDate(SZ.raceDate(S.round))}</b>${S.round ? (S.round === SZ.midRound() ? ' · retour de la pause estivale' : ` · ${SZ.daysAfter(S.round - 1)} jours après le GP précédent`) : ' · ouverture de la saison'}</p><p>Météo : ${wxText(S.wx)}</p><p>Toi : P${me} au championnat, ${myPts} pts${S.gridPen ? ` · <b class="bad">pénalité de ${S.gridPen} places sur la grille</b>` : ''}</p>` + (!S.round ? S.preLog || '' : '') + (career ? boardHTML() + missionHTML(S.mission) : '') + regHTML(S) + (S.round === SZ.midRound() ? newsHTML(S) : '');
    const mid = S.round === SZ.midRound() && career;
    $('szActions').innerHTML = (S.grid ? '' : `<div class="seg mini"><label>Format</label><div>${['sprint', 'long'].map(f => `<button data-cfmt="${f}" class="${settings.format === f ? 'sel' : ''}">${FORMATS[f].name}<small>~${FORMATS[f].minutes} min</small></button>`).join('')}</div></div>`) + (S.grid
      ? `<button class="main" id="szRace">Départ de la course (P${S.grid.indexOf('Toi') + 1} sur la grille)</button>`
      : `<button class="main" id="szQuali">Qualifications : réglages de la voiture →</button>`) +
      `<button id="szSimGP">⏩ Simuler le Grand Prix${S.grid ? '' : ' (qualifs + course)'}</button><p id="szSimProg" class="dim small"></p>` +
      (mid ? `<button id="szMarket">🔁 Mercato de mi-saison <small>(indemnités ×${String(MID_MULT).replace(".", ",")}, ferme au départ)</small></button>` : '');
    document.querySelectorAll('[data-cfmt]').forEach(b => b.onclick = () => { settings.format = b.dataset.cfmt; save(); openSeason(); });
    const ms = $('szMis'); if (ms) ms.onclick = () => { S.mission.acc = true; SZ.saveSeason(); openSeason(); };
    const mk = $('szMarket'); if (mk) mk.onclick = () => {
      show('setup');
      openMarket($('setup'), 'mid', name => { S.midSign = name || S.midSign; SZ.saveSeason(); openSeason(); }, () => openSeason());
    };
    const q = $('szQuali'); if (q) q.onclick = () => openTuning(S.wx, openSeason);
    // tout simuler d'un coup : qualifications puis course avec la stratégie conseillée
    $('szSimGP').onclick = () => {
      const k = curSetup(S.wx);
      if (!S.grid) {
        S.setup = { round: S.round, k };
        const rows = SZ.qualifying(track, simLap(S.wx, k), S.wx?.rain);
        S.quali = rows.map(r => ({ name: r.name, time: r.time })); S.grid = rows.map(r => r.name); SZ.saveSeason();
      }
      document.querySelectorAll('#szActions button').forEach(b => b.disabled = true);
      startRace({ season: true, gridOrder: S.grid, perfBonus: S.dev, wx: { ...S.wx }, sim: true, autopilot: true, setup: k });
    };
    const rc = $('szRace'); if (rc) rc.onclick = async () => { await enableTilt(settings.tilt); showPrerace({ season: true, gridOrder: S.grid, perfBonus: S.dev, wx: S.wx, setup: curSetup(S.wx) }); };
  }
  show('season');
  // événement survenu depuis le dernier GP : il faut trancher avant de continuer
  if (S.event && !S.done) showEvent(S.event, () => { S.event = null; SZ.saveSeason(); openSeason(); });
  // sacre de fin de saison
  if (S.done && career && !S.champCine) {
    const titles = [];
    if (SZ.driverStandings()[0].player) titles.push('Champion du monde des pilotes');
    if (SZ.teamStandings()[0].player) titles.push('Champion du monde des écuries');
    S.champCine = true; SZ.saveSeason();
    const wins = S.results.filter(r => r.pos === 1).length;
    if (titles.length) champCard($('nego'), { year: S.year, titles, line: `${wins} victoire${wins > 1 ? 's' : ''} · ${S.pts['Toi'] || 0} points · ${TEAMS[0].name}` }, () => {});
  }
}
const regHTML = S => S.reg?.length ? `<div class="reg">📜 Règlement ${S.year + 1} : nouvelles règles pour <b>${S.reg.map(treeName).join(' et ')}</b>. Toutes les écuries repartiront de zéro ${S.reg.length > 1 ? "dans ces domaines" : "dans ce domaine"} : tes pièces ${S.reg.length > 1 ? 'de ces pôles' : 'de ce pôle'} seront retirées à l’intersaison. Inutile d’y investir gros d’ici là !</div>` : '';
const newsHTML = S => S.news?.length ? `<div class="news"><b>📰 Mercato</b>${S.news.map(n => `<p>${n}</p>`).join('')}</div>` : '';
// intersaison : dotation (niveau de l'écurie + place au championnat), puis moteur et transferts
function offSeason(tp) {
  const S = SZ.season;
  if (!career) return setupSeason();
  career.lastPos = tp;
  if (S.bonus == null) {
    const tier = TIERS.find(t => t.id === career.tier);
    S.bonus = Math.round((tier.budget * 0.6 + (11 - tp) * 2.5) * 10) / 10;
    const w = renewStaff(S.year + 1);   // salaires des chefs encore sous contrat, départs des autres
    S.wages = w.total;
    S.offNote = w.gone.length ? `Fin de contrat : ${w.gone.join(', ')} quitte${w.gone.length > 1 ? 'nt' : ''} l’écurie, recrute au QG.` : '';
    team.budget = Math.round((team.budget + S.bonus - S.wages) * 10) / 10; saveTeam(); SZ.saveSeason();
  }
  show('setup');
  openMarket($('setup'), 'off', () => { applyRegulation(S.reg || []); SZ.newSeason(S.year + 1, true, (S.reg || []).length); SZ.recordPerf(playerPerf()); openSeason(); }, () => openSeason());
}
function showQuali(simTime) {
  paused = true;
  const time = simTime ?? (R && R.player.finished ? Math.min(R.player.lastLap, R.q?.pb0.lap || 1e9) : null), ai = simTime == null ? R?.q?.ai : null;   // séance jouée : mêmes temps des IA que le tour des temps
  if (R) { clearRace(); R = null; }
  const Q = ctx === 'solo' ? soloQ : SZ.season;
  const rows = SZ.qualifying(track, time, Q.wx?.rain, ai);
  Q.quali = rows.map(r => ({ name: r.name, time: r.time }));
  Q.grid = rows.map(r => r.name); if (Q === SZ.season) SZ.saveSeason();
  const pos = rows.findIndex(r => r.player) + 1;
  $('qTitle').textContent = `Qualifications · ${def.name} · ${pos === 1 ? 'Pole position !' : 'P' + pos}`;
  $('qTable').innerHTML = SZ.qualiHTML(rows);
  show('quali');
}
$('qRace').onclick = () => ctx === 'solo' ? showPrerace({ gridOrder: soloQ.grid, wx: soloQ.wx, setup: curSetup(soloQ.wx) }) : openSeason();
$('qRetry').onclick = async () => {
  const Q = ctx === 'solo' ? soloQ : SZ.season; Q.grid = null;
  const k = curSetup(Q.wx);
  if (ctx === 'solo' && settings.soloQuali === 'sim') return showQuali(simLap(Q.wx, k));
  startRace({ solo: true, wx: Q.wx, setup: k });
};
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

let garageFrom = 'menu';
function showGarage(from) { garageFrom = from; show('garage'); openGarage($('garage'), () => { refreshMenu(); show(garageFrom); }); }
$('garageBtn').onclick = () => showGarage('menu');
$('resGarageBtn').onclick = () => showGarage('results');
$('soundBtn').onclick = () => { settings.sound = !settings.sound; save(); initAudio(settings.sound); refreshMenu(); };
// remise à zéro complète : saison, écurie, garage, budget
function resetAll() {
  SZ.quitSeason(); resetCareer(); resetGarage(); resetInfra(); resetStaff(); resetTeam(); refreshMenu(); show('menu');
}
twoTap($('resetBtn'), resetAll); twoTap($('szReset'), resetAll);
$('absBtn').onclick = () => { settings.abs = !settings.abs; save(); refreshMenu(); };
$('tcBtn').onclick = () => { settings.tc = !settings.tc; save(); refreshMenu(); };
$('lineBtn').onclick = () => { settings.line = ((settings.line || 0) + 1) % 3; save(); refreshMenu(); };
$('tiltBtn').onclick = async () => { settings.tilt = !settings.tilt; save(); refreshMenu(); };
$('camSel').onclick = () => { toggleCam(); refreshMenu(); };
// ---------- écran d'accueil : carrière ou course solo ----------
async function goFull() { try { await document.documentElement.requestFullscreen?.(); await screen.orientation?.lock?.('landscape'); } catch {} }
$('careerBtn').onclick = async () => {
  ctx = 'career'; goFull();
  SZ.season ? openSeason() : setupSeason();
};
$('soloBtn').onclick = () => { ctx = 'solo'; showSolo(); };
$('settingsBtn').onclick = () => show('settings');
$('setBack').onclick = () => { refreshMenu(); show('menu'); };
$('soloBack').onclick = () => { refreshMenu(); show('menu'); };
function showSolo() {
  if (R) { clearRace(); R = null; }
  paused = true;
  if (def.id !== settings.circuit) loadCircuit(settings.circuit);
  showCircuitInfo(); refreshMenu(); show('solo');
}
const soloOpt = (attr, key) => document.querySelectorAll(`[data-${attr}]`).forEach(b => b.onclick = () => { settings[key] = b.dataset[attr]; save(); refreshMenu(); });
soloOpt('sfmt', 'soloFmt'); soloOpt('squali', 'soloQuali'); soloOpt('swx', 'soloWx');
// course solo : météo choisie, puis qualifs jouées ou simulées
let soloQ = null;
$('soloGo').onclick = async () => {
  ctx = 'solo';
  if (def.id !== settings.circuit) { clearTimeout(loadTimer); loadCircuit(settings.circuit); }
  const w = settings.soloWx;
  const wx = params.get('meteo') ? wxParam() : w === 'dry' ? { rain: false, change: null } : w === 'rain' ? { rain: true, change: null } : forecast(def.theme);
  soloQ = { wx, grid: null };
  goFull();
  openTuning(wx, showSolo);
};
// temps de qualification simulé, selon le réglage choisi (k)
const simLap = (wx, k = 2) => {
  const up = carUpgrades(), perf = TEAMS[0].perf * up.vtop, p0 = TEAMS[0].perf;
  return SZ.lapEstimate(track, 0.95, perf, up.acc) * (1 + 0.006 * Math.random()) * 0.985 * (wx.rain ? 1.12 : 1) * setupLap(track, up, k, wx.rain, p0) / setupLap(track, up, 2, wx.rain, p0);
};
// réglage de la voiture pour ce week-end : celui choisi avant les qualifs (parc fermé), sinon le conseil de l'ingénieur
function curSetup(wx) {
  const S = SZ.season, k = ctx === 'career' ? (S?.setup?.round === S?.round ? S.setup.k : null) : soloQ?.setup;
  return k ?? bestSetup(track, carUpgrades(), !!wx?.rain, TEAMS[0].perf);
}
const wxShort = wx => wx.change ? (wx.rain ? '🌦️ Éclaircie' : '🌦️ Averse') : wx.rain ? '🌧️ Pluie' : '☀️ Sec';
// réglages avant les qualifications : 5 préréglages, conseil de l'ingénieur, bandeau de séance (atelier 3D en fond)
function openTuning(wx, back) {
  const S = ctx === 'career' ? SZ.season : null, up = carUpgrades(), perf = TEAMS[0].perf;
  const date = S ? SZ.raceDate(S.round) : new Date(), temps = weekendTemps(def, date, wx.rain, S ? S.year * 100 + S.round : 0);
  const keep = k => { if (S) { S.setup = { round: S.round, k }; SZ.saveSeason(); } else soloQ.setup = k; };
  const play = async k => { keep(k); await enableTilt(settings.tilt); startRace({ solo: true, wx, setup: k }); };
  const sim = k => { keep(k); showQuali(simLap(wx, k)); };
  const simFirst = !S && settings.soloQuali === 'sim';
  showTuning($, {
    sub: `Qualifications · ${def.name} · ${S ? `${SZ.fmtDate(date)} ${S.year}` : 'course solo'}`,
    track, up, perf, wet: !!wx.rain, parc: true, k: S ? (S.setup?.round === S.round ? S.setup.k : null) : soloQ.setup,
    base: SZ.lapEstimate(track, 0.95, perf * up.vtop, up.acc) * 0.985 * (wx.rain ? 1.12 : 1),
    info: [['Piste', `${temps.track} °C`, temps.track >= 45 ? 'hot' : temps.track <= 22 ? 'cold' : ''], ['Air', `${temps.air} °C`], ['Météo', wxShort(wx)],
      ['État', wx.rain ? 'Mouillée' : 'Sèche', wx.rain ? 'bad' : 'ok'], [FORMATS[fmtKey()].name, `${raceFormat().laps} tours`], ['Arrêt', `≈ ${Math.round(pitLane(track).loss)} s`]],
    goLabel: simFirst ? '⏩ Simuler les qualifs' : 'Tour chrono →', simLabel: simFirst ? 'Tour chrono' : '⏩ Simuler',
  }, { go: simFirst ? sim : play, sim: simFirst ? play : sim, back });
  show('tuning');
}
$('camBtn').onclick = toggleCam;

// ---------- avant la course : météo, 3 stratégies proposées, réglage des pneus et du carburant ----------
const chip = t => `<b class="tchip" style="--c:${TYRES[t].color}">${t}</b>`;
function showPrerace(opts) {
  const wx = opts.wx || (params.get('meteo') ? wxParam() : forecast(def.theme));
  const { laps, lapTime } = raceFormat();
  showPreraceScreen($, { laps, lapKm: track.length / 1000, lapTime, wx, pitLoss: pitLane(track).loss, setup: SETUPS[opts.setup ?? 2].name, title: `${FORMATS[fmtKey()].name} · ${laps} tours · ~${Math.round(laps * lapTime * 1.12 / 60)} min` }, {
    go: plan => startRace({ ...opts, wx: { ...wx }, plan }),
    sim: plan => startRace({ ...opts, wx: { ...wx }, plan, sim: true, autopilot: true }),
    back: () => opts.season ? openSeason() : showSolo(),
  });
  show('prerace');
}
// stand et radio
const toggleBox = () => { if (R && !R.solo) boxToggle(R, R.player); };
$('boxBtn').onclick = toggleBox;
$('tyreChip').onclick = () => { if (R && !R.solo) radioOrder(R, 'tyre'); };
addEventListener('keydown', e => { if (e.target?.tagName === 'INPUT') return; if (e.code === 'KeyB') toggleBox(); if (e.code === 'KeyR') toggleRadio(); });
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
$('againBtn').onclick = () => R?.season ? openSeason() : showSolo();
function pause() { if (!R || R.done || paused) return; paused = true; show('pause'); }
document.addEventListener('visibilitychange', () => { if (document.hidden) pause(); });

// choix du circuit : aperçu immédiat, construction 3D après une courte pause
let loadTimer = 0;
function showCircuitInfo() {
  const k = Math.max(0, CIRCUITS.findIndex(c => c.id === settings.circuit));
  const d = CIRCUITS[k], t = buildTrack(d);
  $('circNum').textContent = String(k + 1).padStart(2, '0');
  $('circName').textContent = d.name;
  $('circDots').innerHTML = CIRCUITS.map((c, j) => `<i class="${j === k ? 'on' : ''}"></i>`).join('');
  let dz = 0; for (let i = 0; i < t.N; i++) dz = Math.max(dz, t.Y[i]);
  $('circInfo').textContent = `${d.type} · ${(t.length / 1000).toFixed(2)} km` + (dz > 15 ? ` · dénivelé ${Math.round(dz)} m` : '');
  const cv = $('circPrev'), c = cv.getContext('2d'), W = cv.width, H = cv.height;
  let a = Infinity, b = -Infinity, e = Infinity, f = -Infinity;
  for (let i = 0; i < t.N; i++) { a = Math.min(a, t.X[i]); b = Math.max(b, t.X[i]); e = Math.min(e, t.Z[i]); f = Math.max(f, t.Z[i]); }
  const sc = Math.min((W - 12) / (b - a), (H - 12) / (f - e));
  const P = i => [W / 2 - (t.X[i] - (a + b) / 2) * sc, H / 2 - (t.Z[i] - (e + f) / 2) * sc];
  c.clearRect(0, 0, W, H); c.lineWidth = 4; c.lineJoin = 'round'; c.strokeStyle = '#fff'; c.shadowColor = 'rgba(255,61,46,.8)'; c.shadowBlur = 8; c.beginPath();
  for (let i = 0; i < t.N; i += 3) { const [x, y] = P(i); i ? c.lineTo(x, y) : c.moveTo(x, y); }
  c.closePath(); c.stroke();
  c.shadowBlur = 0; const [x, y] = P(0); c.fillStyle = '#ff3d2e'; c.beginPath(); c.arc(x, y, 4, 0, 7); c.fill();
}
function pickCircuit(step) {
  const k = CIRCUITS.findIndex(c => c.id === settings.circuit);
  settings.circuit = CIRCUITS[(Math.max(0, k) + step + CIRCUITS.length) % CIRCUITS.length].id; save();
  showCircuitInfo(); refreshMenu();
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
