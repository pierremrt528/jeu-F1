// Effets : gerbes d'eau derrière les voitures sous la pluie, fumée des pneus qui glissent.
// Un seul nuage de points partagé (1 appel de rendu), particules recyclées.
import * as THREE from '../lib/three.module.min.js';

const MAX = 500;
const pos = new Float32Array(MAX * 3), vel = new Float32Array(MAX * 3), life = new Float32Array(MAX);
let pts = null, next = 0;

function puff() {
  const cv = document.createElement('canvas'); cv.width = cv.height = 32;
  const c = cv.getContext('2d'), g = c.createRadialGradient(16, 16, 0, 16, 16, 16);
  g.addColorStop(0, 'rgba(255,255,255,1)'); g.addColorStop(1, 'rgba(255,255,255,0)');
  c.fillStyle = g; c.fillRect(0, 0, 32, 32);
  return new THREE.CanvasTexture(cv);
}
export function initFx(scene) {
  const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  pts = new THREE.Points(g, new THREE.PointsMaterial({ map: puff(), color: 0xdfe6ee, size: 2.6, transparent: true, opacity: 0.35, depthWrite: false }));
  pts.frustumCulled = false;
  for (let i = 0; i < MAX; i++) pos[i * 3 + 1] = -1e4;
  scene.add(pts);
}
function spawn(x, y, z, vx, vy, vz, t) {
  const i = next; next = (next + 1) % MAX;
  pos.set([x, y, z], i * 3); vel.set([vx, vy, vz], i * 3); life[i] = t;
}

export function updateFx(R, cam, dt) {
  if (!pts) return;
  pts.visible = !!R;
  if (!R) return;
  for (const c of R.cars) {
    if (c.out || Math.abs(c.v) < 12) continue;
    const dx = c.x - cam.x, dz = c.z - cam.z;
    if (dx * dx + dz * dz > 110 * 110) continue;
    const fx = Math.sin(c.heading), fz = Math.cos(c.heading);
    const spray = R.wet > 0.3 && Math.random() < R.wet * Math.min(1, c.v / 50);
    const smoke = c.sliding && R.wet < 0.3 && c.v > 15 && Math.random() < 0.6;
    if (!spray && !smoke) continue;
    for (let n = 0; n < (spray ? 2 : 1); n++) {
    const s = (Math.random() - 0.5) * 1.6;
    spawn(c.x - fx * 2.3 + fz * s, c.y + 0.4, c.z - fz * 2.3 - fx * s,
      fx * c.v * 0.55 + (Math.random() - 0.5) * 2, 1.5 + Math.random() * 2, fz * c.v * 0.55 + (Math.random() - 0.5) * 2, spray ? 0.5 : 0.9);
    }
  }
  for (let i = 0; i < MAX; i++) {
    if (life[i] <= 0) continue;
    life[i] -= dt;
    const o = i * 3;
    if (life[i] <= 0) { pos[o + 1] = -1e4; continue; }
    pos[o] += vel[o] * dt; pos[o + 1] += vel[o + 1] * dt; pos[o + 2] += vel[o + 2] * dt;
    vel[o] *= 0.93; vel[o + 2] *= 0.93;
  }
  pts.geometry.attributes.position.needsUpdate = true;
}
