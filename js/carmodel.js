// Modèle 3D de monoplace (design original). Un modèle détaillé par ère du règlement, fait dans Blender
// (assets/cars/<id>.bin, voir tools/blender/) ; s'il n'est pas chargé, modèle procédural : carrosserie lissée par sections (coque, nez, pontons, capot),
// ailerons à profils d'aile, halo tubulaire, suspensions, livrée par sommet. Tout est fusionné en une géométrie
// à couleurs par sommet ; roues (pneus arrondis + flancs/jantes texturés) ; ombre douce. 3 appels de rendu par voiture.
import * as THREE from '../lib/three.module.min.js';
import { helmetFn } from './pilot3d.js';
import * as TX from './textures.js';
import { merge } from './scenery.js';

let M = null;
function mats() {
  if (M) return M;
  M = {
    body: new THREE.MeshPhysicalMaterial({ vertexColors: true, metalness: 0.15, roughness: 0.38, clearcoat: 0.8, clearcoatRoughness: 0.15, envMapIntensity: 0.85 }),
    tread: new THREE.MeshStandardMaterial({ map: TX.tyreTread(), roughness: 0.92, metalness: 0 }),
    side: new THREE.MeshStandardMaterial({ map: TX.tyreSide(), roughness: 0.55, metalness: 0.35 }),
    blob: new THREE.MeshBasicMaterial({ map: TX.blob(), transparent: true, depthWrite: false, opacity: 0.85, polygonOffset: true, polygonOffsetFactor: -4 }),
  };
  return M;
}

// puissance signée (sections « superellipse » : n grand = plus carré)
const sp = (v, e) => Math.sign(v) * Math.abs(v) ** e;

// Corps lissé : sections transversales le long de z.
// s = { z, x (centre), y0 (bas), y1 (haut), ym (ligne de plus grande largeur), w (largeur), wb (largeur du bas), n }
function loft(secs, seg = 20) {
  const pos = [], idx = [];
  for (const s of secs) {
    const n = s.n ?? 2.6, e = 2 / n, ym = s.ym ?? (s.y0 + s.y1) / 2, cx = s.x ?? 0;
    for (let k = 0; k <= seg; k++) {
      const a = k / seg * Math.PI * 2, c = Math.cos(a), si = Math.sin(a);
      const top = si >= 0;
      const x = cx + (top ? s.w : (s.wb ?? s.w)) / 2 * sp(c, e);
      const y = top ? ym + (s.y1 - ym) * sp(si, e) : ym + (ym - s.y0) * sp(si, e);
      pos.push(x, y, s.z);
    }
  }
  const R = seg + 1;
  for (let i = 0; i < secs.length - 1; i++) for (let k = 0; k < seg; k++) {
    const a = i * R + k, b = a + 1, c = a + R, d = c + 1;
    // z décroît d'une section à l'autre : faces orientées vers l'extérieur
    if (secs[i + 1].z < secs[i].z) idx.push(a, c, b, b, c, d); else idx.push(a, b, c, b, d, c);
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  g.setIndex(idx); g.computeVertexNormals();
  return g;
}
// bouchon plat d'une section (entrée d'air, arrière)
function cap(s, front, seg = 20) {
  const n = s.n ?? 2.6, e = 2 / n, ym = s.ym ?? (s.y0 + s.y1) / 2, cx = s.x ?? 0;
  const pos = [], ring = [];
  for (let k = 0; k <= seg; k++) {
    const a = k / seg * Math.PI * 2, c = Math.cos(a), si = Math.sin(a), top = si >= 0;
    ring.push([cx + (top ? s.w : (s.wb ?? s.w)) / 2 * sp(c, e), top ? ym + (s.y1 - ym) * sp(si, e) : ym + (ym - s.y0) * sp(si, e)]);
  }
  for (let k = 0; k < seg; k++) {
    const A = ring[k], B = ring[k + 1];
    if (front) pos.push(cx, ym, s.z, A[0], A[1], s.z, B[0], B[1], s.z); else pos.push(cx, ym, s.z, B[0], B[1], s.z, A[0], A[1], s.z);
  }
  const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); g.computeVertexNormals();
  return g;
}
// profil d'aile cambré (corde c, épaisseur t), extrudé sur l'envergure (axe x), bord d'attaque vers l'avant
function airfoil(c, t, span, camber = 0.06) {
  const sh = new THREE.Shape(), N = 10, up = [], lo = [];
  for (let i = 0; i <= N; i++) {
    const u = (1 - Math.cos(i / N * Math.PI)) / 2;   // points resserrés aux bords
    const th = 5 * t * (0.2969 * Math.sqrt(u) - 0.126 * u - 0.3516 * u * u + 0.2843 * u ** 3 - 0.1036 * u ** 4);
    const cm = camber * 4 * u * (1 - u);
    up.push([u * c, (cm + th) * c]); lo.push([u * c, (cm - th) * c]);
  }
  sh.moveTo(...up[0]); for (const p of up.slice(1)) sh.lineTo(...p);
  for (const p of lo.reverse().slice(1)) sh.lineTo(...p);
  const g = new THREE.ExtrudeGeometry(sh, { depth: span, bevelEnabled: false, curveSegments: 1 });
  g.rotateY(Math.PI / 2); g.translate(-span / 2, 0, 0);   // corde vers -z, envergure centrée sur x
  return g;
}
// plaque découpée (dérive, plaques latérales) dans le plan z-y, épaisseur sur x
function plate(pts, th) {
  const sh = new THREE.Shape(); sh.moveTo(pts[0][0], pts[0][1]); for (const p of pts.slice(1)) sh.lineTo(p[0], p[1]);
  const g = new THREE.ExtrudeGeometry(sh, { depth: th, bevelEnabled: false });
  g.rotateY(-Math.PI / 2); g.translate(th / 2, 0, 0);   // (u, v) → (z = u, y = v)
  return g;
}
// tube entre deux points (bras de suspension)
const UP = new THREE.Vector3(0, 1, 0);
function rod(a, b, r = 0.016) {
  const A = new THREE.Vector3(...a), B = new THREE.Vector3(...b), d = B.clone().sub(A);
  const g = new THREE.CylinderGeometry(r, r, d.length(), 4, 1, true);
  g.applyQuaternion(new THREE.Quaternion().setFromUnitVectors(UP, d.normalize()));
  g.translate((A.x + B.x) / 2, (A.y + B.y) / 2, (A.z + B.z) / 2);
  return g;
}

