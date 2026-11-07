// Garage : arbres de développement (moteur, aéro, châssis, fiabilité). Une pièce coûte des M€ et met
// 1 à 3 GP à être livrée ; une seule pièce en développement par pôle. Sauvegarde locale et écran.
import { NO_UP } from './car.js';
import { team, saveTeam } from './team.js';
import { engineOf } from './career.js';
import { partDiscount, partDelay, parallelParts, pitBonus, renderInfra, infra } from './infra.js';
import { CHIEFS, DEPTS, chiefOf, chiefMul, staff, saveStaff, chiefContract, chiefUntil, chiefRelease, signChief } from './staff.js';
import { openNego, preview, previewHTML, fmtM } from './nego.js';
import { teamPos } from './market.js';
import { season, preStep } from './season.js';
import { TEAMS } from './data.js';
import { art, DEPT_ART } from './art.js';
import { frame } from './showroom.js';

// fx : bonus additionnels (0.02 = +2 %). rel : fiabilité. tyre/fuel : usure et consommation (-0.05 = −5 %). pit : secondes à l'arrêt.
// req : toutes les pièces requises ; reqAny : au moins une.
// Chaque pôle a sa propre forme d'arbre (at : [rangée, colonne] sur 3 colonnes) et ses propres prix.
export const TREES = [
  { id: 'moteur', name: 'Moteur', color: '#e5483b', nodes: [   // long tronc, pièces chères
    { id: 'm1', at: [1, 2], name: 'Admission optimisée', desc: 'Meilleur remplissage des cylindres.', cost: 20, fx: { vtop: 0.006, acc: 0.02 } },
    { id: 'm2', at: [2, 1], name: 'Turbo grand débit', desc: 'Plus de puissance à haut régime.', cost: 45, req: ['m1'], fx: { vtop: 0.012 } },
    { id: 'm3', at: [2, 3], name: 'Cartographie couple', desc: 'Des relances plus franches en sortie de virage.', cost: 30, req: ['m1'], fx: { acc: 0.05 } },
    { id: 'm4', at: [3, 2], name: "Récupération d'énergie", desc: "L'énergie du freinage est rendue à l'accélération.", cost: 60, reqAny: ['m2', 'm3'], fx: { acc: 0.04, vtop: 0.005 } },
    { id: 'm5', at: [4, 1], name: 'Mode attaque', desc: 'Une cartographie agressive pour les lignes droites.', cost: 50, req: ['m4'], fx: { vtop: 0.01 } },
    { id: 'm6', at: [4, 3], name: 'Gestion thermique', desc: 'Le moteur consomme moins et casse moins.', cost: 35, req: ['m4'], fx: { fuel: -0.06, rel: 5 } },
    { id: 'm7', at: [5, 1], name: 'Injection haute pression', desc: 'Plus de puissance sur toute la plage de régime.', cost: 80, req: ['m5'], fx: { vtop: 0.008, acc: 0.03 } },
    { id: 'm8', at: [6, 2], name: 'Moteur ultime', desc: 'Le moteur le plus puissant jamais construit par l’écurie.', cost: 140, req: ['m6', 'm7'], fx: { vtop: 0.014, acc: 0.03 } },
  ] },
  { id: 'aero', name: 'Aéro', color: '#3aa0ff', nodes: [   // arbre large à trois branches
    { id: 'a1', at: [1, 2], name: 'Aileron avant affiné', desc: "Plus d'appui sur le train avant.", cost: 12, fx: { kd: 0.04 } },
    { id: 'a2', at: [2, 1], name: 'Fond plat à effet de sol', desc: 'La voiture est plaquée au sol dans les courbes rapides.', cost: 25, req: ['a1'], fx: { kd: 0.06 } },
    { id: 'a4', at: [2, 2], name: 'Écopes de freins', desc: 'L’air refroidit mieux les freins.', cost: 15, req: ['a1'], fx: { brake: 0.04 } },
    { id: 'a3', at: [2, 3], name: 'Carrosserie basse traînée', desc: "Plus de vitesse de pointe, un peu moins d'appui.", cost: 25, req: ['a1'], fx: { vtop: 0.01, kd: -0.02 } },
    { id: 'a5', at: [3, 1], name: 'Diffuseur double', desc: "Beaucoup d'appui supplémentaire.", cost: 45, req: ['a2'], fx: { kd: 0.08 } },
    { id: 'a6', at: [3, 3], name: 'Aileron arrière mobile', desc: "L'aspiration derrière une voiture devient bien plus efficace.", cost: 40, req: ['a3'], fx: { slip: 0.5 } },
    { id: 'a8', at: [4, 1], name: 'Ailerons flexibles', desc: 'De l’appui en courbe, moins de traînée en ligne droite.', cost: 70, req: ['a5'], fx: { kd: 0.05, vtop: 0.004 } },
    { id: 'a7', at: [4, 3], name: 'Canalisation du sillage', desc: 'Suivre une voiture devient plus facile.', cost: 55, req: ['a6'], fx: { slip: 0.3, kd: 0.03 } },
    { id: 'a9', at: [5, 2], name: 'Pack aéro intégral', desc: 'Toute la carrosserie revue en soufflerie.', cost: 95, req: ['a5', 'a6'], fx: { kd: 0.06, vtop: 0.006 } },
    { id: 'a10', at: [6, 2], name: 'Concept aéro radical', desc: 'Une voiture entièrement repensée autour de l’air.', cost: 150, req: ['a9'], fx: { kd: 0.07, vtop: 0.006 } },
  ] },
  { id: 'chassis', name: 'Châssis', color: '#f2b632', nodes: [   // deux racines qui se rejoignent
    { id: 'c1', at: [1, 1], name: 'Suspensions raidies', desc: "Plus d'adhérence mécanique dans les virages lents.", cost: 18, fx: { mu: 0.02 } },
    { id: 'c2', at: [1, 3], name: 'Freins carbone', desc: 'Des freinages plus courts.', cost: 22, fx: { brake: 0.08 } },
    { id: 'c3', at: [2, 1], name: 'Direction rapide', desc: 'La voiture tourne plus vite et plus précisément.', cost: 28, req: ['c1'], fx: { steer: 0.1 } },
    { id: 'c4', at: [2, 3], name: 'Étriers refroidis', desc: 'Les freins restent efficaces toute la course.', cost: 32, req: ['c2'], fx: { brake: 0.07 } },
    { id: 'c5', at: [3, 2], name: 'Allègement du châssis', desc: 'Moins de poids : meilleure accélération et adhérence.', cost: 65, reqAny: ['c3', 'c4'], fx: { acc: 0.03, mu: 0.02 } },
    { id: 'c6', at: [4, 1], name: 'Suspension active', desc: 'Les pneus travaillent moins et s’usent moins.', cost: 85, req: ['c5'], fx: { mu: 0.02, tyre: -0.05 } },
    { id: 'c7', at: [5, 2], name: 'Monocoque nouvelle génération', desc: 'Un châssis plus rigide et plus léger.', cost: 120, req: ['c5', 'c6'], fx: { mu: 0.03, brake: 0.04, steer: 0.06 } },
  ] },
  { id: 'fiab', name: 'Fiabilité', color: '#3ddc84', nodes: [   // petit arbre, pièces bon marché
    { id: 'f1', at: [1, 2], name: 'Radiateurs renforcés', desc: 'Le moteur chauffe moins : moins de casses.', cost: 10, fx: { rel: 10 } },
    { id: 'f2', at: [2, 1], name: 'Structure anti-choc', desc: 'Les contacts avec les murs coûtent moins de vitesse.', cost: 18, req: ['f1'], fx: { wall: -0.25 } },
    { id: 'f3', at: [2, 3], name: 'Fond plat renforcé', desc: "On perd moins de vitesse dans l'herbe et le gravier.", cost: 14, req: ['f1'], fx: { off: 0.15 } },
    { id: 'f4', at: [3, 2], name: 'Boîte de vitesses durcie', desc: 'Des passages de rapports plus rapides et plus sûrs.', cost: 30, reqAny: ['f2', 'f3'], fx: { rel: 15, acc: 0.01 } },
    { id: 'f5', at: [4, 2], name: 'Contrôle qualité', desc: 'Chaque pièce est vérifiée avant la course.', cost: 40, req: ['f4'], fx: { rel: 15, wall: -0.15 } },
    { id: 'f6', at: [5, 2], name: 'Fiabilité totale', desc: 'La voiture est quasi incassable.', cost: 60, req: ['f5'], fx: { rel: 20, off: 0.1 } },
  ] },
  { id: 'stand', name: 'Stands', color: '#c06cff', nodes: [   // losange
    { id: 's1', at: [1, 2], name: 'Pistolets de roue rapides', desc: 'Les roues sont changées plus vite.', cost: 10, fx: { pit: -0.3 } },
    { id: 's2', at: [2, 1], name: 'Gestion des pneus', desc: 'Des réglages qui ménagent la gomme.', cost: 20, req: ['s1'], fx: { tyre: -0.06 } },
    { id: 's3', at: [2, 3], name: 'Carburant optimisé', desc: 'Le moteur consomme moins.', cost: 16, req: ['s1'], fx: { fuel: -0.05 } },
    { id: 's5', at: [3, 1], name: 'Couvertures chauffantes', desc: 'Les pneus arrivent à bonne température.', cost: 32, req: ['s2'], fx: { tyre: -0.06 } },
    { id: 's4', at: [3, 2], name: 'Cric automatique', desc: 'La voiture est levée en un instant.', cost: 25, reqAny: ['s2', 's3'], fx: { pit: -0.3 } },
    { id: 's6', at: [3, 3], name: 'Simulateur de stratégie', desc: 'Des relais mieux calculés.', cost: 30, req: ['s3'], fx: { fuel: -0.05, tyre: -0.03 } },
    { id: 's7', at: [4, 2], name: 'Équipe de stand d’élite', desc: 'Des mécaniciens entraînés tous les jours.', cost: 50, req: ['s4'], fx: { pit: -0.4 } },
    { id: 's9', at: [5, 1], name: 'Muret de champions', desc: 'La meilleure équipe de stand du plateau.', cost: 80, req: ['s5', 's7'], fx: { tyre: -0.06, pit: -0.2 } },
    { id: 's8', at: [5, 3], name: 'Ravitaillement rapide', desc: 'Le plein en un éclair.', cost: 55, req: ['s6', 's7'], fx: { pit: -0.3 } },
  ] },
];
const NODES = Object.fromEntries(TREES.flatMap(t => t.nodes.map(n => [n.id, n])));
const TREE_OF = Object.fromEntries(TREES.flatMap(t => t.nodes.map(n => [n.id, t.id])));
// prix en M€ (Centre de R&D, chef du pôle), délai en jours (Soufflerie, chef), risques d'échec et de retard (chef)
export const price = n => Math.round(n.cost / 5 * partDiscount() * chiefMul(TREE_OF[n.id], 'cost') * 10) / 10;
export const delay = n => Math.max(6, Math.round(partDelay(n.cost <= 20 ? 20 : n.cost <= 55 ? 36 : 56) * chiefMul(TREE_OF[n.id], 'time')));   // 20 à 56 jours
export const risk = n => {
  const k = chiefMul(TREE_OF[n.id], 'risk');
  return { fail: Math.round((n.cost <= 20 ? 6 : n.cost <= 55 ? 10 : 16) * k), late: Math.round(22 * k) };
};

