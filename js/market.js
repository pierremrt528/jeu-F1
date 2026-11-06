// Marché des transferts : à la mi-saison (entre le GP 5 et le GP 6, transferts très chers) et à
// l'intersaison (prix normaux, on renouvelle aussi le contrat moteur). Utilise l'écran #setup.
import { TEAMS } from './data.js';
import { team, saveTeam } from './team.js';
import { career, saveCareer, applyCareer, salary, mateSkill, statsOf, ENGINES, MARKET } from './career.js';
import { salaryCut } from './infra.js';

export const MID_ROUND = 5, MID_MULT = 2.5;
const r1 = v => Math.round(v * 10) / 10;
const statBar = (l, v) => `<div class="cstat"><span>${l}</span><i><em style="width:${v}%"></em></i><b>${Math.round(v)}</b></div>`;

function candidates(mid) {
  if (!career.free) career.free = MARKET.filter(d => d.name !== career.mate.name).map(d => ({ ...d }));
  const list = [{ s: career.mate, from: 'Ton pilote actuel', cur: true }];
  TEAMS.forEach((t, ti) => { if (ti) t.drivers.forEach((d, di) => list.push({ s: statsOf(d), from: t.name, team: ti, slot: di })); });
  for (const d of career.free) list.push({ s: d, from: d.from || 'Libre' });
  for (const c of list) {
    const base = salary(c.s) * (c.s.sponsor ? 1 : salaryCut());
    c.cost = c.cur ? (mid ? 0 : base) : base < 0 ? (mid ? base * 0.5 : base) : base * (mid ? MID_MULT : 1);
    c.cost = r1(c.cost);
  }
  return [list[0], ...list.slice(1).sort((a, b) => b.s.spd - a.s.spd)];
}

// kind : 'mid' (mi-saison) ou 'off' (intersaison : moteur puis pilote)
export function openMarket(el, kind, onDone, onCancel) {
  const mid = kind === 'mid', $ = s => el.querySelector(s);
  const list = candidates(mid), steps = mid ? ['drv'] : ['eng', 'drv'];
  const pick = { drv: list[0], eng: career.engine };
  let step = 0;
  const engCost = id => ENGINES.find(e => e.id === id).price;
  const left = () => r1(team.budget - (steps.includes('eng') ? engCost(pick.eng) : 0) - pick.drv.cost);
  function render() {
    const what = steps[step];
    $('#suTitle').textContent = mid ? '🔁 Mercato de mi-saison' : what === 'eng' ? 'Intersaison · 1. Contrat moteur' : 'Intersaison · 2. Ton deuxième pilote';
    const shown = what === 'eng' ? r1(team.budget - engCost(pick.eng)) : left();
    $('#suBudget').innerHTML = `Budget restant : <b>${shown.toFixed(1)} M€</b>` + (mid ? ` <small>· transferts ×${String(MID_MULT).replace('.', ',')} en cours de saison</small>` : '');
    let h = '';
    if (what === 'eng') h = ENGINES.map(e => {
      const can = e.price <= team.budget || e === ENGINES[ENGINES.length - 1];
      return `<button class="copt eng ${pick.eng === e.id ? 'pick' : ''}" data-v="${e.id}" ${can ? '' : 'disabled'} style="--c:${e.color}"><b>${e.name}${career.engine === e.id ? ' ✓' : ''}</b>
        ${statBar('Performance', e.perf)}${statBar('Fiabilité', e.fiab)}<em>${e.price} M€ / saison</em></button>`;
    }).join('');
    else h = list.map((c, k) => {
      const can = c === pick.drv || c.cost <= left() + pick.drv.cost;
      return `<button class="copt drv ${pick.drv === c ? 'pick' : ''} ${c.cur ? 'cur' : ''}" data-v="${k}" ${can ? '' : 'disabled'}><b>${c.s.name}</b><small>${c.from}</small>
        ${statBar('Vitesse', c.s.spd)}${statBar('Stratégie', c.s.strat)}${statBar('Pneus', c.s.tyre)}${statBar('Potentiel', c.s.pot)}
        <em>${c.cur && mid ? 'Sous contrat' : c.cost < 0 ? `apporte ${-c.cost} M€` : c.cost + ' M€' + (c.cur ? ' (prolonger)' : '')}</em></button>`;
    }).join('');
    $('#suList').innerHTML = h;
    $('#suList').className = 'clist ' + (what === 'eng' ? 'engs' : 'drvs');
    el.querySelectorAll('#suList [data-v]').forEach(b => b.onclick = () => {
      if (what === 'eng') pick.eng = b.dataset.v; else pick.drv = list[+b.dataset.v];
      render();
    });
    $('#suNext').disabled = what === 'drv' && left() < 0;
    $('#suNext').textContent = step < steps.length - 1 ? 'Suivant →' : mid ? (pick.drv.cur ? 'Garder mon équipier' : `Signer ${pick.drv.s.name}`) : 'Lancer la nouvelle saison';
    $('#suBack').textContent = step ? '← Retour' : mid ? 'Fermer' : 'Annuler';
  }
  $('#suNext').onclick = () => {
    if (step < steps.length - 1) { step++; return render(); }
    const c = pick.drv, old = career.mate;
    if (!c.cur) {
      if (c.team) {   // l'ancien équipier part dans l'écurie rivale à sa place
        const t = TEAMS[c.team];
        t.drivers[c.slot] = { name: old.name, skill: mateSkill(old.spd) };
        career.rosters = career.rosters || {};
        career.rosters[t.name] = t.drivers.map(d => ({ name: d.name, skill: d.skill }));
      } else {
        career.free = career.free.filter(d => d.name !== c.s.name);
        career.free.push({ ...old, from: 'Ex-équipier' });
      }
      career.mate = { ...c.s, from: c.team ? 'Ex-' + TEAMS[c.team].name : c.s.from };
    }
    team.budget = left();
    if (!mid) career.engine = pick.eng;
    saveCareer(); applyCareer(); saveTeam();
    onDone(c.cur ? null : c.s.name);
  };
  $('#suBack').onclick = () => { if (step) { step--; render(); } else onCancel(); };
  render();
}
