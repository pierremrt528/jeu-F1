// Marché des transferts : à la mi-saison (entre le GP 5 et le GP 6, transferts très chers) et à
// l'intersaison (prix normaux, on renouvelle aussi le contrat moteur). Utilise l'écran #setup.
import { TEAMS } from './data.js';
import { team, saveTeam } from './team.js';
import { career, saveCareer, applyCareer, salary, mateSkill, statsOf, ENGINES, MARKET, mateContract, untilOf, TIER_POS } from './career.js';
import { salaryCut } from './infra.js';
import { driverCard, engineCard, steps as stepBar, rating } from './ui.js';
import { season, saveSeason, teamStandings } from './season.js';
import { openNego, preview, previewHTML, fmtM, ROLES } from './nego.js';

export const MID_MULT = 2.5;
const r1 = v => Math.round(v * 10) / 10;
const hex = c => typeof c === 'number' ? '#' + c.toString(16).padStart(6, '0') : c;

// place de l'écurie du joueur (attire ou non les pilotes et les chefs) : classement en cours, sinon saison passée, sinon niveau
export function teamPos() {
  if (season && (season.round >= 3 || season.done)) return teamStandings().findIndex(t => t.player) + 1;
  return career?.lastPos || TIER_POS[career?.tier] || 6;
}

function candidates() {
  if (!career.free) career.free = MARKET.filter(d => d.name !== career.mate.name).map(d => ({ ...d }));
  const list = [{ s: career.mate, from: 'Ton pilote actuel', cur: true }];
  TEAMS.forEach((t, ti) => { if (ti) t.drivers.forEach((d, di) => list.push({ s: statsOf(d), from: t.name, team: ti, slot: di })); });
  for (const d of career.free) list.push({ s: d, from: d.from || 'Libre' });
  return [list[0], ...list.slice(1).sort((a, b) => b.s.spd - a.s.spd)];
}
const colorOf = c => c.team ? hex(TEAMS[c.team].color) : c.cur ? '#ff3d2e' : /Formule B/.test(c.from) ? '#22d3c5' : /Vétéran/.test(c.from) ? '#b07cff' : '#8d96a3';

