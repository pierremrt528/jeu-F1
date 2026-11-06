// Étape 6 : pneus (gommes, usure), météo (pluie, piste mouillée), arrêts au stand, dégâts et casse.
import { DS } from './track.js';

// life : part de la distance de course avant usure complète (1 = toute la course)
export const TYRES = {
  S: { name: 'Tendre', color: '#ff3b3b', grip: 1.06, life: 0.6 },
  M: { name: 'Medium', color: '#ffd400', grip: 1.0, life: 1.0 },
  H: { name: 'Dur', color: '#f2f2f2', grip: 0.955, life: 1.6 },
  W: { name: 'Pluie', color: '#2f8cff', grip: 0.86, life: 1.0 },
};

// ---------- météo ----------
// rain : pluie au départ ; change : { at (part de la course), rain } averse qui arrive ou s'arrête
export function forecast(theme, rnd = Math.random) {
  if (theme.surface === 'sand') return { rain: false, change: null };
  const r = rnd();
  if (r < 0.55) return { rain: false, change: null };
  if (r < 0.7) return { rain: true, change: null };
  if (r < 0.88) return { rain: false, change: { at: 0.25 + rnd() * 0.4, rain: true } };
  return { rain: true, change: { at: 0.3 + rnd() * 0.35, rain: false } };
}
export function wxText(wx) {
  if (!wx.change) return wx.rain ? '🌧️ Pluie' : '☀️ Sec';
  const p = Math.round(wx.change.at * 100);
  return wx.rain ? `🌦️ Pluie, éclaircie vers ${p} % de la course` : `🌦️ Sec, averse attendue vers ${p} % de la course`;
}

// adhérence des pneus selon la gomme, l'usure et l'humidité de la piste (0 sec … 1 détrempée)
export function tyreGrip(car, wet) {
  const T = TYRES[car.tyre], w = Math.min(car.wear, 1);
  let g = (T.grip - (car.tyre === 'W' ? 0.04 * wet : 0)) * (1 - 0.16 * w * w);
  if (car.wear > 1) g *= Math.max(0.7, 1 - (car.wear - 1) * 0.6);   // pneus à bout
  if (car.tyre !== 'W') g *= 1 - 0.45 * wet;
  return g;
}

// ---------- préparation de la course ----------
export function setupStrategy(R, track, wx, playerTyre) {
  R.wx = wx; R.raining = wx.rain; R.wet = wx.rain ? 0.9 : 0;
  R.raceDist = Math.max(R.laps, 3) * track.N * DS;
  R.radio = []; R.radioT = 0;
  const failP = c => c.player ? Math.min(0.2, Math.max(0.005, (100 - c.rel) * 0.0025)) : 0.035;
  for (const c of R.cars) {
    c.tyre = R.wet > 0.4 ? 'W' : 'M'; c.wear = 0; c.dmg = 0; c.stops = 0; c.pit = null; c.box = null;
    c.strat = c.stats?.strat ?? 70; c.tyreSkill = c.stats?.tyre ?? 75;
    if (!R.wet && !R.solo) c.tyre = R.laps >= 3 ? (Math.random() < 0.55 ? 'S' : Math.random() < 0.7 ? 'M' : 'H') : 'S';
    if (c.player && playerTyre) c.tyre = playerTyre;
    if (R.solo) c.tyre = R.wet > 0.4 ? 'W' : 'S';
    // casse : tirée au départ, survient à un moment aléatoire de la course
    c.fail = !R.solo && Math.random() < failP(c) * Math.min(1.5, R.laps / 3) ? { at: 0.1 + Math.random() * 0.8, kind: Math.random() < 0.5 ? 'dnf' : 'power' } : null;
  }
}

// part de la course parcourue
const progress = (R, c) => Math.max(0, ((c.lap - 1) * R.track.N + c.idx) / (R.laps * R.track.N));

// gomme conseillée pour finir la course (ou la pluie)
export function bestTyre(R, rem) {
  if (R.wet > 0.4 || (R.raining && R.wet > 0.15)) return 'W';
  const dist = rem * R.laps * R.track.N * DS / R.raceDist;
  return dist <= 0.6 ? 'S' : dist <= 1 ? 'M' : 'H';
}

// faut-il rentrer au stand à ce tour ? (IA et conseils du muret)
export function wantBox(R, c) {
  if (c.lap >= R.laps || c.lap < 1) return null;
  const rem = 1 - progress(R, c);
  const wetNeed = R.wet > 0.45 - (c.strat - 70) / 400, dryNeed = R.wet < 0.2 && !R.raining;
  if (c.tyre !== 'W' && wetNeed) return 'W';
  if (c.tyre === 'W' && dryNeed && rem > 0.2) return bestTyre(R, rem);
  const life = TYRES[c.tyre].life * R.raceDist / (R.laps * R.track.N * DS);
  if (c.wear + rem / life > 1.1 && rem > 0.12) return bestTyre(R, rem);
  if (c.dmg > 0.5 && rem > 0.25) return c.tyre === 'W' ? 'W' : bestTyre(R, rem);
  return null;
}

