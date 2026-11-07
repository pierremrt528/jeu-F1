// Saison : calendrier des 10 circuits, qualifications, points, classements pilotes et écuries,
// progression des écuries rivales, sauvegarde locale (clé gpm-season).
import { TEAMS, CIRCUITS, DIFF, dname } from './data.js';
import { SPEC } from './car.js';
import { DS } from './track.js';

// Calendrier : 22 Grands Prix de mars à début décembre ; la saison va du 1er janvier au 31 décembre.
export const CALENDAR = ['qaltara', 'sahrane', 'coralia', 'neokyara', 'miravel', 'castelmare', 'valmont', 'sorrane', 'brumenval', 'rocheval', 'veldmar',
  'oberthal', 'sierra', 'varenne', 'kerlan', 'nordfjell', 'solenza', 'aurelia', 'kasbara', 'portazur', 'tessaline', 'brekka'];
// jours entre deux GP : week-ends consécutifs (7), 2 ou 3 semaines, pause d'été de 5 semaines (mercato)
export const GAPS = [14, 21, 7, 14, 14, 7, 14, 7, 14, 7, 35, 7, 14, 7, 21, 14, 7, 14, 7, 14, 7];
// anciennes sauvegardes : saison à 10 manches
const OLD_CAL = ['valmont', 'sahrane', 'portazur', 'kerlan', 'sierra', 'veldmar', 'solenza', 'neokyara', 'varenne', 'brekka'], OLD_GAPS = [14, 7, 21, 7, 14, 35, 7, 14, 7];
export const cal = () => season?.cal || (season ? OLD_CAL : CALENDAR);
const gaps = () => season?.gaps || (season ? OLD_GAPS : GAPS);
export const nRounds = () => cal().length;
export const midRound = () => Math.floor(nRounds() / 2);   // mercato d'été et annonce du règlement
const MOIS = ['janv.', 'févr.', 'mars', 'avril', 'mai', 'juin', 'juil.', 'août', 'sept.', 'oct.', 'nov.', 'déc.'];
export const MOIS_LONG = ['Janvier', 'Février', 'Mars', 'Avril', 'Mai', 'Juin', 'Juillet', 'Août', 'Septembre', 'Octobre', 'Novembre', 'Décembre'];
// date du GP k : premier départ le 2e dimanche de mars
export function raceDate(k, year = season?.year || 2027) {
  const d = new Date(year, 2, 8); d.setDate(d.getDate() + (7 - d.getDay()) % 7);
  d.setDate(d.getDate() + gaps().slice(0, k).reduce((a, b) => a + b, 0));
  return d;
}
export const fmtDate = d => `${d.getDate() === 1 ? '1er' : d.getDate()} ${MOIS[d.getMonth()]}`;
// jours entre le GP k et le suivant (après le dernier : jusqu'au 1er janvier, la pré-saison prend le relais)
export const daysAfter = k => k < gaps().length ? gaps()[k] : Math.round((new Date((season?.year || 2027) + 1, 0, 1) - raceDate(k)) / 864e5);
// pré-saison : 1er janvier, présentation de la voiture, essais hivernaux, puis le premier GP (le temps avance par étapes)
export function preDates(year = season?.year || 2027) {
  const f = raceDate(0, year), before = n => { const d = new Date(f); d.setDate(f.getDate() - n); return d; };
  return [new Date(year, 0, 1), before(30), before(16), f];
}
export const preStep = () => season && !season.round && !season.done && !season.grid ? Math.min(3, season.pre || 0) : 3;
// date du jour dans la carrière : étape de la pré-saison, arrivée sur le circuit du 1er GP, sinon le lendemain du dernier GP
export function today() {
  if (!season) return new Date(2027, 0, 1);
  if (season.done) return new Date(season.year, 11, 15);
  if (!season.round) { const p = preStep(); if (p < 3) return preDates()[p]; const d = raceDate(0); d.setDate(d.getDate() - 3); return d; }
  const d = raceDate(season.round - 1); d.setDate(d.getDate() + 1); return d;
}
// les écuries rivales travaillent aussi pendant l'hiver : progrès au prorata des jours (≈ 16,6 jours par GP)
export function devRivals(days) {
  const k = Math.sqrt(10 / nRounds()) * days / 16.6;
  for (const t of TEAMS) if (!t.drivers.some(d => d.player)) season.dev[t.name] = (season.dev[t.name] || 0) + (0.0012 + Math.random() * 0.0018) * k;
  saveSeason();
}
// rendez-vous de la saison hors Grands Prix (affichés dans le calendrier)
export function seasonEvents(year = season?.year || 2027) {
  const n = nRounds(), m = midRound(), last = raceDate(n - 1, year), [, launch, tests] = preDates(year);
  const summer = raceDate(m - 1, year); summer.setDate(summer.getDate() + 1);
  const gala = new Date(last); gala.setDate(last.getDate() + 7);
  return [
    { d: new Date(year, 0, 1), ic: '🎆', t: 'Début de la saison', s: 'premières pièces, recrutements, budget de l’année' },
    { d: launch, ic: '🚗', t: 'Présentation de la voiture', s: 'nouvelle livrée devant la presse' },
    { d: tests, ic: '🧪', t: 'Essais hivernaux', s: '3 jours de roulage avant le premier GP' },
    { d: summer, ic: '☀️', t: 'Pause estivale · mercato d’été', s: `${gaps()[m - 1] || 35} jours sans course, transferts possibles` },
    { d: gala, ic: '🏆', t: 'Gala de fin de saison', s: 'remise des trophées, puis intersaison' },
    { d: new Date(year, 11, 31), ic: '🔁', t: 'Fin de la saison', s: 'contrats, moteur et transferts pour l’an prochain' },
  ];
}
export const POINTS = [25, 18, 15, 12, 10, 8, 6, 4, 2, 1];
const KEY = 'gpm-season';
const DRIVERS = () => TEAMS.flatMap(t => t.drivers.map(d => ({ name: d.name, team: t.name, color: t.color, player: !!d.player })));

