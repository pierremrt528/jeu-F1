// Construction d'un circuit à partir de ses données : ligne centrale échantillonnée,
// profil de vitesse pour l'IA, trajectoire. Les maillages 3D sont dans scenery.js.
import * as THREE from '../lib/three.module.min.js';
import { SPEC } from './car.js';

export const DS = 2;        // pas d'échantillonnage (m)

// Tracé décrit comme une suite de lignes droites et de virages (S longueur, L/R rayon angle),
// fermé automatiquement, avec un profil d'altitude [fraction du tour, altitude].
export function layoutPoints(def) {
  const pts = [[0, 0]]; let h = 0, x = 0, z = 0;
  for (const sg of def.layout) {
    if (sg[0] === 'S') {
      const n = Math.max(1, Math.ceil(sg[1] / 4));
      for (let k = 0; k < n; k++) { x += Math.sin(h) * sg[1] / n; z += Math.cos(h) * sg[1] / n; pts.push([x, z]); }
    } else {
      const a = (sg[0] === 'L' ? 1 : -1) * sg[2] * Math.PI / 180, len = sg[1] * Math.abs(a), n = Math.max(1, Math.ceil(len / 4));
      for (let k = 0; k < n; k++) { h += a / n / 2; x += Math.sin(h) * len / n; z += Math.cos(h) * len / n; h += a / n / 2; pts.push([x, z]); }
    }
  }
  const s = [0]; for (let i = 1; i < pts.length; i++) s.push(s[i - 1] + Math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]));
  const tot = s[s.length - 1], ex = x, ez = z;
  const E = def.elev || [[0, 0]];
  const elev = f => {
    const k = E.concat([[1, E[0][1]]]);
    for (let i = 0; i < k.length - 1; i++) if (f >= k[i][0] && f <= k[i + 1][0]) {
      const t = (f - k[i][0]) / ((k[i + 1][0] - k[i][0]) || 1), u = t * t * (3 - 2 * t);
      return k[i][1] + (k[i + 1][1] - k[i][1]) * u;
    }
    return E[0][1];
  };
  const out = [];
  for (let i = 0; i < pts.length - 1; i += 6) { const f = s[i] / tot; out.push([pts[i][0] - ex * f, pts[i][1] - ez * f, elev(f)]); }
  // départ : sur la première ligne droite, en laissant la place à la grille derrière
  const L0 = def.layout[0][0] === 'S' ? def.layout[0][1] : 200;
  const sStart = Math.min(L0 * 0.6, Math.max(100, L0 - 60));
  const ks = s.findIndex(v => v >= sStart);
  return { points: out, start: [pts[ks][0] - ex * s[ks] / tot, pts[ks][1] - ez * s[ks] / tot] };
}

export function buildTrack(def) {
  if (def.layout && !def.points) Object.assign(def, layoutPoints(def));
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
  return { def, N, length: N * DS, width: w, runoff: def.runoff ?? 9, X, Y, Z, TX, TZ, H, C, RL, V, ground: minY - 0.6 };
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

// Voie des stands (côté droit de la ligne droite de départ), en échantillons signés autour de la ligne :
// entrée de e0 à e1 (la voie s'écarte de la piste), emplacements des écuries, sortie de x0 à x1.
export function pitLane(track) {
  const half = track.width / 2, wall = half + (track.runoff ?? 9), lat = wall + 6;
  const sm = t => { t = Math.max(0, Math.min(1, t)); return t * t * (3 - 2 * t); };
  // longueur de la voie selon le circuit (def.pit : 0,4 courte … 2 très longue) → temps perdu à chaque arrêt
  const sc = track.def?.pit ?? 1, e1 = -125 - Math.max(10, Math.round(15 + 60 * (sc - 1))), x0 = -8 + Math.max(28, Math.round(78 + 100 * (sc - 1)));
  const P = { half, wall, lat, e0: e1 - 50, e1, x0, x1: x0 + 40 };
  // temps perdu : voie au limiteur (22 m/s) comparée à la vitesse de course sur le même tronçon, + arrêt
  let loss = 5.2;
  if (track.V) for (let k = P.e0; k <= P.x1; k++) {
    const v = track.V[(k + track.N) % track.N], f = k < P.e1 ? (k - P.e0) / (P.e1 - P.e0) : k > P.x0 ? 1 - (k - P.x0) / (P.x1 - P.x0) : 1;
    loss += DS / (v + (22 - v) * f) - DS / v;
  }
  P.loss = Math.round(loss * 10) / 10;
  P.latAt = k => k < P.e1 ? half - 1 + (lat - half + 1) * sm((k - P.e0) / (P.e1 - P.e0))
    : k > P.x0 ? lat - (lat - half + 1) * sm((k - P.x0) / (P.x1 - P.x0)) : lat;
  P.box = t => -125 + t * 13;          // emplacement de l'écurie t (0..9), avant et après la ligne
  P.k = (i, N) => i > N / 2 ? i - N : i;  // indice signé par rapport à la ligne
  return P;
}
