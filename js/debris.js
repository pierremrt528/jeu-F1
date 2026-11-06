// Dégâts visibles : éclats de carbone qui volent et restent sur la piste (1 maillage instancié pour tous),
// ailerons qui se tordent puis se détachent (le morceau rebondit sur la piste), remis à neuf au stand.
import * as THREE from '../lib/three.module.min.js';

const MAX = 160, LIFE = 25;
let bits = null, scene = null;
const P = new Float32Array(MAX * 3), V = new Float32Array(MAX * 3), ROT = new Float32Array(MAX * 3), W = new Float32Array(MAX * 3);
const FLOOR = new Float32Array(MAX), T = new Float32Array(MAX);
let next = 0;
const loose = [];   // ailerons détachés
const o3 = new THREE.Object3D(), col = new THREE.Color();

export function initDebris(sc) {
  scene = sc;
  if (bits) { scene.add(bits); return; }
  const g = new THREE.BoxGeometry(0.16, 0.025, 0.22);
  bits = new THREE.InstancedMesh(g, new THREE.MeshLambertMaterial(), MAX);
  bits.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
  bits.frustumCulled = false;
  o3.position.set(0, -1e4, 0); o3.updateMatrix();
  for (let i = 0; i < MAX; i++) { bits.setMatrixAt(i, o3.matrix); bits.setColorAt(i, col.setHex(0x1c1e22)); }
  scene.add(bits);
}

export function clearDebris() {
  T.fill(0);
  for (const l of loose) scene?.remove(l.mesh);
  loose.length = 0;
  if (bits) { o3.position.set(0, -1e4, 0); o3.updateMatrix(); for (let i = 0; i < MAX; i++) bits.setMatrixAt(i, o3.matrix); bits.instanceMatrix.needsUpdate = true; }
}

// éclats projetés depuis l'avant ou l'arrière de la voiture
export function spawnDebris(c, n, rear) {
  if (!bits) return;
  const fx = Math.sin(c.heading), fz = Math.cos(c.heading), off = rear ? -2.4 : 3.1;
  for (let k = 0; k < n; k++) {
    const i = next, s = (Math.random() - 0.5) * 2; next = (next + 1) % MAX;
    P.set([c.x + fx * off + fz * s, c.y + 0.3, c.z + fz * off - fx * s], i * 3);
    const sp = 0.35 + Math.random() * 0.5;
    V.set([fx * c.v * sp + (Math.random() - 0.5) * 9, 2 + Math.random() * 5, fz * c.v * sp + (Math.random() - 0.5) * 9], i * 3);
    W.set([(Math.random() - 0.5) * 25, (Math.random() - 0.5) * 25, (Math.random() - 0.5) * 25], i * 3);
    FLOOR[i] = c.y + 0.03; T[i] = LIFE;
    bits.setColorAt(i, col.setHex(Math.random() < 0.55 ? 0x1c1e22 : Math.random() < 0.6 ? c.team.color : c.team.accent));
  }
  bits.instanceColor.needsUpdate = true;
}

// sommets d'un aileron dans la géométrie de la voiture (fusionnée)
function wingVerts(c, part) {
  const ud = c.mesh.userData, r = ud.ranges?.[part];
  return r ? { r, ud, geo: c.mesh.children[0].geometry } : null;
}

// aileron tordu d'un côté (dégâts partiels)
export function bendWing(c, part, side) {
  const w = wingVerts(c, part); if (!w) return;
  const p = w.geo.attributes.position, b = w.ud.base, [s, n] = w.r;
  for (let k = s; k < s + n; k++) {
    const x = b[k * 3], f = Math.max(0, x * side) / 1;
    p.setXYZ(k, x, b[k * 3 + 1] - 0.13 * f * f + 0.02 * f, b[k * 3 + 2] - (part === 'fw' ? 0.18 : -0.1) * f * f);
  }
  p.needsUpdate = true;
}