export let season = load();
function load() { try { return JSON.parse(localStorage.getItem(KEY)); } catch { return null; } }
export function saveSeason() { try { localStorage.setItem(KEY, JSON.stringify(season)); } catch {} }

// nombre de courses choisi à la création (12, 16 ou 22) : manches réparties sur l'année, mêmes dates
export const seasonOpts = { races: 22 };
function pickRaces(n) {
  if (!n || n >= CALENDAR.length) return { cal: CALENDAR.slice(), gaps: GAPS.slice() };
  const at = [0]; for (const g of GAPS) at.push(at[at.length - 1] + g);
  const idx = Array.from({ length: n }, (_, k) => Math.round(k * (CALENDAR.length - 1) / (n - 1)));
  return { cal: idx.map(i => CALENDAR[i]), gaps: idx.slice(1).map((i, k) => at[i] - at[idx[k]]) };
}
export function newSeason(year, carry = false, nReg = 0) {
  // carry : les écuries rivales gardent la moitié de leurs progrès ; un nouveau règlement les
  // fait reculer et rebat les cartes (certaines écuries comprennent mieux les nouvelles règles)
  const dev = carry && season ? Object.fromEntries(TEAMS.slice(1).map(t => [t.name, (season.dev[t.name] || 0) * 0.5 - nReg * 0.004 + (Math.random() - 0.5) * 0.008 * nReg])) : {};
  season = { year: year || (season ? season.year + 1 : 2027), ...pickRaces(seasonOpts.races), round: 0, pts: {}, results: [], dev, hist: [], grid: null, quali: null, done: false };
  for (const d of DRIVERS()) season.pts[d.name] = 0;
  saveSeason();
  return season;
}
// courbes de performance : une valeur par écurie au départ puis après chaque course
export function recordPerf(playerPerf) {
  season.hist = season.hist || [];
  season.hist.push(Object.fromEntries(TEAMS.map(t => [t.name, t.drivers.some(d => d.player) ? playerPerf : t.perf + (season.dev[t.name] || 0)])));
  saveSeason();
}
export function perfChartHTML() {
  const H = season?.hist || [];
  if (!H.length) return '<p class="hint">Les courbes apparaîtront après la première course.</p>';
  const W = 330, Ht = 170, L = 30, T = 8, B = 18, n = Math.max(nRounds(), H.length - 1);
  const vals = H.flatMap(h => Object.values(h)), lo = Math.min(...vals) - 0.003, hi = Math.max(...vals) + 0.003;
  const X = i => L + (W - L - 6) * i / n, Y = v => T + (Ht - T - B) * (1 - (v - lo) / (hi - lo));
  let g = '';
  for (let k = 0; k <= 4; k++) { const v = lo + (hi - lo) * k / 4; g += `<line x1="${L}" x2="${W - 6}" y1="${Y(v)}" y2="${Y(v)}" stroke="rgba(255,255,255,.08)"/><text x="${L - 3}" y="${Y(v) + 3}" text-anchor="end">${(v * 100).toFixed(1)}</text>`; }
  for (let i = 0; i <= n; i += n > 12 ? 2 : 1) g += `<text x="${X(i)}" y="${Ht - 4}" text-anchor="middle">${i ? i : 'D'}</text>`;
  const teams = TEAMS.map(t => ({ t, me: t.drivers.some(d => d.player), last: H[H.length - 1][t.name] })).sort((a, b) => a.me - b.me);
  for (const { t, me } of teams) {
    const pts = H.map((h, i) => `${X(i).toFixed(1)},${Y(h[t.name] ?? lo).toFixed(1)}`).join(' ');
    g += `<polyline points="${pts}" fill="none" stroke="${hex(t.color)}" stroke-width="${me ? 3.2 : 1.4}" stroke-linejoin="round" ${me ? '' : 'opacity=".85"'}/>`;
    if (me) { const i = H.length - 1; g += `<circle cx="${X(i)}" cy="${Y(H[i][t.name])}" r="4" fill="#fff" stroke="${hex(t.color)}" stroke-width="2"/>`; }
  }
  const leg = teams.slice().sort((a, b) => b.last - a.last).map(({ t, me, last }) => {
    const d = H.length > 1 ? (last - H[0][t.name]) * 100 : 0;
    return `<div class="${me ? 'me' : ''}"><i style="background:${hex(t.color)}"></i>${t.name}<b>${(last * 100).toFixed(1)} <small>${d >= 0 ? '+' : ''}${d.toFixed(1)}</small></b></div>`;
  }).join('');
  return `<svg class="perfchart" viewBox="0 0 ${W} ${Ht}">${g}</svg><p class="dim small">Indice de performance de chaque voiture (D = départ de la saison, puis après chaque GP). Le chiffre en petit : progression depuis le début de saison.</p><div class="perflegend">${leg}</div>`;
}
export function quitSeason() { season = null; try { localStorage.removeItem(KEY); } catch {} }

