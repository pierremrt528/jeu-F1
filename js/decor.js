// Décors de circuit modélisés dans Blender (assets/decor/<nom>.bin, voir tools/blender/decors.py) : stands des écuries,
// motorhomes, panneaux publicitaires, passerelle, postes de commissaires, murs de pneus, tours de télévision, mâts
// d'éclairage et 3 styles d'immeubles. Chaque modèle est instancié (un appel de rendu par type) ; la teinte de l'instance
// (couleur de l'écurie, de la façade, de la bande des pneus) ne colore que les faces prévues (K = 0).
import * as THREE from '../lib/three.module.min.js';
import * as TX from './textures.js';

// palette des décors (props.py : PPAL), 0 = teinte de l'instance, 11 = panneau publicitaire
const PAL = [0xffffff, 0xf2f2ef, 0xb9bcc0, 0x3a3e44, 0x141619, 0x7f9db5, 0xd42a2a, 0xf2c21a, 0x1c1c1e, 0xe8742a, 0x2f8f4a, 0xffffff, 0x2a3a4a, 0xfff3c4, 0xc9b79c, 0x6d7278];
export const BUILDINGS = { verre: [8, 16, 32], logements: [5, 10, 18], ancien: [3, 5, 8] };
const NAMES = ['garage', 'motorhome', 'panneau', 'commissaires', 'pneus', 'camera', 'projecteur', 'pilier', 'tablier',
  ...Object.entries(BUILDINGS).flatMap(([k, fs]) => fs.map(f => `imm-${k}-${f}`))];
const GEO = {};

function parse(buf) {
  const dv = new DataView(buf);
  if (dv.getUint32(0, true) !== 0x434d5047) return null;   // 'GPMC'
  const hl = dv.getUint32(4, true), head = JSON.parse(new TextDecoder().decode(new Uint8Array(buf, 8, hl)));
  const nv = head.nv, ni = head.ni, q = head.q || 200; let o = 8 + hl;
  const qp = new Int16Array(buf, o, nv * 3); o += nv * 6;
  const qn = new Int8Array(buf, o, nv * 3); o += nv * 3;
  const K = new Uint8Array(buf, o, nv); o += nv; o += (4 - (nv * 10) % 4) % 4;
  const index = new Uint16Array(buf, o, ni).slice(); o += ni * 2; o += (4 - (ni * 2) % 4) % 4;
  const quv = new Int16Array(buf, o, nv * 2);
  const pos = new Float32Array(nv * 3), nor = new Float32Array(nv * 3), col = new Float32Array(nv * 3), tint = new Float32Array(nv), uv = new Float32Array(nv * 2);
  const c = new THREE.Color();
  for (let i = 0; i < nv; i++) {
    const x = qn[i * 3], y = qn[i * 3 + 1], z = qn[i * 3 + 2], l = Math.hypot(x, y, z) || 1;
    for (let a = 0; a < 3; a++) pos[i * 3 + a] = qp[i * 3 + a] / q;
    nor[i * 3] = x / l; nor[i * 3 + 1] = y / l; nor[i * 3 + 2] = z / l;
    c.setHex(PAL[K[i]] ?? 0xffffff); col.set([c.r, c.g, c.b], i * 3);
    tint[i] = K[i] === 0 ? 1 : 0;
    uv[i * 2] = quv[i * 2] / 1000; uv[i * 2 + 1] = quv[i * 2 + 1] / 1000;
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  g.setAttribute('normal', new THREE.BufferAttribute(nor, 3));
  g.setAttribute('color', new THREE.BufferAttribute(col, 3));
  g.setAttribute('tint', new THREE.BufferAttribute(tint, 1));
  g.setAttribute('uv', new THREE.BufferAttribute(uv, 2));
  g.setIndex(new THREE.BufferAttribute(index, 1));
  if (head.ad != null) { g.addGroup(0, head.ad, 0); g.addGroup(head.ad, ni - head.ad, 1); }
  g.computeBoundingSphere();
  return g;
}
// chargés avant la construction des circuits (6 s au plus) ; sans eux, le décor simple d'avant reste en place
await Promise.race([
  Promise.all(NAMES.map(n => fetch(`assets/decor/${n}.bin`).then(r => r.ok ? r.arrayBuffer() : null)
    .then(b => { const g = b && parse(b); if (g) GEO[n] = g; }).catch(() => {}))),
  new Promise(ok => setTimeout(ok, 6000)),
]);
export const hasDecor = n => !!GEO[n];

// teinte par instance limitée aux faces K = 0, décalage de la marque sur les panneaux (adOff)
function patch(m, ad) {
  m.onBeforeCompile = s => {
    s.vertexShader = 'attribute float tint;\n' + (ad ? 'attribute float adOff;\n' : '') + s.vertexShader
      .replace('#include <color_vertex>', `#include <color_vertex>
#ifdef USE_INSTANCING_COLOR
#ifdef USE_COLOR
  vColor.xyz = color.xyz * mix(vec3(1.0), instanceColor.xyz, tint);
#else
  vColor.xyz = mix(vec3(1.0), instanceColor.xyz, tint);
#endif
#endif`)
      .replace('#include <uv_vertex>', ad ? '#include <uv_vertex>\n#ifdef USE_MAP\n  vMapUv.x += adOff;\n#endif' : '#include <uv_vertex>');
  };
  m.customProgramCacheKey = () => 'gpm-decor' + (ad ? '-ad' : '');
  return m;
}
let MATS = null;
function mats() {
  if (MATS) return MATS;
  const ad = n => { const t = TX.adBoards(); t.repeat.set(n / 8, 1); return patch(new THREE.MeshLambertMaterial({ map: t }), true); };
  MATS = { body: patch(new THREE.MeshLambertMaterial({ vertexColors: true }), false), ad1: ad(1), ad4: ad(4) };
  return MATS;
}

// items : [{ x, y, z, ry, sx, sy, sz, color, ad }] → InstancedMesh ajouté au groupe
const _m = new THREE.Matrix4(), _q = new THREE.Quaternion(), _e = new THREE.Euler(), _c = new THREE.Color(), _v = new THREE.Vector3(), _s = new THREE.Vector3();
export function instances(group, name, items) {
  const g = GEO[name]; if (!g || !items.length) return null;
  const M = mats(), geo = g.clone();
  const hasAd = g.groups.length > 0;
  if (hasAd) geo.setAttribute('adOff', new THREE.InstancedBufferAttribute(new Float32Array(items.map(it => (it.ad ?? 0) / 8)), 1));
  const mesh = new THREE.InstancedMesh(geo, hasAd ? [M.body, name === 'tablier' ? M.ad4 : M.ad1] : M.body, items.length);
  items.forEach((it, k) => {
    _e.set(0, it.ry || 0, 0); _q.setFromEuler(_e);
    _m.compose(_v.set(it.x, it.y, it.z), _q, _s.set(it.sx ?? 1, it.sy ?? 1, it.sz ?? 1));
    mesh.setMatrixAt(k, _m);
    mesh.setColorAt(k, _c.set(it.color ?? 0xffffff));
  });
  mesh.computeBoundingSphere();
  group.add(mesh);
  return mesh;
}
