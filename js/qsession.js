// Séance de qualification jouée (tour lancé) : chrono de séance, tour des temps des 20 pilotes (temps des IA révélés
// au fil du tour du joueur : amélioration en vert, nouvelle pole provisoire en violet), secteurs S1/S2/S3 (violet =
// meilleur de la séance, vert = record perso, jaune = plus lent), écart en direct avec la pole provisoire (profil de
// vitesse de référence calé sur son temps) et place estimée. Records perso et temps des IA gardés si on refait le tour.
import { DS } from './track.js';
import { SPEC } from './car.js';

const hex = c => '#' + c.toString(16).padStart(6, '0');
const code = n => n === 'Toi' ? 'TOI' : n.split(' ').pop().normalize('NFD').replace(/[̀-ͯ]/g, '').slice(0, 3).toUpperCase();
const sec = t => t >= 60 ? `${Math.floor(t / 60)}:${(t % 60).toFixed(3).padStart(6, '0')}` : t.toFixed(3);
const lap = t => `${Math.floor(t / 60)}:${(t % 60).toFixed(3).padStart(6, '0')}`;
const sgn = (d, n = 3) => (d < 0 ? '−' : '+') + Math.abs(d).toFixed(n);
let pb = { key: '' };   // séance en cours : records perso (secteurs, tour) et temps des IA

// temps cumulés le long du tour (profil de vitesse des IA) : référence pour l'écart en direct
function profile(track) {
  const { N, V } = track, cum = new Float32Array(N + 1);
  let v = V[0] * 0.95;
  for (let pass = 0; pass < 2; pass++) {
    let t = 0;
    for (let i = 0; i < N; i++) {
      const a = SPEC.acc * Math.max(0, 1 - Math.pow(v / SPEC.vtop, 2.2));
      v = Math.min(V[(i + 1) % N] * 0.95, Math.sqrt(v * v + 2 * a * DS));
      t += DS / Math.max(v, 5); if (pass) cum[i + 1] = t;
    }
  }
  return cum;
}

// gen() : classement de qualification des IA (temps finaux) ; key : circuit + manche ; est : temps au tour estimé
export function makeQuali(R, gen, key, est) {
  const { N } = R.track, cum = profile(R.track), rnd = Math.random;
  if (pb.key !== key) pb = { key, s: [0, 0, 0], lap: 0, ai: gen().filter(r => !r.player).map(r => ({ name: r.name, color: r.color, time: r.time })) };
  const b = [0, Math.floor(N / 3), Math.floor(2 * N / 3), N];
  for (const id of ['qTower', 'qSec']) document.getElementById(id).innerHTML = '';   // pas d'affichage de la tentative précédente
  R.q = {
    cum, b, share: [0, 1, 2].map(k => (cum[b[k + 1]] - cum[b[k]]) / cum[N]), split: [0, 0], sec: [0, 0, 0], col: ['', '', ''], pb0: { s: pb.s.slice(), lap: pb.lap },
    clock0: Math.ceil((est * 1.15 + 12) / 5) * 5, cache: {},
    // chaque IA boucle son dernier tour à un moment du tour du joueur ; 3 sur 4 ont déjà un temps provisoire
    ai: pb.ai.map(r => ({ ...r, code: code(r.name), c: hex(r.color), prov: rnd() < 0.75 ? r.time + 0.15 + rnd() * 1.1 : 0, at: 0.04 + rnd() * 0.94, shown: false })),
  };
}
const shownTime = a => a.shown ? a.time : a.prov;
const bestShown = q => q.ai.reduce((m, a) => { const t = shownTime(a); return t && (!m || t < m) ? t : m; }, 0);

export function updateQuali(R) {
  const q = R.q, p = R.player, { N } = R.track;
  // secteurs : passage à 1/3 et 2/3 du tour (instant corrigé avec la vitesse)
  if (p.lap === 1 && !p.finished) for (let k = 0; k < 2; k++) {
    if (q.split[k] || p.idx < q.b[k + 1] || p.idx > q.b[k + 1] + N / 6 || (k && !q.split[0])) continue;
    q.split[k] = R.t - p.lapStart - (p.idx - q.b[k + 1]) * DS / Math.max(5, p.v);
    q.sec[k] = q.split[k] - (k ? q.split[0] : 0); q.col[k] = color(q, k);
  }
  if (p.finished && !q.end) {
    q.end = true;
    if (q.split[1]) { q.sec[2] = p.lastLap - q.split[1]; q.col[2] = color(q, 2); }
    for (let k = 0; k < 3; k++) if (q.sec[k] > 0 && (!pb.s[k] || q.sec[k] < pb.s[k])) pb.s[k] = q.sec[k];
    if (!pb.lap || p.lastLap < pb.lap) pb.lap = p.lastLap;
  }
  // temps des IA révélés au fil du tour du joueur (tous à la fin de la séance)
  const f = q.end ? 2 : p.lap === 1 ? p.idx / N : 0;
  for (const a of q.ai) if (!a.shown && f >= a.at) {
    const best = bestShown(q);
    a.shown = true; a.flashT = R.t; a.flash = !best || a.time < best ? 'pur' : 'grn';
  }
}
// couleur d'un secteur : violet = meilleur de la séance, vert = record perso, jaune = plus lent
function color(q, k) {
  const s = q.sec[k], fast = bestShown(q) * q.share[k], own = q.pb0.s[k];
  if (s <= fast && (!own || s <= own)) return 'pur';
  return !own || s <= own ? 'grn' : 'yel';
}