export const roundCircuit = () => CIRCUITS.find(c => c.id === cal()[season.round]);

// Estimation d'un tour lancé pour un pilote IA (même modèle que la physique : profil de vitesse
// × niveau, limité par l'accélération) + un peu d'aléa
export function lapEstimate(track, skill, perf, accMul = 1) {
  const { N, V } = track, vtop = SPEC.vtop * perf;
  let v = V[0] * skill, t = 0;
  for (let pass = 0; pass < 2; pass++) for (let i = 0; i < N; i++) {
    const a = SPEC.acc * perf * accMul * Math.max(0, 1 - Math.pow(v / vtop, 2.2));
    v = Math.min(V[(i + 1) % N] * skill, Math.sqrt(v * v + 2 * a * DS));
    if (pass === 1) t += DS / Math.max(v, 5);
  }
  return t;
}

// Temps des IA en qualifications ; le joueur fournit son temps (null = pas de temps)
export function qualifying(track, playerTime, wet = false, prev = null) {   // prev : temps des IA déjà connus (séance jouée)
  const rows = [];
  for (const t of TEAMS) for (const d of t.drivers) {
    if (d.player) { rows.push({ name: d.name, team: t.name, color: t.color, player: true, time: playerTime }); continue; }
    const perf = t.perf + (season?.dev[t.name] || 0);
    const time = prev?.find(r => r.name === d.name)?.time ?? lapEstimate(track, d.skill * DIFF.k, perf) * (1 + 0.006 * (Math.random() - 0.3)) * 0.985 * (wet ? 1.12 : 1);
    rows.push({ name: d.name, team: t.name, color: t.color, time });
  }
  rows.sort((a, b) => (a.time || 1e9) - (b.time || 1e9));
  return rows;
}

