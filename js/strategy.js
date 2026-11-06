// Stratégie de course : formats (sprint / long), pneus (gommes, usure accentuée par les glissades),
// carburant (poids, consommation, ravitaillement), météo, plans d'arrêts proposés, arrêts au stand,
// radio avec le muret, dégâts et casse.
import { DS, pitLane } from './track.js';

// km : distance avant usure complète à rythme normal sans glisser
export const TYRES = {
  S: { name: 'Tendre', color: '#ff3b3b', grip: 1.06, km: 18, pace: 0 },
  M: { name: 'Medium', color: '#ffd400', grip: 1.0, km: 30, pace: 0.9 },
  H: { name: 'Dur', color: '#f2f2f2', grip: 0.955, km: 45, pace: 1.7 },
  W: { name: 'Pluie', color: '#2f8cff', grip: 0.86, km: 30, pace: 0 },
};
export const FORMATS = {
  sprint: { name: 'Sprint', minutes: 11, prize: 1 },
  long: { name: 'Long', minutes: 22, prize: 1.6 },
};
export const FPK = 1.7, TANK = 110;   // carburant : kg par km, réservoir (kg)
export const MODES = {
  push: { name: 'Attaque', v: 1.012, a: 1.04, fuel: 1.15, wear: 1.3 },
  norm: { name: 'Normal', v: 1, a: 1, fuel: 1, wear: 1 },
  save: { name: 'Économie', v: 0.985, a: 0.94, fuel: 0.8, wear: 0.75 },
};
export const lapsFor = (lapTime, format) => Math.max(3, Math.min(40, Math.round(FORMATS[format].minutes * 60 / (lapTime * 1.12))));
export const fuelFor = (laps, lapKm) => Math.ceil(laps * lapKm * FPK * 1.03 + 2);

// ---------- météo ----------
export function forecast(theme, rnd = Math.random) {
  if (theme.surface === 'sand') return { rain: false, change: null };
  const r = rnd();
  if (r < 0.55) return { rain: false, change: null };
  if (r < 0.7) return { rain: true, change: null };
  if (r < 0.88) return { rain: false, change: { at: 0.25 + rnd() * 0.4, rain: true } };
  return { rain: true, change: { at: 0.3 + rnd() * 0.35, rain: false } };
}
export function wxText(wx) {
  if (!wx.change) return wx.rain ? '🌧️ Pluie' : '☀️ Sec';
  const p = Math.round(wx.change.at * 100);
  return wx.rain ? `🌦️ Pluie, éclaircie vers ${p} % de la course` : `🌦️ Sec, averse attendue vers ${p} % de la course`;
}

// adhérence des pneus selon la gomme, l'usure et l'humidité de la piste (0 sec … 1 détrempée)
export function tyreGrip(car, wet) {
  const T = TYRES[car.tyre], w = Math.min(car.wear, 1);
  let g = (T.grip - (car.tyre === 'W' ? 0.04 * wet : 0)) * (1 - 0.16 * w * w);
  if (car.wear > 1) g *= Math.max(0.7, 1 - (car.wear - 1) * 0.6);   // pneus à bout
  if (car.tyre !== 'W') g *= 1 - 0.45 * wet;
  return g;
}

