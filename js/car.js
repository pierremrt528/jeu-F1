// Monoplace : état et physique (pneus progressifs, transfert de masse, appui aéro, aides) (le modèle 3D est dans carmodel.js).
import * as THREE from '../lib/three.module.min.js';
import { locate } from './track.js';
import { buildCarMesh } from './carmodel.js';

// Unités : mètres, secondes. vtop ≈ 335 km/h.
export const SPEC = {
  vtop: 93, acc: 12.5, brake0: 17, brakeK: 0.0032, mu: 1.65, kd: 0.0034,
  wb: 3.6, steerMax: 0.34, slip: 0.07,
};

export class Car {
  constructor(team, driver, perf) {
    this.team = team; this.name = driver.name; this.player = !!driver.player;
    this.skill = driver.skill || 1; this.perf = perf;
    this.x = 0; this.y = 0; this.z = 0; this.heading = 0; this.pitch = 0;
    this.v = 0; this.idx = 0; this.lat = 0; this.beta = 0; this.ax = 0; this.ay = 0; this.spinT = 0;
    this.steer = 0; this.throttle = 0; this.brake = 0;
    this.lap = 0; this.lapStart = 0; this.lastLap = 0; this.bestLap = 0; this.finished = false; this.finishTime = 0;
    this.slipstream = 0; this.offTrack = false;
    this.lane = 0; this.laneBias = 0; this.passSide = 0; this.passTimer = 0;
    this.mesh = buildCarMesh(team.color, team.accent, driver.player ? 0xffc21a : (driver.helmet || team.accent));
  }
  get gear() {
    if (this.v < -0.5) return 'R';
    if (this.v < 0.5) return 'N';
    const f = this.v / (SPEC.vtop * this.perf);
    const T = [0.16, 0.27, 0.38, 0.49, 0.60, 0.71, 0.83];
    let g = 1; for (const t of T) if (f > t) g++;
    return g;
  }
}

export const NO_UP = { vtop: 1, acc: 1, brake: 1, mu: 1, kd: 1, steer: 1, slip: 1, off: 1, wall: 1, rel: 0, tyre: 1, fuel: 1, pit: 0 };
// aides au pilotage : les IA ont ABS, antipatinage et contrôle de stabilité
export const AI_ASSIST = { abs: true, tc: true, stab: true, line: 0 };

// courbe de pneu : linéaire, sommet arrondi, puis l'adhérence baisse quand le pneu glisse franchement
function tyreCurve(r) {
  if (r <= 0.75) return r;
  const s = 0.75 + 0.25 * Math.tanh((r - 0.75) / 0.25);
  return s * (1 - 0.15 * Math.min(1, Math.max(0, r - 1.15) / 0.8));
}
const clamp = (x, a, b) => x < a ? a : x > b ? b : x;