// ---------- pendant la course ----------
const say = (R, txt) => { R.radio.push(txt); };
export function updateStrategy(R, dt) {
  const { track } = R, N = track.N;
  // météo : l'averse arrive ou s'arrête selon la progression du leader
  const lead = R.order[0];
  if (R.wx.change && !R.wx.done && lead && progress(R, lead) >= R.wx.change.at) {
    R.wx.done = true; R.raining = R.wx.change.rain;
    say(R, R.raining ? '📻 Muret : la pluie arrive ! Surveille la piste.' : '📻 Muret : la pluie s’arrête, la piste va sécher.');
  }
  if (R.wx.change && !R.wx.warned && lead && progress(R, lead) >= R.wx.change.at - 0.12) {
    R.wx.warned = true; if (R.wx.change.rain) say(R, '📻 Muret : averse prévue dans moins d’un tour.');
  }
  R.wet = Math.max(0, Math.min(1, R.wet + (R.raining ? dt / 35 : -dt / 90)));

  for (const c of R.cars) {
    if (c.out) continue;
    // usure : distance parcourue, glissades, gestion des pneus du pilote, gomme pluie sur le sec
    const life = TYRES[c.tyre].life * R.raceDist * (R.solo ? 4 : 1);
    let k = (c.sliding ? 2.5 : 1) * (1.3 - c.tyreSkill / 250) * (c.player ? 1 : 0.92);
    if (c.tyre === 'W') k *= 1 + 2.4 * (1 - R.wet);
    c.wear += Math.abs(c.v) * dt / life * k;
    c.gripMul = tyreGrip(c, R.wet) * (1 - 0.06 * c.dmg);
    c.dmgKd = 1 - 0.45 * c.dmg;
    // casse
    if (c.fail && !c.failed && progress(R, c) >= c.fail.at) {
      c.failed = true;
      if (c.fail.kind === 'dnf') { c.dnf = true; if (c.player) say(R, '📻 Muret : casse moteur… on doit abandonner. Désolé.'); }
      else { c.powerLoss = 0.05; if (c.player) say(R, '📻 Muret : perte de puissance moteur, on continue en mode dégradé.'); }
    }
    if (c.dnf) {
      c.throttle = 0; c.brake = 0.6;
      if (c.v < 1) { c.out = true; c.mesh.visible = false; c.v = 0; }
      continue;
    }
    // demande d'arrêt : IA (ou joueur en pilote auto) à l'approche de la ligne
    const zone = c.idx > N - 140 && c.idx < N - 125;
    if (zone && !c.pit && (!c.player || R.autopilot) && !c.box) { const t = wantBox(R, c); if (t) c.box = t; }
    if (zone && !c.pit && c.box && c.lap < R.laps && c.lap >= 1) {
      c.frac = 0;
      c.pit = { phase: 'in', s: 0, stopAt: (N - c.idx + 8) * DS, stop: 2.3 + Math.random() * 0.8 + (c.dmg > 0.15 ? 3 : 0) };
      if (c.player) say(R, `📻 Muret : box, box ! On met des ${TYRES[c.box].name.toLowerCase()}s.`);
    }
    // conseils du muret pour le joueur
    if (c.player && !R.autopilot && zone && !c.pit && !c.adv) {
      const t = wantBox(R, c);
      if (t) { c.adv = true; say(R, `📻 Muret : rentre ce tour, pneus ${TYRES[t].name} conseillés (bouton STAND).`); }
    }
    if (c.player && c.idx > 200 && c.idx < 210) c.adv = false;
  }
  if (R.radio.length) { R.radioMsg = R.radio.shift(); R.radioT = 5; }
  R.radioT -= dt;
}

// dégâts : appelé après un choc contre un mur (impact 0..1) ou une voiture
export function addDamage(R, c, amount) {
  if (amount < 0.03 || R.solo) return;
  const before = c.dmg;
  c.dmg = Math.min(1, c.dmg + amount);
  if (c.player && before < 0.15 && c.dmg >= 0.15) say(R, '📻 Muret : aileron avant endommagé, tu perds de l’appui. Un arrêt le réparera.');
}

// ---------- voie des stands (déplacement imposé, limiteur 80 km/h) ----------
const VPIT = 22;
export function pitStep(R, c, dt) {
  const { track } = R, N = track.N, p = c.pit;
  const pitLat = track.width / 2 + Math.max(1.4, Math.min(track.runoff - 1.6, 5));
  const prev = c.idx;
  if (p.phase === 'stop') {
    c.v = 0; p.stop -= dt;
    if (p.stop <= 0) {
      c.tyre = c.box; c.wear = 0; c.dmg = 0; c.stops++; c.box = null; p.phase = 'out'; p.s = 0;
    }
  } else {
    c.v = c.v > VPIT ? Math.max(VPIT, c.v - 30 * dt) : Math.min(VPIT, c.v + 8 * dt);
    const ds = c.v * dt; p.s += ds;
    let f = c.idx + c.frac + ds / DS;
    if (p.phase === 'in' && p.s >= p.stopAt) p.phase = 'stop';
    c.idx = Math.floor(f) % N; c.frac = f - Math.floor(f);
    if (p.phase === 'out' && c.idx >= 70 && c.idx < N / 2) { c.pit = null; c.lane = c.lat; }
  }
  // décalage latéral vers la voie des stands puis retour en piste
  const target = p.phase === 'out' && c.idx > 40 ? 0 : pitLat;
  c.lat += (target - c.lat) * Math.min(1, dt * 1.6);
  const i = c.idx, j = (i + 1) % N, t = c.frac || 0;
  const x = track.X[i] + (track.X[j] - track.X[i]) * t, z = track.Z[i] + (track.Z[j] - track.Z[i]) * t;
  c.x = x - track.TZ[i] * c.lat; c.z = z + track.TX[i] * c.lat; c.y = track.Y[i];
  c.heading = track.H[i]; c.pitch = 0; c.throttle = 0; c.brake = 0; c.sliding = false; c.offTrack = false;
  return prev;
}
