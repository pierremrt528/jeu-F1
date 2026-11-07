// Pilote du joueur en 3D (atelier du QG, cinématiques) : combinaison aux couleurs de l'écurie, visage, coiffure,
// barbe, casque tenu à la main. Une seule géométrie à couleurs par sommet (1 appel de rendu). Motifs de casque
// partagés avec la voiture (helmetFn).
import * as THREE from '../lib/three.module.min.js';

export const SKINS = [0xf1c9a5, 0xe0ac7e, 0xc68642, 0x8d5524, 0x5a3825];
export const HAIRS = ['Court', 'Long', 'Rasé', 'Bouclé'];
export const HAIR_COLS = [0x1b1410, 0x4a2c17, 0x8a5a2b, 0xd9b26a, 0xa33a1c];
export const HELMETS = ['Uni', 'Bande', 'Anneau', 'Vague', 'Damier', 'Deux tons'];
export const DEFAULT_LOOK = { skin: 1, hair: 0, hc: 1, beard: 0 };

// couleur d'un point du casque (centre cx, cy, cz ; rayon r) ; h = { m (motif), p, s (couleurs hex) }
export function helmetFn(h, cx, cy, cz, r) {
  return (x, y, z) => {
    const dx = (x - cx) / r, dy = (y - cy) / r, dz = (z - cz) / r;
    let s = false;
    if (h.m === 1) s = Math.abs(dx) < 0.24 && dy > -0.2;
    else if (h.m === 2) s = dy > -0.08 && dy < 0.22;
    else if (h.m === 3) s = Math.abs(dy - 0.25 * Math.sin(Math.atan2(dx, dz) * 3)) < 0.14;
    else if (h.m === 4) s = dy > -0.1 && (Math.floor(Math.atan2(dx, dz) * 6 / Math.PI) + Math.floor(dy * 4.5)) % 2 !== 0;
    else if (h.m === 5) s = dy < 0.05;
    return s ? h.s : h.p;
  };
}