// ---------- plans d'arrêts ----------
const softest = km => ['S', 'M', 'H'].find(t => TYRES[t].km >= km * 1.15);
const harder = t => ({ S: 'M', M: 'H', H: 'H', W: 'W' })[t];
function estimate(st, lapKm, lapTime) {
  let t = 0;
  st.forEach((s, i) => {
    const f = fuelFor(s.laps, lapKm), wearEnd = s.laps * lapKm / TYRES[s.tyre].km;
    t += s.laps * (lapTime + TYRES[s.tyre].pace + 0.035 * f / 2 + 1.2 * wearEnd * wearEnd / 2);
    if (i) t += 22 + 2.4 + f / 10;
  });
  return t;
}
// Trois stratégies (prudente, équilibrée, agressive) selon le nombre de tours, la longueur du circuit et la météo
export function makePlans(laps, lapKm, lapTime, wet) {
  const pool = [];
  for (let n = 0; n <= 4; n++) {
    const L = Array.from({ length: n + 1 }, (_, i) => Math.floor(laps / (n + 1)) + (i < laps % (n + 1) ? 1 : 0));
    if (L.some(l => l < 2) || L.some(l => fuelFor(l, lapKm) > TANK)) continue;
    const soft = L.map(l => softest(l * lapKm));
    if (soft.includes(undefined)) continue;
    for (const tyres of [soft, soft.map(t => TYRES[harder(t)].km >= 0 ? harder(t) : t)]) {
      const st = L.map((l, i) => ({ laps: l, tyre: wet && i === 0 ? 'W' : tyres[i] }));
      const sig = st.map(s => s.tyre + s.laps).join('-');
      if (!pool.some(p => p.sig === sig)) pool.push({ n, st, sig, time: estimate(st, lapKm, lapTime) });
    }
  }
  if (!pool.length) pool.push({ n: 0, st: [{ laps, tyre: wet ? 'W' : 'H' }], sig: 'x', time: laps * lapTime });
  const byTime = pool.slice().sort((a, b) => a.time - b.time);
  const minN = Math.min(...pool.map(p => p.n));
  const safe = pool.filter(p => p.n === minN).sort((a, b) => b.time - a.time)[0];   // moins d'arrêts, gommes dures
  const bal = byTime.find(p => p !== safe) || safe;
  const agg = pool.filter(p => p !== safe && p !== bal).sort((a, b) => b.n - a.n || a.time - b.time)[0] || bal;
  const out = [['Prudente', 'Peu d’arrêts, pneus durables, voiture lourde au départ.', safe], ['Équilibrée', 'Compromis entre arrêts et usure des pneus.', bal], ['Agressive', 'Plus d’arrêts, pneus tendres, voiture légère.', agg]];
  const best = Math.min(safe.time, bal.time, agg.time);
  return out.map(([name, desc, p]) => ({ name, desc, best: p.time === best, stints: p.st.map(s => ({ ...s })), stops: p.n, time: p.time, fuel: Math.min(TANK, fuelFor(p.st[0].laps, lapKm)) }));
}

// ---------- préparation de la course ----------
export function setupStrategy(R, track, wx, plan) {
  R.wx = wx; R.raining = wx.rain; R.wet = wx.rain ? 0.9 : 0;
  R.lapKm = track.length / 1000; R.pitP = pitLane(track);
  R.radio = []; R.radioT = 0;
  const lapT = R.lapTime || 80;
  const plans = R.solo ? null : makePlans(R.laps, R.lapKm, lapT, R.wet > 0.4);
  const failP = c => c.player ? Math.min(0.2, Math.max(0.005, (100 - c.rel) * 0.0025)) : 0.035;
  for (const c of R.cars) {
    c.wear = 0; c.dmg = 0; c.stops = 0; c.pit = null; c.box = null; c.mode = 'norm'; c.stint = 0;
    c.strat = c.stats?.strat ?? 70; c.tyreSkill = c.stats?.tyre ?? 75;
    if (R.solo) { c.tyre = R.wet > 0.4 ? 'W' : 'S'; c.fuel = 15; c.plan = null; continue; }
    // plan : celui du joueur, sinon un des trois au hasard (les bons stratèges choisissent mieux)
    const bi = Math.max(0, plans?.findIndex(p => p.best) ?? 1), pick = c.player && plan ? plan : plans[Math.random() < 0.3 + c.strat / 250 ? bi : Math.floor(Math.random() * 3)];
    c.plan = pick.stints.map(s => ({ ...s }));
    c.tyre = c.plan[0].tyre;
    c.fuel = c.player && plan ? plan.fuel : Math.min(TANK, fuelFor(c.plan[0].laps, R.lapKm) + 1);
    c.fail = Math.random() < failP(c) * Math.min(1.5, R.laps / 10) ? { at: 0.1 + Math.random() * 0.8, kind: Math.random() < 0.5 ? 'dnf' : 'power' } : null;
  }
}

