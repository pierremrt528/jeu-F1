// Monoplace : état et physique arcade (le modèle 3D est dans carmodel.js).
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
    this.v = 0; this.idx = 0; this.lat = 0;
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

export const NO_UP = { vtop: 1, acc: 1, brake: 1, mu: 1, kd: 1, steer: 1, slip: 1, off: 1, wall: 1, rel: 0 };

export function stepCar(car, track, dt) {
  const g = 9.81;
  let v = car.v;
  const half = track.width / 2;
  car.offTrack = Math.abs(car.lat) > half + 1.3;
  const u = car.up || NO_UP;   // améliorations du garage (multiplicateurs)
  const grip = car.offTrack ? 0.55 * u.off : 1;
  const vtop = SPEC.vtop * car.perf * u.vtop * (1 + SPEC.slip * u.slip * car.slipstream) * (car.offTrack ? 0.5 : 1);

  // longitudinal
  let a;
  if (car.throttle > 0 && v >= -0.5) a = car.throttle * SPEC.acc * car.perf * u.acc * (1 - Math.pow(Math.max(v, 0) / vtop, 2.2));
  else a = -Math.sign(v) * (0.8 + 0.0009 * v * v);
  if (car.offTrack) a -= v * 0.35;
  if (car.brake > 0) {
    if (v > 0.3) a -= car.brake * (SPEC.brake0 + SPEC.brakeK * v * v) * grip * u.brake;
    else if (car.throttle === 0 && car.canReverse) a = -car.brake * 5; // marche arrière lente pour se dégager
  }
  // pente : dénivelé entre l'échantillon courant et le suivant
  const i1 = (car.idx + 1) % track.N;
  const slope = (track.Y[i1] - track.Y[car.idx]) / 2;
  const fwd = Math.cos(car.heading - track.H[car.idx]);
  a -= g * slope * fwd;
  v += a * dt;
  if (v < -6) v = -6;
  if (v < 0 && !car.canReverse) v = 0;
  if (Math.abs(v) < 0.05 && car.throttle === 0 && car.brake === 0) v = 0;

  // latéral : braquage réduit avec la vitesse, limité par l'adhérence (appui aéro inclus)
  const delta = SPEC.steerMax * u.steer * car.steer / (1 + Math.abs(v) / 20);
  let yaw = v * Math.tan(delta) / SPEC.wb;
  const latMax = SPEC.mu * u.mu * grip * (g + SPEC.kd * u.kd * v * v);
  const latWant = Math.abs(v * yaw);
  car.sliding = latWant > latMax;
  if (car.sliding) {
    yaw *= latMax / latWant;
    v -= Math.sign(v) * Math.min(latWant - latMax, 40) * 0.1 * dt; // sous-virage : on frotte
  }
  car.heading -= yaw * dt;
  car.v = v;
  car.x += Math.sin(car.heading) * v * dt;
  car.z += Math.cos(car.heading) * v * dt;

  // position sur la piste
  const prev = car.idx;
  car.idx = locate(track, car.x, car.z, car.idx);
  const i = car.idx, rx = -track.TZ[i], rz = track.TX[i];
  const dx = car.x - track.X[i], dz = car.z - track.Z[i];
  car.lat = dx * rx + dz * rz;
  const along = dx * track.TX[i] + dz * track.TZ[i];
  const j = along >= 0 ? (i + 1) % track.N : (i - 1 + track.N) % track.N;
  const t = Math.min(1, Math.abs(along) / 2);
  const ty = track.Y[i] + (track.Y[j] - track.Y[i]) * t;
  car.y = ty;
  car.pitch = -Math.atan((track.Y[(i + 1) % track.N] - track.Y[i]) / 2) * fwd;

  // murs
  const lim = half + track.runoff - 1.0;
  if (Math.abs(car.lat) > lim) {
    const s = Math.sign(car.lat), over = Math.abs(car.lat) - lim;
    car.x -= rx * s * over; car.z -= rz * s * over; car.lat = s * lim;
    const rel = Math.atan2(Math.sin(car.heading - track.H[i]), Math.cos(car.heading - track.H[i]));
    const dir = Math.abs(rel) < Math.PI / 2 ? 0 : Math.PI;
    const impact = Math.abs(Math.sin(rel));
    car.v *= 1 - Math.min(0.7, 0.1 + impact * 0.9) * u.wall;
    const target = track.H[i] + dir;
    const d = Math.atan2(Math.sin(target - car.heading), Math.cos(target - car.heading));
    car.heading += d * 0.5;
    car.hitWall = 0.4;
  }
  return prev;
}

export function placeMesh(car) {
  const m = car.mesh;
  m.position.set(car.x, car.y, car.z);
  m.rotation.set(car.pitch, car.heading, 0);
}

