// Contrats de sponsors de l'écurie : un principal et jusqu'à 3 secondaires (config/sponsors.js).
// Prime à la signature, montant annuel versé par parts après chaque GP, objectif à chaque course avec bonus.
// career.sponsors = { main: id, sec: [id] }.
import SPONSORS from './config/sponsors.js';

export { SPONSORS };
export const MAX_SEC = 3;
export const GOALS = {
  finish: { t: 'Voir l’arrivée', ok: r => !r.dnf },
  pts: { t: 'Marquer des points', ok: r => !r.dnf && r.pos <= 10 },
  top5: { t: 'Finir dans le top 5', ok: r => !r.dnf && r.pos <= 5 },
  podium: { t: 'Monter sur le podium', ok: r => !r.dnf && r.pos <= 3 },
  gain: { t: 'Gagner 3 places en course', ok: r => !r.dnf && r.gained >= 3 },
  fl: { t: 'Signer le meilleur tour', ok: r => !!r.fastest },
  beatMate: { t: 'Finir devant ton équipier', ok: r => !r.dnf && r.pos < r.matePos },
};
export const spOf = id => SPONSORS.find(s => s.id === id);
export const offers = (kind, rep) => SPONSORS.filter(s => s.kind === kind && s.rep <= rep);
export const mySponsors = c => c?.sponsors ? [spOf(c.sponsors.main), ...(c.sponsors.sec || []).map(spOf)].filter(Boolean) : [];
const r1 = v => Math.round(v * 10) / 10;
const f1 = v => r1(v).toFixed(1).replace('.', ',');

// carte d'un sponsor (création)
export function sponsorCard(s, n, { pick, disabled } = {}) {
  return `<button class="copt spn ${pick ? 'pick' : ''}" data-sp="${s.id}" ${disabled ? 'disabled' : ''} style="--c:${s.color}">
    <span class="spt"><b>${s.name}</b><small>${s.sector}${s.rep ? ` · réputation ${s.rep}+` : ''}</small></span>
    <span class="spk"><span><small>Par saison</small><b>${f1(s.year)} M€</b></span><span><small>Signature</small><b>+${f1(s.sign)} M€</b></span></span>
    <span class="spg">🎯 ${GOALS[s.goal].t} : <b>+${f1(s.bonus)} M€</b> par course</span>
    <em class="price">${f1(s.year / n)} M€ par GP</em></button>`;
}

// après une course : versements et objectifs (r : pos, dnf, gained, fastest, matePos)
export function paySponsors(c, r, n) {
  const list = mySponsors(c);
  if (!list.length) return null;
  let total = 0, met = 0;
  const lines = list.map(s => {
    const ok = GOALS[s.goal].ok(r), v = s.year / n + (ok ? s.bonus : 0);
    total += v; met += ok;
    return { name: s.name, ok, v: r1(v), goal: GOALS[s.goal].t };
  });
  return { total: r1(total), met, n: list.length, lines };
}
// objectifs de la prochaine course (écran de saison)
export function sponsorGoalsHTML(c) {
  const list = mySponsors(c);
  if (!list.length) return '';
  return `<div class="spgoals"><h4>🤝 Objectifs des sponsors</h4>${list.map(s => `<p><i style="background:${s.color}"></i><b>${s.name}</b> ${GOALS[s.goal].t.toLowerCase()} <small>+${f1(s.bonus)} M€</small></p>`).join('')}</div>`;
}
