// Voiture de sécurité : sort après un abandon ou un gros accident, regroupe le peloton,
// dépassements interdits (pénalité de 5 s pour le joueur), puis rentre au stand.
import * as THREE from '../lib/three.module.min.js';
import { Car, stepCar, placeMesh } from './car.js';
import { aiDrive } from './ai.js';
import { DS } from './track.js';

function buildScMesh() {
  const g = new THREE.Group();
  const body = new THREE.MeshStandardMaterial({ color: 0xe8eaee, metalness: 0.5, roughness: 0.3 });
  const dark = new THREE.MeshStandardMaterial({ color: 0x15181d, metalness: 0.3, roughness: 0.4 });
  const box = (w, h, l, x, y, z, m) => { const b = new THREE.Mesh(new THREE.BoxGeometry(w, h, l), m); b.position.set(x, y, z); g.add(b); return b; };
  box(1.95, 0.55, 4.5, 0, 0.55, 0, body);              // caisse
  box(1.6, 0.45, 2.0, 0, 1.05, -0.3, dark);             // habitacle vitré
  box(1.7, 0.08, 0.3, 0, 0.9, -2.2, dark);              // aileron
  for (const [x, z] of [[-0.9, 1.4], [0.9, 1.4], [-0.9, -1.4], [0.9, -1.4]]) box(0.3, 0.62, 0.62, x, 0.31, z, dark);
  // bande verte/jaune et rampe lumineuse
  box(1.97, 0.12, 4.52, 0, 0.62, 0, new THREE.MeshBasicMaterial({ color: 0x1ec94a }));
  const bar = box(1.2, 0.12, 0.25, 0, 1.33, -0.3, new THREE.MeshBasicMaterial({ color: 0xffb300 }));
  g.userData.bar = bar;
  return g;
}

export function requestSafety(R, why, force = false) {
  if (R.solo || R.sc || R.scUsed || R.phase !== 'race') return;
  const lead = R.order[0];
  if (!lead || (!force && (lead.lap - 1 + lead.idx / R.track.N) / R.laps > 0.8)) return;
  if (!force && Math.random() > (why === 'dnf' ? 0.75 : 0.35)) return;
  R.scUsed = true;
  const { track } = R, N = track.N;
  const car = new Car({ name: 'Sécurité', color: 0xffffff, accent: 0 }, { name: 'SC' }, 0.5);
  R.scene.remove(car.mesh); car.mesh = buildScMesh(); R.scene.add(car.mesh);
  const i = (lead.idx + 110) % N;
  Object.assign(car, { idx: i, heading: track.H[i], x: track.X[i], z: track.Z[i], y: track.Y[i], lat: 0, lane: 0, v: 40, skill: 0.56, up: null });
  placeMesh(car);
  R.sc = { car, t: 0, ending: 0 };
  R.radio.push('📻 Muret : voiture de sécurité ! Ralentis, interdiction de dépasser.');
}

export function updateSafety(R, dt) {
  const sc = R.sc;
  for (const c of R.cars) c.scMode = !!sc && !sc.ending;
  if (!sc) return;
  sc.t += dt;
  const car = sc.car, lead = R.order.find(c => !c.out && !c.pit);
  aiDrive(car, R.track, [], dt);
  stepCar(car, R.track, dt);
  placeMesh(car);
  car.mesh.userData.bar.visible = Math.floor(sc.t * 3) % 2 === 0;
  const run = R.order.filter(c => !c.out && !c.pit), N = R.track.N;
  const spread = run.length > 1 ? ((run[0].lap - run[run.length - 1].lap) * N + run[0].idx - run[run.length - 1].idx) * DS : 0;
  // fin : peloton regroupé derrière, ou trop long
  if (!sc.ending && ((sc.t > 25 && spread < run.length * 22) || sc.t > 75)) {
    sc.ending = 4;
    R.radio.push('📻 Muret : la voiture de sécurité rentre. Prépare la relance !');
  }
  if (sc.ending) {
    sc.ending -= dt;
    if (sc.ending <= 0) {
      R.scene.remove(car.mesh); R.sc = null;
      R.radio.push('📻 Muret : fin de la voiture de sécurité, c’est reparti !');
      return;
    }
  }
  // joueur : limiteur doux et pénalité s'il passe une voiture qui était devant lui
  const p = R.player, prog = c => ((c.lap * N + c.idx) * DS);
  if (!sc.ending && !p.pit && !p.out) {
    if (p.v > R.track.V[p.idx] * 0.72 && !R.autopilot) p.throttle = 0;
    if (!p.scAhead) p.scAhead = new Set(run.slice(0, run.indexOf(p)));
    for (const c of p.scAhead) {
      if (c.out || c.pit || c.dnf) { p.scAhead.delete(c); continue; }
      if (prog(p) - prog(c) > 8) {
        p.scAhead.delete(c);
        if (!R.autopilot) { p.penalty = (p.penalty || 0) + 5; R.radio.push('📻 Muret : dépassement sous voiture de sécurité, pénalité de 5 s !'); }
      }
    }
  } else p.scAhead = null;
}
