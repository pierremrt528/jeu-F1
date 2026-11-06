// Modèle 3D de monoplace (design original) : carrosserie fusionnée en une géométrie à couleurs par sommet,
// roues fusionnées (bande de roulement + flancs texturés), ombre douce. 3 appels de rendu par voiture.
import * as THREE from '../lib/three.module.min.js';
import * as TX from './textures.js';
import { merge } from './scenery.js';

let M = null;
function mats() {
  if (M) return M;
  M = {
    body: new THREE.MeshStandardMaterial({ vertexColors: true, metalness: 0.35, roughness: 0.32 }),
    tread: new THREE.MeshLambertMaterial({ map: TX.tyreTread() }),
    side: new THREE.MeshStandardMaterial({ map: TX.tyreSide(), roughness: 0.7, metalness: 0.1 }),
    blob: new THREE.MeshBasicMaterial({ map: TX.blob(), transparent: true, depthWrite: false, opacity: 0.8, polygonOffset: true, polygonOffsetFactor: -4 }),
  };
  return M;
}

// boîte effilée : avant (z>0) et arrière (z<0) réduits / décalés
function tbox(w, h, l, f = {}) {
  const g = new THREE.BoxGeometry(w, h, l).toNonIndexed(), p = g.attributes.position;
  for (let i = 0; i < p.count; i++) {
    const front = p.getZ(i) > 0;
    const sw = front ? (f.fw ?? 1) : (f.bw ?? 1), sh = front ? (f.fh ?? 1) : (f.bh ?? 1), dy = front ? (f.fy ?? 0) : (f.by ?? 0);
    p.setX(i, p.getX(i) * sw); p.setY(i, p.getY(i) * sh + dy);
  }
  g.computeVertexNormals();
  return g;
}

