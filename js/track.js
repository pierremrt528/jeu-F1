// Construction d'un circuit à partir de ses données : ligne centrale échantillonnée,
// profil de vitesse pour l'IA, trajectoire, puis maillages 3D (route, vibreurs, talus, murs, décor).
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

// ---------------- Maillages ----------------
export function buildTrackMeshes(track, scene) {
  const { N, X, Y, Z, TX, TZ, C, width: w, def } = track;
  const th = def.theme;
  const group = new THREE.Group();
  const lambert = (o) => new THREE.MeshLambertMaterial(o);
  const vcMat = lambert({ vertexColors: true });

  // Route (bandes indexées)
  {
    const pos = [], idx = [];
    for (let i = 0; i <= N; i++) {
      const j = i % N, rx = -TZ[j], rz = TX[j];
      pos.push(X[j] - rx * w / 2, Y[j] + 0.02, Z[j] - rz * w / 2, X[j] + rx * w / 2, Y[j] + 0.02, Z[j] + rz * w / 2);
      if (i < N) { const a = i * 2; idx.push(a, a + 1, a + 2, a + 1, a + 3, a + 2); }
    }
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); g.setIndex(idx); g.computeVertexNormals();
    group.add(new THREE.Mesh(g, lambert({ color: 0x3a3c41 })));
  }

  // Quads colorés (lignes blanches, vibreurs, talus, murs) dans un seul maillage
  const P = [], Col = [];
  const col = new THREE.Color();
  const quad = (a, b, c, d, hex) => {
    // a,b côté i ; c,d côté i+1  (a/c intérieur, b/d extérieur)
    P.push(...a, ...c, ...b, ...b, ...c, ...d);
    col.setHex(hex); for (let k = 0; k < 6; k++) Col.push(col.r, col.g, col.b);
  };
  const pt = (j, off, dy) => [X[j] + -TZ[j] * off, Y[j] + dy, Z[j] + TX[j] * off];
  const wall = w / 2 + RUNOFF;
  for (let i = 0; i < N; i++) {
    const j = (i + 1) % N;
    const kerb = Math.abs(C[i]) > 1 / 220;
    for (const s of [-1, 1]) {
      // ligne blanche de bord
      quad(pt(i, s * (w / 2 - 0.45), 0.03), pt(i, s * (w / 2 - 0.2), 0.03), pt(j, s * (w / 2 - 0.45), 0.03), pt(j, s * (w / 2 - 0.2), 0.03), 0xf2f2f2);
      // vibreurs rouge/blanc dans les virages
      if (kerb) quad(pt(i, s * w / 2, 0.06), pt(i, s * (w / 2 + 1.3), 0.06), pt(j, s * w / 2, 0.06), pt(j, s * (w / 2 + 1.3), 0.06), (i >> 1) % 2 ? 0xd42020 : 0xf4f4f4);
      // herbe du dégagement, puis talus jusqu'au sol
      const gIn = w / 2 + (kerb ? 1.3 : 0);
      quad(pt(i, s * gIn, 0.0), pt(i, s * (wall + 3), -0.05), pt(j, s * gIn, 0.0), pt(j, s * (wall + 3), -0.05), th.grass);
      const fall = (k) => [X[k] + -TZ[k] * s * 70, track.ground, Z[k] + TX[k] * s * 70];
      quad(pt(i, s * (wall + 3), -0.05), fall(i), pt(j, s * (wall + 3), -0.05), fall(j), th.grassFar);
      // mur (glissière) alternant blanc/rouge
      quad(pt(i, s * wall, 0), pt(i, s * wall, 1.0), pt(j, s * wall, 0), pt(j, s * wall, 1.0), (i >> 2) % 2 ? 0xe6e6e6 : 0xc0392b);
    }
  }
  {
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.Float32BufferAttribute(P, 3));
    g.setAttribute('color', new THREE.Float32BufferAttribute(Col, 3));
    g.computeVertexNormals();
    group.add(new THREE.Mesh(g, new THREE.MeshLambertMaterial({ vertexColors: true, side: THREE.DoubleSide })));
  }

  // Sol
  let minX = Infinity, maxX = -Infinity, minZ = Infinity, maxZ = -Infinity;
  for (let i = 0; i < N; i++) { minX = Math.min(minX, X[i]); maxX = Math.max(maxX, X[i]); minZ = Math.min(minZ, Z[i]); maxZ = Math.max(maxZ, Z[i]); }
  const cx = (minX + maxX) / 2, cz = (minZ + maxZ) / 2, size = Math.max(maxX - minX, maxZ - minZ) + 2400;
  const ground = new THREE.Mesh(new THREE.PlaneGeometry(size, size), lambert({ color: th.ground }));
  ground.rotation.x = -Math.PI / 2; ground.position.set(cx, track.ground - 0.05, cz);
  group.add(ground);

  // Ligne de départ en damier
  {
    const cv = document.createElement('canvas'); cv.width = 64; cv.height = 8;
    const c = cv.getContext('2d');
    for (let x = 0; x < 16; x++) for (let y = 0; y < 2; y++) { c.fillStyle = (x + y) % 2 ? '#111' : '#fff'; c.fillRect(x * 4, y * 4, 4, 4); }
    const tex = new THREE.CanvasTexture(cv); tex.magFilter = THREE.NearestFilter; tex.colorSpace = THREE.SRGBColorSpace;
    const m = new THREE.Mesh(new THREE.PlaneGeometry(w, 1.6), new THREE.MeshBasicMaterial({ map: tex }));
    m.rotation.order = 'YXZ'; m.rotation.set(-Math.PI / 2, track.H[0], 0);
    m.position.set(X[0], Y[0] + 0.05, Z[0]);
    group.add(m);
  }

  // Grille de départ : marques blanches
  for (let k = 0; k < 20; k++) {
    const i = (N - 5 - k * 4) % N, s = k % 2 ? 1 : -1;
    const m = new THREE.Mesh(new THREE.PlaneGeometry(2.4, 0.3), new THREE.MeshBasicMaterial({ color: 0xeeeeee }));
    m.rotation.order = 'YXZ'; m.rotation.set(-Math.PI / 2, track.H[i], 0);
    const p = pt(i, s * 3, 0.04); m.position.set(p[0], p[1], p[2]);
    group.add(m);
  }

  // Portique de départ avec 5 feux
  const lights = [];
  {
    const h = track.H[0], rx = -TZ[0], rz = TX[0];
    const dark = lambert({ color: 0x222428 });
    for (const s of [-1, 1]) {
      const post = new THREE.Mesh(new THREE.BoxGeometry(0.6, 8, 0.6), dark);
      post.position.set(X[0] + rx * s * (w / 2 + 1.5), Y[0] + 4, Z[0] + rz * s * (w / 2 + 1.5));
      group.add(post);
    }
    const beam = new THREE.Mesh(new THREE.BoxGeometry(w + 3.6, 1.4, 0.8), dark);
    beam.position.set(X[0], Y[0] + 7.4, Z[0]); beam.rotation.y = h;
    group.add(beam);
    for (let k = 0; k < 5; k++) {
      const l = new THREE.Mesh(new THREE.BoxGeometry(0.8, 0.8, 0.3), new THREE.MeshBasicMaterial({ color: 0x330000 }));
      const off = (k - 2) * 1.4;
      l.position.set(X[0] + rx * off - TX[0] * 0.5, Y[0] + 7.4, Z[0] + rz * off - TZ[0] * 0.5); l.rotation.y = h;
      group.add(l); lights.push(l);
    }
  }

  // Tribunes et stands le long de la ligne droite de départ
  {
    const stand = lambert({ color: 0x8a9099 }), roof = lambert({ color: 0xe8e8e8 }), crowd = lambert({ color: 0x6a5acd });
    for (let k = -2; k <= 3; k++) {
      const i = (k * 30 + N) % N, s = -1;
      const p = pt(i, s * (w / 2 + RUNOFF + 9), 0);
      const g = new THREE.Group();
      const b = new THREE.Mesh(new THREE.BoxGeometry(50, 6, 10), stand); b.position.y = 3; g.add(b);
      const c = new THREE.Mesh(new THREE.BoxGeometry(48, 0.6, 8), crowd); c.position.set(0, 6.3, 0); c.rotation.x = 0.35; g.add(c);
      const r = new THREE.Mesh(new THREE.BoxGeometry(52, 0.4, 12), roof); r.position.y = 11; g.add(r);
      g.position.set(p[0], track.ground, p[2]); g.rotation.y = track.H[i] + Math.PI / 2;
      group.add(g);
    }
    const pits = lambert({ color: 0xdfe3e8 });
    for (let k = -1; k <= 2; k++) {
      const i = (k * 40 + N) % N, p = pt(i, w / 2 + RUNOFF + 12, 0);
      const b = new THREE.Mesh(new THREE.BoxGeometry(76, 7, 14), pits);
      b.position.set(p[0], track.ground + 3.5, p[2]); b.rotation.y = track.H[i] + Math.PI / 2;
      group.add(b);
    }
  }

  // Arbres (instanciés) loin de la piste, collines à l'horizon
  {
    const grid = new Map(), cell = 40;
    for (let i = 0; i < N; i += 2) {
      const key = Math.floor(X[i] / cell) + ',' + Math.floor(Z[i] / cell);
      if (!grid.has(key)) grid.set(key, []); grid.get(key).push(i);
    }
    const nearest = (x, z) => {
      let bd = Infinity, bi = 0; const gx = Math.floor(x / cell), gz = Math.floor(z / cell);
      for (let a = -2; a <= 2; a++) for (let b = -2; b <= 2; b++) {
        const l = grid.get((gx + a) + ',' + (gz + b)); if (!l) continue;
        for (const i of l) { const d = (X[i] - x) ** 2 + (Z[i] - z) ** 2; if (d < bd) { bd = d; bi = i; } }
      }
      return [Math.sqrt(bd), bi];
    };
    const rnd = mulberry(7);
    const crown = new THREE.InstancedMesh(new THREE.ConeGeometry(3.2, 9, 6), lambert({ color: th.treeColor }), th.trees);
    const trunk = new THREE.InstancedMesh(new THREE.CylinderGeometry(0.4, 0.5, 3, 5), lambert({ color: 0x5b3a1e }), th.trees);
    const m = new THREE.Matrix4(); let n = 0, tries = 0;
    while (n < th.trees && tries++ < th.trees * 20) {
      const x = minX - 300 + rnd() * (maxX - minX + 600), z = minZ - 300 + rnd() * (maxZ - minZ + 600);
      const [d, i] = nearest(x, z);
      if (d < w / 2 + RUNOFF + 6) continue;
      if (Math.abs(((i + N / 2) % N) - N / 2) < 80 && d < 60) continue; // pas devant les tribunes
      const t = Math.min(1, Math.max(0, (d - (w / 2 + RUNOFF + 3)) / (70 - (w / 2 + RUNOFF + 3))));
      const y = d < 200 ? Y[i] + (track.ground - Y[i]) * t : track.ground;
      const sc = 0.7 + rnd() * 0.8;
      m.makeScale(sc, sc, sc); m.setPosition(x, y + 3 * sc + 4.5 * sc, z); crown.setMatrixAt(n, m);
      m.makeScale(sc, sc, sc); m.setPosition(x, y + 1.5 * sc, z); trunk.setMatrixAt(n, m);
      n++;
    }
    crown.count = trunk.count = n;
    group.add(crown, trunk);

    const hills = new THREE.InstancedMesh(new THREE.ConeGeometry(1, 1, 7), lambert({ color: 0x5f7f58 }), th.hills);
    const R = size / 2 - 500;
    for (let k = 0; k < th.hills; k++) {
      const a = (k / th.hills) * Math.PI * 2 + rnd() * 0.2, r = R * (0.75 + rnd() * 0.3);
      const hgt = 80 + rnd() * 160, rad = 180 + rnd() * 200;
      m.makeScale(rad, hgt, rad); m.setPosition(cx + Math.cos(a) * r, track.ground + hgt / 2, cz + Math.sin(a) * r);
      hills.setMatrixAt(k, m);
    }
    group.add(hills);
  }

  scene.add(group);
  return { group, lights };
}

export function mulberry(a) {
  return () => { a |= 0; a = a + 0x6D2B79F5 | 0; let t = Math.imul(a ^ a >>> 15, 1 | a); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; };
}
