// Déroulement d'une course : grille, feux de départ, IA, collisions, aspiration, tours, classement.
import { Car, stepCar, placeMesh } from './car.js';
import { aiDrive } from './ai.js';
import { readInput } from './input.js';
import { DS } from './track.js';
import { TEAMS } from './data.js';

export function createRace(track, scene, laps, opts = {}) {
  const cars = [];
  for (const team of TEAMS) for (const d of team.drivers) cars.push(new Car(team, d, team.perf));
  // grille : IA rangées par niveau, le joueur part en milieu de grille (ou à la place demandée)
  const player = cars.find(c => c.player);
  const ai = cars.filter(c => !c.player).sort((a, b) => b.skill * b.perf - a.skill * a.perf);
  const gridPos = Math.min(19, Math.max(0, (opts.grid ?? 10) - 1));
  ai.splice(gridPos, 0, player);
  const { N } = track;
  ai.forEach((c, k) => {
    const i = (N - 5 - k * 4) % N, s = k % 2 ? 1 : -1;
    c.idx = i; c.heading = track.H[i];
    c.x = track.X[i] - track.TZ[i] * s * 3; c.z = track.Z[i] + track.TX[i] * s * 3; c.y = track.Y[i];
    c.lat = s * 3; c.lane = s * 3; c.laneBias = (Math.random() - 0.5) * 1.5;
    c.grid = k + 1;
    placeMesh(c); scene.add(c.mesh);
  });
  return {
    track, cars, player, laps, order: ai.slice(),
    t: -1, phase: 'lights', lightsOn: 0, goAt: 4.2 + 0.4 + Math.random() * 0.8,
    autopilot: !!opts.autopilot, done: false,
  };
}

export function updateRace(R, dt) {
  const { track, cars } = R;
  const N = track.N;
  R.t += dt;
  // feux : un de plus toutes les 0,8 s puis extinction après un délai aléatoire
  if (R.phase === 'lights') {
    R.lightsOn = Math.max(0, Math.min(5, Math.floor((R.t) / 0.8) + 1));
    if (R.t >= R.goAt) { R.phase = 'race'; R.lightsOn = 0; R.raceStart = R.t; for (const c of cars) c.lapStart = R.t; }
  }
  const racing = R.phase !== 'lights';

  for (const c of cars) {
    if (c.player && !R.autopilot && !c.finished) readInput(c, dt);
    else aiDrive(c, track, cars, dt);
    c.canReverse = racing && c.player && !R.autopilot;
    if (!racing) { c.throttle = 0; c.brake = 1; }
    if (c.finished && c.v > 20) { c.throttle = 0; }
  }

  // aspiration : voiture devant à moins de 45 m, alignée
  for (const c of cars) {
    let s = 0;
    const fx = Math.sin(c.heading), fz = Math.cos(c.heading);
    for (const o of cars) {
      if (o === c) continue;
      const dx = o.x - c.x, dz = o.z - c.z, along = dx * fx + dz * fz;
      if (along < 4 || along > 45) continue;
      const side = Math.abs(dx * -fz + dz * fx);
      if (side < 2.2) s = Math.max(s, 1 - along / 45);
    }
    c.slipstream += (s - c.slipstream) * Math.min(1, dt * 3);
  }

  for (const c of cars) {
    const prev = stepCar(c, track, dt);
    c.hitWall = Math.max(0, (c.hitWall || 0) - dt);
    // passage de la ligne
    if (prev > N * 0.75 && c.idx < N * 0.25) {
      c.lap++;
      if (c.lap > 1) {
        const lt = R.t - c.lapStart;
        c.lastLap = lt; if (!c.bestLap || lt < c.bestLap) c.bestLap = lt;
      }
      c.lapStart = R.t;
      if (c.lap > R.laps && !c.finished) { c.finished = true; c.finishTime = R.t - R.raceStart; }
    } else if (prev < N * 0.25 && c.idx > N * 0.75) c.lap--;
  }

  collide(cars);
  for (const c of cars) placeMesh(c);

  // classement : tours puis distance parcourue (les arrivés par temps)
  const prog = c => c.finished ? 1e9 - c.finishTime : c.lap * N + c.idx;
  R.order = cars.slice().sort((a, b) => prog(b) - prog(a));
  if (R.player.finished && !R.done) { R.done = true; R.endT = R.t; }
}

// Collisions entre voitures : deux cercles par voiture (avant / arrière)
function collide(cars) {
  const r = 1.05, P = [1.4, -1.4];
  for (let a = 0; a < cars.length; a++) for (let b = a + 1; b < cars.length; b++) {
    const A = cars[a], B = cars[b];
    if (Math.abs(A.x - B.x) > 6 || Math.abs(A.z - B.z) > 6 || Math.abs(A.y - B.y) > 3) continue;
    const fa = [Math.sin(A.heading), Math.cos(A.heading)], fb = [Math.sin(B.heading), Math.cos(B.heading)];
    for (const pa of P) for (const pb of P) {
      const ax = A.x + fa[0] * pa, az = A.z + fa[1] * pa, bx = B.x + fb[0] * pb, bz = B.z + fb[1] * pb;
      let dx = bx - ax, dz = bz - az; const d = Math.hypot(dx, dz);
      if (d >= 2 * r || d === 0) continue;
      dx /= d; dz /= d;
      const push = (2 * r - d) / 2;
      A.x -= dx * push; A.z -= dz * push; B.x += dx * push; B.z += dz * push;
      const an = A.v * (fa[0] * dx + fa[1] * dz), bn = B.v * (fb[0] * dx + fb[1] * dz);
      const close = an - bn;
      if (close > 0) {
        const k = close * 0.55;
        A.v -= k * (fa[0] * dx + fa[1] * dz);
        B.v += k * (fb[0] * dx + fb[1] * dz);
        A.bump = B.bump = 0.25;
      }
    }
  }
}

// Résultats : arrivés par temps, autres estimés d'après leur retard
export function results(R) {
  const { track, cars } = R;
  const total = R.laps * track.N;
  const t = R.endT - R.raceStart;
  return R.order.map((c, k) => {
    let time = c.finishTime;
    if (!c.finished) {
      const done = (c.lap - 1) * track.N + c.idx, left = Math.max(0, total - done) * DS;
      time = t + left / 70;
    }
    return { pos: k + 1, name: c.name, team: c.team.name, color: c.team.color, player: c.player, time, best: c.bestLap, finished: c.finished };
  });
}