// forme de la monoplace selon le règlement en vigueur (reglement.js) : nez, ailerons, pontons, dérive
const SHAPE0 = { nose: 1, fwEl: 3, fwSpan: 1, pod: 1, podH: 1, fin: 1, rwW: 1, rwH: 0, beam: 1 };
export const CAR_SHAPE = { ...SHAPE0 };
let eraId = 'fondation';
const FORCE = new URLSearchParams(location.search).get('ere');   // test : ?ere=effet-sol (toutes les voitures de cette ère)
export function setCarShape(s, id) { Object.assign(CAR_SHAPE, SHAPE0, s || {}); eraId = FORCE || id || 'fondation'; }

// ---------- modèles Blender (une monoplace par ère : ERAS[k].id dans reglement.js) ----------
// Fichier binaire : 'GPMC', taille de l'en-tête JSON { nv, ni, ranges, meta.wheels }, positions int16 (1/8000 m),
// normales int8, indice de palette K par sommet (0 livrée, 1 accent, 2 casque, puis PAL), indices uint16.
const ERA_IDS = ['fondation', 'effet-sol', 'hybride', 'aero-active', 'securite'];
const PAL = [0, 0, 0, 0x17191d, 0x08090a, 0x0d1013, 0x1f9e55, 0x5d6c7c, 0xff2a2a, 0x2a2d33, 0x1a1a1c];
const MODELS = {};
function parseModel(buf) {
  const dv = new DataView(buf);
  if (dv.getUint32(0, true) !== 0x434d5047) return null;   // 'GPMC'
  const hl = dv.getUint32(4, true), head = JSON.parse(new TextDecoder().decode(new Uint8Array(buf, 8, hl)));
  const nv = head.nv, ni = head.ni; let o = 8 + hl;
  const qp = new Int16Array(buf, o, nv * 3); o += nv * 6;
  const qn = new Int8Array(buf, o, nv * 3); o += nv * 3;
  const k = new Uint8Array(buf, o, nv).slice(); o += nv; o += (4 - (nv * 10) % 4) % 4;
  const index = new Uint16Array(buf, o, ni).slice();
  const pos = new Float32Array(nv * 3), nor = new Float32Array(nv * 3);
  for (let i = 0; i < nv; i++) {
    const x = qn[i * 3], y = qn[i * 3 + 1], z = qn[i * 3 + 2], l = Math.hypot(x, y, z) || 1;
    nor[i * 3] = x / l; nor[i * 3 + 1] = y / l; nor[i * 3 + 2] = z / l;
    for (let a = 0; a < 3; a++) pos[i * 3 + a] = qp[i * 3 + a] / 8000;
  }
  // ailerons : plages de sommets (contiguës) pour les dégâts, et plages d'indices pour le morceau qui se détache
  const ranges = {}, iranges = head.ranges || {};
  for (const [t, [s, n]] of Object.entries(iranges)) {
    let a = 1e9, b = -1; for (let i = s; i < s + n; i++) { a = Math.min(a, index[i]); b = Math.max(b, index[i]); }
    ranges[t] = [a, b - a + 1];
  }
  const nAttr = new THREE.BufferAttribute(nor, 3), iAttr = new THREE.BufferAttribute(index, 1);
  return { pos, nAttr, iAttr, k, ranges, iranges, wheels: head.meta?.wheels };
}
// chargés au démarrage ; l'atelier se reconstruit quand ils arrivent (showroom.js)
export const modelsReady = Promise.all(ERA_IDS.map(id => fetch(`assets/cars/${id}.bin`)
  .then(r => r.ok ? r.arrayBuffer() : null).then(b => { const m = b && parseModel(b); if (m) MODELS[id] = m; }).catch(() => {})));