export const progress = (R, c) => Math.max(0, ((c.lap - 1) * R.track.N + c.idx) / (R.laps * R.track.N));
const lapsLeft = (R, c) => Math.max(0, R.laps - (c.lap - 1) - c.idx / R.track.N);
export const fuelLaps = (R, c) => c.fuel / (R.lapKm * FPK * MODES[c.mode].fuel);
export const tyreLaps = (R, c) => Math.max(0, (1 - c.wear) * TYRES[c.tyre].km / R.lapKm);
const plannedStop = c => c.plan && c.stint < c.plan.length - 1 ? c.plan.slice(0, c.stint + 1).reduce((a, s) => a + s.laps, 0) : null;

// ce qu'on fait au prochain arrêt : gomme et carburant pour le relais suivant
export function nextStop(R, c) {
  const rem = Math.max(1, Math.round(lapsLeft(R, c)));
  let laps = rem, tyre;
  if (c.plan && c.stint < c.plan.length - 1) {
    const nx = c.plan[c.stint + 1];
    laps = Math.min(rem, c.plan.slice(c.stint + 1).length > 1 ? nx.laps : rem); tyre = nx.tyre;
  }
  if (R.wet > 0.4 || (R.raining && R.wet > 0.15)) tyre = 'W';
  else if (!tyre || tyre === 'W') tyre = softest(laps * R.lapKm) || 'H';
  const fuel = Math.max(0, Math.min(TANK, fuelFor(laps, R.lapKm)) - c.fuel);
  return { tyre, fuel: Math.round(fuel) };
}

// faut-il rentrer à ce tour ? (IA, pilote auto, conseils du muret)
export function wantBox(R, c) {
  if (c.lap >= R.laps || c.lap < 1) return false;
  const left = lapsLeft(R, c);
  const wetNeed = R.wet > 0.45 - (c.strat - 70) / 400, dryNeed = R.wet < 0.2 && !R.raining;
  if (c.tyre !== 'W' && wetNeed) return true;
  if (c.tyre === 'W' && dryNeed && left > 2) return true;
  if (plannedStop(c) === c.lap && (fuelLaps(R, c) < left || tyreLaps(R, c) < left * 0.9)) return true;
  if (tyreLaps(R, c) < Math.min(left, 1.3) && left > 1.5) return true;
  if (fuelLaps(R, c) / MODES.save.fuel < left - 0.1 && fuelLaps(R, c) < 2.2) return true;
  if (c.dmg > 0.5 && left > 3) return true;
  return false;
}