// ---------- état sauvegardé ----------
const KEY = 'gpm-garage';
export const garage = (() => {
  let s = { owned: [], dev: {} };
  try { s = Object.assign(s, JSON.parse(localStorage.getItem(KEY)) || {}); } catch {}
  return s;
})();
garage.dev = garage.dev || {};
function save() { try { localStorage.setItem(KEY, JSON.stringify(garage)); } catch {} }

const owned = id => garage.owned.includes(id);
garage.owned = garage.owned.filter(id => NODES[id]);   // anciennes sauvegardes
for (const k in garage.dev) if (!NODES[garage.dev[k]?.id]) delete garage.dev[k];
// anciennes sauvegardes : délais comptés en GP → jours
for (const d of Object.values(garage.dev)) if (!d.days) { d.left *= 10; d.days = 1; }
const unlocked = n => (!n.req || n.req.every(owned)) && (!n.reqAny || n.reqAny.some(owned));

export function carUpgrades() {
  const up = { ...NO_UP };
  for (const id of garage.owned) {
    const fx = NODES[id]?.fx || {};
    for (const k in fx) up[k] += fx[k];
  }
  const e = engineOf(); if (e) up.rel += e.fiab - 50;   // fiabilité du motoriste
  up.pit += pitBonus();                                  // centre des mécaniciens
  return up;
}