// Enregistre le résultat d'une course de la saison
export function recordRace(rows) {
  const fast = rows.filter(r => r.best).sort((a, b) => a.best - b.best)[0];
  const res = rows.map(r => {
    let p = r.dnf ? 0 : POINTS[r.pos - 1] || 0;
    if (fast && r.name === fast.name && r.pos <= 10) p += 1;   // point du meilleur tour
    season.pts[r.name] = (season.pts[r.name] || 0) + p;
    return { name: r.name, pos: r.pos, pts: p };
  });
  const me = rows.find(r => r.player);
  season.results.push({ circuit: cal()[season.round], pos: me.pos, pts: res.find(r => r.name === me.name).pts, podium: rows.slice(0, 3).map(r => r.name), fast: fast?.name });
  // les écuries rivales progressent un peu à chaque course
  const k = Math.sqrt(10 / nRounds());   // saison plus longue : progrès par course un peu plus faibles
  for (const t of TEAMS) if (!t.drivers.some(d => d.player)) season.dev[t.name] = (season.dev[t.name] || 0) + (0.0012 + Math.random() * 0.0018) * k;
  season.round++; season.grid = null; season.quali = null; season.wx = null;
  if (season.round >= nRounds()) season.done = true;
  saveSeason();
}

export function driverStandings() {
  return DRIVERS().map(d => ({ ...d, pts: season.pts[d.name] || 0 }))
    .sort((a, b) => b.pts - a.pts || bestFinish(a.name) - bestFinish(b.name));
}
export function teamStandings() {
  return TEAMS.map(t => ({ name: t.name, color: t.color, player: t.drivers.some(d => d.player), pts: t.drivers.reduce((s, d) => s + (season.pts[d.name] || 0), 0) }))
    .sort((a, b) => b.pts - a.pts);
}
function bestFinish() { return 0; }

// ---------- rendu HTML ----------
const hex = c => '#' + c.toString(16).padStart(6, '0');
export function calendarHTML() {
  const C = cal(), y = season.year, items = [];
  C.forEach((id, k) => items.push({ d: raceDate(k), k, id }));
  for (const e of seasonEvents(y)) items.push({ d: e.d, ev: e });
  items.sort((a, b) => a.d - b.d || (a.ev ? -1 : 1));
  const now = today();
  let h = '', month = -1;
  for (const it of items) {
    if (it.d.getMonth() !== month) { month = it.d.getMonth(); h += `<h5 class="calm">${MOIS_LONG[month]} ${y}</h5>`; }
    if (it.ev) { h += `<div class="cal ev ${+it.d === +now && !season.round ? 'now' : it.d < now ? 'past' : ''}"><span>${it.ev.ic}</span><div>${it.ev.t}<small>${fmtDate(it.d)} · ${it.ev.s}</small></div><em></em></div>`; continue; }
    const k = it.k, c = CIRCUITS.find(x => x.id === it.id), r = season.results[k];
    const cls = k === season.round && !season.done ? 'next' : r ? 'past' : '';
    const res = r ? `<b class="${r.pos <= 3 ? 'pod' : ''}">P${r.pos}</b> <small>+${r.pts} pts</small>` : k === season.round ? '<b>À suivre</b>' : '';
    const gap = k ? gaps()[k - 1] : 0, tag = !k ? '<i class="gap">ouverture</i>' : gap === 7 ? '<i class="gap b2b">enchaîné</i>' : gap >= 28 ? `<i class="gap long">pause ${gap / 7} sem.</i>` : `<i class="gap">${gap / 7} sem.</i>`;
    h += `<div class="cal ${cls}"><span>${k + 1}</span><div>${c.name}<small>${fmtDate(it.d)} · ${c.type}</small></div><em>${res || tag}</em></div>`;
  }
  return h;
}
export function standingsHTML(kind) {
  if (kind === 'perf') return perfChartHTML();
  const list = kind === 'teams' ? teamStandings() : driverStandings();
  return `<table class="stand">${list.map((r, k) => `<tr class="${r.player ? 'me' : ''}"><td>${k + 1}</td><td><i style="background:${hex(r.color)}"></i>${kind === 'teams' ? r.name : dname(r.name)}${kind === 'teams' ? '' : `<small>${r.team}</small>`}</td><td>${r.pts}</td></tr>`).join('')}</table>`;
}
export function qualiHTML(rows) {
  const pole = rows[0].time;
  return `<table>${rows.map((r, k) => `<tr class="${r.player ? 'me' : ''}"><td>${k + 1}</td><td><i style="background:${hex(r.color)}"></i>${dname(r.name)}<small>${r.team}</small></td><td>${r.time ? (k ? '+' + (r.time - pole).toFixed(3) : fmtT(r.time)) : 'sans temps'}</td></tr>`).join('')}</table>`;
}
const fmtT = t => { const m = Math.floor(t / 60), s = t - m * 60; return m + ':' + (s < 10 ? '0' : '') + s.toFixed(3); };
