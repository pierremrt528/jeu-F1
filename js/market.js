// Marché des transferts : à la mi-saison (entre le GP 5 et le GP 6, transferts très chers) et à
// l'intersaison (prix normaux, on renouvelle aussi le contrat moteur). Utilise l'écran #setup.
import { TEAMS } from './data.js';
import { team, saveTeam } from './team.js';
import { career, saveCareer, applyCareer, salary, mateSkill, statsOf, ENGINES, MARKET } from './career.js';
import { salaryCut } from './infra.js';
import { driverCard, engineCard, steps as stepBar } from './ui.js';

export const MID_ROUND = 5, MID_MULT = 2.5;
const r1 = v => Math.round(v * 10) / 10;

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
    $('#suTitle').textContent = mid ? 'Mercato' : what === 'eng' ? 'Contrat moteur' : 'Transferts';
    stepBar($('#suSteps'), mid ? [] : ['Moteur', 'Pilote'], step);
    $('#suHint').textContent = mid ? `Mi-saison : les transferts coûtent ×${String(MID_MULT).replace('.', ',')}. Un pilote d’une écurie rivale prend la place de ton équipier, qui part chez eux.`
      : what === 'eng' ? 'Intersaison : renouvelle ou change de motoriste pour la saison prochaine.' : 'Intersaison : garde ton équipier (prolongation) ou recrute un pilote libre ou rival.';
    const shown = what === 'eng' ? r1(team.budget - engCost(pick.eng)) : left();
    $('#suBudget').innerHTML = `<small>Budget restant</small><b>${shown.toFixed(1)} M€</b>`;
    let h = '';
    if (what === 'eng') h = ENGINES.map(e => {
      const can = e.price <= team.budget || e === ENGINES[ENGINES.length - 1];
      return engineCard(e, { pick: pick.eng === e.id, disabled: !can, cur: career.engine === e.id });
    }).join('');
    else h = list.map((c, k) => {
      const can = c === pick.drv || c.cost <= left() + pick.drv.cost;
      return driverCard(c.s, { k, from: c.from, color: c.team ? TEAMS[c.team].color : c.cur ? '#ff3d2e' : null, pick: pick.drv === c, disabled: !can, cur: c.cur,
        price: c.cur && mid ? 'Sous contrat' : c.cost < 0 ? `apporte ${-c.cost} M€` : c.cost + ' M€' + (c.cur ? ' (prolonger)' : '') });
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
        t.drivers[c.slot] = { name: old.name, skill: mateSkill(old.spd), stats: { ...old } };
        career.rosters = career.rosters || {};
        career.rosters[t.name] = t.drivers.map(d => ({ name: d.name, skill: d.skill, stats: d.stats }));
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

// ---------- transferts des écuries IA ----------
// À l'ouverture du mercato (après le GP 5 et en fin de saison), les écuries rivales bougent aussi :
// échanges entre écuries, recrutement d'un jeune de Formule B ou d'un vétéran. Renvoie les actualités.
const FIRST = 'ABCDEFGHJKLMNOPRSTVWY';
const LAST = ['Arnaud', 'Belmonte', 'Corvel', 'Dalsgaard', 'Esteve', 'Faraday', 'Granvik', 'Hollis', 'Ibarra', 'Jurek', 'Kowane', 'Lavigne', 'Mirza', 'Nkemelu', 'Ostrander', 'Pellerin', 'Quaresma', 'Rautio', 'Sallès', 'Tervo', 'Ulloa', 'Valtonen', 'Wexler', 'Yilmazer', 'Zanetti'];
const rnd = (a, b) => a + Math.floor(Math.random() * (b - a + 1));
const isYoung = d => /Formule B/.test(d.from || '');
const isVet = d => /Vétéran/.test(d.from || '');
function newYoungster(taken) {
  let name;
  do name = FIRST[rnd(0, FIRST.length - 1)] + '. ' + LAST[rnd(0, LAST.length - 1)]; while (taken.has(name));
  taken.add(name);
  return { name, from: `Formule B · ${rnd(1, 6)}e`, spd: rnd(66, 81), strat: rnd(58, 74), tyre: rnd(60, 78), pot: rnd(72, 95) };
}
const val = s => s.spd + s.strat * 0.15 + s.tyre * 0.15;   // valeur d'un pilote pour une écurie

export function aiMercato(kind) {
  if (!career) return [];
  if (!career.free) career.free = MARKET.filter(d => d.name !== career.mate.name).map(d => ({ ...d }));
  const news = [], mid = kind === 'mid', taken = new Set();
  TEAMS.forEach(t => t.drivers.forEach(d => taken.add(d.name)));
  career.free.forEach(d => taken.add(d.name));
  // intersaison : une nouvelle promotion de Formule B arrive, les vétérans progressent moins
  if (!mid) {
    for (let k = 0; k < 3; k++) career.free.push(newYoungster(taken));
    // une saison de plus : les jeunes à potentiel progressent, les vétérans déclinent un peu
    TEAMS.slice(1).forEach(t => {
      t.drivers = t.drivers.map(d => {
        const st = { ...statsOf(d) };
        st.spd = Math.max(50, Math.min(99, Math.round(st.spd + (st.pot >= 65 ? st.pot / 100 * rnd(1, 3) : -rnd(0, 2)))));
        return { name: d.name, skill: mateSkill(st.spd), stats: st };
      });
      career.rosters = career.rosters || {};
      career.rosters[t.name] = t.drivers.map(d => ({ ...d }));
    });
  }
  const rivals = () => TEAMS.slice(1).map((t, k) => ({ t, ti: k + 1 }));
  const place = (ti, di, s) => {
    const t = TEAMS[ti];
    t.drivers[di] = { name: s.name, skill: mateSkill(s.spd), stats: { ...s, from: t.name } };
    career.rosters = career.rosters || {};
    career.rosters[t.name] = t.drivers.map(d => ({ name: d.name, skill: d.skill, stats: d.stats }));
  };
  const moves = mid ? rnd(1, 2) : rnd(3, 5);
  for (let m = 0; m < moves; m++) {
    const kindMove = Math.random();
    const r = rivals();
    if (kindMove < 0.4) {
      // échange : une écurie mieux classée (plus performante) attire le meilleur pilote d'une plus petite
      const big = r.filter(x => x.t.perf >= 0.97), small = r.filter(x => x.t.perf < 0.975);
      const A = big[rnd(0, big.length - 1)], B = small[rnd(0, small.length - 1)];
      if (!A || !B || A === B) continue;
      const ai = A.t.drivers.reduce((w, d, k) => val(statsOf(d)) < val(statsOf(A.t.drivers[w])) ? k : w, 0);
      const bi = B.t.drivers.reduce((w, d, k) => val(statsOf(d)) > val(statsOf(B.t.drivers[w])) ? k : w, 0);
      const a = statsOf(A.t.drivers[ai]), b = statsOf(B.t.drivers[bi]);
      if (val(b) <= val(a)) continue;
      place(A.ti, ai, b); place(B.ti, bi, a);
      news.push(`🔁 ${b.name} quitte ${B.t.name} pour ${A.t.name} ; ${a.name} fait le chemin inverse.`);
    } else {
      // recrutement d'un pilote libre (jeune ou vétéran) à la place du pilote le plus faible d'une écurie
      const young = kindMove < 0.75;
      const pool = career.free.filter(d => (young ? isYoung(d) : isVet(d) || !isYoung(d)) && !d.sponsor);
      if (!pool.length) continue;
      const c = pool.sort((x, y) => (young ? y.pot - x.pot : val(y) - val(x)))[rnd(0, Math.min(2, pool.length - 1))];
      const X = r[rnd(0, r.length - 1)];
      const wi = X.t.drivers.reduce((w, d, k) => val(statsOf(d)) < val(statsOf(X.t.drivers[w])) ? k : w, 0);
      const out = statsOf(X.t.drivers[wi]);
      if (!young && val(c) <= val(out)) continue;   // un vétéran n'est pris que s'il est meilleur
      career.free = career.free.filter(d => d.name !== c.name);
      career.free.push({ ...out, from: (out.spd >= 88 ? 'Vétéran' : 'Libre') + ' · ex-' + X.t.name });
      place(X.ti, wi, c);
      news.push(young ? `🎓 ${X.t.name} recrute le jeune ${c.name} (${c.from}) et se sépare de ${out.name}.`
        : `🧓 ${X.t.name} mise sur l'expérience de ${c.name} ; ${out.name} est libéré.`);
    }
  }
  // le marché libre ne grossit pas indéfiniment : on garde les 14 meilleurs
  career.free = career.free.sort((a, b) => val(b) + b.pot * 0.2 - val(a) - a.pot * 0.2).slice(0, 14);
  saveCareer(); applyCareer();
  return news;
}