export function resetGarage() { garage.owned = []; garage.dev = {}; save(); }

// Pièces de départ selon le niveau de l'écurie : n pièces par pôle, les moins chères accessibles d'abord
export function grantParts(n) {
  for (const t of TREES) for (let k = 0; k < n; k++) {
    const c = t.nodes.filter(x => !owned(x.id) && unlocked(x)).sort((a, b) => a.cost - b.cost)[0];
    if (c) garage.owned.push(c.id);
  }
  save();
}
// Nouveau règlement : 1 ou 2 pôles de la voiture changent ; tirage en fin de saison, appliqué à la suivante
export function drawRegulation() {
  const pool = TREES.filter(t => t.id !== 'stand').map(t => t.id).sort(() => Math.random() - 0.5);
  return pool.slice(0, Math.random() < 0.35 ? 2 : 1);
}
export const treeName = id => TREES.find(t => t.id === id)?.name || id;
export function applyRegulation(ids) {
  for (const id of ids) {
    const t = TREES.find(x => x.id === id);
    garage.owned = garage.owned.filter(o => !t.nodes.some(n => n.id === o));
    delete garage.dev[id]; delete garage.dev[id + '#2'];
  }
  save();
}
// indice de performance de la voiture du joueur (même échelle que la « perf » des écuries IA)
export function perfFactor(u = carUpgrades()) {
  return 1 + 0.6 * (u.vtop - 1) + 0.15 * (u.acc - 1) + 0.08 * (u.kd - 1) + 0.25 * (u.mu - 1) + 0.04 * (u.brake - 1) + 0.04 * (u.steer - 1);
}
// Entre deux GP : les recherches avancent du nombre de jours écoulés. À la fin des essais, une pièce
// peut prendre du retard (une fois) ou échouer (argent perdu). Renvoie { done, fail, late }.
export function tickDevelopment(days) {
  const out = { done: [], fail: [], late: [] };
  for (const k in garage.dev) {
    const d = garage.dev[k], n = NODES[d.id];
    d.left -= days;
    while (d.left <= 0) {
      const r = risk(n);
      if (!d.rolled && Math.random() * 100 < r.late) { const x = 8 + Math.floor(Math.random() * 17); d.left += x; if (d.tot) d.tot += x; d.rolled = true; out.late.push(`${n.name} (+${x} j)`); continue; }
      if (Math.random() * 100 < r.fail) out.fail.push(n.name); else { garage.owned.push(d.id); out.done.push(n.name); }
      delete garage.dev[k]; break;
    }
  }
  save();
  return out;
}

// événements : retard (+) ou avance (−) de toutes les recherches en cours, en jours
export function shiftDev(days) {
  for (const k in garage.dev) { const d = garage.dev[k]; d.left = Math.max(1, d.left + days); if (d.tot && days > 0) d.tot += days; }
  save();
}
export const devCount = () => Object.keys(garage.dev).length;