export function buildPilot(look = DEFAULT_LOOK, suit = { p: 0xd81e1e, s: 0xffffff }, helmet = null) {
  const parts = [];
  const add = (geo, col, x = 0, y = 0, z = 0, rz = 0, sx = 1, sy = 1, sz = 1) => {
    if (geo.index) geo = geo.toNonIndexed();
    geo.deleteAttribute('uv'); geo.scale(sx, sy, sz); if (rz) geo.rotateZ(rz); geo.translate(x, y, z); parts.push([geo, col]);
  };
  const skin = SKINS[look.skin] ?? SKINS[1], hair = HAIR_COLS[look.hc] ?? HAIR_COLS[1], dark = 0x15171b, glove = 0x202329;
  // combinaison : bandes de la couleur secondaire sur les côtés, ceinture sombre
  const suitFn = (x, y) => (y > 0.84 && y < 0.9) ? dark : Math.abs(x) > 0.135 && y > 0.9 && y < 1.36 ? suit.s : (Math.abs(x) > 0.15 && y < 0.84) ? suit.s : suit.p;
  for (const s of [-1, 1]) {
    add(new THREE.BoxGeometry(0.12, 0.08, 0.25), dark, s * 0.1, 0.04, 0.03);                                   // bottines
    add(new THREE.CapsuleGeometry(0.075, 0.6, 3, 8), suitFn, s * 0.1, 0.46, 0);                                 // jambes
    add(new THREE.CapsuleGeometry(0.055, 0.46, 3, 8), (x, y) => y < 0.83 ? suit.s : suit.p, s * 0.245, 1.06, 0, s * 0.1);   // bras
    add(new THREE.SphereGeometry(0.058, 8, 6), glove, s * 0.272, 0.76, 0.01);                                   // gants
  }
  add(new THREE.CapsuleGeometry(0.17, 0.34, 4, 12), suitFn, 0, 1.08, 0, 0, 1, 1, 0.72);                         // torse
  add(new THREE.CylinderGeometry(0.075, 0.08, 0.06, 12), suit.s, 0, 1.4, 0);                                     // col
  add(new THREE.CylinderGeometry(0.05, 0.055, 0.1, 10), skin, 0, 1.44, 0);                                       // cou
  const HY = 1.565;
  add(new THREE.SphereGeometry(0.105, 16, 12), skin, 0, HY, 0, 0, 0.9, 1.08, 0.98);                              // tête
  for (const s of [-1, 1]) {
    add(new THREE.SphereGeometry(0.014, 6, 4), 0x16120f, s * 0.036, HY + 0.02, 0.094);                          // yeux
    add(new THREE.BoxGeometry(0.035, 0.007, 0.01), hair, s * 0.036, HY + 0.045, 0.095);                         // sourcils
    add(new THREE.SphereGeometry(0.02, 6, 4), skin, s * 0.094, HY, 0, 0, 0.5, 1, 1);                            // oreilles
  }
  add(new THREE.BoxGeometry(0.02, 0.035, 0.025), skin, 0, HY - 0.005, 0.1);                                      // nez
  add(new THREE.BoxGeometry(0.04, 0.007, 0.01), 0x7a4038, 0, HY - 0.05, 0.096);                                 // bouche
  // coiffure
  const cap = (r, th, sx = 0.92, sy = 1.08, z = -0.004) => add(new THREE.SphereGeometry(r, 16, 10, 0, Math.PI * 2, 0, th), hair, 0, HY, z, 0, sx, sy, 1);
  if (look.hair === 0) cap(0.112, 1.15);
  if (look.hair === 1) { cap(0.113, 1.25); add(new THREE.BoxGeometry(0.19, 0.2, 0.07), hair, 0, HY - 0.09, -0.07); }
  if (look.hair === 2) cap(0.107, 0.95, 0.91, 1.07, -0.006);
  if (look.hair === 3) cap(0.128, 1.3, 0.95, 1.05, -0.01);
  if (look.beard) add(new THREE.SphereGeometry(0.108, 14, 6, Math.PI / 2 - 1.15, 2.3, 1.85, 0.75), hair, 0, HY, 0.004, 0, 0.92, 1.08, 1);
  // casque tenu à la main droite
  if (helmet) {
    const hx = 0.31, hy = 0.86, hz = 0.12, r = 0.14;
    add(new THREE.SphereGeometry(r, 18, 12), helmetFn(helmet, hx, hy, hz, r), hx, hy, hz, 0, 1, 1.05, 1.12);
    add(new THREE.SphereGeometry(r * 1.02, 12, 3, Math.PI / 2 - 0.9, 1.8, 1.25, 0.42), 0x0d1013, hx, hy, hz, 0, 1, 1.05, 1.12);
  }
  // fusion en une géométrie
  let n = 0; for (const [g] of parts) n += g.attributes.position.count;
  const pos = new Float32Array(n * 3), nor = new Float32Array(n * 3), col = new Float32Array(n * 3), c = new THREE.Color();
  let o = 0;
  for (const [g, spec] of parts) {
    const P = g.attributes.position.array, cnt = g.attributes.position.count;
    pos.set(P, o * 3); nor.set(g.attributes.normal.array, o * 3);
    for (let k = 0; k < cnt; k++) {
      c.setHex(typeof spec === 'function' ? spec(P[k * 3], P[k * 3 + 1], P[k * 3 + 2]) : spec);
      col.set([c.r, c.g, c.b], (o + k) * 3);
    }
    o += cnt; g.dispose();
  }
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  geo.setAttribute('normal', new THREE.BufferAttribute(nor, 3));
  geo.setAttribute('color', new THREE.BufferAttribute(col, 3));
  geo.computeBoundingSphere();
  return new THREE.Mesh(geo, new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.62, metalness: 0.05 }));
}