function modelMesh(md, color, accent, helmet) {
  const m = mats(), n = md.k.length;
  const hm = typeof helmet === 'object' && helmet, hf = hm && helmetFn(hm, 0, 0.84, 0.3, 0.15);
  const col = new Float32Array(n * 3), c = new THREE.Color();
  const fixed = [color, accent, hm ? 0 : helmet];
  for (let i = 0; i < n; i++) {
    const k = md.k[i];
    c.setHex(k === 2 && hf ? hf(md.pos[i * 3], md.pos[i * 3 + 1], md.pos[i * 3 + 2]) : k < 3 ? fixed[k] : PAL[k] ?? 0);
    col[i * 3] = c.r; col[i * 3 + 1] = c.g; col[i * 3 + 2] = c.b;
  }
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.BufferAttribute(md.pos.slice(), 3));   // propre à chaque voiture (ailerons tordus)
  geo.setAttribute('normal', md.nAttr);
  geo.setAttribute('color', new THREE.BufferAttribute(col, 3));
  geo.setIndex(md.iAttr);
  geo.computeBoundingSphere();
  const group = new THREE.Group();
  group.rotation.order = 'YXZ';
  group.add(new THREE.Mesh(geo, m.body));
  group.userData = { ranges: md.ranges, iranges: md.iranges, base: md.pos.slice() };
  group.add(new THREE.Mesh(wheels(md.wheels), [m.tread, m.side]));
  const sh = new THREE.Mesh(new THREE.PlaneGeometry(2.4, 5.9), m.blob);
  sh.rotation.x = -Math.PI / 2; sh.position.set(0, 0.03, 0.1); sh.renderOrder = 1;
  group.add(sh);
  return group;
}