export function buildCarMesh(color, accent, helmet) {
  const m = mats();
  const parts = [];
  let tag = null;   // 'fw' / 'rw' : ailerons avant / arrière (peuvent se tordre ou se détacher, voir debris.js)
  const add = (geo, hex, x, y, z, rx = 0, ry = 0) => {
    if (geo.index) geo = geo.toNonIndexed();
    geo.rotateX(rx); geo.rotateY(ry); geo.translate(x, y, z); parts.push([geo, hex, tag]);
  };
  const B = (w, h, l) => new THREE.BoxGeometry(w, h, l);
  const carbon = 0x1c1e22, black = 0x0b0b0c;
  // fond plat, coque, nez
  add(tbox(1.8, 0.05, 3.5, { fw: 0.7 }), carbon, 0, 0.09, -0.35);
  add(tbox(0.84, 0.5, 2.2, { fw: 0.74, fh: 0.82, fy: -0.04 }), color, 0, 0.43, 0.6);
  add(tbox(0.6, 0.38, 1.95, { fw: 0.42, fh: 0.5, fy: -0.1 }), color, 0, 0.37, 2.55);
  add(tbox(0.26, 0.19, 0.3, { fw: 0.7, fh: 0.8 }), accent, 0, 0.26, 3.6);
  // aileron avant
  tag = 'fw';
  add(B(1.96, 0.035, 0.52), carbon, 0, 0.1, 3.2);
  add(B(1.72, 0.03, 0.22), accent, 0, 0.18, 3.1, -0.38);
  add(B(1.5, 0.03, 0.16), accent, 0, 0.24, 3.0, -0.6);
  for (const s of [-1, 1]) add(B(0.03, 0.3, 0.64), color, s * 0.98, 0.21, 3.15);
  tag = null;
  // pontons, entrées d'air
  add(tbox(1.56, 0.5, 1.95, { fh: 0.92, bw: 0.42, bh: 0.5, by: -0.06 }), color, 0, 0.42, -0.3);
  for (const s of [-1, 1]) add(B(0.3, 0.3, 0.04), black, s * 0.6, 0.48, 0.66);
  // capot moteur, prise d'air, aileron de requin
  add(tbox(0.62, 0.62, 2.5, { fw: 0.9, bw: 0.28, bh: 0.42, by: -0.12 }), color, 0, 0.78, -1.15);
  add(tbox(0.34, 0.34, 0.55, { bw: 0.8, bh: 0.7 }), accent, 0, 1.13, -0.62);
  add(B(0.24, 0.18, 0.02), black, 0, 1.16, -0.34);
  add(tbox(0.025, 0.32, 1.1, { fh: 0.2, fy: -0.12 }), color, 0, 1.05, -1.5);
  // habitacle, casque, volant
  add(B(0.56, 0.04, 0.92), black, 0, 0.69, 0.62);
  add(new THREE.SphereGeometry(0.155, 14, 10), helmet, 0, 0.86, 0.3);
  add(B(0.24, 0.07, 0.05), 0x15151a, 0, 0.88, 0.44);
  add(B(0.29, 0.13, 0.04), carbon, 0, 0.79, 0.8);
  add(B(0.11, 0.055, 0.012), 0x2cd36a, 0, 0.8, 0.775);
  // halo
  add(B(0.06, 0.06, 0.6), black, 0, 0.92, 0.98, 0.42);
  for (const s of [-1, 1]) add(B(0.05, 0.05, 0.62), black, s * 0.27, 1.01, 0.44, 0, s * 0.12);
  add(B(0.58, 0.05, 0.05), black, 0, 1.0, 0.13);
  add(B(0.32, 0.05, 0.05), black, 0, 1.0, 0.76);
  // rétroviseurs
  for (const s of [-1, 1]) {
    add(B(0.16, 0.03, 0.03), carbon, s * 0.48, 0.77, 0.98);
    add(B(0.21, 0.1, 0.06), color, s * 0.62, 0.8, 0.98);
    add(B(0.18, 0.075, 0.01), 0xa9b6c4, s * 0.62, 0.8, 0.948);
  }
  // aileron arrière, diffuseur, feu de pluie
  tag = 'rw';
  add(B(1.02, 0.04, 0.38), carbon, 0, 0.93, -2.48);
  add(B(1.02, 0.035, 0.25), accent, 0, 1.05, -2.44, 0.3);
  for (const s of [-1, 1]) add(tbox(0.035, 0.64, 0.8, { fh: 0.8, fy: -0.06 }), color, s * 0.52, 0.83, -2.44);
  add(B(0.06, 0.5, 0.1), carbon, 0, 0.66, -2.36);
  tag = null;
  add(tbox(1.0, 0.2, 0.5, { fh: 0.6, fy: -0.04 }), carbon, 0, 0.2, -2.3);
  add(B(0.1, 0.08, 0.03), 0xff2020, 0, 0.36, -2.58);
  // suspensions
  for (const s of [-1, 1]) {
    for (const [z, y] of [[1.75, 0.46], [1.75, 0.3], [-1.6, 0.48], [-1.6, 0.3]]) add(B(0.5, 0.028, 0.045), carbon, s * 0.58, y, z, 0, s * 0.18);
  }

  let n = 0; for (const [g] of parts) n += g.attributes.position.count;
  const pos = new Float32Array(n * 3), nor = new Float32Array(n * 3), col = new Float32Array(n * 3);
  const c = new THREE.Color(); let o = 0;
  const ranges = {};
  for (const [g, hex, t] of parts) {
    if (t) { const r = ranges[t] || (ranges[t] = [o, 0]); r[1] = o + g.attributes.position.count - r[0]; }
    pos.set(g.attributes.position.array, o * 3); nor.set(g.attributes.normal.array, o * 3);
    c.setHex(hex);
    for (let k = 0; k < g.attributes.position.count; k++) col.set([c.r, c.g, c.b], (o + k) * 3);
    o += g.attributes.position.count;
  }
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  geo.setAttribute('normal', new THREE.BufferAttribute(nor, 3));
  geo.setAttribute('color', new THREE.BufferAttribute(col, 3));

  const group = new THREE.Group();
  group.rotation.order = 'YXZ';
  group.add(new THREE.Mesh(geo, m.body));
  group.userData = { ranges, base: pos.slice() };
  group.add(new THREE.Mesh(wheels(), [m.tread, m.side]));
  const sh = new THREE.Mesh(new THREE.PlaneGeometry(2.4, 5.8), m.blob);
  sh.rotation.x = -Math.PI / 2; sh.position.set(0, 0.03, -0.1); sh.renderOrder = 1;
  group.add(sh);
  return group;
}

let WHEELS = null;
function wheels() {
  if (WHEELS) return WHEELS;
  const treads = [], sides = [];
  for (const [x, z, r, w] of [[0.83, 1.75, 0.33, 0.35], [-0.83, 1.75, 0.33, 0.35], [0.8, -1.6, 0.36, 0.43], [-0.8, -1.6, 0.36, 0.43]]) {
    const t = new THREE.CylinderGeometry(r, r, w, 22, 1, true); t.rotateZ(Math.PI / 2); t.translate(x, r, z); treads.push(t);
    for (const s of [-1, 1]) {
      const c = new THREE.CircleGeometry(r, 22); c.rotateY(s * Math.PI / 2); c.translate(x + s * w / 2, r, z); sides.push(c);
    }
  }
  const a = merge(treads), b = merge(sides);
  const g = merge([a, b]);
  g.addGroup(0, a.attributes.position.count, 0);
  g.addGroup(a.attributes.position.count, b.attributes.position.count, 1);
  return (WHEELS = g);
}
