// Montage de l'écurie en début de saison : niveau de l'écurie (budget), 2e pilote, motoriste.
// Le choix est appliqué à l'Écurie Martel (TEAMS[0]) : performance de la voiture et équipier.
import { TEAMS, PLAYER, DIFFS } from './data.js';
import { team, saveTeam } from './team.js';
import { mateGrowth } from './infra.js';
import { driverCard, engineCard, tierCard, steps, rating } from './ui.js';
import { openNego, preview, previewHTML, fmtM, ans, ROLES } from './nego.js';
import { season, seasonOpts } from './season.js';
import { applyTheme, PALETTE, colorOf, clash, DEFAULT_COLORS } from './theme.js';
import { SHAPES, logoSVG, autoAbbr, cleanAbbr } from './logo.js';
import { frame, focus } from './showroom.js';
import { SKINS, HAIRS, HAIR_COLS, HELMETS, DEFAULT_LOOK } from './pilot3d.js';
import NATIONS from './config/nations.js';
import ENGINES from './config/motoristes.js';
import { offers, sponsorCard, spOf, MAX_SEC } from './sponsors.js';

export const TIERS = [
  { id: 'new', name: 'Nouvelle écurie', desc: 'Petit budget, voiture en retrait, ni pièces ni bâtiments : tout est à construire.', budget: 30, perf: 0.955, rep: 30, moral: 70 },
  { id: 'mid', name: 'Milieu de peloton', desc: 'Budget moyen, quelques pièces et bâtiments déjà en place. Objectif : les points.', budget: 70, perf: 0.96, parts: 1, infra: { rd: 1, hosp: 1, pitc: 1 }, rep: 50, moral: 60 },
  { id: 'top', name: 'Top team', desc: 'Énorme budget, voiture développée et usine moderne. Obligation de gagner.', budget: 140, perf: 0.97, parts: 2, infra: { rd: 2, wind: 1, hosp: 2, media: 1, acad: 1, pitc: 2 }, rep: 80, moral: 55 },
];
// Motoristes inventés : config/motoristes.js (puissance, fiabilité, consommation, coût annuel)
export { ENGINES };
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

