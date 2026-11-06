// Saison : calendrier des 10 circuits, qualifications, points, classements pilotes et écuries,
// progression des écuries rivales, sauvegarde locale (clé gpm-season).
import { TEAMS, CIRCUITS } from './data.js';
import { SPEC } from './car.js';
import { DS } from './track.js';

export const CALENDAR = ['valmont', 'sahrane', 'portazur', 'kerlan', 'sierra', 'veldmar', 'solenza', 'neokyara', 'varenne', 'brekka'];
export const POINTS = [25, 18, 15, 12, 10, 8, 6, 4, 2, 1];
const KEY = 'gpm-season';
const DRIVERS = () => TEAMS.flatMap(t => t.drivers.map(d => ({ name: d.name, team: t.name, color: t.color, player: !!d.player })));

export let season = load();
function load() { try { return JSON.parse(localStorage.getItem(KEY)); } catch { return null; } }
export function saveSeason() { try { localStorage.setItem(KEY, JSON.stringify(season)); } catch {} }

export function newSeason(year, carry = false, nReg = 0) {
  // carry : les écuries rivales gardent la moitié de leurs progrès ; un nouveau règlement les
  // fait reculer et rebat les cartes (certaines écuries comprennent mieux les nouvelles règles)
  const dev = carry && season ? Object.fromEntries(TEAMS.slice(1).map(t => [t.name, (season.dev[t.name] || 0) * 0.5 - nReg * 0.004 + (Math.random() - 0.5) * 0.008 * nReg])) : {};
  season = { year: year || (season ? season.year + 1 : 2027), round: 0, pts: {}, results: [], dev, hist: [], grid: null, quali: null, done: false };
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
  const W = 330, Ht = 170, L = 30, T = 8, B = 18, n = Math.max(10, H.length - 1);
  const vals = H.flatMap(h => Object.values(h)), lo = Math.min(...vals) - 0.003, hi = Math.max(...vals) + 0.003;
  const X = i => L + (W - L - 6) * i / n, Y = v => T + (Ht - T - B) * (1 - (v - lo) / (hi - lo));
  let g = '';
  for (let k = 0; k <= 4; k++) { const v = lo + (hi - lo) * k / 4; g += `<line x1="${L}" x2="${W - 6}" y1="${Y(v)}" y2="${Y(v)}" stroke="rgba(255,255,255,.08)"/><text x="${L - 3}" y="${Y(v) + 3}" text-anchor="end">${(v * 100).toFixed(1)}</text>`; }
  for (let i = 0; i <= n; i++) g += `<text x="${X(i)}" y="${Ht - 4}" text-anchor="middle">${i ? i : 'D'}</text>`;
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

export const roundCircuit = () => CIRCUITS.find(c => c.id === CALENDAR[season.round]);

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
export function qualifying(track, playerTime, wet = false) {
  const rows = [];
  for (const t of TEAMS) for (const d of t.drivers) {
    if (d.player) { rows.push({ name: d.name, team: t.name, color: t.color, player: true, time: playerTime }); continue; }
    const perf = t.perf + (season?.dev[t.name] || 0);
    const time = lapEstimate(track, d.skill, perf) * (1 + 0.006 * (Math.random() - 0.3)) * 0.985 * (wet ? 1.12 : 1);
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
  season.results.push({ circuit: CALENDAR[season.round], pos: me.pos, pts: res.find(r => r.name === me.name).pts, podium: rows.slice(0, 3).map(r => r.name), fast: fast?.name });
  // les écuries rivales progressent un peu à chaque course
  for (const t of TEAMS) if (!t.drivers.some(d => d.player)) season.dev[t.name] = (season.dev[t.name] || 0) + 0.0012 + Math.random() * 0.0018;
  season.round++; season.grid = null; season.quali = null; season.wx = null;
  if (season.round >= CALENDAR.length) season.done = true;
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
  return CALENDAR.map((id, k) => {
    const c = CIRCUITS.find(x => x.id === id), r = season.results[k];
    const cls = k === season.round && !season.done ? 'next' : r ? 'past' : '';
    const res = r ? `<b class="${r.pos <= 3 ? 'pod' : ''}">P${r.pos}</b> <small>+${r.pts} pts</small>` : k === season.round ? '<b>À suivre</b>' : '';
    return `<div class="cal ${cls}"><span>${k + 1}</span><div>${c.name}<small>${c.type}</small></div><em>${res}</em></div>`;
  }).join('');
}
export function standingsHTML(kind) {
  if (kind === 'perf') return perfChartHTML();
  const list = kind === 'teams' ? teamStandings() : driverStandings();
  return `<table class="stand">${list.map((r, k) => `<tr class="${r.player ? 'me' : ''}"><td>${k + 1}</td><td><i style="background:${hex(r.color)}"></i>${r.name}${kind === 'teams' ? '' : `<small>${r.team}</small>`}</td><td>${r.pts}</td></tr>`).join('')}</table>`;
}
export function qualiHTML(rows) {
  const pole = rows[0].time;
  return `<table>${rows.map((r, k) => `<tr class="${r.player ? 'me' : ''}"><td>${k + 1}</td><td><i style="background:${hex(r.color)}"></i>${r.name}<small>${r.team}</small></td><td>${r.time ? (k ? '+' + (r.time - pole).toFixed(3) : fmtT(r.time)) : 'sans temps'}</td></tr>`).join('')}</table>`;
}
const fmtT = t => { const m = Math.floor(t / 60), s = t - m * 60; return m + ':' + (s < 10 ? '0' : '') + s.toFixed(3); };
