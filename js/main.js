// Point d'entrée : rendu Three.js, caméras, boucle de jeu, écrans.
import * as THREE from '../lib/three.module.min.js';
import { CIRCUITS } from './data.js';
import { buildTrack } from './track.js';
import { buildTrackMeshes, buildEnvironment } from './scenery.js';
import { setAniso } from './textures.js';
import { createRace, updateRace, results } from './race.js';
import { initInput, enableTilt, resetInput } from './input.js';
import { initHud, updateHud, fmt } from './hud.js';

const $ = id => document.getElementById(id);
const params = new URLSearchParams(location.search);
const settings = Object.assign({ cam: 'chase', tilt: false, laps: 3 }, load());
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
const def = CIRCUITS[0];
const track = buildTrack(def);
const { sky } = buildEnvironment(scene, renderer, track);
const { lights } = buildTrackMeshes(track, scene);
initHud(track);

let R = null, paused = true, last = performance.now();
const camState = { h: 0, x: 0, y: 0, z: 0 };

function startRace() {
  if (R) for (const c of R.cars) scene.remove(c.mesh);
  R = createRace(track, scene, +params.get('tours') || settings.laps, { autopilot: params.has('demo'), grid: +params.get('grille') || 10 });
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
    if (R.done && R.t - R.endT > 2.5) showResults();
  }
  updateCamera(dt);
  sky.position.copy(camera.position);
  renderer.render(scene, camera);
  // fps + qualité adaptative
  fpsAcc += dt; fpsN++;
  if (fpsAcc > 1) {
    fps = Math.round(fpsN / fpsAcc); fpsAcc = 0; fpsN = 0;
    if (!paused && fps < 45 && pixelRatio > 0.75) { if (++slowT >= 2) { pixelRatio = Math.max(0.75, pixelRatio - 0.25); renderer.setPixelRatio(pixelRatio);
renderer.toneMapping = THREE.ACESFilmicToneMapping; renderer.toneMappingExposure = 1.05;
setAniso(renderer.capabilities.getMaxAnisotropy()); resize(); slowT = 0; } }
    else slowT = 0;
  }
  if (!paused) updateHud(R, params.has('fps') ? fps + ' i/s · ' + renderer.info.render.calls + ' appels · ' + Math.round(renderer.info.render.triangles / 1000) + 'k tri' : 0);
}

// ---------- écrans ----------
function show(id) {
  for (const s of ['menu', 'hud', 'pause', 'results']) $(s).classList.toggle('hidden', s !== id);
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
  $('resultTitle').textContent = me.pos === 1 ? 'Victoire !' : me.pos <= 3 ? `Podium : P${me.pos}` : `Arrivée : P${me.pos}`;
  show('results');
}

function refreshMenu() {
  document.querySelectorAll('[data-laps]').forEach(b => b.classList.toggle('sel', +b.dataset.laps === settings.laps));
  $('tiltBtn').textContent = settings.tilt ? 'Inclinaison' : 'Boutons';
  $('camBtn').textContent = settings.cam === 'cockpit' ? '🎥 1re' : '🎥 3e';
  $('camSel').textContent = settings.cam === 'cockpit' ? 'Cockpit (1re pers.)' : 'Arrière (3e pers.)';
}
document.querySelectorAll('[data-laps]').forEach(b => b.onclick = () => { settings.laps = +b.dataset.laps; save(); refreshMenu(); });
$('tiltBtn').onclick = async () => { settings.tilt = !settings.tilt; save(); refreshMenu(); };
$('camSel').onclick = () => { toggleCam(); refreshMenu(); };
$('startBtn').onclick = async () => {
  await enableTilt(settings.tilt);
  try { await document.documentElement.requestFullscreen?.(); await screen.orientation?.lock?.('landscape'); } catch {}
  startRace();
};
$('camBtn').onclick = toggleCam;
$('pauseBtn').onclick = () => pause();
$('resumeBtn').onclick = () => { paused = false; last = performance.now(); show('hud'); };
$('restartBtn').onclick = startRace;
$('quitBtn').onclick = $('menuBtn').onclick = () => { paused = true; show('menu'); };
$('againBtn').onclick = startRace;
function pause() { if (!R || R.done || paused) return; paused = true; show('pause'); }
document.addEventListener('visibilitychange', () => { if (document.hidden) pause(); });

$('circuitName').textContent = `${def.name} · ${def.type} · ${(track.length / 1000).toFixed(2)} km`;
initInput(toggleCam, pause);
refreshMenu();
setFov();
// vue d'attente du menu : survol du circuit
camera.position.set(track.X[0] - 60, track.Y[0] + 40, track.Z[0] - 90); camera.lookAt(track.X[0] + 80, track.Y[0], track.Z[0] + 40);
show('menu');
if (params.has('demo')) startRace();
requestAnimationFrame(loop);
