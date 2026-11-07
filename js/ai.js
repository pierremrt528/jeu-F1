// Pilotes IA : suivent la trajectoire (poursuite pure), respectent le profil de vitesse,
// et se décalent pour dépasser quand une voiture plus lente est devant.
import { SPEC } from './car.js';
import { DS } from './track.js';

export function aiDrive(car, track, cars, dt) {
  const { N, X, Z, TZ, TX, V, RL, width } = track;
  const v = Math.max(car.v, 1);

  // voiture devant sur la même portion de piste ?
  let ahead = null, gap = 1e9;
  for (const o of cars) {
    if (o === car || o.out || o.pit) continue;
    const d = ((o.idx - car.idx + N) % N) * DS;
    if (d > 0 && d < 40 && Math.abs(o.lat - car.lat) < (car.scMode ? 99 : 2.6) && d < gap) { gap = d; ahead = o; }
  }
  car.passTimer -= dt;
  const lim = width / 2 - 1.4;
  let laneTarget = RL[car.idx] * 0.85 + car.laneBias;
  let vCap = Infinity;
  if (car.scMode) {
    // voiture de sécurité : file indienne, pas de dépassement
    if (ahead) vCap = ahead.v + (gap - 16) * 0.6;
  } else if (ahead && (ahead.v < car.v + 3 || gap < 14)) {
    if (car.passTimer <= 0) {
      const left = ahead.lat - 3.4, right = ahead.lat + 3.4;
      car.passSide = Math.abs(left) < Math.abs(right) ? -1 : 1;
      car.passTimer = 2.5;
    }
    laneTarget = ahead.lat + car.passSide * 3.4;
    if (gap < 12) vCap = ahead.v - 0.5 + gap * 0.15; // ne pas percuter
  }
  // consigne d'équipe : l'équipier n°2 s'écarte et lève le pied quand le joueur le suit de près
  const p = car.yieldTo;
  car.yielding = false;
  if (p && car.lap >= 2 && !car.scMode && !p.out && !p.pit && !p.finished) {
    const d = car.lap * N + car.idx - (p.lap * N + p.idx);
    car.yieldCd = Math.max(0, (car.yieldCd || 0) - dt);
    if (d > 0 && d < 16 && car.yieldCd <= 0) {
      car.yielding = true; laneTarget = (p.lat >= car.lat ? -1 : 1) * lim;
      car.yieldRun = (car.yieldRun || 0) + dt;
      if (car.yieldRun > 7) { car.yieldCd = 40; car.yieldRun = 0; }   // le joueur n'arrive pas à passer : il reprend son rythme
    } else if (d <= 0) car.yieldRun = 0;
  }
  laneTarget = Math.max(-lim, Math.min(lim, laneTarget));
  car.lane += (laneTarget - car.lane) * Math.min(1, dt * 1.5);

  car.steer = pursuit(car, track, car.lane);

  // vitesse cible
  let vt = V[car.idx];
  for (let k = 1; k <= 4; k++) vt = Math.min(vt, V[(car.idx + k) % N]);
  vt = Math.min(vt * (car.scMode ? (ahead ? 0.72 : 0.9) : 1) * car.skill * Math.sqrt((car.gripMul ?? 1) / (1 + ((car.mass || 1) - 1) * 0.25) * (0.4 + 0.6 * (car.dmgKd ?? 1))), vCap);
  if (car.yielding) vt *= 0.9;
  if (car.offTrack) vt = Math.min(vt, 25);
  if (car.v < vt - 0.8) { car.throttle = 1; car.brake = 0; }
  else if (car.v > vt + 1.2) { car.throttle = 0; car.brake = Math.min(1, (car.v - vt) / 6); }
  else { car.throttle = 0.45; car.brake = 0; }
}

// direction : poursuite pure vers un point devant sur la trajectoire (décalée de `lane`)
export function pursuit(car, track, lane) {
  const { N, X, Z, TZ, TX } = track, v = Math.max(car.v, 1);
  const Ld = 9 + v * 0.5;
  const j = (car.idx + Math.round(Ld / DS)) % N;
  const tx = X[j] - TZ[j] * lane, tz = Z[j] + TX[j] * lane;
  const dx = tx - car.x, dz = tz - car.z;
  const fx = Math.sin(car.heading), fz = Math.cos(car.heading);
  const lx = dx * -fz + dz * fx, lz = dx * fx + dz * fz;  // repère voiture : x vers la droite
  const alpha = Math.atan2(lx, lz);
  const kWant = 2 * Math.sin(alpha) / Math.hypot(dx, dz);
  const kMax = Math.tan(SPEC.steerMax / (1 + v / 20)) / SPEC.wb;
  return Math.max(-1, Math.min(1, kWant / kMax));
}

// Aide à la trajectoire du joueur : 1 = freine avant les virages trop rapides,
// 2 = en plus, garde la voiture sur sa ligne quand on ne braque pas (la ligne suit le joueur quand il braque)
export function driveAssist(car, track, dt) {
  const { N, V, RL, width } = track;
  if (car.pit || car.spinT > 0) return;
  let vt = V[car.idx];
  for (let k = 1; k <= 6; k++) vt = Math.min(vt, V[(car.idx + k) % N]);
  vt *= 1.04 * Math.sqrt((car.gripMul ?? 1) * (0.4 + 0.6 * (car.dmgKd ?? 1)));
  if (car.v > vt + 1.5) { car.brake = Math.max(car.brake, Math.min(1, (car.v - vt) / 6)); car.throttle = 0; }
  if (car.assist.line < 2) return;
  const lim = width / 2 - 1.2;
  if (car.aLane == null) car.aLane = car.lat;
  if (Math.abs(car.steer) > 0.15) car.aLane = car.lat;
  else car.aLane += (RL[car.idx] * 0.85 - car.aLane) * Math.min(1, dt * 0.6);
  car.aLane = Math.max(-lim, Math.min(lim, car.aLane));
  const pp = pursuit(car, track, car.aLane);
  car.steer = Math.abs(car.steer) > 0.15 ? car.steer * 0.8 + pp * 0.2 : pp;
}