// ---------- écran ----------
const STATS = [
  ['Vitesse de pointe', u => u.vtop - 1, 0.09], ['Accélération', u => u.acc - 1, 0.3], ['Appui aéro', u => u.kd - 1, 0.4],
  ['Adhérence', u => u.mu - 1, 0.12], ['Freinage', u => u.brake - 1, 0.25], ['Agilité', u => u.steer - 1, 0.18],
  ['Usure des pneus', u => 1 - u.tyre, 0.4], ['Consommation', u => 1 - u.fuel, 0.16], ['Arrêt au stand', u => -u.pit, 3], ['Fiabilité', u => u.rel / 60, 1],
];
const pct = (k, v) => k === 'rel' ? `+${v}` : k === 'pit' ? `${v.toFixed(1)} s` : `${v > 0 ? '+' : ''}${(v * 100).toFixed(v * 100 % 1 ? 1 : 0)} %`;
const FXN = { vtop: 'vitesse de pointe', acc: 'accélération', kd: 'appui', mu: 'adhérence', brake: 'freinage', steer: 'agilité', slip: 'aspiration', off: 'hors piste', wall: 'perte aux chocs', rel: 'fiabilité', tyre: 'usure des pneus', fuel: 'consommation', pit: 'à l’arrêt' };
// emplacements de développement d'un pôle (2 avec la Soufflerie niveau 3)
const slots = t => [t.id, t.id + '#2'].slice(0, parallelParts());
const devOf = id => Object.values(garage.dev).find(d => d.id === id);

let zone = 'usine', tab = 'apercu', sel = null, root = null;
const ICONS = { moteur: '⚡', aero: '🌀', chassis: '🔩', fiab: '🛡️', stand: '🔧', infra: '🏗️' };
const ZONES = {
  usine: { name: 'Usine', title: 'Recherche', tabs: ['apercu', 'moteur', 'aero', 'chassis', 'fiab', 'stand'] },
  inst: { name: 'Installations', title: 'Installations', tabs: ['infra'] },
  chefs: { name: 'Chefs', title: 'Chefs de pôle', tabs: Object.keys(DEPTS) },
};
const DESC = { moteur: 'Puissance et relances', aero: 'Appui et traînée', chassis: 'Adhérence, freins, direction', fiab: 'Casse, chocs, hors-piste', stand: 'Arrêts, pneus, carburant' };
const treeById = id => TREES.find(t => t.id === id);
const tname = t => t.id === 'stand' ? 'Pit stop' : t.name;
// un effet est-il favorable ? (usure, consommation et temps d'arrêt : plus bas = mieux)
const infraBusy = () => !!infra.build;
const good = (k, v) => ['tyre', 'fuel', 'pit', 'wall'].includes(k) ? v < 0 : v > 0;
const fr = v => String(v).replace('.', ',');
const hex = c => '#' + c.toString(16).padStart(6, '0');
const tall = () => innerHeight > innerWidth;
const left = d => Math.max(0, d.left);
const prog = d => Math.max(0.04, Math.min(1, 1 - d.left / (d.tot || Math.max(d.left, delay(NODES[d.id])))));
const nodeState = n => owned(n.id) ? 'own' : devOf(n.id) ? 'wip' : unlocked(n) ? 'ok' : 'lock';
const ORDER = t => t.nodes.slice().sort((a, b) => a.at[0] - b.at[0] || a.at[1] - b.at[1]);
const chain = t => `<span class="chain" style="--c:${t.color}">${ORDER(t).map(n => `<i class="${nodeState(n)}"></i>`).join('')}</span>`;
const ring = (t, f) => `<span class="ring"><svg viewBox="0 0 60 60"><circle cx="30" cy="30" r="26"/><circle class="pg" cx="30" cy="30" r="26" stroke-dasharray="${(163.4 * f).toFixed(1)} 200"/></svg>${art(DEPT_ART[t.id])}</span>`;
const moraleTxt = m => m < 35 ? ['Bas', 'bad'] : m < 55 ? ['Normal', ''] : m < 75 ? ['Bon', 'ok'] : ['Excellent', 'ok'];

