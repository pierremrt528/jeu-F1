// Stratégie de course : formats (sprint / long), pneus (gommes, usure accentuée par les glissades),
// carburant (poids, consommation, ravitaillement), météo, plans d'arrêts proposés, arrêts au stand,
// radio avec le muret, dégâts et casse.
import { DS, pitLane } from './track.js';
import { spawnDebris, bendWing, detachWing, repairCar } from './debris.js';

// grip : adhérence neuve ; wpl : usure par tour à rythme normal sans glisser (1 = pneu à 0 %)
export const TYRES = {
  S: { name: 'Tendre', color: '#ff3b3b', grip: 1.09, wpl: 0.16 },
  M: { name: 'Medium', color: '#ffd400', grip: 1.0, wpl: 0.085 },
  H: { name: 'Dur', color: '#f2f2f2', grip: 0.93, wpl: 0.055 },
  W: { name: 'Pluie', color: '#2f8cff', grip: 0.86, wpl: 0.06 },
};
export const FORMATS = {
  flash: { name: 'Éclair', minutes: 0, prize: 0.3 },   // course solo : 3 tours
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

// adhérence selon la vie restante du pneu (r = 1 neuf … 0 usé) :
// 100-85 % grip maximal, 85-60 % bon, 60-40 % assez faible, sous 40 % presque plus de grip
export function lifeGrip(r) {
  if (r >= 0.85) return 1;
  if (r >= 0.6) return 1 - 0.03 * (0.85 - r) / 0.25;
  if (r >= 0.4) return 0.97 - 0.1 * (0.6 - r) / 0.2;
  return Math.max(0.6, 0.87 - 0.27 * (0.4 - r) / 0.4);
}
const GRIPTXT = { max: 'maximal', bon: 'bon', faible: 'faible', nul: 'quasi nul' };
export const lifeBand = r => r >= 0.85 ? 'max' : r >= 0.6 ? 'bon' : r >= 0.4 ? 'faible' : 'nul';
// adhérence des pneus selon la gomme, l'usure et l'humidité de la piste (0 sec … 1 détrempée)
export function tyreGrip(car, wet) {
  const T = TYRES[car.tyre];
  let g = (T.grip - (car.tyre === 'W' ? 0.04 * wet : 0)) * lifeGrip(1 - car.wear);
  if (car.flat) g *= 0.55;   // crevaison
  if (car.tyre !== 'W') g *= 1 - 0.45 * wet;
  return g;
}

// ---------- plans d'arrêts ----------
export const PIT_LOSS = 24.4, KPLAN = 1.05;                // temps perdu par défaut dans la voie des stands, usure prévue
const paceOf = (g, lapTime) => lapTime * 0.45 * (1 / Math.sqrt(g) - 1);   // secondes perdues par tour
const MAXW = 0.6;                                          // le muret fait rentrer avant la chute (40 %)
// déroulé tour par tour d'une stratégie : vie des pneus en fin de tour, temps estimé du tour
export function planLaps(stints, lapKm, lapTime, wet, pitLoss = PIT_LOSS) {
  const out = []; let t = 0, lap = 0;
  stints.forEach((s, i) => {
    const T = TYRES[s.tyre], f0 = fuelFor(s.laps, lapKm), w = T.wpl * KPLAN * (s.tyre === 'W' && !wet ? 3 : 1);
    if (i) t += pitLoss + f0 / 10;
    for (let k = 0; k < s.laps; k++) {
      const r0 = 1 - k * w, r1 = 1 - (k + 1) * w, rm = (r0 + r1) / 2;
      let g = T.grip * lifeGrip(rm) * (s.tyre === 'W' ? (wet ? 1 : 0.95) : wet ? 0.6 : 1);
      if (r1 < 0) g *= 0.8;
      const lt = lapTime + paceOf(g, lapTime) + 0.035 * f0 * (1 - (k + 0.5) / s.laps);
      t += lt; lap++;
      out.push({ lap, tyre: s.tyre, stint: i, r0, r: r1, t: lt + (i && !k ? pitLoss + f0 / 10 : 0), lt });
    }
  });
  return { laps: out, time: t };
}
const softest = laps => ['S', 'M', 'H'].find(t => laps * TYRES[t].wpl * KPLAN <= MAXW);
// coût d'un relais (gomme, tours) : indépendant de sa place dans la course
function stintCost(tyre, n, lapKm, lapTime, wet) {
  if (n * TYRES[tyre].wpl * KPLAN > MAXW || fuelFor(n, lapKm) > TANK) return Infinity;
  return planLaps([{ tyre, laps: n }], lapKm, lapTime, wet).time;
}
// meilleure répartition des tours pour une suite de gommes (programmation dynamique)
function bestSplit(seq, laps, cost) {
  let prev = new Map([[0, { c: 0, L: [] }]]);
  for (let i = 0; i < seq.length; i++) {
    const nx = new Map();
    for (const [done, v] of prev) for (let n = 1; done + n <= laps; n++) {
      if (i === seq.length - 1 && done + n !== laps) continue;
      const c = v.c + cost(seq[i], n);
      if (c < (nx.get(done + n)?.c ?? Infinity)) nx.set(done + n, { c, L: [...v.L, n] });
    }
    prev = nx;
  }
  const r = prev.get(laps);
  return r && isFinite(r.c) ? seq.map((t, i) => ({ tyre: t, laps: r.L[i] })) : null;
}
const NAMES = ['Recommandée', 'Alternative 1', 'Alternative 2'];
const descOf = st => st.length === 1 ? 'Un seul relais : on gère les pneus jusqu’au bout.'
  : st.some(s => s.tyre === 'S') ? 'Tendres rapides mais fragiles : plus d’attaque, plus d’arrêts.'
  : 'Gommes durables : relais longs et peu de risques.';
export function planInfo(stints, lapKm, lapTime, wet, pitLoss) {
  const pl = planLaps(stints, lapKm, lapTime, wet, pitLoss);
  return { stints, stops: stints.length - 1, time: pl.time, fuel: Math.min(TANK, fuelFor(stints[0].laps, lapKm)), laps: pl.laps };
}
// stratégies recommandées : toutes les combinaisons de 1 à 4 relais, les 3 meilleures (variées)
export function makePlans(laps, lapKm, lapTime, wet, pitLoss) {
  const memo = new Map(), cost = (t, n) => { const k = t + n; if (!memo.has(k)) memo.set(k, stintCost(t, n, lapKm, lapTime, wet)); return memo.get(k); };
  const dry = wet ? ['W'] : ['S', 'M', 'H'], seen = new Set(), cands = [];
  const rec = seq => {
    if (seq.length) {
      const key = seq.slice().sort().join('');
      if (!seen.has(key)) {
        seen.add(key);
        const ord = seq.slice().sort((a, b) => 'SMHW'.indexOf(a) - 'SMHW'.indexOf(b));
        const st = bestSplit(ord, laps, cost);
        if (st) cands.push(planInfo(st, lapKm, lapTime, wet, pitLoss));
      }
    }
    if (seq.length < Math.min(4, laps)) for (const t of dry) rec([...seq, t]);
  };
  rec([]);
  if (!cands.length) cands.push(planInfo(split(wet ? 'W' : 'H', laps), lapKm, lapTime, wet, pitLoss));
  cands.sort((a, b) => a.time - b.time);
  // variété : la meilleure, puis la meilleure avec un autre nombre d'arrêts, puis la suivante
  const pick = [cands[0]];
  const other = cands.find(c => c.stops !== cands[0].stops); if (other) pick.push(other);
  for (const c of cands) if (pick.length < 3 && !pick.includes(c) && !pick.some(p => p.stops === c.stops && p.stints[0].tyre === c.stints[0].tyre)) pick.push(c);
  for (const c of cands) if (pick.length < 3 && !pick.includes(c)) pick.push(c);
  pick.sort((a, b) => a.time - b.time);
  return pick.map((p, k) => ({ ...p, name: NAMES[k], desc: descOf(p.stints), best: !k }));
}
function split(t, laps) {   // repli : relais égaux d'une même gomme
  const n = Math.max(1, Math.ceil(laps * TYRES[t].wpl * KPLAN / 0.7)), out = [];
  for (let i = 0; i < n; i++) out.push({ tyre: t, laps: Math.floor(laps / n) + (i < laps % n ? 1 : 0) });
  return out;
}

// stratégies risquées ou à contre-pied, tentées par 1 ou 2 IA à chaque course
function riskyPlan(plans, laps, wet, lapKm) {
  const w = t => laps * TYRES[t].wpl * KPLAN, best = plans[0].stints;
  const opts = [];
  if (!wet) {
    const t = ['S', 'M', 'H'].filter(t => w(t) > 0.7 && w(t) < 1.4).sort((a, b) => Math.abs(w(a) - 1) - Math.abs(w(b) - 1))[0];
    if (t && fuelFor(laps, lapKm) <= TANK) opts.push({ txt: `sans arrêt en ${TYRES[t].name.toLowerCase()}s`, stints: [{ tyre: t, laps }] });
    const n = Math.max(1, Math.ceil(w('S') / 0.92));
    if (n <= 4) opts.push({ txt: 'tendres poussés jusqu’à la corde', stints: Array.from({ length: n }, (_, i) => ({ tyre: 'S', laps: Math.floor(laps / n) + (i < laps % n ? 1 : 0) })) });
    if (laps >= 6) opts.push({ txt: 'trois arrêts en tendres', stints: Array.from({ length: 4 }, (_, i) => ({ tyre: 'S', laps: Math.floor(laps / 4) + (i < laps % 4 ? 1 : 0) })) });
  }
  if (best.length > 1) {
    const st = best.map(x => ({ ...x })), m = Math.max(1, Math.round(st[1].laps * 0.5));
    if (st[1].laps - m >= 1) { st[0].laps += m; st[1].laps -= m; opts.push({ txt: 'premier relais très long', stints: st }); }
  }
  if (!opts.length) return null;
  const o = opts[Math.floor(Math.random() * opts.length)];
  return { name: 'Risquée', txt: o.txt, stints: o.stints };
}

// ---------- préparation de la course ----------
export function setupStrategy(R, track, wx, plan) {
  R.wx = wx; R.raining = wx.rain; R.wet = wx.rain ? 0.9 : 0;
  R.lapKm = track.length / 1000; R.pitP = pitLane(track);
  R.radio = []; R.radioT = 0;
  const lapT = R.lapTime || 80;
  const plans = R.solo ? null : makePlans(R.laps, R.lapKm, lapT, R.wet > 0.4, R.pitP.loss);
  const failP = c => c.player ? Math.min(0.2, Math.max(0.005, (100 - c.rel) * 0.0025)) : 0.035;
  for (const c of R.cars) {
    c.wear = 0; c.dmg = 0; c.dmgR = 0; c.fwOff = c.rwOff = false; c.stops = 0; c.pit = null; c.box = null; c.mode = 'norm'; c.stint = 0; c.flat = false; c.risky = false;
    c.strat = c.stats?.strat ?? 70; c.tyreSkill = c.stats?.tyre ?? 75;
    if (R.solo) { c.tyre = R.wet > 0.4 ? 'W' : 'S'; c.fuel = 15; c.plan = null; continue; }
    // plan : celui du joueur, sinon un des trois au hasard (les bons stratèges choisissent mieux)
    const pick = c.player && plan ? plan : plans[Math.random() < 0.3 + c.strat / 250 ? 0 : Math.floor(Math.random() * plans.length)];
    c.plan = pick.stints.map(s => ({ ...s }));
    c.tyre = c.plan[0].tyre;
    c.fuel = c.player && plan ? plan.fuel : Math.min(TANK, fuelFor(c.plan[0].laps, R.lapKm) + 1);
    c.fail = Math.random() < failP(c) * Math.min(1.5, R.laps / 10) ? { at: 0.1 + Math.random() * 0.8, kind: Math.random() < 0.5 ? 'dnf' : 'power' } : null;
  }
  // 1 ou 2 écuries IA tentent un coup : stratégie risquée, pneus poussés jusqu'au bout
  if (R.solo || R.laps < 4) return;
  const ai = R.cars.filter(c => !c.player).sort(() => Math.random() - 0.5).slice(0, Math.random() < 0.5 ? 1 : 2), told = [];
  for (const c of ai) {
    const rp = riskyPlan(plans, R.laps, R.wet > 0.4, R.lapKm);
    if (!rp) continue;
    c.risky = true; c.plan = rp.stints.map(s => ({ ...s })); c.tyre = c.plan[0].tyre;
    c.fuel = Math.min(TANK, fuelFor(c.plan[0].laps, R.lapKm) + 1);
    told.push(`${c.name} (${rp.txt})`);
  }
  if (told.length) say(R, `📻 Muret : stratégie risquée chez ${told.join(' et ')}.`);
}

export const progress = (R, c) => Math.max(0, ((c.lap - 1) * R.track.N + c.idx) / (R.laps * R.track.N));
const lapsLeft = (R, c) => Math.max(0, R.laps - (c.lap - 1) - c.idx / R.track.N);
export const fuelLaps = (R, c) => c.fuel / (R.lapKm * FPK * MODES[c.mode].fuel);
// tours avant que le pneu tombe à 0 % ; vie restante prévue à l'arrivée
export const tyreLaps = (R, c) => Math.max(0, (1 - c.wear) / (TYRES[c.tyre].wpl * KPLAN));
const lifeAtEnd = (R, c) => 1 - c.wear - lapsLeft(R, c) * TYRES[c.tyre].wpl * KPLAN;
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
  else if (!tyre || tyre === 'W') tyre = softest(laps) || 'H';
  const fuel = Math.max(0, Math.min(TANK, fuelFor(laps, R.lapKm)) - c.fuel);
  return { tyre, fuel: Math.round(fuel) };
}

