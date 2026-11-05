// Monoplace : modèle 3D low-poly (une seule géométrie par voiture) et physique arcade.
import * as THREE from '../lib/three.module.min.js';
import { locate, RUNOFF } from './track.js';

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
    this.mesh = buildCarMesh(team.color, team.accent, driver.player ? 0xffffff : team.accent);
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

export function stepCar(car, track, dt) {
  const g = 9.81;
  let v = car.v;
  const half = track.width / 2;
  car.offTrack = Math.abs(car.lat) > half + 1.3;
  const grip = car.offTrack ? 0.55 : 1;
  const vtop = SPEC.vtop * car.perf * (1 + SPEC.slip * car.slipstream) * (car.offTrack ? 0.5 : 1);

  // longitudinal
  let a;
  if (car.throttle > 0 && v >= -0.5) a = car.throttle * SPEC.acc * car.perf * (1 - Math.pow(Math.max(v, 0) / vtop, 2.2));
  else a = -Math.sign(v) * (0.8 + 0.0009 * v * v);
  if (car.offTrack) a -= v * 0.35;
  if (car.brake > 0) {
    if (v > 0.3) a -= car.brake * (SPEC.brake0 + SPEC.brakeK * v * v) * grip;
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
  const delta = SPEC.steerMax * car.steer / (1 + Math.abs(v) / 20);
  let yaw = v * Math.tan(delta) / SPEC.wb;
  const latMax = SPEC.mu * grip * (g + SPEC.kd * v * v);
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
  const lim = half + RUNOFF - 1.0;
  if (Math.abs(car.lat) > lim) {
    const s = Math.sign(car.lat), over = Math.abs(car.lat) - lim;
    car.x -= rx * s * over; car.z -= rz * s * over; car.lat = s * lim;
    const rel = Math.atan2(Math.sin(car.heading - track.H[i]), Math.cos(car.heading - track.H[i]));
    const dir = Math.abs(rel) < Math.PI / 2 ? 0 : Math.PI;
    const impact = Math.abs(Math.sin(rel));
    car.v *= 1 - Math.min(0.7, 0.1 + impact * 0.9);
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

// --------- modèle 3D : pièces fusionnées en une géométrie à couleurs par sommet ---------
function buildCarMesh(color, accent, helmet) {
  const parts = [];
  const add = (geo, hex, x, y, z, rx = 0, rz = 0) => {
    geo.rotateX(rx); geo.rotateZ(rz); geo.translate(x, y, z);
    parts.push([geo.index ? geo.toNonIndexed() : geo, hex]);
  };
  const B = (w, h, l) => new THREE.BoxGeometry(w, h, l);
  const dark = 0x15171a, carbon = 0x2b2e33;
  add(B(1.4, 0.02, 4.6), 0x050505, 0, 0.03, 0);            // ombre
  add(B(0.75, 0.5, 3.0), color, 0, 0.42, 0.1);             // monocoque
  add(B(0.45, 0.3, 1.5), color, 0, 0.36, 2.25, 0.08);      // nez
  add(B(1.9, 0.07, 0.55), accent, 0, 0.16, 2.95);          // aileron avant
  add(B(0.06, 0.25, 0.6), accent, 0.94, 0.25, 2.95);
  add(B(0.06, 0.25, 0.6), accent, -0.94, 0.25, 2.95);
  add(B(1.55, 0.42, 1.7), color, 0, 0.36, -0.35);          // pontons
  add(B(0.55, 0.55, 1.8), color, 0, 0.75, -1.05, -0.1);    // capot moteur
  add(B(1.1, 0.08, 0.4), accent, 0, 1.0, -2.35);           // aileron arrière
  add(B(1.1, 0.06, 0.3), accent, 0, 0.82, -2.3);
  add(B(0.06, 0.6, 0.6), carbon, 0.56, 0.75, -2.3);
  add(B(0.06, 0.6, 0.6), carbon, -0.56, 0.75, -2.3);
  add(B(0.32, 0.3, 0.34), helmet, 0, 0.82, 0.25);          // casque
  add(B(0.05, 0.05, 0.5), carbon, 0, 0.98, 0.75);          // halo
  add(B(0.6, 0.06, 0.08), carbon, 0, 0.98, 0.2);
  const wheel = (x, z, r, wd) => add(new THREE.CylinderGeometry(r, r, wd, 12), dark, x, r, z, 0, Math.PI / 2);
  wheel(0.88, 1.75, 0.34, 0.36); wheel(-0.88, 1.75, 0.34, 0.36);
  wheel(0.85, -1.55, 0.37, 0.44); wheel(-0.85, -1.55, 0.37, 0.44);

  let n = 0; for (const [g] of parts) n += g.attributes.position.count;
  const pos = new Float32Array(n * 3), nor = new Float32Array(n * 3), col = new Float32Array(n * 3);
  const c = new THREE.Color(); let o = 0;
  for (const [g, hex] of parts) {
    pos.set(g.attributes.position.array, o * 3); nor.set(g.attributes.normal.array, o * 3);
    c.setHex(hex);
    for (let k = 0; k < g.attributes.position.count; k++) { col[(o + k) * 3] = c.r; col[(o + k) * 3 + 1] = c.g; col[(o + k) * 3 + 2] = c.b; }
    o += g.attributes.position.count;
  }
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  geo.setAttribute('normal', new THREE.BufferAttribute(nor, 3));
  geo.setAttribute('color', new THREE.BufferAttribute(col, 3));
  const mesh = new THREE.Mesh(geo, CAR_MAT || (CAR_MAT = new THREE.MeshLambertMaterial({ vertexColors: true })));
  mesh.rotation.order = 'YXZ';
  return mesh;
}
let CAR_MAT = null;
