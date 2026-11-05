// Construction d'un circuit à partir de ses données : ligne centrale échantillonnée,
// profil de vitesse pour l'IA, trajectoire. Les maillages 3D sont dans scenery.js.
import * as THREE from '../lib/three.module.min.js';
import { SPEC } from './car.js';

export const DS = 2;        // pas d'échantillonnage (m)
export const RUNOFF = 9;    // largeur du dégagement herbeux avant le mur (m)

export function buildTrack(def) {
  const curve = new THREE.CatmullRomCurve3(def.points.map(p => new THREE.Vector3(p[0], p[2], p[1])), true, 'centripetal');
  const N = Math.round(curve.getLength() / DS);
  let pts = curve.getSpacedPoints(N).slice(0, N);
  // la ligne de départ est le point le plus proche de def.start (ou du 1er point)
  const st = def.start || [def.points[1][0] * 0.8, def.points[1][1]];
  let best = 0, bd = Infinity;
  pts.forEach((p, i) => { const d = (p.x - st[0]) ** 2 + (p.z - st[1]) ** 2; if (d < bd) { bd = d; best = i; } });
  pts = pts.slice(best).concat(pts.slice(0, best));

  const X = new Float32Array(N), Z = new Float32Array(N), Y = new Float32Array(N);
  const TX = new Float32Array(N), TZ = new Float32Array(N), H = new Float32Array(N);
  for (let i = 0; i < N; i++) { X[i] = pts[i].x; Y[i] = pts[i].y; Z[i] = pts[i].z; }
  for (let i = 0; i < N; i++) {
    const a = (i - 1 + N) % N, b = (i + 1) % N;
    let tx = X[b] - X[a], tz = Z[b] - Z[a]; const l = Math.hypot(tx, tz) || 1;
    TX[i] = tx / l; TZ[i] = tz / l; H[i] = Math.atan2(TX[i], TZ[i]);
  }
  // courbure signée (positive = virage à gauche), lissée
  const raw = new Float32Array(N);
  for (let i = 0; i < N; i++) {
    let d = H[(i + 2) % N] - H[(i - 2 + N) % N];
    d = Math.atan2(Math.sin(d), Math.cos(d));
    raw[i] = d / (4 * DS);
  }
  const C = smooth(raw, 4), Cwide = smooth(raw, 22);
  const w = def.width;

  // trajectoire de course : décalée vers l'intérieur des virages
  const RL = new Float32Array(N);
  const lim = w / 2 - 2.2;
  for (let i = 0; i < N; i++) RL[i] = Math.max(-lim, Math.min(lim, -Cwide[i] * 260));

  // profil de vitesse (IA) : limite d'adhérence en virage puis freinage en arrière
  const mu = SPEC.mu * 0.94, g = 9.81;
  const V = new Float32Array(N);
  for (let i = 0; i < N; i++) {
    let c = 0; for (let k = -3; k <= 3; k++) c = Math.max(c, Math.abs(C[(i + k + N) % N]));
    const den = c - mu * SPEC.kd;
    V[i] = den > 0 ? Math.min(SPEC.vtop, Math.sqrt(mu * g / den)) : SPEC.vtop;
  }
  for (let pass = 0; pass < 2; pass++) {
    for (let i = N - 1; i >= 0; i--) {
      const vn = V[(i + 1) % N];
      const br = 0.8 * (SPEC.brake0 + SPEC.brakeK * vn * vn);
      V[i] = Math.min(V[i], Math.sqrt(vn * vn + 2 * br * DS));
    }
  }
  let minY = Infinity; for (let i = 0; i < N; i++) minY = Math.min(minY, Y[i]);
  return { def, N, length: N * DS, width: w, X, Y, Z, TX, TZ, H, C, RL, V, ground: minY - 0.6 };
}

function smooth(a, r) {
  const N = a.length, o = new Float32Array(N);
  for (let i = 0; i < N; i++) { let s = 0; for (let k = -r; k <= r; k++) s += a[(i + k + N) % N]; o[i] = s / (2 * r + 1); }
  return o;
}

// Cherche l'échantillon le plus proche autour de l'index précédent ; renvoie la position latérale
export function locate(track, x, z, idx) {
  const { N, X, Z } = track;
  let best = idx, bd = Infinity;
  for (let k = -12; k <= 24; k++) {
    const i = (idx + k + N) % N, d = (X[i] - x) ** 2 + (Z[i] - z) ** 2;
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}

// Repère du point i : normale vers la droite = (-TZ, TX)
export const rightX = (t, i) => -t.TZ[i];
export const rightZ = (t, i) => t.TX[i];

export function mulberry(a) {
  return () => { a |= 0; a = a + 0x6D2B79F5 | 0; let t = Math.imul(a ^ a >>> 15, 1 | a); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; };
}