// hiérarchie des écuries (indice de performance), avec la voiture du joueur éventuellement améliorée (u2)
function perfTable(u2) {
  const rows = TEAMS.map((t, k) => ({ name: t.name, color: t.color, me: !k, v: k ? t.perf + (season?.dev?.[t.name] || 0) : t.perf * perfFactor() }));
  if (u2) rows[0].v2 = TEAMS[0].perf * perfFactor(u2);
  return rows.sort((a, b) => b.v - a.v);
}
function cmpHTML() {
  const rows = perfTable(), lo = Math.min(...rows.map(r => r.v)) - 0.004, hi = Math.max(...rows.map(r => r.v)) + 0.001;
  const me = rows.findIndex(r => r.me), keep = k => k === 0 || Math.abs(k - Math.max(2, me)) <= 2;
  return `<h4>Comparaison des performances</h4>` + rows.map((r, k) => `<div class="cr ${r.me ? 'me' : ''} ${keep(k) ? '' : 'far'}"><small>${k + 1}</small><i style="background:${hex(r.color)}"></i><span>${r.name}</span><em><u style="width:${6 + 94 * (r.v - lo) / (hi - lo)}%"></u></em><b>${(r.v * 100).toFixed(1).replace('.', ',')}</b></div>`).join('');
}
// effet d'une pièce sur la place de la voiture dans la hiérarchie
function impactHTML(u2) {
  const rows = perfTable(u2), me = rows.find(r => r.me), p1 = rows.indexOf(me) + 1, p2 = rows.filter(r => !r.me && r.v > me.v2).length + 1;
  const d = (me.v2 - me.v) * 100;
  return `<p class="impact"><small>Performance</small><b>${(me.v * 100).toFixed(1).replace('.', ',')} → ${(me.v2 * 100).toFixed(1).replace('.', ',')}</b><em class="${d > 0.005 ? 'up' : ''}">${d > 0.005 ? '+' + d.toFixed(2).replace('.', ',') : '='}</em><span>P${p1}${p2 < p1 ? ` → <b>P${p2}</b>` : ''} du plateau</span></p>`;
}
// barres « Ta voiture » (u2 : avec la pièce sélectionnée)
function statsHTML(u, u2) {
  const bw = (f, max) => Math.max(0, Math.min(100, 100 * f / max));
  return `<h4>Ta voiture${u2 ? ' <small>▲ avec cette pièce</small>' : ''}</h4>` + STATS.map(([name, f, max]) => {
    const v = f(u), w = bw(v, max), g = u2 ? Math.max(0, bw(f(u2), max) - w) : 0;
    const val = name === 'Fiabilité' ? 50 + u.rel : name === 'Arrêt au stand' ? (v ? '−' + v.toFixed(1) + ' s' : '0 s') : (name === 'Usure des pneus' || name === 'Consommation' ? (v ? '−' : '') : v >= 0 ? '+' : '') + (v * 100).toFixed(1) + ' %';
    return `<div class="${g ? 'up' : ''}"><span>${name}</span><i><em style="width:${w}%"></em>${g ? `<u style="left:${w}%;width:${g}%"></u>` : ''}</i><small>${val}</small></div>`;
  }).join('');
}
// la voiture de l'atelier 3D se place dans la partie libre de l'écran
function frameCar() {
  const ov = zone === 'usine' && tab === 'apercu';
  if (ov) tall() ? frame(0.02, 0.98, 0.27, 0.52) : frame(0.62, 0.99, 0.22, 0.66);
  else tall() ? frame(0.1, 0.9, 0.62, 0.9) : frame(0.5, 0.98, 0.3, 0.9);
}

export function openGarage(el, onClose) {
  root = el; root.querySelector('#garageClose').onclick = onClose;
  render();
}

function render() {
  const $ = s => root.querySelector(s);
  lastTall = tall();
  $('#gPts').textContent = team.budget.toFixed(1);
  if (!ZONES[zone].tabs.includes(tab)) tab = ZONES[zone].tabs[0];
  $('#gTitle').textContent = ZONES[zone].title;
  $('#gTeam').textContent = TEAMS[0].name + (zone === 'usine' ? ` · ${garage.owned.length}/${TREES.reduce((s, t) => s + t.nodes.length, 0)} pièces installées` : '');
  $('#gZones').innerHTML = Object.entries(ZONES).map(([k, z]) => `<button data-zone="${k}" class="${k === zone ? 'sel' : ''}">${z.name}</button>`).join('');
  root.querySelectorAll('[data-zone]').forEach(b => b.onclick = () => { zone = b.dataset.zone; tab = ZONES[zone].tabs[0]; sel = null; render(); });
  const rd = zone === 'usine', ov = rd && tab === 'apercu', card = root.querySelector('.garage');
  card.classList.toggle('rd', rd); card.classList.toggle('one', ZONES[zone].tabs.length < 2);
  root.classList.toggle('isov', ov);
  $('#gRD').classList.toggle('hidden', !ov); $('.gbody').classList.toggle('hidden', ov); $('#gInfo').classList.toggle('hidden', !rd || ov);
  frameCar();
  if (rd) return renderRD($, ov);
  $('#gTabs').innerHTML = ZONES[zone].tabs.map(id => {
    if (zone === 'chefs') { const c = chiefOf(id), t = treeById(id); return `<button data-tab="${id}" class="${id === tab ? 'sel' : ''}" style="--c:${t.color}"><span class="ti">${ICONS[id]}</span><span class="tn">${t.name}</span><small>${c ? c.name : 'poste vacant'}</small><i class="tp"></i></button>`; }
    return `<button data-tab="infra" class="${tab === 'infra' ? 'sel' : ''}" style="--c:#c06cff"><span class="ti">🏗️</span><span class="tn">Bâtiments</span><small>${infraBusy() ? '⏳ chantier' : '6 bâtiments'}</small><i class="tp"></i></button>`;
  }).join('');
  root.querySelectorAll('[data-tab]').forEach(b => b.onclick = () => { tab = b.dataset.tab; sel = null; render(); });
  card.classList.toggle('infra', true);
  if (tab === 'infra') return renderInfra(root, sel, id => { sel = id; render(); }, render);
  return renderChefs($);
}

// ---------- usine : vue d'ensemble des pôles, puis arbre de chaque pôle ----------
function renderRD($, ov) {
  root.querySelector('.garage').classList.remove('infra');
  $('#gTabs').innerHTML = ZONES.usine.tabs.map(id => {
    if (id === 'apercu') return `<button data-tab="apercu" class="${tab === id ? 'sel' : ''}" style="--c:var(--acc)">Aperçu</button>`;
    const t = treeById(id), n = t.nodes.filter(x => owned(x.id)).length;
    return `<button data-tab="${id}" class="${tab === id ? 'sel' : ''}" style="--c:${t.color}">${art(DEPT_ART[id])}${tname(t)}<small>${slots(t).some(k => garage.dev[k]) ? '⏳' : ''}${n}/${t.nodes.length}</small></button>`;
  }).join('');
  root.querySelectorAll('[data-tab]').forEach(b => b.onclick = () => { tab = b.dataset.tab; sel = null; render(); });
  return ov ? renderOverview($) : renderTree($);
}