export function buildCarMesh(color, accent, helmet) {
  if (FORCE) eraId = FORCE;
  if (MODELS[eraId]) return modelMesh(MODELS[eraId], color, accent, helmet);
  const m = mats(), S = CAR_SHAPE;
  const NZ = z => z > 2.1 ? 2.1 + (z - 2.1) * S.nose : z;   // nez plus long ou plus court
  const parts = [];
  let tag = null;   // 'fw' / 'rw' : ailerons avant / arrière (peuvent se tordre ou se détacher, voir debris.js)
  const add = (geo, col, x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0) => {
    if (geo.index) geo = geo.toNonIndexed();
    geo.deleteAttribute('uv');
    if (rx) geo.rotateX(rx); if (ry) geo.rotateY(ry); if (rz) geo.rotateZ(rz);
    geo.translate(x, y, z); parts.push([geo, col, tag]);
  };
  const carbon = 0x17191d, black = 0x08090a, dark = 0x2a2d33;
  // livrée : bas de caisse en carbone, bandeau de couleur d'accent sur l'arête du capot
  const livery = (x, y, z) => y < 0.235 ? carbon : (Math.abs(x) < 0.07 && y > 0.62 && z < 0.2 && z > -1.9) ? accent : color;
  const nose = (x, y, z) => z > NZ(3.38) ? accent : y < 0.21 ? carbon : color;

  // fond plat (contour vu de dessus) + bords relevés
  {
    const sh = new THREE.Shape(), o = [[0.25, 1.45], [0.62, 0.95], [0.82, 0.62], [0.84, -1.2], [0.6, -1.5], [0.55, -2.2], [-0.55, -2.2], [-0.6, -1.5], [-0.84, -1.2], [-0.82, 0.62], [-0.62, 0.95], [-0.25, 1.45]];
    sh.moveTo(o[0][0], o[0][1]); for (const p of o.slice(1)) sh.lineTo(p[0], p[1]);
    const g = new THREE.ExtrudeGeometry(sh, { depth: 0.035, bevelEnabled: false }); g.rotateX(Math.PI / 2);
    add(g, carbon, 0, 0.115, 0);
    for (const s of [-1, 1]) add(new THREE.BoxGeometry(0.018, 0.07, 1.8), carbon, s * 0.835, 0.13, -0.3);
  }
  // diffuseur
  add(new THREE.BoxGeometry(1.08, 0.025, 0.5), carbon, 0, 0.15, -2.35, -0.32);
  for (const x of [-0.5, -0.25, 0, 0.25, 0.5]) add(new THREE.BoxGeometry(0.014, 0.18, 0.5), carbon, x, 0.17, -2.33);

  // coque et nez
  const tub = [
    { z: 3.74, y0: 0.2, y1: 0.27, w: 0.1, n: 2.2 },
    { z: 3.62, y0: 0.18, y1: 0.31, w: 0.2, n: 2.4 },
    { z: 3.3, y0: 0.18, y1: 0.37, w: 0.27, wb: 0.22 },
    { z: 2.7, y0: 0.21, y1: 0.47, w: 0.33, wb: 0.25 },
    { z: 2.1, y0: 0.24, y1: 0.58, w: 0.42, wb: 0.32 },
    { z: 1.55, y0: 0.2, y1: 0.66, ym: 0.48, w: 0.56, wb: 0.42 },
    { z: 1.05, y0: 0.14, y1: 0.7, ym: 0.46, w: 0.7, wb: 0.5 },
    { z: 0.4, y0: 0.13, y1: 0.7, ym: 0.44, w: 0.8, wb: 0.56 },
    { z: -0.25, y0: 0.13, y1: 0.66, ym: 0.42, w: 0.78, wb: 0.56, n: 2.8 },
  ];
  for (const r of tub) r.z = NZ(r.z);
  add(loft(tub, 18).toNonIndexed(), nose); add(cap(tub[0], true), accent);
  // ouverture du cockpit + appuie-tête
  {
    const g = new THREE.CircleGeometry(1, 24); g.rotateX(-Math.PI / 2); g.scale(0.25, 1, 0.5);
    g.scale(0.9, 1, 1); add(g, black, 0, 0.685, 0.45);
    add(loft([{ z: 0.05, y0: 0.62, y1: 0.76, w: 0.56 }, { z: -0.12, y0: 0.62, y1: 0.78, w: 0.6 }, { z: -0.3, y0: 0.62, y1: 0.72, w: 0.5 }], 14), carbon);
  }
  // capot moteur et boîte à air
  const cover = [
    { z: 0.12, y0: 0.4, y1: 1.06, ym: 0.72, w: 0.42, n: 2.2 },
    { z: -0.12, y0: 0.36, y1: 1.13, ym: 0.7, w: 0.52, n: 2.4 },
    { z: -0.55, y0: 0.3, y1: 0.99, ym: 0.6, w: 0.58 },
    { z: -1.1, y0: 0.25, y1: 0.8, ym: 0.48, w: 0.52 },
    { z: -1.7, y0: 0.22, y1: 0.6, ym: 0.4, w: 0.36 },
    { z: -2.2, y0: 0.24, y1: 0.5, ym: 0.36, w: 0.2 },
    { z: -2.42, y0: 0.29, y1: 0.43, ym: 0.36, w: 0.08, n: 2 },
  ];
  add(loft(cover, 18).toNonIndexed(), livery);
  add(cap({ z: 0.125, y0: 0.9, y1: 1.03, w: 0.26, n: 2.4 }, true), black);   // prise d'air
  for (const s of [-1, 1]) add(cap({ z: 0.122, x: s * 0.13, y0: 0.62, y1: 0.78, w: 0.1, n: 2 }, true), black);
  // dérive (aileron de requin)
  if (S.fin === 1) add(plate([[-0.3, 1.02], [-1.6, 0.66], [-2.1, 0.55], [-2.1, 0.72], [-1.4, 0.9], [-0.6, 1.04]], 0.016), color);
  if (S.fin === 2) add(plate([[-0.3, 1.04], [-1.7, 0.8], [-2.25, 0.86 + S.rwH], [-2.25, 1.08 + S.rwH], [-1.4, 1.06], [-0.6, 1.1]], 0.016), color);
  // pontons avec contre-dépouille
  for (const s of [-1, 1]) {
    const pod = [
      { z: 0.88, x: s * 0.56, y0: 0.27, y1: 0.6, ym: 0.48, w: 0.38, wb: 0.22, n: 3.4 },
      { z: 0.72, x: s * 0.58, y0: 0.2, y1: 0.62, ym: 0.5, w: 0.48, wb: 0.24, n: 3.4 },
      { z: 0.2, x: s * 0.6, y0: 0.15, y1: 0.6, ym: 0.48, w: 0.5, wb: 0.22, n: 3.2 },
      { z: -0.5, x: s * 0.55, y0: 0.15, y1: 0.52, ym: 0.42, w: 0.42, wb: 0.2 },
      { z: -1.2, x: s * 0.44, y0: 0.15, y1: 0.4, ym: 0.32, w: 0.3, wb: 0.18 },
      { z: -1.85, x: s * 0.3, y0: 0.16, y1: 0.28, ym: 0.24, w: 0.14, wb: 0.1, n: 2.2 },
    ];
    for (const r of pod) { r.x *= 0.6 + 0.4 * S.pod; r.w *= S.pod; r.y1 = 0.15 + (r.y1 - 0.15) * S.podH; r.ym = 0.15 + (r.ym - 0.15) * S.podH; }
    add(loft(pod, 14).toNonIndexed(), livery);
    add(cap({ ...pod[0], z: 0.885, w: 0.3, y0: 0.31, y1: 0.57 }, true), black);
  }

  // halo (tube) et pilier central
  {
    const loop = new THREE.CatmullRomCurve3([[-0.29, 0.7, -0.02], [-0.29, 0.92, 0.22], [-0.24, 1.0, 0.55], [0, 1.03, 0.78], [0.24, 1.0, 0.55], [0.29, 0.92, 0.22], [0.29, 0.7, -0.02]].map(p => new THREE.Vector3(...p)));
    add(new THREE.TubeGeometry(loop, 16, 0.032, 5), carbon);
    const pil = new THREE.CatmullRomCurve3([[0, 1.03, 0.78], [0, 0.92, 0.98], [0, 0.72, 1.12]].map(p => new THREE.Vector3(...p)));
    add(new THREE.TubeGeometry(pil, 4, 0.028, 5), carbon);
  }
  // pilote : casque, visière, volant
  {
    // casque : couleur unie, ou { m (motif), p, s } pour le casque dessiné du joueur
    const hm = typeof helmet === 'object' && helmet;
    const h = new THREE.SphereGeometry(0.15, hm ? 18 : 12, hm ? 12 : 8); h.scale(1, 1.05, 1.12); add(h, hm ? helmetFn(hm, 0, 0.84, 0.3, 0.15) : helmet, 0, 0.84, 0.3);
    const v = new THREE.SphereGeometry(0.153, 10, 3, Math.PI / 2 - 0.9, 1.8, 1.25, 0.42); v.scale(1, 1.05, 1.12); add(v, 0x0d1013, 0, 0.84, 0.3);
    add(new THREE.BoxGeometry(0.25, 0.1, 0.03), carbon, 0, 0.74, 0.8, 0.25);
    add(new THREE.BoxGeometry(0.07, 0.035, 0.01), 0x1f9e55, 0, 0.745, 0.783, 0.25);
  }
  // rétroviseurs
  for (const s of [-1, 1]) {
    add(rod([s * 0.34, 0.68, 0.95], [s * 0.55, 0.79, 0.98], 0.012), carbon);
    const g = new THREE.SphereGeometry(0.1, 8, 5); g.scale(1.1, 0.5, 0.45); add(g, color, s * 0.6, 0.8, 0.98);
    add(new THREE.CircleGeometry(0.075, 12).scale(1.2, 0.45, 1), 0x5d6c7c, s * 0.6, 0.8, 0.93, 0, Math.PI);
  }
  // caméra embarquée (« T » au-dessus de la boîte à air)
  add(new THREE.BoxGeometry(0.2, 0.035, 0.06), accent, 0, 1.17, -0.12);

  // aileron avant : 3 éléments, plaques latérales
  tag = 'fw';
  const fs = S.fwSpan, dz = NZ(3.4) - 3.4;
  add(airfoil(0.42, 0.09, 1.92 * fs, -0.05), carbon, 0, 0.1, 3.66 + dz, 0.06);
  add(airfoil(0.26, 0.09, 1.78 * fs, -0.08), color, 0, 0.15, 3.3 + dz, 0.35);
  if (S.fwEl > 2) add(airfoil(0.18, 0.09, 1.6 * fs, -0.08), accent, 0, 0.22, 3.12 + dz, 0.7);
  if (S.fwEl > 3) add(airfoil(0.13, 0.09, 1.4 * fs, -0.08), color, 0, 0.29, 3.0 + dz, 0.95);
  for (const s of [-1, 1]) {
    add(plate([[3.72, 0.08], [3.72, 0.2], [3.45, 0.33], [2.98, 0.34], [2.92, 0.08]], 0.02), color, s * 0.97 * fs, 0, dz);
    add(new THREE.BoxGeometry(0.05, 0.02, 0.5), carbon, s * 0.94 * fs, 0.08, 3.3 + dz);
  }
  for (const s of [-1, 1]) add(new THREE.BoxGeometry(0.02, 0.14, 0.32), carbon, s * 0.09, 0.15, 3.42 + dz);   // supports sous le nez
  tag = null;

  // aileron arrière : plan principal, volet, plaques latérales, aileron de poutre, support central
  tag = 'rw';
  const rw = S.rwW, rh = S.rwH;
  add(airfoil(0.38, 0.1, 1.0 * rw, -0.07), carbon, 0, 0.88 + rh, -2.2, 0.15);
  add(airfoil(0.24, 0.09, 1.0 * rw, -0.08), accent, 0, 1.0 + rh, -2.52, 0.7);
  for (const s of [-1, 1]) {
    add(plate([[-2.18, 0.5], [-2.12, 0.98 + rh], [-2.22, 1.18 + rh], [-2.72, 1.18 + rh], [-2.76, 0.86 + rh], [-2.6, 0.5]], 0.022), color, s * 0.51 * rw, 0, 0);
  }
  if (S.beam) add(airfoil(0.2, 0.1, 0.88, -0.05), carbon, 0, 0.5, -2.32, 0.1);
  add(plate([[-2.32, 0.4], [-2.4, 0.95], [-2.55, 0.95], [-2.55, 0.4]], 0.04), carbon);
  tag = null;
  add(new THREE.BoxGeometry(0.11, 0.07, 0.03), 0xff2a2a, 0, 0.33, -2.62);   // feu de pluie

  // suspensions (triangles + biellettes) et porte-moyeux
  for (const s of [-1, 1]) {
    for (const [zf, zr, yy, wz, wx] of [[1.95, 1.55, 0.44, 1.75, 0.72], [1.95, 1.55, 0.27, 1.75, 0.72], [-1.3, -1.75, 0.44, -1.6, 0.68], [-1.3, -1.75, 0.25, -1.6, 0.68]]) {
      add(rod([s * 0.2, yy + 0.04, zf], [s * wx, yy, wz]), carbon);
      add(rod([s * 0.22, yy + 0.04, zr], [s * wx, yy, wz]), carbon);
    }
    add(rod([s * 0.22, 0.56, 1.7], [s * 0.7, 0.29, 1.75], 0.014), carbon);
    add(rod([s * 0.4, 0.5, -1.55], [s * 0.66, 0.27, -1.6], 0.014), carbon);
    for (const [z, r] of [[1.75, 0.33], [-1.6, 0.36]]) {
      const hub = new THREE.CylinderGeometry(0.1, 0.1, 0.12, 8, 1, true); hub.rotateZ(Math.PI / 2); add(hub, dark, s * 0.66, r, z);
    }
  }

  // assemblage en une géométrie à couleurs par sommet
  let n = 0; for (const [g] of parts) n += g.attributes.position.count;
  const pos = new Float32Array(n * 3), nor = new Float32Array(n * 3), col = new Float32Array(n * 3);
  const c = new THREE.Color(); let o = 0;
  const ranges = {};
  for (const [g, spec, t] of parts) {
    const cnt = g.attributes.position.count, P = g.attributes.position.array;
    if (t) { const r = ranges[t] || (ranges[t] = [o, 0]); r[1] = o + cnt - r[0]; }
    pos.set(P, o * 3); nor.set(g.attributes.normal.array, o * 3);
    if (typeof spec !== 'function') c.setHex(spec);
    for (let k = 0; k < cnt; k++) {
      if (typeof spec === 'function') c.setHex(spec(P[k * 3], P[k * 3 + 1], P[k * 3 + 2]));
      col.set([c.r, c.g, c.b], (o + k) * 3);
    }
    o += cnt;
  }
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  geo.setAttribute('normal', new THREE.BufferAttribute(nor, 3));
  geo.setAttribute('color', new THREE.BufferAttribute(col, 3));
  geo.computeBoundingSphere();

  const group = new THREE.Group();
  group.rotation.order = 'YXZ';
  group.add(new THREE.Mesh(geo, m.body));
  group.userData = { ranges, base: pos.slice() };
  group.add(new THREE.Mesh(wheels(), [m.tread, m.side]));
  const sh = new THREE.Mesh(new THREE.PlaneGeometry(2.4, 5.9), m.blob);
  sh.rotation.x = -Math.PI / 2; sh.position.set(0, 0.03, 0.1); sh.renderOrder = 1;
  group.add(sh);
  return group;
}