const put = (q, id, v) => { if (q.cache[id] !== v) { q.cache[id] = v; document.getElementById(id).innerHTML = v; } };
export function qualiHud(R) {
  const q = R.q, p = R.player, ref = bestShown(q), run = p.lap === 1 && !p.finished;
  const cur = run ? R.t - p.lapStart : 0;
  const delta = run && ref && p.idx > R.track.N * 0.03 ? cur - q.cum[p.idx] * ref / q.cum[R.track.N] : null;
  // tour des temps : IA (temps révélé ou provisoire) + joueur (meilleur tour de la séance, ou estimation en direct s'il est en train de l'améliorer)
  const best0 = q.pb0.lap, live = delta != null && (!best0 || ref + delta < best0) ? ref + delta : 0;
  const me = { code: 'TOI', c: hex(p.team.color), me: true, t: p.finished ? Math.min(p.lastLap, best0 || 1e9) : live || best0, live: !!live };
  const rows = q.ai.map(a => ({ code: a.code, c: a.c, t: shownTime(a), fl: a.shown && R.t - a.flashT < 4 ? a.flash : '' })).concat(me)
    .sort((a, b) => (a.t || 1e9) - (b.t || 1e9));
  const t1 = rows[0].t, pos = rows.indexOf(me) + 1;
  const rem = q.clock0 - R.t, clock = q.end ? 'Terminé' : rem > 0 ? `${Math.floor(rem / 60)}:${String(Math.floor(rem % 60)).padStart(2, '0')}` : '🏁 0:00';
  put(q, 'qTower', `<div class="qh"><b>Qualifs</b><span class="${rem < 30 && !q.end ? 'low' : ''}">${clock}</span></div>` + rows.map((r, k) => {
    const v = !r.t ? (r.me ? (p.lap < 1 ? 'Sortie' : 'Lancé') : 'En piste') : k === 0 ? (r.live ? '~' : '') + lap(r.t) : (r.live ? '~' : '+') + (r.live ? sgn(r.t - t1, 1) : (r.t - t1).toFixed(3));
    return `<div class="${r.me ? 'me' : ''} ${r.fl}"><b>${String(k + 1).padStart(2, '0')}</b><i style="background:${r.c}"></i><em>${r.code}</em><span class="${r.t ? '' : 'st'}">${v}</span></div>`;
  }).join(''));
  // secteurs, tour en cours, écart avec la pole provisoire
  const cells = [0, 1, 2].map(k => {
    const s = q.sec[k], own = q.pb0.s[k];
    return `<div class="qc ${q.col[k]}"><small>S${k + 1}</small><b>${s ? sec(s) : own ? `<i>${sec(own)}</i>` : '--.---'}</b></div>`;
  }).join('');
  const line = p.finished ? `<b class="qt">${lap(p.lastLap)}</b><span class="qp ${pos === 1 ? 'pur' : pos <= 10 ? 'grn' : ''}">P${pos}${pos === 1 && !(best0 && best0 < p.lastLap) ? ' · pole provisoire' : ''}</span>` + (best0 && best0 < p.lastLap ? `<small>meilleur ${lap(best0)}</small>` : '')
    : run ? `<b class="qt">${lap(cur)}</b>` + (delta != null ? `<span class="qd ${delta < 0 ? 'neg' : 'pos'}">${sgn(delta, 2)}</span><small>vs P1 ${lap(ref)}</small>` : '')
    : `<b class="qt dim">Tour de lancement</b>${q.pb0.lap ? `<small>Ton meilleur ${lap(q.pb0.lap)}</small>` : ''}`;
  put(q, 'qSec', `<div class="q3">${cells}</div><div class="ql">${line}</div>`);
}