function renderOverview($) {
  const total = TREES.reduce((s, t) => s + t.nodes.length, 0), [mt, mc] = moraleTxt(team.moral);
  const cap = TREES.length * parallelParts(), used = devCount();
  const eff = [partDiscount() < 1 ? `coûts −${Math.round((1 - partDiscount()) * 100)} %` : '', partDelay(100) < 100 ? `délais −${100 - Math.round(partDelay(100))} %` : ''].filter(Boolean).join(' · ') || 'coûts et délais standard';
  const cards = TREES.map(t => {
    const n = t.nodes.filter(x => owned(x.id)).length, w = slots(t).map(k => garage.dev[k]).filter(Boolean), c = chiefOf(t.id);
    return `<button class="dcard" data-go="${t.id}" style="--c:${t.color}"><span class="dn">${tname(t)}</span><span class="dd">${DESC[t.id]}</span>${ring(t, n / t.nodes.length)}` +
      `<span class="dc"><b>${n}</b>/${t.nodes.length} pièces</span>${chain(t)}` +
      (w.length ? w.map(d => `<span class="dw"><span>⏳ ${NODES[d.id].name}</span><i><em style="width:${prog(d) * 100}%"></em></i><small>prête dans ${left(d)} j</small></span>`).join('')
        : `<span class="dw free">${n === t.nodes.length ? '✓ Pôle complet' : '● Pôle libre : lance une pièce'}</span>`) +
      `<span class="dch">${c ? '👤 ' + c.name : '👤 Pas de chef'}</span></button>`;
  }).join('');
  const all = Object.values(garage.dev).sort((a, b) => a.left - b.left);
  const next = `<div class="dcard next"><span class="dn">Usine</span><span class="dd">Moral <b class="${mc}">${mt}</b> · ${eff}</span>` +
    `<span class="slots"><small>Recherches</small>${Array.from({ length: cap }, (_, k) => `<i class="${k < used ? 'on' : ''}"></i>`).join('')}<i class="n">${used}/${cap}</i></span>` +
    (all.length ? '<div class="nxl">' + all.slice(0, 3).map(d => `<span class="nx" style="--c:${treeById(TREE_OF[d.id]).color}">${art(d.id)}<b>${NODES[d.id].name}</b><em>${left(d)} j</em></span>`).join('') + '</div>'
      : '<p class="hint">Aucune recherche en cours : touche un pôle pour lancer une pièce.</p>') + '</div>';
  $('#gRD').innerHTML = `<div class="ov"><div class="ovcards">${cards}${next}</div><div class="ovside"><div class="cmp">${cmpHTML()}</div></div></div>`;
  root.querySelectorAll('[data-go]').forEach(b => b.onclick = () => { tab = b.dataset.go; sel = null; render(); });
}