// ---------- radio : réponses du muret ----------
const say = (R, txt) => { R.radio.push(txt); };
const gapTo = (R, a, b) => Math.abs(((a.lap * R.track.N + a.idx) - (b.lap * R.track.N + b.idx)) * DS) / Math.max(30, (a.v + b.v) / 2);
export function radioAsk(R, what) {
  const p = R.player, left = lapsLeft(R, p);
  if (what === 'tyres') {
    const tl = tyreLaps(R, p);
    say(R, `📻 Pneus ${TYRES[p.tyre].name.toLowerCase()}s usés à ${Math.round(p.wear * 100)} % : encore ~${tl.toFixed(1)} tours. ${p.slideWear > 0.25 ? 'Tu glisses beaucoup, ça les use vite !' : 'Usure normale.'}`);
  } else if (what === 'fuel') {
    const fl = fuelLaps(R, p);
    say(R, `📻 Carburant : ${Math.round(p.fuel)} kg, de quoi faire ${fl.toFixed(1)} tours. ${fl >= left + 0.2 ? 'Ça suffit jusqu’à l’arrivée.' : fl * 1.25 >= left ? 'Juste : passe en économie pour finir.' : 'Il faudra ravitailler.'}`);
  } else if (what === 'weather') {
    const w = R.wx.change && !R.wx.done ? (R.wx.change.rain ? ' Averse attendue plus tard dans la course.' : ' Éclaircie attendue plus tard.') : '';
    say(R, `📻 Météo : ${R.raining ? 'il pleut' : 'temps sec'}, piste ${R.wet > 0.6 ? 'détrempée' : R.wet > 0.25 ? 'humide' : 'sèche'}.${w}`);
  } else if (what === 'rivals') {
    const run = R.order.filter(c => !c.out), k = run.indexOf(p), a = run[k - 1], b = run[k + 1];
    const d = c => `${c.name} (${TYRES[c.tyre].name.toLowerCase()}, ${c.stops} arrêt${c.stops > 1 ? 's' : ''}${c.pit ? ', au stand' : ''})`;
    say(R, '📻 ' + (a ? `Devant : ${d(a)} à ${gapTo(R, a, p).toFixed(1)} s. ` : 'Tu es en tête ! ') + (b ? `Derrière : ${d(b)} à ${gapTo(R, p, b).toFixed(1)} s.` : ''));
  } else if (what === 'plan') {
    const ps = plannedStop(p), ns = nextStop(R, p);
    say(R, ps ? `📻 Plan : arrêt à la fin du tour ${ps}, pneus ${TYRES[ns.tyre].name.toLowerCase()}s, +${ns.fuel} kg.` : `📻 Plus d’arrêt prévu. Il reste ${left.toFixed(1)} tours.`);
  }
}
export function radioOrder(R, what) {
  const p = R.player;
  if (MODES[what]) { p.mode = what; say(R, `📻 Bien reçu, mode ${MODES[what].name.toLowerCase()}.`); return; }
  if (what === 'box') { boxToggle(R, p); return; }
  if (!p.plan) return;
  const last = p.plan.length - 1;
  if (what === 'later' || what === 'earlier') {
    if (p.stint >= last) { say(R, '📻 Aucun arrêt prévu. Utilise STAND pour rentrer quand tu veux.'); return; }
    const s = p.plan[p.stint], n = p.plan[p.stint + 1], d = what === 'later' ? 1 : -1;
    if (s.laps + d < 1 || n.laps - d < 1) return;
    s.laps += d; n.laps -= d;
    say(R, `📻 Arrêt déplacé à la fin du tour ${plannedStop(p)}.`);
  }
  if (what === 'tyre') {
    if (p.stint >= last) p.plan.push({ laps: 0, tyre: 'M' });
    const n = p.plan[p.stint + 1], k = Object.keys(TYRES);
    n.tyre = k[(k.indexOf(n.tyre) + 1) % k.length];
    if (p.box) p.box.tyre = n.tyre;
    say(R, `📻 Prochains pneus : ${TYRES[n.tyre].name.toLowerCase()}s.`);
  }
  if (what === 'fuel+' || what === 'fuel-') {
    p.fuelAdj = Math.max(-60, Math.min(60, (p.fuelAdj || 0) + (what === 'fuel+' ? 5 : -5)));
    say(R, `📻 Ravitaillement au prochain arrêt : ${p.fuelAdj >= 0 ? '+' : ''}${p.fuelAdj} kg par rapport au plan.`);
  }
}
export function boxToggle(R, p) {
  if (p.pit || R.solo || R.done) return;
  if (p.box) { p.box = null; say(R, '📻 Arrêt annulé.'); return; }
  const ns = nextStop(R, p);
  p.box = { tyre: ns.tyre, fuel: ns.fuel };
  say(R, `📻 Box à la fin du tour : ${TYRES[ns.tyre].name.toLowerCase()}s, +${Math.max(0, ns.fuel + (p.fuelAdj || 0))} kg.`);
}