// l'aileron se détache : il disparaît de la voiture et part rebondir sur la piste
export function detachWing(c, part) {
  const w = wingVerts(c, part); if (!w) return;
  const p = w.geo.attributes.position, nrm = w.geo.attributes.normal, cl = w.geo.attributes.color, [s, n] = w.r;
  const pos = new Float32Array(n * 3), nor = new Float32Array(n * 3), cc = new Float32Array(n * 3);
  let cx = 0, cy = 0, cz = 0;
  for (let k = 0; k < n; k++) { cx += p.getX(s + k); cy += p.getY(s + k); cz += p.getZ(s + k); }
  cx /= n; cy /= n; cz /= n;
  for (let k = 0; k < n; k++) {
    pos.set([p.getX(s + k) - cx, p.getY(s + k) - cy, p.getZ(s + k) - cz], k * 3);
    nor.set([nrm.getX(s + k), nrm.getY(s + k), nrm.getZ(s + k)], k * 3);
    cc.set([cl.getX(s + k), cl.getY(s + k), cl.getZ(s + k)], k * 3);
    p.setXYZ(s + k, 0, 0.3, 0);   // sommets confondus : plus rien n'est dessiné
  }
  p.needsUpdate = true;
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  g.setAttribute('normal', new THREE.BufferAttribute(nor, 3));
  g.setAttribute('color', new THREE.BufferAttribute(cc, 3));
  const mesh = new THREE.Mesh(g, c.mesh.children[0].material);
  c.mesh.updateMatrixWorld();
  mesh.position.set(cx, cy, cz).applyMatrix4(c.mesh.matrixWorld);
  mesh.rotation.set(0, c.heading, 0);
  scene.add(mesh);
  const fx = Math.sin(c.heading), fz = Math.cos(c.heading), sp = part === 'fw' ? 0.55 : 0.4, side = (Math.random() - 0.5) * 8;
  loose.push({ mesh, t: 30, floor: c.y + 0.05, v: [fx * c.v * sp + fz * side, 3 + Math.random() * 4, fz * c.v * sp - fx * side], w: [(Math.random() - 0.5) * 12, (Math.random() - 0.5) * 10, (Math.random() - 0.5) * 12] });
  if (loose.length > 6) scene.remove(loose.shift().mesh);
  spawnDebris(c, 14, part === 'rw');
}

// réparation au stand : ailerons neufs
export function repairCar(c) {
  const ud = c.mesh.userData; if (!ud.base) return;
  const p = c.mesh.children[0].geometry.attributes.position;
  p.array.set(ud.base); p.needsUpdate = true;
}

export function updateDebris(dt) {
  if (!bits || dt <= 0) return;
  let any = false;
  for (let i = 0; i < MAX; i++) {
    if (T[i] <= 0) continue;
    any = true; T[i] -= dt;
    const o = i * 3;
    if (T[i] <= 0) { o3.position.set(0, -1e4, 0); o3.scale.setScalar(1); o3.updateMatrix(); bits.setMatrixAt(i, o3.matrix); continue; }
    if (P[o + 1] > FLOOR[i] || V[o + 1] > 0) {
      V[o + 1] -= 9.81 * dt;
      for (let a = 0; a < 3; a++) { P[o + a] += V[o + a] * dt; ROT[o + a] += W[o + a] * dt; }
      if (P[o + 1] < FLOOR[i]) {   // rebond puis glissade
        P[o + 1] = FLOOR[i];
        if (V[o + 1] < -2) { V[o + 1] *= -0.3; V[o] *= 0.6; V[o + 2] *= 0.6; W[o] *= 0.5; W[o + 2] *= 0.5; }
        else { V[o + 1] = 0; ROT[o] = 0; ROT[o + 2] = 0; }
      }
    } else if (V[o] || V[o + 2]) {
      const f = Math.max(0, 1 - 6 * dt); V[o] *= f; V[o + 2] *= f; W[o + 1] *= f;
      P[o] += V[o] * dt; P[o + 2] += V[o + 2] * dt; ROT[o + 1] += W[o + 1] * dt;
      if (Math.abs(V[o]) + Math.abs(V[o + 2]) < 0.05) V[o] = V[o + 2] = 0;
    } else continue;
    o3.position.set(P[o], P[o + 1], P[o + 2]); o3.rotation.set(ROT[o], ROT[o + 1], ROT[o + 2]);
    o3.scale.setScalar(T[i] < 2 ? T[i] / 2 : 1); o3.updateMatrix();
    bits.setMatrixAt(i, o3.matrix);
  }
  if (any) bits.instanceMatrix.needsUpdate = true;
  for (let k = loose.length - 1; k >= 0; k--) {
    const l = loose[k], m = l.mesh;
    l.t -= dt;
    if (l.t <= 0) { scene.remove(m); loose.splice(k, 1); continue; }
    if (m.position.y > l.floor || l.v[1] > 0) {
      l.v[1] -= 9.81 * dt;
      m.position.x += l.v[0] * dt; m.position.y += l.v[1] * dt; m.position.z += l.v[2] * dt;
      m.rotation.x += l.w[0] * dt; m.rotation.y += l.w[1] * dt; m.rotation.z += l.w[2] * dt;
      if (m.position.y < l.floor) {
        m.position.y = l.floor;
        if (l.v[1] < -2.5) { l.v[1] *= -0.35; l.v[0] *= 0.55; l.v[2] *= 0.55; l.w = l.w.map(x => x * 0.5); }
        else { l.v[1] = 0; const rx = ((m.rotation.x % 6.283) + 6.283) % 6.283; m.rotation.x = rx > 1.57 && rx < 4.71 ? Math.PI : 0; m.rotation.z = 0; }
      }
    } else if (Math.abs(l.v[0]) + Math.abs(l.v[2]) > 0.05) {
      const f = Math.max(0, 1 - 2.5 * dt); l.v[0] *= f; l.v[2] *= f;
      m.position.x += l.v[0] * dt; m.position.z += l.v[2] * dt; m.rotation.y += l.w[1] * dt * f; l.w[1] *= f;
    }
  }
}
