// Montage de l'écurie en début de saison : niveau de l'écurie (budget), 2e pilote, motoriste.
// Le choix est appliqué à l'Écurie Martel (TEAMS[0]) : performance de la voiture et équipier.
import { TEAMS } from './data.js';
import { team, saveTeam } from './team.js';

export const TIERS = [
  { id: 'new', name: 'Nouvelle écurie', desc: 'Petit budget, voiture en retrait. Tout est à construire.', budget: 30, perf: 0.955, rep: 30, moral: 70 },
  { id: 'mid', name: 'Milieu de peloton', desc: 'Budget moyen, voiture correcte. Objectif : les points.', budget: 70, perf: 0.97, rep: 50, moral: 60 },
  { id: 'top', name: 'Top team', desc: 'Énorme budget, voiture de pointe. Obligation de gagner.', budget: 140, perf: 0.985, rep: 80, moral: 55 },
];
// Motoristes inventés : perf et fiab sur 100, prix en M€ pour la saison
export const ENGINES = [
  { id: 'valdrin', name: 'Valdrin', perf: 94, fiab: 72, price: 40, color: '#e5483b' },
  { id: 'orsetti', name: 'Orsetti Motori', perf: 88, fiab: 90, price: 34, color: '#3aa0ff' },
  { id: 'hexadyne', name: 'Hexadyne', perf: 80, fiab: 82, price: 20, color: '#f2b632' },
  { id: 'kiyora', name: 'Kiyora', perf: 70, fiab: 94, price: 10, color: '#3ddc84' },
];
// Pilotes libres : vitesse, stratégie, gestion des pneus, potentiel (sur 100), salaire en M€ / saison
export const MARKET = [
  { name: 'V. Kessler', from: 'Vétéran', spd: 90, strat: 88, tyre: 85, pot: 40 },
  { name: 'H. Sandoval', from: 'Vétéran', spd: 86, strat: 80, tyre: 90, pot: 45 },
  { name: 'A. Rivière', from: 'Ex-équipier', spd: 80, strat: 75, tyre: 74, pot: 60 },
  { name: 'L. Marchetti', from: 'Formule B · champion', spd: 84, strat: 66, tyre: 70, pot: 92 },
  { name: 'S. Okoro', from: 'Formule B · 2e', spd: 79, strat: 70, tyre: 72, pot: 88 },
  { name: 'E. Vasquez', from: 'Formule B · 4e', spd: 74, strat: 62, tyre: 76, pot: 82 },
  { name: 'T. Aaltonen', from: 'Formule B · 7e', spd: 68, strat: 70, tyre: 66, pot: 75 },
  { name: 'M. Dufresne', from: 'Payant (sponsor)', spd: 62, strat: 60, tyre: 64, pot: 50, sponsor: 4 },
];
export const salary = d => d.sponsor ? -d.sponsor : Math.round((Math.pow((d.spd * 0.6 + d.strat * 0.15 + d.tyre * 0.15 + d.pot * 0.1) / 100, 6) * 40) * 10) / 10;
export const mateSkill = spd => 0.87 + spd * 0.001;   // 90 → 0.96
export const enginePerf = e => 1 + (e.perf - 85) * 0.0012;

const KEY = 'gpm-career';
export let career = (() => { try { return JSON.parse(localStorage.getItem(KEY)); } catch { return null; } })();
export function resetCareer() { career = null; try { localStorage.removeItem(KEY); } catch {} applyCareer(); }
export function saveCareer() { try { localStorage.setItem(KEY, JSON.stringify(career)); } catch {} }

// applique le montage à l'Écurie Martel
const BASE = { perf: TEAMS[0].perf, mate: { ...TEAMS[0].drivers[1] } };
export function applyCareer() {
  const t = TEAMS[0];
  if (!career) { t.perf = BASE.perf; t.drivers[1] = { ...BASE.mate }; return; }
  const tier = TIERS.find(x => x.id === career.tier), eng = ENGINES.find(x => x.id === career.engine);
  t.perf = tier.perf * enginePerf(eng);
  t.drivers[1] = { name: career.mate.name, skill: mateSkill(career.mate.spd), stats: career.mate };
}
applyCareer();
export const engineOf = () => career && ENGINES.find(x => x.id === career.engine);