// ---------- pendant la course ----------
export function updateStrategy(R, dt) {
  const { track } = R, N = track.N, P = R.pitP;
  const lead = R.order[0];
  if (R.wx.change && !R.wx.done && lead && progress(R, lead) >= R.wx.change.at) {
    R.wx.done = true; R.raining = R.wx.change.rain;
    say(R, R.raining ? '📻 Muret : la pluie arrive ! Surveille la piste.' : '📻 Muret : la pluie s’arrête, la piste va sécher.');
  }
  if (R.wx.change && !R.wx.warned && lead && progress(R, lead) >= R.wx.change.at - 0.08) {
    R.wx.warned = true; if (R.wx.change.rain) say(R, '📻 Muret : averse prévue dans quelques minutes.');
  }
  R.wet = Math.max(0, Math.min(1, R.wet + (R.raining ? dt / 35 : -dt / 90)));

  for (const c of R.cars) {
    if (c.out) continue;
    const M = MODES[c.mode], dist = Math.abs(c.v) * dt;
    // usure : distance, glissades (fortement), sorties de piste, mode, gestion des pneus du pilote
    const slide = c.sliding ? Math.min(1, c.slip || 0.3) : 0;
    c.slideWear = (c.slideWear || 0) * (1 - dt * 0.3) + slide * dt * 0.3;
    let k = (1 + 7 * slide + (c.offTrack ? 1.5 : 0)) * M.wear * (1.3 - c.tyreSkill / 250) * (c.player ? 1 : 0.92);
    if (c.tyre === 'W') k *= 1 + 2.4 * (1 - R.wet);
    if (!R.solo) c.wear += dist / (TYRES[c.tyre].km * 1000) * k;
    // carburant : consommation, poids, panne sèche
    if (!R.solo && !c.pit && !c.finished) c.fuel = Math.max(0, c.fuel - dist / 1000 * FPK * M.fuel);
    c.mass = 1 + c.fuel / 750;
    c.powerMul = M.v; c.accMul = M.a;
    c.gripMul = tyreGrip(c, R.wet) * (1 - 0.06 * c.dmg);
    c.dmgKd = 1 - 0.45 * c.dmg;
    if (c.fuel <= 0 && !c.dnf && !R.solo && !c.finished) { c.dnf = true; if (c.player) say(R, '📻 Muret : panne sèche… c’est fini. On a mal calculé le carburant.'); }
    if (c.player && !c.lowFuel && !R.solo && fuelLaps(R, c) < lapsLeft(R, c) - 0.3 && fuelLaps(R, c) < 1.5 && !c.box) { c.lowFuel = true; say(R, '📻 Muret : carburant critique ! Rentre au stand ou passe en économie.'); }
    // casse
    if (c.fail && !c.failed && progress(R, c) >= c.fail.at) {
      c.failed = true;
      if (c.fail.kind === 'dnf') { c.dnf = true; R.scWhy = 'dnf'; if (c.player) say(R, '📻 Muret : casse moteur… on doit abandonner. Désolé.'); }
      else { c.powerLoss = 0.05; if (c.player) say(R, '📻 Muret : perte de puissance moteur, on continue en mode dégradé.'); }
    }
    if (c.dnf) {
      c.throttle = 0; c.brake = 0.6;
      if (c.v < 1) { c.out = true; c.mesh.visible = false; c.v = 0; }
      continue;
    }
    // IA : économie de carburant si nécessaire
    if (!c.player || R.autopilot) {
      const need = plannedStop(c) ? plannedStop(c) - (c.lap - 1) - c.idx / N : lapsLeft(R, c);
      c.mode = fuelLaps(R, { ...c, mode: 'norm' }) < need + 0.15 ? 'save' : 'norm';
    }
    // décision d'arrêt avant l'entrée des stands
    const k0 = P.k(c.idx, N), zone = k0 > P.e0 - 30 && k0 < P.e0 - 10;
    if (zone && !c.pit && !c.box && (!c.player || R.autopilot) && wantBox(R, c)) c.box = nextStop(R, c);
    if (zone && !c.pit && c.box && c.lap < R.laps && c.lap >= 1) {
      const add = Math.max(0, Math.min(TANK - c.fuel, c.box.fuel + (c.player ? c.fuelAdj || 0 : 0)));
      c.box.fuel = Math.round(add);
      c.frac = 0;
      c.pit = { phase: 'in', k: k0, stop: 2.3 + Math.random() * 0.6 + add / 10 + (c.dmg > 0.15 ? 3 : 0) };
      if (c.player) say(R, `📻 Muret : box, box ! ${TYRES[c.box.tyre].name}s et +${c.box.fuel} kg.`);
    }
    // conseils du muret pour le joueur (un tour avant l'arrêt prévu, ou si besoin)
    if (c.player && !R.autopilot && zone && !c.pit && !c.box && !c.adv) {
      c.adv = true;
      if (plannedStop(c) === c.lap + 1) say(R, '📻 Muret : arrêt prévu au prochain tour.');
      else if (wantBox(R, c)) { const ns = nextStop(R, c); say(R, `📻 Muret : rentre ce tour (STAND) : ${TYRES[ns.tyre].name.toLowerCase()}s, +${ns.fuel} kg.`); }
    }
    if (c.player && k0 > 200 && k0 < 210) c.adv = false;
  }
  if (R.radio.length && R.radioT < 1.5) { R.radioMsg = R.radio.shift(); R.radioT = 5.5; }
  R.radioT -= dt;
}