function renderTree($) {
  const t = treeById(tab), chief = chiefOf(tab), T = tall();
  const free = slots(t).find(k => !garage.dev[k]), money = n => team.budget >= price(n);
  const n0 = t.nodes.filter(x => owned(x.id)).length, lv = Math.max(...t.nodes.map(n => n.at[0]));
  $('#gInfo').innerHTML = `<span class="st"><small>${tname(t)}</small><b>${n0}<i>/${t.nodes.length}</i>${chain(t)}</b></span><span class="st"><small>Chef de pôle</small><b>${chief ? chief.name : '—'}</b></span>` +
    `<span class="st"><small>Recherches</small><b class="slots">${slots(t).map(k => `<i class="${garage.dev[k] ? 'on' : ''}"></i>`).join('')}<i class="n">${free ? 'libre' : 'occupé'}</i></b></span>`;
  const tree = $('#gTree');
  tree.style.setProperty('--lv', lv);
  tree.innerHTML = `<svg id="gLines"></svg>` + t.nodes.map(n => {
    const w = devOf(n.id), st = owned(n.id) ? 'own' : w ? 'wip' : unlocked(n) ? (free && money(n) ? 'ok' : 'poor') : 'lock';
    const [r, c] = T ? n.at : [n.at[1], n.at[0]];
    const foot = st === 'own' ? 'Installée' : st === 'wip' ? `<i class="pb"><em style="width:${prog(w) * 100}%"></em></i>${left(w)} j` : `<span class="pr">${fr(price(n))} M€</span>${delay(n)} j`;
    return `<button class="pc ${st} ${sel === n.id ? 'pick' : ''}" data-node="${n.id}" style="--c:${t.color};grid-row:${r};grid-column:${c}"><span class="pcode">${n.id.toUpperCase()}</span><span class="pst">${{ own: '✓', wip: '⏳', ok: '+', poor: '+', lock: '🔒' }[st]}</span>${art(n.id)}<b>${n.name}</b><small>${foot}</small></button>`;
  }).join('');
  root.querySelectorAll('[data-node]').forEach(b => b.onclick = () => { sel = b.dataset.node; render(); if (tall()) $('#gDetail').scrollIntoView({ behavior: 'smooth', block: 'nearest' }); });
  requestAnimationFrame(drawLines);
  // détail de la pièce sélectionnée
  const n = sel && NODES[sel], D = $('#gDetail');
  if (!T) root.querySelector('.gside').scrollTop = 0;
  D.style.setProperty('--c', t.color);
  const u = carUpgrades(), u2 = { ...u };
  if (n && !owned(n.id)) for (const k in n.fx) u2[k] += n.fx[k];
  if (n) {
    const w = devOf(n.id), can = !owned(n.id) && !w && unlocked(n) && free && money(n), rk = risk(n);
    const why = !unlocked(n) ? 'Pièce verrouillée : développe d’abord les pièces requises.' : !free ? `Le pôle ${t.name} développe déjà : ${slots(t).map(k => NODES[garage.dev[k].id].name).join(', ')}` : !money(n) ? `Budget insuffisant : il manque ${fr((price(n) - team.budget).toFixed(1))} M€.` : '';
    const reqs = (n.req || n.reqAny || []).map(r => `<span class="${owned(r) ? 'ok' : ''}">${owned(r) ? '✓' : '○'} ${NODES[r].name}</span>`).join(n.reqAny ? '<em>ou</em>' : '');
    D.innerHTML = `<div class="ph"><span class="pimg">${art(n.id)}</span><span class="pt"><small>Pôle ${tname(t)} · ${n.id.toUpperCase()}</small><b>${n.name}</b></span></div><p class="desc">${n.desc}</p>` +
      `<p class="fx">${Object.entries(n.fx).map(([k, v]) => `<span class="${good(k, v) ? '' : 'bad'}">${good(k, v) ? '▲' : '▼'} ${pct(k, v)} ${FXN[k]}</span>`).join('')}</p>` +
      (reqs ? `<p class="reqs"><small>Requiert</small>${reqs}</p>` : '') +
      (owned(n.id) ? `<p class="state done">✓ Pièce installée sur la voiture</p>` : w ? `<p class="state wip">⏳ En recherche · prête dans ${left(w)} jours${w.rolled ? ' (retardée)' : ''}</p>` + impactHTML(u2) :
        `<div class="dcost three"><span><small>Coût</small><b>${fr(price(n))} M€</b></span><span><small>Délai</small><b>${delay(n)} j</b></span><span class="rk"><small>Échec</small><b>${rk.fail} %</b></span></div>` +
        impactHTML(u2) + `<p class="chief">👤 ${chief ? `Chef ${t.name} : <b>${chief.name}</b>` : 'Pas de chef pour ce pôle'} · risque de retard ${rk.late} %</p>` +
        (why ? `<p class="why">${why}</p>` : '') + `<button id="gBuy" class="main" ${can ? '' : 'disabled'}>Lancer la recherche</button>`);
    const buy = $('#gBuy');
    if (buy) buy.onclick = () => {
      if (!can) return;
      team.budget = Math.round((team.budget - price(n)) * 10) / 10; saveTeam();
      garage.dev[free] = { id: n.id, left: delay(n), tot: delay(n), days: 1 }; save(); render();
    };
  } else D.innerHTML = `<div class="ph"><span class="pimg">${art(DEPT_ART[t.id])}</span><span class="pt"><small>${n0}/${t.nodes.length} pièces · ${chief ? 'chef ' + chief.name : 'pas de chef'}</small><b>${tname(t)}</b></span></div>` +
    `<p class="hint">Touche une pièce pour voir ses effets. Une recherche se paie tout de suite et dure 20 à 56 jours : elle avance avec le calendrier (pré-saison, puis entre les Grands Prix). À la fin des essais, elle peut prendre du retard ou échouer : un bon chef de pôle réduit ces risques.</p>`;
  $('#gStats').innerHTML = statsHTML(u, n && !owned(n.id) ? u2 : null);
}