export function stepCar(car, track, dt) {
  const g = 9.81, N = track.N, i0 = car.idx;
  let v = car.v;
  const half = track.width / 2, alat = Math.abs(car.lat);
  car.offTrack = alat > half + 1.3;
  const u = car.up || NO_UP;   // améliorations du garage (multiplicateurs)
  const as = car.assist || AI_ASSIST;
  // surface : vibreurs (mêmes zones que le décor), petites bosses fixes le long de la piste
  const kerb = Math.abs(track.C[i0]) > 1 / 220 && alat > half - 0.1 && alat < half + 1.4;
  car.kerb = kerb && v > 8 ? 1 : 0;
  const bump = Math.sin(i0 * 0.73 + 1.3) * Math.sin(i0 * 0.191) * Math.sin(i0 * 0.047 + 0.5);
  // relief : sur une crête la voiture s'allège, dans une compression elle s'écrase
  const vc = (track.Y[(i0 - 6 + N) % N] - 2 * track.Y[i0] + track.Y[(i0 + 6) % N]) / 144;
  const nz = clamp(1 + v * v * vc / g, 0.45, 1.6);
  const grip = (car.offTrack ? 0.55 * u.off : 1) * (kerb ? 0.9 : 1) * (1 + 0.03 * bump * Math.min(1, v / 60)) * (car.gripMul ?? 1);
  const mass = car.mass || 1;   // carburant : plus lourd = moins d'accélération, de freinage et d'adhérence
  const vtop = SPEC.vtop * car.perf * u.vtop * (car.powerMul || 1) * (1 - (car.powerLoss || 0)) * (1 - 0.02 * (car.dmg || 0)) * (car.rwOff ? 1.03 : 1) * (1 + SPEC.slip * u.slip * car.slipstream) * (car.offTrack ? 0.5 : 1);

  // adhérence par essieu : transfert de masse (freinage → avant, accélération → arrière),
  // appui aéro (réduit dans le sillage d'une voiture, par un aileron abîmé), relief
  const lt = clamp(-(car.ax || 0) / 40, -0.35, 0.35);
  const aero = SPEC.kd * u.kd * (1 - 0.3 * (car.wake || 0)) * v * v;
  const muB = SPEC.mu * u.mu * grip / (1 + (mass - 1) * 0.25);
  const roll = Math.min(1, Math.abs(car.ay || 0) / 45);
  const capF = Math.max(0.5, muB * (g * nz * Math.pow(1 + lt, 0.85) + aero * (car.dmgKd ?? 1)) * (1 - 0.05 * roll * roll));
  let capR = Math.max(0.5, muB * (g * nz * Math.pow(1 - lt, 0.85) + aero * (car.rwKd ?? 1)) * (1 - 0.05 * roll * roll));

  // longitudinal : traction (roues arrière) et freinage limités par l'adhérence
  let a = -Math.sign(v) * (0.8 + 0.0009 * v * v), spin = 0, lock = false;
  car.absOn = car.tcOn = false;
  if (car.throttle > 0 && v >= -0.5) {
    let ad = car.throttle * SPEC.acc * car.perf * u.acc * (car.accMul || 1) / mass * (1 - Math.pow(Math.max(v, 0) / vtop, 2.2));
    const uy = Math.min(0.95, Math.abs(car.ay || 0) / capR);
    const room = Math.max(capR * 0.2, capR * 0.65 * Math.sqrt(1 - uy * uy));   // seules les roues arrière motrices
    if (ad > room) {
      if (as.tc) { ad = room * 0.95; car.tcOn = true; }
      else { spin = Math.min(1, (ad - room) / capR); ad = room * 0.85 + (ad - room) * 0.2; }   // patinage
    }
    a = ad;
  }
  if (car.offTrack) a -= v * 0.35;
  if (car.brake > 0) {
    if (v > 0.3) {
      let ab = car.brake * (SPEC.brake0 + SPEC.brakeK * v * v) * u.brake / Math.sqrt(mass);
      const lim = 1.1 * capF, uy = Math.min(0.95, Math.abs(car.ay || 0) / capF);
      const room = lim * Math.sqrt(1 - uy * uy);
      if (ab > room) {
        if (as.abs) { ab = room * 0.97; car.absOn = true; }
        else { ab = lim * 0.78; lock = v > 4; }   // roues bloquées : on freine moins et on ne tourne plus
      }
      a -= ab;
    } else if (car.throttle === 0 && car.canReverse) a = -car.brake * 5; // marche arrière lente pour se dégager
  }
  // pente : dénivelé entre l'échantillon courant et le suivant
  const slope = (track.Y[(i0 + 1) % N] - track.Y[i0]) / 2;
  const fwd = Math.cos(car.heading - track.H[i0]);
  a -= g * slope * fwd;
  const v0 = v;
  v += a * dt;
  if (v < -6) v = -6;
  if (v < 0 && !car.canReverse) v = 0;
  if (Math.abs(v) < 0.05 && car.throttle === 0 && car.brake === 0) v = 0;

  // latéral : la voiture avance selon `course`, elle pointe selon heading = course + beta (angle de dérive)
  let beta = car.beta || 0;
  let course = car.heading - beta;
  let yawO = 0, under = 0;
  if (car.spinT > 0) {
    // tête-à-queue : la voiture tourne sur elle-même et ralentit fortement
    car.spinT -= dt; beta += car.spinDir * 5.5 * dt; v *= Math.exp(-1.8 * dt);
    if (car.spinT <= 0) { course += beta; beta = 0; }
  } else {
    const delta = SPEC.steerMax * u.steer * car.steer / (1 + Math.abs(v) / 20) * (lock ? 0.25 : 1);
    let yaw = v * Math.tan(delta) / SPEC.wb;
    // avant : sous-virage progressif (le nez élargit quand on dépasse l'adhérence)
    const want = Math.abs(v * yaw), rF = want / (capF * 1.06);
    if (want > 0.01) { const got = capF * 1.06 * tyreCurve(rF); under = Math.max(0, rF - 1); yaw *= got / want; }
    // arrière : survirage si l'adhérence restante (après traction/freinage, patinage) ne suffit pas
    const ay = Math.abs(v * yaw);
    const ux = Math.min(as.tc ? 0.8 : 0.97, Math.max(0, a) / (capR * 0.65) + (car.brake > 0 ? 0.35 * car.brake * Math.min(1, -a / (1.1 * capF)) : 0));
    const capRl = capR * 1.06 * (1 - 0.6 * spin) * Math.sqrt(1 - ux * ux);
    if (v > 6 && ay > capRl) yawO = Math.sign(yaw) * (ay - capRl) / v * (as.stab ? 0.35 : 1);
    // vibreurs à pleine vitesse : à-coups dans le volant
    if (kerb && v > 25) beta += (Math.random() - 0.5) * 0.05 * Math.min(1, v / 60) * Math.sqrt(dt * 60) * (as.stab ? 0.5 : 1);
    beta -= yawO * dt;
    // l'arrière raccroche (plus vite avec de la marge d'adhérence, en contre-braquant, ou avec l'ESP)
    const margin = Math.max(0, capRl - ay) / capR;
    const counter = Math.max(0, Math.sign(beta) * car.steer);   // braquer dans le sens de la glisse = contre-braquer
    const rec = (1.6 + 5 * margin + 6 * counter) * (as.stab ? 2.5 : 1) * (v < 6 ? 4 : 1);
    beta -= beta * Math.min(1, rec * dt);
    course -= yaw * dt;
    if (Math.abs(beta) > 1.15 && v > 12) { car.spinT = 1.4; car.spinDir = Math.sign(beta); }
    car.ay += (v * (yaw + yawO) - (car.ay || 0)) * Math.min(1, dt * 10);
    v -= Math.sign(v) * (Math.min(want - capF * 1.06 * tyreCurve(rF), 40) * 0.1 * (rF > 1 ? 1 : 0) + Math.abs(Math.sin(beta)) * Math.abs(v) * 0.8) * dt;
  }
  if (car.spinT > 0) car.ay = 0;
  car.ax = (car.ax || 0) + ((v - v0) / dt - (car.ax || 0)) * Math.min(1, dt * 8);
  car.lock = lock;
  car.slip = Math.max(under, Math.abs(beta) * 3, lock ? 0.6 : 0, spin, car.spinT > 0 ? 0.8 : 0);
  car.sliding = car.slip > 0.08 && Math.abs(v) > 5;
  car.beta = beta;
  car.heading = course + beta;
  car.v = v;
  car.x += Math.sin(course) * v * dt;
  car.z += Math.cos(course) * v * dt;

  // position sur la piste
  const prev = car.idx;
  car.idx = locate(track, car.x, car.z, car.idx);
  const i = car.idx, rx = -track.TZ[i], rz = track.TX[i];
  const dx = car.x - track.X[i], dz = car.z - track.Z[i];
  car.lat = dx * rx + dz * rz;
  const along = dx * track.TX[i] + dz * track.TZ[i];
  const j = along >= 0 ? (i + 1) % N : (i - 1 + N) % N;
  const t = Math.min(1, Math.abs(along) / 2);
  car.y = track.Y[i] + (track.Y[j] - track.Y[i]) * t;
  car.pitch = -Math.atan((track.Y[(i + 1) % N] - track.Y[i]) / 2) * fwd;
  // assiette visible : plongée au freinage, cabré à l'accélération, roulis en virage, vibreurs et bosses
  car.dive = clamp(-car.ax * 0.0006, -0.015, 0.03);
  car.roll = clamp(-(car.ay || 0) * 0.00045, -0.025, 0.025);
  car.yOff = car.kerb ? (Math.random() - 0.5) * 0.05 : bump * 0.012 * Math.min(1, v / 60);

  // murs
  const lim = (car.lat > 0 ? track.WR[i] : track.WL[i]) - 1.0;
  if (Math.abs(car.lat) > lim) {
    const s = Math.sign(car.lat), over = Math.abs(car.lat) - lim;
    car.x -= rx * s * over; car.z -= rz * s * over; car.lat = s * lim;
    const rel = Math.atan2(Math.sin(course - track.H[i]), Math.cos(course - track.H[i]));
    const dir = Math.abs(rel) < Math.PI / 2 ? 0 : Math.PI;
    const impact = Math.abs(Math.sin(rel));
    if (car.v > 25 && !(car.hitWall > 0) && impact > 0.25) {
      const d = impact * Math.min(1, car.v / 60) * 0.25 * u.wall;
      if (Math.abs(car.beta) > 1 || car.spinT > 0) car.dmgNewR = (car.dmgNewR || 0) + d; else car.dmgNew = (car.dmgNew || 0) + d;
    }
    car.v *= 1 - Math.min(0.7, 0.1 + impact * 0.9) * u.wall;
    const target = track.H[i] + dir;
    const d = Math.atan2(Math.sin(target - car.heading), Math.cos(target - car.heading));
    car.heading += d * 0.5; car.beta *= 0.3;
    car.hitWall = 0.4;
  }
  return prev;
}

export function placeMesh(car) {
  const m = car.mesh;
  m.position.set(car.x, car.y + (car.yOff || 0), car.z);
  m.rotation.set(car.pitch + (car.dive || 0), car.heading, car.roll || 0);
}
