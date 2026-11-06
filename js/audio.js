// Sons : moteurs = vrais enregistrements (sons/*.wav, un par motoriste, voir sons/CREDITS.txt) joués en boucle
// et accélérés selon le régime ; le reste est généré en Web Audio (crissements, vent, pluie, chocs, feux, radio).
// Si un fichier manque, le moteur synthétique d'origine prend le relais.
import { ENGINES, engineOf } from './career.js';
let ctx = null, master, nodes = null, on = true;
// vitesse de lecture au ralenti et au rupteur (l'enregistrement est à régime moyen-haut)
const SAMPLE = { valdrin: [0.55, 1.35], orsetti: [0.5, 1.25], hexadyne: [0.55, 1.3], kiyora: [0.6, 1.4] };
const bufs = {};
function loadSamples() {
  for (const id in SAMPLE) fetch(`sons/${id}.wav`).then(r => r.ok ? r.arrayBuffer() : Promise.reject())
    .then(a => new Promise((ok, ko) => ctx.decodeAudioData(a, ok, ko))).then(b => { bufs[id] = b; }).catch(() => {});
}
// une voix échantillonnée : boucle -> passe-bas -> volume ; démarrée dès que le fichier est décodé
function voice(id, vol) {
  const f = ctx.createBiquadFilter(); f.type = 'lowpass'; f.frequency.value = 3000;
  const g = ctx.createGain(); g.gain.value = 0; f.connect(g).connect(master);
  return { id, f, g, vol, src: null };
}
function feed(v) {
  if (v.src || !bufs[v.id]) return !!v.src;
  const s = ctx.createBufferSource(); s.buffer = bufs[v.id]; s.loop = true;
  s.connect(v.f); s.start(0, Math.random() * s.buffer.duration); v.src = s; return true;
}
const engId = c => c.player || c.teamIdx === 0 ? (engineOf()?.id || 'orsetti') : ENGINES[(c.teamIdx || 0) % ENGINES.length].id;