// ---------- mercato des chefs de département : candidats et négociation ----------
function renderChefs($) {
  const dept = tab, t = treeById(dept), cur = chiefOf(dept), ck = cur && chiefContract(dept), yr = season?.year || 2027;
  const bar = (l, v) => `<span class="cb"><small>${l}</small><i><em style="width:${v * 2.8}%"></em></i><b>−${v} %</b></span>`;
  const subOf = c => ({ kind: 'chief', key: c.id, name: c.name, from: c.from, r: 60 + (c.cost + c.time + c.risk) / 2, base: c.fee, cur: cur === c, color: t.color,
    floor: cur === c ? Math.round(ck.sal * 1.05 * 10) / 10 : 0 });
  // prolongation du chef en poste : le nouveau contrat démarre la saison suivante, seule la prime se paie tout de suite
  const nctx = c => ({ win: `${yr}-r${season?.round ?? 0}${preStep() < 3 ? 'p' + preStep() : ''}`,   // pré-saison : une fenêtre par étape
    store: staff.talks = staff.talks || {}, rep: team.rep, pos: teamPos(), mid: false, team: TEAMS[0].name,
    year: yr + (cur === c ? 1 : 0), save: saveStaff, avail: () => team.budget,
    cost: k => {
      const ext = cur === c, rel = cur && !ext ? chiefRelease(dept) : 0, sal = ext ? 0 : k.sal;
      return { now: Math.round((k.bonus + sal + rel) * 10) / 10, parts: [['prime', k.bonus], ['saison ' + yr, sal], ['départ de ' + (cur?.name || ''), rel]] };
    } });
  $('#gTree').innerHTML = CHIEFS.filter(c => c.dept === dept).map(c => {
    const v = cur === c ? null : preview(subOf(c), nctx(c));
    const fee = v ? (v.sal != null ? `≈ ${fmtM(v.sal)} M€ / saison` : '') + previewHTML(v) : `✓ En poste · jusqu’en ${chiefUntil(ck)}`;
    return `<button class="node chef ${v ? (v.off ? 'poor' : 'ok') : 'own'} ${sel === c.id ? 'pick' : ''}" data-chief="${c.id}" style="--c:${t.color}">
    <span class="ch"><span class="av">${c.name.replace(/^[A-Z]\.\s*/, '').slice(0, 3).toUpperCase()}</span><span><b>${c.name}</b><small class="cf">${c.from}</small></span></span>
    ${bar('Coûts', c.cost)}${bar('Délais', c.time)}${bar('Échecs', c.risk)}<small class="fee">${fee}</small></button>`;
  }).join('');
  root.querySelectorAll('[data-chief]').forEach(b => b.onclick = () => { sel = b.dataset.chief; render(); });
  const c = CHIEFS.find(x => x.id === sel && x.dept === dept);
  $('#gDetail').style.setProperty('--c', t.color);
  if (c) {
    const isCur = cur === c, rel = cur && !isCur ? chiefRelease(dept) : 0;
    $('#gDetail').innerHTML = `<div class="dhead"><span class="dk">${ICONS[dept]} Chef ${t.name}</span><b>${c.name}</b></div><p class="desc">${c.from}. Sur toutes les pièces du pôle ${t.name} :</p>` +
      `<p class="fx"><span>▼ ${c.cost} % de coût</span><span>▼ ${c.time} % de délai</span><span>▼ ${c.risk} % de risque d’échec et de retard</span></p>` +
      (cur && !isCur ? `<p class="chief">Remplace <b>${cur.name}</b>${rel ? ` · indemnité de départ ${fmtM(rel)} M€` : ''}</p>` : '') +
      (isCur ? `<p class="state done">✓ En poste jusqu’en ${chiefUntil(ck)} · ${fmtM(ck.sal)} M€ par saison</p>` : `<p class="chief">Salaire de référence ≈ ${fmtM(c.fee)} M€ par saison, payé à la signature puis à chaque intersaison.</p>`) +
      `<button id="cHire" class="main">${isCur ? 'Prolonger le contrat' : '🤝 Négocier'}</button>`;
    $('#cHire').onclick = () => openNego(subOf(c), nctx(c), { done: k => { signChief(c, k, nctx(c).cost(k).now, isCur); render(); }, close: render });
  } else $('#gDetail').innerHTML = `<p class="hint">Chaque pôle peut avoir un chef. Il réduit le coût des pièces, la durée des recherches ou le risque qu’elles échouent ou prennent du retard. Touche un candidat puis négocie son contrat (salaire, durée, prime) : les meilleurs boudent les écuries sans résultats, et un contrat terminé libère le poste à l’intersaison.</p>`;
  $('#gStats').innerHTML = `<h4>Ton équipe technique</h4>` + Object.entries(DEPTS).map(([d, name]) => { const x = chiefOf(d), k = x && chiefContract(d); return `<p class="istat">${ICONS[d]} ${name} : <b>${x ? x.name : '—'}</b> <small>${x ? `−${x.cost} % · −${x.time} % · −${x.risk} % · jusqu’en ${chiefUntil(k)}` : ''}</small></p>`; }).join('');
}

function drawLines() {
  const tree = root.querySelector('#gTree'), svg = root.querySelector('#gLines'), tr = treeById(tab);
  if (!tree || !svg || !tr || !tr.nodes) return;
  const R = tree.getBoundingClientRect(), T = tall();
  svg.setAttribute('width', tree.scrollWidth); svg.setAttribute('height', tree.scrollHeight);
  const box = id => { const b = tree.querySelector(`[data-node="${id}"]`).getBoundingClientRect(); return { l: b.left - R.left + tree.scrollLeft, r: b.right - R.left + tree.scrollLeft, t: b.top - R.top + tree.scrollTop, b: b.bottom - R.top + tree.scrollTop }; };
  let h = '';
  for (const n of tr.nodes) for (const q of (n.req || n.reqAny || [])) {
    const a = box(q), b = box(n.id);
    let d;
    if (T) { const x1 = (a.l + a.r) / 2, y1 = a.b, x2 = (b.l + b.r) / 2, y2 = b.t, m = (y1 + y2) / 2; d = `M${x1} ${y1}C${x1} ${m} ${x2} ${m} ${x2} ${y2}`; }
    else { const x1 = a.r, y1 = (a.t + a.b) / 2, x2 = b.l, y2 = (b.t + b.b) / 2, m = (x1 + x2) / 2; d = `M${x1} ${y1}C${m} ${y1} ${m} ${y2} ${x2} ${y2}`; }
    const cls = owned(q) && owned(n.id) ? 'full' : owned(q) ? 'on' : '';
    h += `<path d="${d}" class="${cls}" ${n.reqAny ? 'stroke-dasharray="5 4"' : ''}/>`;
  }
  svg.style.color = tr.color; svg.innerHTML = h;
}
// rotation de l'écran : l'arbre change de sens
let lastTall = null;
addEventListener('resize', () => {
  if (!root || root.classList.contains('hidden')) return;
  if (tall() !== lastTall && zone === 'usine') { lastTall = tall(); return render(); }
  drawLines(); frameCar();
});
