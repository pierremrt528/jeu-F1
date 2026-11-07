// Sons : moteurs = vrais enregistrements (sons/<motoriste>-<régime>.wav, voir sons/CREDITS.txt) : boucles à régime
// constant (ralenti / bas / moyen / haut) fondues l'une dans l'autre selon le régime, chacune légèrement accélérée
// autour de son régime d'origine ; accélérateur = plus fort et plus brillant, lever de pied = étouffé.
// Le reste est généré en Web Audio (crissements, vent, pluie, chocs, feux, radio). Repli synthétique si un fichier manque.
import { ENGINES, engineOf } from './career.js';
let ctx = null, master, nodes = null, on = true;
// couches de chaque motoriste : [fichier, régime nominal 0 (ralenti) → 1 (rupteur)] ; pitch = caractère global
const ENG = {
  valdrin: { pitch: 1.25, layers: [['idle', 0], ['low', 0.2], ['mid', 0.5], ['high', 0.85]] },
  orsetti: { pitch: 0.9, layers: [['idle', 0], ['mid', 0.5]] },
  hexadyne: { pitch: 1.05, layers: [['idle', 0], ['low', 0.25], ['mid', 0.55], ['high', 0.9]] },
  kiyora: { pitch: 1.15, layers: [['idle', 0], ['high', 0.7]] },
};
const bufs = {};
function loadSamples() {
  for (const id in ENG) for (const [l] of ENG[id].layers) fetch(`sons/${id}-${l}.wav`).then(r => r.ok ? r.arrayBuffer() : Promise.reject())
    .then(a => new Promise((ok, ko) => ctx.decodeAudioData(a, ok, ko))).then(b => { bufs[id + l] = b; }).catch(() => {});
}
const ready = id => ENG[id].layers.every(([l]) => bufs[id + l]);
// un groupe = un moteur : ses boucles -> passe-bas -> volume. Créé à la demande, arrêté quand on change de moteur.
function group(vol) {
  const f = ctx.createBiquadFilter(); f.type = 'lowpass'; f.frequency.value = 3000; f.Q.value = 0.7;
  const g = ctx.createGain(); g.gain.value = 0; f.connect(g).connect(master);
  return { f, g, vol, id: null, L: [] };
}
function useEngine(G, id) {
  if (G.id === id) return G.L.length > 0;
  for (const l of G.L) l.src.stop();
  G.L = []; G.id = null;
  if (!ready(id)) return false;
  for (const [l, pos] of ENG[id].layers) {
    const src = ctx.createBufferSource(), g = ctx.createGain(); src.buffer = bufs[id + l]; src.loop = true; g.gain.value = 0;
    src.connect(g).connect(G.f); src.start(0, Math.random() * src.buffer.duration); G.L.push({ src, g, pos });
  }
  G.id = id; return true;
}
// x = régime 0..1, th = accélérateur 0..1
function drive(G, x, th, vol) {
  const E = ENG[G.id], n = G.L.length, rpm = q => 0.3 + 0.7 * q;
  for (let i = 0; i < n; i++) {
    const l = G.L[i], a = i ? G.L[i - 1].pos : -1, b = i < n - 1 ? G.L[i + 1].pos : 2;
    let w = x <= l.pos ? (x - a) / (l.pos - a) : (b - x) / (b - l.pos);
    w = Math.max(0, Math.min(1, w)); w = Math.sin(w * Math.PI / 2); // fondu à puissance constante
    set(l.g.gain, w, 0.05);
    set(l.src.playbackRate, E.pitch * Math.max(0.75, Math.min(1.5, rpm(x) / rpm(l.pos))), 0.05);
  }
  set(G.f.frequency, 1200 + 7000 * (0.25 + 0.75 * th) * (0.5 + 0.5 * x));
  set(G.g.gain, vol * (0.45 + 0.55 * th));
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
    nodes.gme = group(0.5); nodes.goth = group(0.32);
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
  const real = useEngine(nodes.gme, myId), x = p.v < 0.5 ? 0 : 0.08 + 0.92 * r;
  if (real) drive(nodes.gme, x, p.throttle, p.out ? 0 : nodes.gme.vol);
  set(nodes.me.o1.frequency, base); set(nodes.me.o2.frequency, base / 2);
  set(nodes.me.f.frequency, 500 + 2200 * r * (0.4 + 0.6 * p.throttle));
  set(nodes.me.g.gain, p.out || real ? 0 : nodes.me.vol * (0.45 + 0.55 * p.throttle));
  // voiture la plus proche (pas le joueur)
  let best = null, bd = 1e9;
  for (const c of R.cars) { if (c === p || c.out) continue; const d = Math.hypot(c.x - camPos.x, c.z - camPos.z); if (d < bd) { bd = d; best = c; } }
  const near = best ? Math.max(0, 1 - bd / 60) : 0, oId = best && engId(best);
  const oReal = !!best && useEngine(nodes.goth, oId);
  if (oReal) drive(nodes.goth, best.v < 0.5 ? 0 : 0.08 + 0.92 * rev(best, 93 * best.perf), best.throttle ?? 1, nodes.goth.vol * near);
  else if (nodes.goth.id) set(nodes.goth.g.gain, 0);
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