function noiseBuffer() {
  const b = ctx.createBuffer(1, ctx.sampleRate * 2, ctx.sampleRate), d = b.getChannelData(0);
  for (let i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1;
  return b;
}
function noise(type, freq, q = 1) {
  const s = ctx.createBufferSource(); s.buffer = nodes.nb; s.loop = true;
  const f = ctx.createBiquadFilter(); f.type = type; f.frequency.value = freq; f.Q.value = q;
  const g = ctx.createGain(); g.gain.value = 0;
  s.connect(f).connect(g).connect(master); s.start();
  return { f, g };
}
function engine(vol) {
  const o1 = ctx.createOscillator(), o2 = ctx.createOscillator();
  o1.type = 'sawtooth'; o2.type = 'square';
  const f = ctx.createBiquadFilter(); f.type = 'lowpass'; f.frequency.value = 900; f.Q.value = 3;
  const g = ctx.createGain(); g.gain.value = 0;
  const g2 = ctx.createGain(); g2.gain.value = 0.5;
  o1.connect(f); o2.connect(g2).connect(f); f.connect(g).connect(master);
  o1.start(); o2.start();
  return { o1, o2, f, g, vol };
}

// à appeler depuis un geste de l'utilisateur (règle des navigateurs)
export function initAudio(enabled) {
  on = enabled;
  if (!on) { if (master) master.gain.value = 0; return; }
  if (!ctx) {
    const AC = window.AudioContext || window.webkitAudioContext; if (!AC) return;
    ctx = new AC();
    master = ctx.createGain(); master.gain.value = 0; master.connect(ctx.destination);
    nodes = { nb: noiseBuffer() };
    nodes.me = engine(0.22); nodes.other = engine(0.12);
    nodes.sme = {}; nodes.soth = {};
    for (const id in SAMPLE) { nodes.sme[id] = voice(id, 0.5); nodes.soth[id] = voice(id, 0.3); }
    loadSamples();
    nodes.screech = noise('bandpass', 1500, 6);
    nodes.wind = noise('lowpass', 500);
    nodes.rain = noise('highpass', 2500);
  }
  ctx.resume?.();
}
export function setSound(enabled) { on = enabled; if (!on && master) master.gain.value = 0; }

function blip(freq, dur, vol = 0.2, type = 'sine', when = 0) {
  if (!ctx || !on) return;
  const t = ctx.currentTime + when, o = ctx.createOscillator(), g = ctx.createGain();
  o.type = type; o.frequency.value = freq;
  g.gain.setValueAtTime(vol, t); g.gain.exponentialRampToValueAtTime(0.001, t + dur);
  o.connect(g).connect(master); o.start(t); o.stop(t + dur + 0.05);
}
function thump(vol) {
  if (!ctx || !on) return;
  const s = ctx.createBufferSource(); s.buffer = nodes.nb;
  const f = ctx.createBiquadFilter(); f.type = 'lowpass'; f.frequency.value = 300;
  const g = ctx.createGain(), t = ctx.currentTime;
  g.gain.setValueAtTime(vol, t); g.gain.exponentialRampToValueAtTime(0.001, t + 0.35);
  s.connect(f).connect(g).connect(master); s.start(t, Math.random()); s.stop(t + 0.4);
}

const set = (p, v, k = 0.06) => p.setTargetAtTime(v, ctx.currentTime, k);
// régime moteur : même modèle de rapports que le HUD
function rev(car, vtop) {
  const f = Math.max(0, car.v) / vtop, T = [0, 0.16, 0.27, 0.38, 0.49, 0.60, 0.71, 0.83, 1.05];
  let k = 0; while (k < T.length - 2 && f > T[k + 1]) k++;
  return Math.min(1, (f - T[k]) / (T[k + 1] - T[k]));
}
let lastLights = 0, lastRadio = '', lastHit = 0;
export function updateAudio(R, camPos, active) {
  if (!ctx || !nodes) return;
  set(master.gain, on && active ? 0.9 : 0, 0.15);
  if (!R || !active) return;
  const p = R.player, vtop = 93 * p.perf;
  // moteur du joueur
  const r = rev(p, vtop), base = p.v < 0.5 ? 55 : 80 + 230 * r, myId = engId(p);
  let real = false;
  for (const id in nodes.sme) {
    const v = nodes.sme[id], mine = id === myId && feed(v); real ||= mine;
    if (!v.src) continue;
    const [lo, hi] = SAMPLE[id], rr = p.v < 0.5 ? 0 : 0.12 + 0.88 * r;
    set(v.src.playbackRate, lo + (hi - lo) * rr);
    set(v.f.frequency, 900 + 6000 * (0.35 + 0.65 * p.throttle));
    set(v.g.gain, mine && !p.out ? v.vol * (0.5 + 0.5 * p.throttle) : 0);
  }
  set(nodes.me.o1.frequency, base); set(nodes.me.o2.frequency, base / 2);
  set(nodes.me.f.frequency, 500 + 2200 * r * (0.4 + 0.6 * p.throttle));
  set(nodes.me.g.gain, p.out || real ? 0 : nodes.me.vol * (0.45 + 0.55 * p.throttle));
  // voiture la plus proche (pas le joueur)
  let best = null, bd = 1e9;
  for (const c of R.cars) { if (c === p || c.out) continue; const d = Math.hypot(c.x - camPos.x, c.z - camPos.z); if (d < bd) { bd = d; best = c; } }
  const near = best ? Math.max(0, 1 - bd / 60) : 0, oId = best && engId(best);
  let oReal = false;
  for (const id in nodes.soth) {
    const v = nodes.soth[id], it = id === oId && feed(v); oReal ||= it;
    if (!v.src) continue;
    if (it) { const [lo, hi] = SAMPLE[id], rb = rev(best, 93 * best.perf); set(v.src.playbackRate, lo + (hi - lo) * (0.12 + 0.88 * rb)); }
    set(v.g.gain, it ? v.vol * near : 0);
  }
  if (best) {
    const rb = rev(best, 93 * best.perf), fb = best.v < 0.5 ? 50 : 85 + 220 * rb;
    set(nodes.other.o1.frequency, fb); set(nodes.other.o2.frequency, fb / 2);
    set(nodes.other.f.frequency, 400 + 1500 * rb);
  }
  set(nodes.other.g.gain, oReal ? 0 : nodes.other.vol * near);
  // crissements, vent, pluie
  set(nodes.screech.g.gain, p.sliding && p.v > 12 ? 0.08 : 0, 0.04);
  set(nodes.wind.g.gain, Math.min(0.12, Math.abs(p.v) / 700));
  set(nodes.rain.g.gain, R.raining ? 0.06 : 0, 0.8);
  // choc
  if (p.hitWall > 0.35 && R.t - lastHit > 0.5) { lastHit = R.t; thump(0.6); }
  if (p.bump > 0.55 && R.t - lastHit > 0.5) { lastHit = R.t; thump(0.35); }
  // feux : un bip par feu, un plus aigu au départ
  if (R.phase === 'lights' && R.lightsOn > lastLights) blip(660, 0.18, 0.18, 'square');
  if (R.phase !== 'lights' && lastLights > 0) blip(1320, 0.35, 0.2, 'square');
  lastLights = R.phase === 'lights' ? R.lightsOn : 0;
  // radio du muret
  if (R.radioT > 0 && R.radioMsg && R.radioMsg !== lastRadio) { blip(1200, 0.07, 0.12); blip(900, 0.09, 0.12, 'sine', 0.09); }
  lastRadio = R.radioT > 0 ? R.radioMsg : '';
}
export function suspendAudio() { if (master) set(master.gain, 0, 0.05); }