// Le potentiel fait progresser l'équipier après chaque course de la saison
export function growMate() {
  if (!career) return null;
  const m = career.mate, g = Math.round(m.pot / 100 * (0.4 + Math.random() * 1.2) * 10) / 10;
  if (m.spd >= 99) return null;
  m.spd = Math.min(99, Math.round((m.spd + g) * 10) / 10);
  saveCareer(); applyCareer();
  return g;
}

// ---------- écran de montage (3 étapes) ----------
const statBar = (l, v) => `<div class="cstat"><span>${l}</span><i><em style="width:${v}%"></em></i><b>${Math.round(v)}</b></div>`;
export function openSetup(el, onDone, onCancel) {
  const pick = { tier: null, mate: null, engine: null };
  let step = 0;
  const $ = s => el.querySelector(s);
  const left = () => {
    let b = pick.tier ? TIERS.find(t => t.id === pick.tier).budget : 0;
    if (pick.mate) b -= salary(pick.mate);
    if (pick.engine) b -= ENGINES.find(e => e.id === pick.engine).price;
    return Math.round(b * 10) / 10;
  };
  function render() {
    const titles = ['1. Ton écurie', '2. Ton deuxième pilote', '3. Ton motoriste'];
    $('#suTitle').textContent = titles[step];
    $('#suBudget').innerHTML = pick.tier ? `Budget restant : <b>${left().toFixed(1)} M€</b>` : 'Choisis le niveau de ton écurie';
    let h = '';
    if (step === 0) h = TIERS.map(t => `<button class="copt ${pick.tier === t.id ? 'pick' : ''}" data-v="${t.id}"><b>${t.name}</b><small>${t.desc}</small><em>${t.budget} M€</em></button>`).join('');
    if (step === 1) h = MARKET.map((d, k) => {
      const s = salary(d), can = s <= left() + (pick.mate ? salary(pick.mate) : 0);
      return `<button class="copt drv ${pick.mate === d ? 'pick' : ''}" data-v="${k}" ${can ? '' : 'disabled'}><b>${d.name}</b><small>${d.from}</small>
        ${statBar('Vitesse', d.spd)}${statBar('Stratégie', d.strat)}${statBar('Pneus', d.tyre)}${statBar('Potentiel', d.pot)}
        <em>${s < 0 ? `apporte ${-s} M€` : s + ' M€ / saison'}</em></button>`;
    }).join('');
    if (step === 2) h = ENGINES.map(e => {
      const can = e.price <= left() + (pick.engine ? ENGINES.find(x => x.id === pick.engine).price : 0);
      return `<button class="copt eng ${pick.engine === e.id ? 'pick' : ''}" data-v="${e.id}" ${can ? '' : 'disabled'} style="--c:${e.color}"><b>${e.name}</b>
        ${statBar('Performance', e.perf)}${statBar('Fiabilité', e.fiab)}<em>${e.price} M€ / saison</em></button>`;
    }).join('');
    $('#suList').innerHTML = h;
    $('#suList').className = 'clist ' + ['tiers', 'drvs', 'engs'][step];
    el.querySelectorAll('#suList [data-v]').forEach(b => b.onclick = () => {
      const v = b.dataset.v;
      if (step === 0) { pick.tier = v; pick.mate = null; pick.engine = null; }
      if (step === 1) pick.mate = MARKET[+v];
      if (step === 2) pick.engine = v;
      render();
    });
    const ok = [pick.tier, pick.mate, pick.engine][step];
    $('#suNext').disabled = !ok;
    $('#suNext').textContent = step < 2 ? 'Suivant →' : 'Lancer la saison';
    $('#suBack').textContent = step ? '← Retour' : 'Annuler';
  }
  $('#suNext').onclick = () => {
    if (step < 2) { step++; return render(); }
    const tier = TIERS.find(t => t.id === pick.tier);
    career = { tier: pick.tier, engine: pick.engine, mate: { ...pick.mate } };
    saveCareer(); applyCareer();
    Object.assign(team, { budget: left(), rep: tier.rep, moral: tier.moral, boost: {}, boostTxt: '' }); saveTeam();
    onDone();
  };
  $('#suBack').onclick = () => { if (step) { step--; render(); } else onCancel(); };
  render();
}
