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
    if (d > 0 && d < 40 && Math.abs(o.lat - car.lat) < 2.6 && d < gap) { gap = d; ahead = o; }
  }
  car.passTimer -= dt;
  const lim = width / 2 - 1.4;
  let laneTarget = RL[car.idx] * 0.85 + car.laneBias;
  let vCap = Infinity;
  if (ahead && (ahead.v < car.v + 3 || gap < 14)) {
    if (car.passTimer <= 0) {
      const left = ahead.lat - 3.4, right = ahead.lat + 3.4;
      car.passSide = Math.abs(left) < Math.abs(right) ? -1 : 1;
      car.passTimer = 2.5;
    }
    laneTarget = ahead.lat + car.passSide * 3.4;
    if (gap < 12) vCap = ahead.v - 0.5 + gap * 0.15; // ne pas percuter
  }
  laneTarget = Math.max(-lim, Math.min(lim, laneTarget));
  car.lane += (laneTarget - car.lane) * Math.min(1, dt * 1.5);

  // direction : poursuite pure vers un point devant sur la trajectoire
  const Ld = 9 + v * 0.5;
  const j = (car.idx + Math.round(Ld / DS)) % N;
  const tx = X[j] - TZ[j] * car.lane, tz = Z[j] + TX[j] * car.lane;
  const dx = tx - car.x, dz = tz - car.z;
  const fx = Math.sin(car.heading), fz = Math.cos(car.heading);
  const lx = dx * -fz + dz * fx, lz = dx * fx + dz * fz;  // repère voiture : x vers la droite
  const alpha = Math.atan2(lx, lz);
  const kWant = 2 * Math.sin(alpha) / Math.hypot(dx, dz);
  const kMax = Math.tan(SPEC.steerMax / (1 + v / 20)) / SPEC.wb;
  car.steer = Math.max(-1, Math.min(1, kWant / kMax));

  // vitesse cible
  let vt = V[car.idx];
  for (let k = 1; k <= 4; k++) vt = Math.min(vt, V[(car.idx + k) % N]);
  vt = Math.min(vt * car.skill * Math.sqrt((car.gripMul ?? 1) * (0.4 + 0.6 * (car.dmgKd ?? 1))), vCap);
  if (car.offTrack) vt = Math.min(vt, 25);
  if (car.v < vt - 0.8) { car.throttle = 1; car.brake = 0; }
  else if (car.v > vt + 1.2) { car.throttle = 0; car.brake = Math.min(1, (car.v - vt) / 6); }
  else { car.throttle = 0.45; car.brake = 0; }
}