// applique le montage à l'écurie du joueur (nom choisi à la création, transferts aux écuries rivales)
const BASE = { name: TEAMS[0].name, perf: TEAMS[0].perf, rosters: TEAMS.map(t => t.drivers.map(d => ({ ...d }))) };
// nom d'écurie saisi par le joueur : sans caractères HTML, 24 caractères au plus
export const cleanName = s => String(s || '').replace(/[<>&"]/g, '').replace(/\s+/g, ' ').trim().slice(0, 24);
const taken = n => TEAMS.slice(1).some(t => t.name.toLowerCase() === cleanName(n).toLowerCase());
const abbrTaken = a => TEAMS.slice(1).some(t => autoAbbr(t.name) === a);
export const DEFAULT_LOGO = { shape: 0, p: 'rouge', s: 'blanc' };
export const helmetHex = h => h && { m: h.m, p: colorOf(h.p).hex, s: colorOf(h.s).hex };
export const natOf = id => NATIONS.find(n => n.id === id);
export function applyCareer() {
  const t = TEAMS[0];
  t.name = cleanName(career?.name) || BASE.name;
  // identité et couleurs (anciennes sauvegardes : rouge et blanc, abréviation tirée du nom)
  t.abbr = career?.abbr || autoAbbr(t.name);
  t.logo = career?.logo || DEFAULT_LOGO;
  // pilote du joueur et réglages de carrière (anciennes sauvegardes : « Toi », 22 courses)
  PLAYER.name = career?.player?.name || 'Toi';
  t.look = career?.player?.look; t.helmet = career?.player ? helmetHex(career.player.helmet) : null;
  seasonOpts.races = career?.opts?.races || 22;
  applyTheme(career?.colors || null);
  TEAMS.forEach((tm, k) => { if (k) tm.drivers = (career?.rosters?.[tm.name] || BASE.rosters[k]).map(d => ({ ...d })); });
  if (!career) { t.perf = BASE.perf; t.drivers[1] = { ...BASE.rosters[0][1] }; return; }
  const tier = TIERS.find(x => x.id === career.tier), eng = ENGINES.find(x => x.id === career.engine);
  t.perf = tier.perf * enginePerf(eng);
  // statut du contrat : n°1 un peu plus rapide, n°2 un peu moins mais il laisse passer le joueur
  const role = career.contract?.role;
  t.drivers[1] = { name: career.mate.name, skill: mateSkill(career.mate.spd) * (role === 'n1' ? 1.003 : role === 'n2' ? 0.997 : 1), stats: career.mate, role };
}
// contrat de l'équipier { sal, years, role, bonus, since, paid } ; anciennes sauvegardes : 1 an au salaire de base
export function mateContract() {
  if (!career) return null;
  if (!career.contract) { const y = season?.year || 2027; career.contract = { sal: salary(career.mate), years: 1, role: 'eq', bonus: 0, since: y, paid: y }; }
  return career.contract;
}
export const untilOf = k => k.since + k.years - 1;
// place de l'écurie pour les négociations : classement en cours, sinon saison passée, sinon niveau
export const TIER_POS = { new: 9, mid: 5, top: 2 };
// stats d'un pilote d'une écurie rivale, tirées de son niveau (toujours les mêmes pour un nom donné)
export function statsOf(d) {
  if (d.stats) return d.stats;
  let h = 0; for (const ch of d.name) h = (h * 31 + ch.charCodeAt(0)) >>> 0;
  const r = k => ((h >>> (k * 5)) % 23) - 11;
  const spd = Math.max(50, Math.min(99, Math.round((d.skill - 0.87) / 0.001)));
  return { name: d.name, spd, strat: Math.max(50, Math.min(97, spd - 6 + r(1))), tyre: Math.max(50, Math.min(97, spd - 5 + r(2))), pot: Math.max(30, Math.min(95, 60 + r(3) * 2)) };
}
applyCareer();
export const engineOf = () => career && ENGINES.find(x => x.id === career.engine);

// Le potentiel fait progresser l'équipier après chaque course de la saison
export function growMate() {
  if (!career) return null;
  const m = career.mate, g = Math.round(m.pot / 100 * (0.4 + Math.random() * 1.2) * mateGrowth() * 10) / 10;
  if (m.spd >= 99) return null;
  m.spd = Math.min(99, Math.round((m.spd + g) * 10) / 10);
  saveCareer(); applyCareer();
  return g;
}

// ---------- écran de montage (5 étapes : identité, couleurs, niveau, équipier, motoriste) ----------
// aides : { abs, tc, line } des réglages du jeu, proposées à l'étape Carrière
export function openSetup(el, onDone, onCancel, aids = {}) {
  const c0 = career?.colors || DEFAULT_COLORS;
  const pick = { tier: null, mate: null, engine: null, k: null, cost: 0, name: TEAMS[0].name, abbr: '', logo: { ...(career?.logo || DEFAULT_LOGO) }, colors: { ...c0 },
    opts: { races: 22, diff: 'normal', abs: aids.abs ?? true, tc: aids.tc ?? true, line: aids.line || 0 },
    sp: { main: null, sec: [] },
    player: { name: '', nat: 'fr', num: 7, look: { ...DEFAULT_LOOK }, helmet: { m: 1, p: 'jaune', s: 'carbone' } } };
  const year = season ? season.year + 1 : 2027, talks = {};
  const STEPS = [
    ['carr', 'Carrière', 'Carrière', 'Nombre de courses, niveau des pilotes IA, aides au pilotage et point de départ de ton écurie.'],
    ['pilote', 'Pilote', 'Pilote', 'Tu es propriétaire et pilote : ton nom, ta nationalité, ton numéro, ton allure et ton casque.'],
    ['ident', 'Identité', 'Identité', 'Le nom, l’abréviation de 3 lettres (classements, chronos) et le logo de ton écurie.'],
    ['cols', 'Couleurs', 'Couleurs', 'Couleur primaire et secondaire : livrée, combinaison, QG et menus. Une paire déjà portée par une écurie rivale n’est pas proposée.'],
    ['moteur', 'Moteur', 'Motoriste', 'Le moteur fixe la vitesse de pointe ; la fiabilité évite la casse. Prix payé chaque saison.'],
    ['spons', 'Sponsors', 'Sponsors', 'Un sponsor principal et jusqu’à 3 secondaires : prime à la signature, versement après chaque GP et bonus si l’objectif de la course est atteint.'],
    ['equip', 'Équipier', 'Équipier', 'Touche un pilote pour négocier avec son agent : salaire, durée, statut, prime. Les meilleurs boudent les petites écuries.'],
  ];
  let step = 0;
  const cur = () => STEPS[step][0];
  const $ = s => el.querySelector(s);
  const left = () => {
    let b = pick.tier ? TIERS.find(t => t.id === pick.tier).budget : 0;
    if (pick.mate) b -= pick.cost;
    if (pick.engine) b -= ENGINES.find(e => e.id === pick.engine).price;
    for (const id of [pick.sp.main, ...pick.sp.sec]) if (id) b += spOf(id).sign;
    return Math.round(b * 10) / 10;
  };
  const abbr = () => cleanAbbr(pick.abbr) || autoAbbr(pick.name);
  const idOk = () => !taken(pick.name) && abbr().length === 3 && !abbrTaken(abbr());
  const pname = () => cleanName(pick.player.name);
  function render() {
    const c = cur(), room = c === 'pilote' || c === 'cols';
    el.classList.toggle('room', room);
    if (room) { focus(c === 'pilote' ? 'pilot' : 'car'); innerHeight > innerWidth ? frame(0.04, 0.96, 0.04, 0.36) : frame(0.52, 0.99, 0.08, 0.84); }
    $('#suTitle').textContent = STEPS[step][2];
    steps($('#suSteps'), STEPS.map(x => x[1]), step);
    $('#suHint').textContent = STEPS[step][3];
    $('#suBudget').innerHTML = pick.tier ? `<small>Budget restant</small><b>${left().toFixed(1)} M€</b>` : '<small>Budget</small><b>—</b>';
    let h = '';
    if (c === 'carr') h = careerHTML();
    if (c === 'pilote') h = pilotHTML();
    if (c === 'ident') h = identityHTML();
    if (c === 'cols') h = colorsHTML();
    if (c === 'equip') h = MARKET.map((d, k) => {
      if (pick.mate === d) return driverCard(d, { k, from: d.from, pick: true, price: `✍️ ${ans(pick.k.years)} · ${fmtM(pick.k.sal)} M€ / saison${pick.k.role !== 'eq' ? ' · ' + ROLES[pick.k.role] : ''}` });
      const v = preview(subOf(d), nctx());
      return driverCard(d, { k, from: d.from, disabled: v.off, price: (v.sal < 0 ? `apporte ${fmtM(-v.sal)} M€` : v.sal != null ? `≈ ${fmtM(v.sal)} M€ / saison` : '') + previewHTML(v) });
    }).join('');
    if (c === 'spons') h = sponsorsHTML();
    if (c === 'moteur') h = ENGINES.map(e => {
      const can = e.price <= left() + (pick.engine ? ENGINES.find(x => x.id === pick.engine).price : 0) + (pick.mate ? pick.cost : 0);
      return engineCard(e, { pick: pick.engine === e.id, disabled: !can });
    }).join('');
    $('#suList').innerHTML = h;
    $('#suList').className = 'clist ' + { carr: 'ident', pilote: 'ident', ident: 'ident', cols: 'cols', spons: 'ident', moteur: 'engs', equip: 'drvs' }[c];
    if (c === 'carr') bindCareer();
    if (c === 'pilote') bindPilot();
    if (c === 'ident') bindIdentity();
    if (c === 'cols') bindColors();
    if (c === 'spons') bindSponsors();
    if (c === 'moteur' || c === 'equip') el.querySelectorAll('#suList [data-v]').forEach(b => b.onclick = () => {
      const v = b.dataset.v;
      if (c === 'equip') {
        const d = MARKET[+v];
        if (pick.mate === d) return;
        return openNego(subOf(d), nctx(), { done: k => { pick.mate = d; pick.k = k; pick.cost = Math.round((k.sal + k.bonus) * 10) / 10; render(); }, close: render });
      }
      if (c === 'moteur') { pick.engine = v; if (left() < 0) { pick.mate = null; pick.k = null; pick.cost = 0; } }
      render();
    });
    const ok = { carr: !!pick.tier, pilote: pname().length >= 2, ident: idOk(), cols: !clash(pick.colors.p, pick.colors.s), moteur: !!pick.engine, spons: !!pick.sp.main, equip: !!pick.mate }[c];
    $('#suNext').disabled = !ok;
    $('#suNext').textContent = step < STEPS.length - 1 ? 'Suivant →' : 'Arriver au QG →';
    $('#suBack').textContent = step ? '← Retour' : 'Annuler';
  }
  $('#suNext').onclick = () => {
    if (step < STEPS.length - 1) { step++; if (cur() === 'pilote' || cur() === 'cols') preview3D(); return render(); }
    const tier = TIERS.find(t => t.id === pick.tier);
    el.classList.remove('room'); focus('car');
    const P = pick.player;
    career = { v: 2, name: cleanName(pick.name) || BASE.name, abbr: abbr(), logo: { ...pick.logo }, colors: { ...pick.colors }, opts: { ...pick.opts },
      player: { name: pname(), nat: P.nat, num: P.num, look: { ...P.look }, helmet: { ...P.helmet } },
      sponsors: { main: pick.sp.main, sec: [...pick.sp.sec] },
      tier: pick.tier, engine: pick.engine, mate: { ...pick.mate }, contract: { ...pick.k, since: year, paid: year } };
    saveCareer(); applyCareer();
    Object.assign(team, { budget: left(), rep: tier.rep, moral: tier.moral, boost: {}, boostTxt: '', conf: 60, deals: [] }); saveTeam();
    onDone();
  };
  $('#suBack').onclick = () => { if (step) { step--; if (cur() === 'pilote' || cur() === 'cols') preview3D(); render(); } else { el.classList.remove('room'); focus('car'); applyCareer(); onCancel(); } };
  const seg = (key, list, val) => `<div class="segs">${list.map(([v, t]) => `<button class="${val === v ? 'sel' : ''}" data-${key}="${v}">${t}</button>`).join('')}</div>`;
  // ---- étape Carrière : nombre de courses, difficulté, aides, niveau de départ ----
  function careerHTML() {
    const O = pick.opts;
    return `<div class="crow"><div class="idsec"><h4>Nombre de courses</h4>${seg('races', [[12, '12 GP'], [16, '16 GP'], [22, '22 GP']], O.races)}</div>
      <div class="idsec"><h4>Pilotes IA</h4>${seg('diff', Object.entries(DIFFS).map(([k, d]) => [k, d.name]), O.diff)}</div></div>
      <div class="idsec"><h4>Aides au pilotage</h4><div class="segs"><button class="${O.abs ? 'sel' : ''}" data-aid="abs">ABS ${O.abs ? '✓' : '✗'}</button><button class="${O.tc ? 'sel' : ''}" data-aid="tc">Antipatinage ${O.tc ? '✓' : '✗'}</button><button class="${O.line ? 'sel' : ''}" data-aid="line">Trajectoire : ${['aucune', 'freinage', 'freinage + direction'][O.line]}</button></div></div>
      <div class="idsec"><h4>Niveau de départ</h4><div class="clist tiers">${TIERS.map((t, k) => tierCard(t, k, pick.tier === t.id)).join('')}</div></div>`;
  }
  function bindCareer() {
    const O = pick.opts;
    el.querySelectorAll('[data-races]').forEach(b => b.onclick = () => { O.races = +b.dataset.races; render(); });
    el.querySelectorAll('[data-diff]').forEach(b => b.onclick = () => { O.diff = b.dataset.diff; render(); });
    el.querySelectorAll('[data-aid]').forEach(b => b.onclick = () => { const a = b.dataset.aid; if (a === 'line') O.line = (O.line + 1) % 3; else O[a] = !O[a]; render(); });
    el.querySelectorAll('.tiers [data-v]').forEach(b => b.onclick = () => { pick.tier = b.dataset.v; pick.mate = null; pick.k = null; pick.cost = 0; pick.engine = null; pick.sp = { main: null, sec: [] }; render(); });
  }
  // ---- étape Sponsors : un principal (obligatoire), jusqu'à 3 secondaires ----
  function sponsorsHTML() {
    const rep = TIERS.find(t => t.id === pick.tier)?.rep ?? 30, n = pick.opts.races, S = pick.sp;
    return `<div class="idsec"><h4>Sponsor principal <small>${S.main ? spOf(S.main).name : 'à choisir'}</small></h4><div class="clist spns">${offers('main', rep).map(s => sponsorCard(s, n, { pick: S.main === s.id })).join('')}</div></div>
      <div class="idsec"><h4>Sponsors secondaires <small>${S.sec.length}/${MAX_SEC}</small></h4><div class="clist spns">${offers('sec', rep).map(s => sponsorCard(s, n, { pick: S.sec.includes(s.id), disabled: !S.sec.includes(s.id) && S.sec.length >= MAX_SEC })).join('')}</div></div>
      <p class="cnote">Les marques les plus prestigieuses attendent une écurie réputée : ta réputation de départ est de ${rep}.</p>`;
  }
  function bindSponsors() {
    el.querySelectorAll('[data-sp]').forEach(b => b.onclick = () => {
      const s = spOf(b.dataset.sp), S = pick.sp;
      if (s.kind === 'main') S.main = s.id;
      else S.sec = S.sec.includes(s.id) ? S.sec.filter(x => x !== s.id) : [...S.sec, s.id].slice(0, MAX_SEC);
      render();
    });
  }
  // ---- étape Pilote : nom, nationalité, numéro, apparence (3D), casque ----
  const dots = (key, list, val, title) => `<div class="sws dots">${list.map((c, k) => `<button class="sw ${val === k ? 'pick' : ''}" data-${key}="${k}" style="--sw:#${c.toString(16).padStart(6, '0')}" aria-label="${title} ${k + 1}"><i></i></button>`).join('')}</div>`;
  function pilotHTML() {
    const P = pick.player, L = P.look, H = P.helmet;
    return `<div class="crow"><label class="tname"><span>Nom du pilote</span><input id="suPName" maxlength="24" autocomplete="off" spellcheck="false" placeholder="ex. Alex Martin" value="${pname()}"></label>
      <label class="tname num"><span>N°</span><input id="suNum" type="number" min="1" max="99" inputmode="numeric" value="${P.num}"></label></div>
      <label class="tname"><span>Nationalité</span><select id="suNat">${NATIONS.map(n => `<option value="${n.id}" ${n.id === P.nat ? 'selected' : ''}>${n.flag} ${n.name}</option>`).join('')}</select></label>
      <div class="crow"><div class="idsec"><h4>Teint</h4>${dots('skin', SKINS, L.skin, 'Teint')}</div><div class="idsec"><h4>Cheveux</h4>${dots('hc', HAIR_COLS, L.hc, 'Couleur')}</div></div>
      <div class="idsec"><h4>Coiffure</h4>${seg('hair', HAIRS.map((n, k) => [k, n]), L.hair)}${seg('beard', [[0, 'Rasé de près'], [1, 'Barbe']], L.beard)}</div>
      <div class="idsec"><h4>Casque · motif</h4>${seg('hm', HELMETS.map((n, k) => [k, n]), H.m)}</div>
      <div class="idsec"><h4>Casque · couleurs</h4><div class="sws">${swatches('hp', H.p)}</div><div class="sws">${swatches('hs', H.s, id => id === H.p ? 'Même couleur' : '')}</div></div>`;
  }
  function bindPilot() {
    const P = pick.player, set = () => { preview3D(); render(); };
    const nm = $('#suPName'); nm.oninput = () => { P.name = nm.value; $('#suNext').disabled = pname().length < 2; };
    const nu = $('#suNum'); nu.onchange = () => { P.num = Math.max(1, Math.min(99, Math.round(+nu.value) || 7)); nu.value = P.num; };
    $('#suNat').onchange = e => { P.nat = e.target.value; };
    for (const k of ['skin', 'hc', 'hair', 'beard']) el.querySelectorAll(`[data-${k}]`).forEach(b => b.onclick = () => { P.look[k] = +b.dataset[k]; set(); });
    el.querySelectorAll('[data-hm]').forEach(b => b.onclick = () => { P.helmet.m = +b.dataset.hm; set(); });
    el.querySelectorAll('[data-hp]').forEach(b => b.onclick = () => { P.helmet.p = b.dataset.hp; if (P.helmet.s === P.helmet.p) P.helmet.s = PALETTE.find(c => c.id !== P.helmet.p).id; set(); });
    el.querySelectorAll('[data-hs]').forEach(b => b.onclick = () => { P.helmet.s = b.dataset.hs; set(); });
  }
  // ---- étape Identité : nom, abréviation, logo (forme + 2 couleurs) ----
  const swatches = (cls, cur, dis = () => '') => PALETTE.map(c => { const why = dis(c.id); return `<button class="sw ${cur === c.id ? 'pick' : ''}" data-${cls}="${c.id}" style="--sw:#${c.hex.toString(16).padStart(6, '0')}" ${why ? `disabled title="${why}"` : ''} aria-label="${c.name}"><i></i><small>${c.name}</small></button>`; }).join('');
  function identityHTML() {
    const a = abbr(), L = pick.logo;
    return `<div class="idprev"><span class="idlogo">${logoSVG(L)}</span><span class="idtxt"><b>${cleanName(pick.name) || BASE.name}</b><i>${a}</i></span></div>
      <label class="tname ${taken(pick.name) ? 'bad' : ''}"><span>Nom de ton écurie</span><input id="suName" maxlength="24" autocomplete="off" spellcheck="false" placeholder="${BASE.name}" value="${cleanName(pick.name)}"><small>Nom déjà pris par une écurie rivale</small></label>
      <label class="tname abbr ${a.length === 3 && abbrTaken(a) ? 'bad' : ''}"><span>Abréviation</span><input id="suAbbr" maxlength="3" autocomplete="off" spellcheck="false" placeholder="${autoAbbr(pick.name)}" value="${cleanAbbr(pick.abbr)}"><small>Abréviation déjà prise par une écurie rivale</small></label>
      <div class="idsec"><h4>Logo · forme</h4><div class="shapes">${SHAPES.map((S, k) => `<button class="shp ${L.shape === k ? 'pick' : ''}" data-shape="${k}" title="${S.name}">${logoSVG({ ...L, shape: k })}</button>`).join('')}</div></div>
      <div class="idsec"><h4>Logo · couleur principale</h4><div class="sws">${swatches('lp', L.p)}</div>
      <h4>Logo · couleur secondaire</h4><div class="sws">${swatches('ls', L.s, id => id === L.p ? 'Même couleur que la principale' : '')}</div></div>`;
  }
  function bindIdentity() {
    const nm = $('#suName'), ab = $('#suAbbr'), prev = () => {
      $('.idtxt b').textContent = cleanName(pick.name) || BASE.name; $('.idtxt i').textContent = abbr();
      nm.parentNode.classList.toggle('bad', taken(pick.name)); ab.parentNode.classList.toggle('bad', abbr().length === 3 && abbrTaken(abbr()));
      ab.placeholder = autoAbbr(pick.name); $('#suNext').disabled = !idOk();
    };
    nm.oninput = () => { pick.name = nm.value; prev(); };
    ab.oninput = () => { pick.abbr = ab.value = cleanAbbr(ab.value); prev(); };
    el.querySelectorAll('[data-shape]').forEach(b => b.onclick = () => { pick.logo.shape = +b.dataset.shape; render(); });
    el.querySelectorAll('[data-lp]').forEach(b => b.onclick = () => { pick.logo.p = b.dataset.lp; if (pick.logo.s === pick.logo.p) pick.logo.s = PALETTE.find(c => c.id !== pick.logo.p).id; render(); });
    el.querySelectorAll('[data-ls]').forEach(b => b.onclick = () => { pick.logo.s = b.dataset.ls; render(); });
  }
  // ---- étape Couleurs : primaire + secondaire, sans reprendre la paire d'une écurie rivale ----
  function colorsHTML() {
    const C = pick.colors, bad = clash(C.p, C.s);
    const why = id => id === C.p ? 'Même couleur que la primaire' : clash(C.p, id) ? `Déjà portée avec ta primaire par ${clash(C.p, id).name}` : '';
    return `<div class="idsec"><h4>Couleur primaire <small>${colorOf(C.p).name}</small></h4><div class="sws big">${swatches('cp', C.p)}</div></div>
      <div class="idsec"><h4>Couleur secondaire <small>${colorOf(C.s).name}</small></h4><div class="sws big">${swatches('cs', C.s, why)}</div>
      <p class="cnote">${bad ? `⚠️ ${bad.name} roule déjà en ${colorOf(C.p).name.toLowerCase()} et ${colorOf(C.s).name.toLowerCase()} : choisis une autre secondaire.` : PALETTE.some(c => c.id !== C.p && clash(C.p, c.id)) ? `Grisées : couleurs déjà portées avec ta primaire par une écurie rivale (${TEAMS.slice(1).filter(t => PALETTE.some(c => clash(C.p, c.id) === t)).map(t => t.name).join(', ')}).` : ''}</p></div>`;
  }
  function bindColors() {
    const set = () => { preview3D(); render(); };
    el.querySelectorAll('[data-cp]').forEach(b => b.onclick = () => {
      const C = pick.colors; C.p = b.dataset.cp;
      if (C.s === C.p || clash(C.p, C.s)) C.s = PALETTE.find(c => c.id !== C.p && !clash(C.p, c.id) && c.id !== C.s)?.id || C.s;
      set();
    });
    el.querySelectorAll('[data-cs]').forEach(b => b.onclick = () => { pick.colors.s = b.dataset.cs; set(); });
  }
  // aperçu dans l'atelier 3D : nom, abréviation et logo choisis (rétablis par applyCareer si on annule)
  function preview3D() { Object.assign(TEAMS[0], { name: cleanName(pick.name) || BASE.name, abbr: abbr(), logo: { ...pick.logo }, look: { ...pick.player.look }, helmet: helmetHex(pick.player.helmet) }); applyTheme(pick.colors); }
  applyTheme(pick.colors);
  // négociation : réputation et place selon le niveau choisi ; le budget restant hors pilote
  const subOf = d => ({ kind: 'drv', key: d.name, name: d.name, from: d.from, r: rating(d), base: salary(d), sponsor: d.sponsor, color: /Formule B/.test(d.from) ? '#22d3c5' : /Vétéran/.test(d.from) ? '#b07cff' : '#8d96a3' });
  const nctx = () => {
    const tier = TIERS.find(t => t.id === pick.tier);
    return { win: 'setup-' + pick.tier, store: talks, rep: tier.rep, pos: TIER_POS[tier.id], team: TEAMS[0].name, year,
      avail: () => Math.round((left() + pick.cost) * 10) / 10, cost: k => ({ now: Math.round((k.sal + k.bonus) * 10) / 10, parts: [['prime', k.bonus], ['1re saison', k.sal]] }) };
  };
  render();
}