// Roues : pneu arrondi (révolution) + flancs/jantes texturés ; ax = [[voie, z, rayon, largeur] avant, arrière] selon le modèle
const WHEELS = {};
const AX0 = [[0.83, 1.75, 0.33, 0.35], [0.8, -1.6, 0.36, 0.43]];
function wheels(ax = AX0) {
  const key = JSON.stringify(ax);
  if (WHEELS[key]) return WHEELS[key];
  const treads = [], sides = [];
  for (const [x, z, r, w] of ax.flatMap(([t, z, r, w]) => [[t, z, r, w], [-t, z, r, w]])) {
    const ri = r * 0.66, b = 0.05, h = w / 2, prof = [];
    prof.push(new THREE.Vector2(ri, -h));
    for (let k = 0; k <= 2; k++) prof.push(new THREE.Vector2(r - b + b * Math.sin(Math.PI / 4 * k), -h + b - b * Math.cos(Math.PI / 4 * k)));
    for (let k = 0; k <= 2; k++) prof.push(new THREE.Vector2(r - b + b * Math.cos(Math.PI / 4 * k), h - b + b * Math.sin(Math.PI / 4 * k)));
    prof.push(new THREE.Vector2(ri, h));
    const t = new THREE.LatheGeometry(prof, 20); t.rotateZ(Math.PI / 2); t.translate(x, r, z); treads.push(t);
    for (const s of [-1, 1]) {
      const c = new THREE.CircleGeometry(r - b * 0.6, 20); c.rotateY(s * Math.PI / 2); c.translate(x + s * (h - 0.004), r, z); sides.push(c);
    }
  }
  const a = merge(treads), bb = merge(sides);
  const g = merge([a, bb]);
  g.addGroup(0, a.attributes.position.count, 0);
  g.addGroup(a.attributes.position.count, bb.attributes.position.count, 1);
  return (WHEELS[key] = g);
}