// faut-il rentrer à ce tour ? (IA, pilote auto, conseils du muret)
export function wantBox(R, c) {
  if (c.lap >= R.laps || c.lap < 1) return false;
  const left = lapsLeft(R, c);
  if (c.flat) return true;
  const wetNeed = R.wet > 0.45 - (c.strat - 70) / 400, dryNeed = R.wet < 0.2 && !R.raining;
  if (c.tyre !== 'W' && wetNeed) return true;
  if (c.tyre === 'W' && dryNeed && left > 2) return true;
  if (plannedStop(c) === c.lap && (fuelLaps(R, c) < left || lifeAtEnd(R, c) < (c.risky ? -0.1 : 0.35))) return true;
  if (1 - c.wear < (c.risky ? 0.02 : 0.25) && left > 1.5) return true;
  if (fuelLaps(R, c) / MODES.save.fuel < left - 0.1 && fuelLaps(R, c) < 2.2) return true;
  if ((c.dmg > 0.5 || c.dmgR > 0.5) && left > 2) return true;
  return false;
}

// ---------- radio : réponses du muret ----------
const say = (R, txt) => { R.radio.push(txt); };
const gapTo = (R, a, b) => Math.abs(((a.lap * R.track.N + a.idx) - (b.lap * R.track.N + b.idx)) * DS) / Math.max(30, (a.v + b.v) / 2);
export function radioAsk(R, what) {
  const p = R.player, left = lapsLeft(R, p);
  if (what === 'brief') {   // point complet en un message : pneus, carburant, écarts, arrêt, météo
    const run = R.order.filter(c => !c.out), k = run.indexOf(p), a = run[k - 1], b = run[k + 1];
    const fl = fuelLaps(R, p), ps = plannedStop(p), ns = nextStop(R, p);
    const wx = R.raining ? ' · Pluie' : R.wx.change && !R.wx.done ? (R.wx.change.rain ? ' · Averse attendue' : ' · Éclaircie attendue') : '';
    say(R, `📻 Pneus ${Math.max(0, Math.round(100 - p.wear * 100))} % (grip ${GRIPTXT[lifeBand(1 - p.wear)]})${p.slideWear > 0.25 ? ', tu glisses trop' : ''} · Essence ${fl.toFixed(1)} t${fl < left + 0.2 ? ' ⚠' : ''}` +
      (a ? ` · Devant ${a.name} ${gapTo(R, a, p).toFixed(1)} s` : ' · En tête') + (b ? ` · Derrière ${gapTo(R, p, b).toFixed(1)} s` : '') +
      (ps ? ` · Arrêt fin du tour ${ps} (${TYRES[ns.tyre].name.toLowerCase()}s)` : ' · Plus d’arrêt prévu') + wx);
    return;
  }
  if (what === 'tyres') {
    const r = 1 - p.wear, toWeak = Math.max(0, (r - 0.4) / (TYRES[p.tyre].wpl * KPLAN));
    say(R, `📻 Pneus ${TYRES[p.tyre].name.toLowerCase()}s à ${Math.max(0, Math.round(r * 100))} %, grip ${GRIPTXT[lifeBand(r)]}${r > 0.4 ? ` : ~${toWeak.toFixed(1)} tours avant la chute.` : '. Il faut changer !'} ${p.slideWear > 0.25 ? 'Tu glisses beaucoup, ça les use vite !' : 'Usure normale.'}`);
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
  if (what === 'mode') { const k = Object.keys(MODES); what = k[(k.indexOf(p.mode) + 1) % k.length]; }
  if (MODES[what]) { p.mode = what; say(R, `📻 Bien reçu, mode ${MODES[what].name.toLowerCase()}.`); return; }
  if (what === 'box') { boxToggle(R, p); return; }
  if (!p.plan) return;
  const last = p.plan.length - 1;
  if (what === 'tyre') {
    if (p.stint >= last) p.plan.push({ laps: 0, tyre: 'M' });
    const n = p.plan[p.stint + 1], k = Object.keys(TYRES);
    n.tyre = k[(k.indexOf(n.tyre) + 1) % k.length];
    if (p.box) p.box.tyre = n.tyre;
    say(R, `📻 Prochains pneus : ${TYRES[n.tyre].name.toLowerCase()}s.`);
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
    let k = (1 + 7 * slide + (c.offTrack ? 1.5 : 0)) * M.wear * (1.3 - c.tyreSkill / 250) * (c.player ? c.up?.tyre ?? 1 : 0.92);
    if (c.tyre === 'W') k *= 1 + 2.4 * (1 - R.wet);
    if (!R.solo && !c.pit) c.wear += dist / (R.lapKm * 1000) * TYRES[c.tyre].wpl * k;
    // pneu à 0 % : il peut exploser à tout moment ; on rentre au stand au ralenti
    if (!R.solo && !c.flat && !c.pit && c.wear >= 1 && Math.random() < dt * (0.03 + 0.4 * (c.wear - 1))) {
      c.flat = true; c.flats = (c.flats || 0) + 1;
      if (c.lap < R.laps) c.box = nextStop(R, c);
      if (c.player) say(R, '📻 Muret : crevaison ! Rentre au stand doucement, on change les pneus.');
      else say(R, `📻 Muret : crevaison pour ${c.name} !`);
    } else if (c.player && !c.tyreWarn && !R.solo && c.wear > 0.6 && !c.pit) { c.tyreWarn = true; say(R, '📻 Muret : tes pneus passent sous 40 %, ils n’ont presque plus de grip.'); }
    // carburant : consommation, poids, panne sèche
    if (!R.solo && !c.pit && !c.finished) c.fuel = Math.max(0, c.fuel - dist / 1000 * FPK * M.fuel * (c.up?.fuel ?? 1));
    c.mass = 1 + c.fuel / 750;
    c.powerMul = M.v * (c.flat ? 0.8 : 1); c.accMul = M.a;
    c.gripMul = tyreGrip(c, R.wet) * (1 - 0.04 * c.dmg - 0.03 * c.dmgR);
    // appui perdu : aileron avant → sous-virage, aileron arrière → survirage à haute vitesse
    c.dmgKd = c.fwOff ? 0.3 : 1 - 0.45 * c.dmg;
    c.rwKd = c.rwOff ? 0.3 : 1 - 0.45 * c.dmgR;
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
      c.pit = { phase: 'in', k: k0, stop: Math.max(1.4, 2.3 + Math.random() * 0.6 + (c.up?.pit ?? 0)) + add / 10 + (c.dmg > 0.15 ? 3 : 0) + (c.dmgR > 0.15 ? 4 : 0) + (c.fwOff || c.rwOff ? 2 : 0) };
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
// rear : choc à l'arrière (aileron arrière). Éclats projetés, aileron tordu puis arraché.
export function addDamage(R, c, amount, rear) {
  if (amount < 0.03 || R.solo) return;
  const k = rear ? 'dmgR' : 'dmg', off = rear ? 'rwOff' : 'fwOff', part = rear ? 'rw' : 'fw';
  const before = c[k];
  c[k] = Math.min(1, c[k] + amount);
  if (c.mesh.visible) spawnDebris(c, Math.min(20, 3 + Math.round(amount * 50)), rear);
  if (amount > 0.18) R.scWhy = R.scWhy || 'crash';
  if (!c[off] && (amount > 0.22 && Math.random() < amount * 2.5 || c[k] >= 0.95 && amount > 0.08)) {
    c[off] = true; c[k] = 1; detachWing(c, part);
    if (c.player) say(R, rear ? '📻 Muret : tu as perdu l’aileron arrière ! L’arrière va décrocher, rentre au stand.' : '📻 Muret : l’aileron avant est parti ! Grosse perte d’appui, rentre au stand.');
    else say(R, `📻 Muret : ${c.name} a perdu son aileron ${rear ? 'arrière' : 'avant'}, débris sur la piste !`);
    return;
  }
  if (before < 0.3 && c[k] >= 0.3) bendWing(c, part, Math.random() < 0.5 ? -1 : 1);
  if (c.player && before < 0.15 && c[k] >= 0.15) say(R, rear ? '📻 Muret : aileron arrière abîmé, l’arrière sera instable. Un arrêt le réparera.' : '📻 Muret : aileron avant endommagé, tu perds de l’appui. Un arrêt le réparera.');
}

// ---------- voie des stands (déplacement imposé, limiteur 80 km/h) ----------
const VPIT = 22;
export function pitStep(R, c, dt) {
  const { track } = R, N = track.N, p = c.pit, P = R.pitP;
  const prev = c.idx, boxK = P.box(c.teamIdx ?? 0);
  if (p.phase === 'stop') {
    c.v = 0; p.stop -= dt;
    if (p.stop <= 0) {
      c.tyre = c.box.tyre; c.fuel = Math.min(TANK, c.fuel + c.box.fuel); c.wear = 0; c.dmg = 0; c.dmgR = 0; c.fwOff = c.rwOff = false; repairCar(c); c.stops++; c.box = null; c.flat = false; c.tyreWarn = false;
      c.lowFuel = false; if (c.player) c.fuelAdj = 0;
      if (c.plan) {
        if (c.stint >= c.plan.length - 1) c.plan.push({ laps: 0, tyre: c.tyre });
        c.stint++; c.plan[c.stint].tyre = c.tyre;
        // si le carburant et les pneus suffisent jusqu'à l'arrivée, plus d'autre arrêt
        const left = lapsLeft(R, c);
        if (fuelLaps(R, c) >= left + 0.1 && lifeAtEnd(R, c) >= 0.38) { c.plan.length = c.stint + 1; c.plan[c.stint].laps = Math.ceil(left); }
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
  c.heading = track.H[i]; c.pitch = 0; c.beta = 0; c.spinT = 0; c.ax = c.ay = 0; c.dive = c.roll = c.yOff = 0; c.throttle = 0; c.brake = 0; c.sliding = false; c.offTrack = false;
  return prev;
}