// kind : 'mid' (mi-saison, pendant la pause estivale) ou 'off' (intersaison : moteur puis pilote).
// Chaque pilote se négocie (nego.js) ; la signature est payée tout de suite.
export function openMarket(el, kind, onDone, onCancel) {
  const mid = kind === 'mid', $ = s => el.querySelector(s);
  const year = season.year, ny = mid ? year : year + 1, win = `${year}-${kind}`;   // ny : 1re saison des nouveaux contrats
  const T = career.talks = career.talks || {};
  const steps = mid ? ['drv'] : ['eng', 'drv'];
  let list = candidates(), eng = career.engine, step = 0;
  const engCost = id => ENGINES.find(e => e.id === id).price;
  const K = () => mateContract(), valid = () => untilOf(K()) >= ny;
  const due = () => !mid && valid() && K().paid < ny ? K().sal : 0;       // salaire de l'équipier pour la saison prochaine
  const left = () => r1(team.budget - (mid ? 0 : engCost(eng) + due()));
  // indemnité de départ d'un équipier encore sous contrat qu'on libère : la moitié des saisons restantes
  const release = () => { const k = K(), n = untilOf(k) - year; return n > 0 && k.sal > 0 ? r1(0.5 * k.sal * n) : 0; };
  const subOf = c => ({ kind: 'drv', key: c.s.name, name: c.s.name, from: c.cur ? c.s.from || '' : c.from, r: rating(c.s), cur: !!c.cur,
    base: salary(c.s) * (c.s.sponsor ? 1 : salaryCut()), sponsor: c.s.sponsor, club: c.team ? TEAMS[c.team].name : null, color: colorOf(c),
    floor: c.cur && valid() && K().sal > 0 ? r1(K().sal * 1.05) : 0 });
  const costOf = (c, k, fee) => {
    const rel = c.cur || c.team ? 0 : release(), sal = mid ? r1(k.sal * 0.5) : k.sal;
    return { now: r1(fee + k.bonus + sal + rel), parts: [['indemnité', fee], ['prime', k.bonus], [mid ? 'demi-saison' : 'saison ' + ny, sal], ['départ de ' + career.mate.name, rel]] };
  };
  const nctx = c => ({ win, store: T, rep: team.rep, pos: teamPos(), mid, feeMult: mid ? MID_MULT : 1, team: TEAMS[0].name, year: ny,
    swap: c.team ? career.mate.name : null, save: saveCareer, avail: () => r1(team.budget - (mid ? 0 : engCost(eng))), cost: (k, fee) => costOf(c, k, fee) });
  function sign(c, k, fee) {
    const old = career.mate;
    team.budget = r1(team.budget - costOf(c, k, fee).now);
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
    career.contract = { sal: k.sal, years: k.years, role: k.role, bonus: k.bonus, since: ny, paid: ny };
    T.signed = win;
    (season.news = season.news || []).push(c.cur ? `✍️ ${c.s.name} prolonge chez ${TEAMS[0].name} jusqu’en ${ny + k.years - 1}.`
      : `✍️ ${c.s.name} signe chez ${TEAMS[0].name} jusqu’en ${ny + k.years - 1}${fee ? ` (indemnité de ${fmtM(fee)} M€ pour ${TEAMS[c.team].name})` : ''}.`);
    saveCareer(); applyCareer(); saveTeam(); saveSeason();
    if (mid) return onDone(c.s.name);
    list = candidates(); render();
  }
  // contrat terminé et personne de signé : un pilote de réserve prend le volant (1 an, statut n°2)
  function reserve() {
    const taken = new Set(); TEAMS.forEach(t => t.drivers.forEach(d => taken.add(d.name))); career.free.forEach(d => taken.add(d.name));
    const y = newYoungster(taken);
    career.free.push({ ...career.mate, from: 'Ex-équipier' });
    career.mate = { ...y, from: 'Pilote de réserve', spd: rnd(62, 68), strat: rnd(55, 62), tyre: rnd(58, 66), pot: rnd(60, 76) };
    career.contract = { sal: 1, years: 1, role: 'n2', bonus: 0, since: ny, paid: ny - 1 };
  }
  function render() {
    const what = steps[step], k = K(), u = untilOf(k), lock = T.signed === win;
    $('#suTitle').textContent = mid ? 'Mercato' : what === 'eng' ? 'Contrat moteur' : 'Transferts';
    stepBar($('#suSteps'), mid ? [] : ['Moteur', 'Pilote'], step);
    $('#suHint').textContent = mid ? `Mi-saison : indemnités de transfert ×${String(MID_MULT).replace('.', ',')} et agents plus gourmands. Touche un pilote pour négocier. Un pilote d’une écurie rivale prend la place de ton équipier, qui part chez eux.`
      : what === 'eng' ? 'Intersaison : renouvelle ou change de motoriste pour la saison prochaine. ' + (season.offNote || '')
      : lock ? 'Recrutement bouclé pour cette intersaison.'
      : valid() ? `${career.mate.name} est sous contrat jusqu’en ${u}. Touche un pilote pour négocier ; libérer un équipier sous contrat coûte une indemnité de départ.`
      : `Le contrat de ${career.mate.name} est terminé : touche sa carte pour le prolonger, ou négocie avec un autre pilote.`;
    const shown = what === 'eng' ? r1(team.budget - engCost(eng)) : left();
    $('#suBudget').innerHTML = `<small>Budget restant</small><b>${shown.toFixed(1)} M€</b>`;
    let h = '';
    if (what === 'eng') h = ENGINES.map(e => {
      const can = e.price <= team.budget || e === ENGINES[ENGINES.length - 1];
      return engineCard(e, { pick: eng === e.id, disabled: !can, cur: career.engine === e.id });
    }).join('');
    else h = list.map((c, i) => {
      if (c.cur) return driverCard(c.s, { k: i, from: `Ton équipier · ${ROLES[k.role]}`, color: '#ff3d2e', cur: true, pick: !mid && valid(),
        price: (mid || valid() ? `Sous contrat jusqu’en ${u}${!mid ? ` · ${fmtM(k.sal)} M€` : ''}` : 'Contrat terminé') + (!mid && !lock ? previewHTML(preview(subOf(c), nctx(c))) : '') });
      const v = preview(subOf(c), nctx(c));
      return driverCard(c.s, { k: i, from: c.from, color: colorOf(c), disabled: v.off || lock,
        price: (v.sal < 0 ? `apporte ${fmtM(-v.sal)} M€` : v.sal != null ? `≈ ${fmtM(v.sal)} M€ / saison` : '') + (v.fee ? `<small> + indemnité ${fmtM(v.fee)} M€</small>` : '') + previewHTML(v) });
    }).join('');
    $('#suList').innerHTML = h;
    $('#suList').className = 'clist ' + (what === 'eng' ? 'engs' : 'drvs');
    el.querySelectorAll('#suList [data-v]').forEach(b => b.onclick = () => {
      if (what === 'eng') { eng = b.dataset.v; return render(); }
      const c = list[+b.dataset.v];
      if (c.cur && (mid || lock)) return;
      openNego(subOf(c), nctx(c), { done: (k, fee) => sign(c, k, fee), close: render });
    });
    $('#suNext').disabled = false;
    $('#suNext').textContent = step < steps.length - 1 ? 'Suivant →' : mid ? 'Garder mon équipier' : valid() ? 'Lancer la nouvelle saison' : 'Lancer avec un pilote de réserve';
    $('#suBack').textContent = step ? '← Retour' : mid ? 'Fermer' : 'Annuler';
  }
  $('#suNext').onclick = () => {
    if (step < steps.length - 1) { step++; return render(); }
    if (mid) return onDone(null);
    if (!valid()) reserve();
    const k = K();
    team.budget = r1(team.budget - engCost(eng) - (k.paid < ny ? k.sal : 0)); k.paid = ny;
    if (team.budget < 0) {   // découvert épongé par la direction, au prix de la réputation
      team.budget = 0; team.rep = Math.max(0, team.rep - 8); team.moral = Math.max(0, team.moral - 10);
    }
    career.engine = eng;
    saveCareer(); applyCareer(); saveTeam();
    onDone(null);
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