// dégâts : appelé après un choc contre un mur ou une voiture
export function addDamage(R, c, amount) {
  if (amount < 0.03 || R.solo) return;
  const before = c.dmg;
  c.dmg = Math.min(1, c.dmg + amount);
  if (amount > 0.18) R.scWhy = R.scWhy || 'crash';
  if (c.player && before < 0.15 && c.dmg >= 0.15) say(R, '📻 Muret : aileron avant endommagé, tu perds de l’appui. Un arrêt le réparera.');
}

// ---------- voie des stands (déplacement imposé, limiteur 80 km/h) ----------
const VPIT = 22;
export function pitStep(R, c, dt) {
  const { track } = R, N = track.N, p = c.pit, P = R.pitP;
  const prev = c.idx, boxK = P.box(c.teamIdx ?? 0);
  if (p.phase === 'stop') {
    c.v = 0; p.stop -= dt;
    if (p.stop <= 0) {
      c.tyre = c.box.tyre; c.fuel = Math.min(TANK, c.fuel + c.box.fuel); c.wear = 0; c.dmg = 0; c.stops++; c.box = null;
      c.lowFuel = false; if (c.player) c.fuelAdj = 0;
      if (c.plan) {
        if (c.stint >= c.plan.length - 1) c.plan.push({ laps: 0, tyre: c.tyre });
        c.stint++; c.plan[c.stint].tyre = c.tyre;
        // si le carburant et les pneus suffisent jusqu'à l'arrivée, plus d'autre arrêt
        const left = lapsLeft(R, c);
        if (fuelLaps(R, c) >= left + 0.1 && tyreLaps(R, c) >= left) { c.plan.length = c.stint + 1; c.plan[c.stint].laps = Math.ceil(left); }
      }
      p.phase = 'out';
    }
  } else {
    if (p.k < P.e1) c.v = Math.max(VPIT, c.v - 30 * dt);                       // freinage vers le limiteur
    else if (p.phase === 'out' && p.k > P.x0) c.v = Math.min(55, c.v + 14 * dt); // sortie : on relance
    else c.v = c.v > VPIT ? Math.max(VPIT, c.v - 30 * dt) : Math.min(VPIT, c.v + 10 * dt);
    p.k += c.v * dt / DS;
    if (p.phase === 'in' && p.k >= boxK) { p.k = boxK; p.phase = 'stop'; c.v = 0; }
    if (p.phase === 'out' && p.k >= P.x1) { c.pit = null; c.lane = c.lat; }
  }
  const f = ((p.k % N) + N) % N;
  c.idx = Math.floor(f) % N; c.frac = f - Math.floor(f);
  c.lat += (P.latAt(p.k) - c.lat) * Math.min(1, dt * 4);
  const i = c.idx, j = (i + 1) % N, t = c.frac;
  const x = track.X[i] + (track.X[j] - track.X[i]) * t, z = track.Z[i] + (track.Z[j] - track.Z[i]) * t;
  c.x = x - track.TZ[i] * c.lat; c.z = z + track.TX[i] * c.lat; c.y = track.Y[i];
  c.heading = track.H[i]; c.pitch = 0; c.throttle = 0; c.brake = 0; c.sliding = false; c.offTrack = false;
  return prev;
}
