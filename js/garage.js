// Garage : arbres de développement (moteur, aéro, châssis, fiabilité). Une pièce coûte des M€ et met
// 1 à 3 GP à être livrée ; une seule pièce en développement par pôle. Sauvegarde locale et écran.
import { NO_UP } from './car.js';
import { team, saveTeam } from './team.js';
import { engineOf } from './career.js';
import { partDiscount, partDelay, parallelParts, pitBonus, renderInfra, infra } from './infra.js';
import { CHIEFS, DEPTS, chiefOf, chiefMul, hire } from './staff.js';

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
export const delay = n => Math.max(3, Math.round(partDelay(n.cost <= 20 ? 10 : n.cost <= 55 ? 18 : 28) * chiefMul(TREE_OF[n.id], 'time')));
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
export function perfFactor() {
  const u = carUpgrades();
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
      if (!d.rolled && Math.random() * 100 < r.late) { const x = 4 + Math.floor(Math.random() * 9); d.left += x; d.rolled = true; out.late.push(`${n.name} (+${x} j)`); continue; }
      if (Math.random() * 100 < r.fail) out.fail.push(n.name); else { garage.owned.push(d.id); out.done.push(n.name); }
      delete garage.dev[k]; break;
    }
  }
  save();
  return out;
}

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

let zone = 'usine', tab = 'moteur', sel = null, root = null;
const ICONS = { moteur: '⚡', aero: '🌀', chassis: '🔩', fiab: '🛡️', stand: '🔧', infra: '🏗️' };
const ZONES = {
  usine: { name: 'Usine', tabs: ['moteur', 'aero', 'chassis', 'fiab'] },
  inst: { name: 'Installations', tabs: ['infra', 'stand'] },
  chefs: { name: 'Chefs', tabs: Object.keys(DEPTS) },
};
const treeById = id => TREES.find(t => t.id === id);
// un effet est-il favorable ? (usure, consommation et temps d'arrêt : plus bas = mieux)
const infraBusy = () => !!infra.build;
const good = (k, v) => ['tyre', 'fuel', 'pit', 'wall'].includes(k) ? v < 0 : v > 0;
export function openGarage(el, onClose) {
  root = el; root.querySelector('#garageClose').onclick = onClose;
  render();
}
const chiefLine = c => c ? `<span>💰 −${c.cost} %</span><span>⏱ −${c.time} %</span><span>🧪 −${c.risk} %</span>` : '<span>aucun bonus</span>';

function render() {
  const $ = s => root.querySelector(s);
  $('#gPts').textContent = team.budget.toFixed(1);
  if (!ZONES[zone].tabs.includes(tab)) tab = ZONES[zone].tabs[0];
  $('#gZones').innerHTML = Object.entries(ZONES).map(([k, z]) => `<button data-zone="${k}" class="${k === zone ? 'sel' : ''}">${z.name}</button>`).join('');
  root.querySelectorAll('[data-zone]').forEach(b => b.onclick = () => { zone = b.dataset.zone; tab = ZONES[zone].tabs[0]; sel = null; render(); });
  $('#gTabs').innerHTML = ZONES[zone].tabs.map(id => {
    if (zone === 'chefs') { const c = chiefOf(id), t = treeById(id); return `<button data-tab="${id}" class="${id === tab ? 'sel' : ''}" style="--c:${t.color}"><span class="ti">${ICONS[id]}</span><span class="tn">${t.name}</span><small>${c ? c.name : 'poste vacant'}</small><i class="tp"></i></button>`; }
    if (id === 'infra') return `<button data-tab="infra" class="${tab === 'infra' ? 'sel' : ''}" style="--c:#c06cff"><span class="ti">🏗️</span><span class="tn">Bâtiments</span><small>${infraBusy() ? '⏳ chantier' : '6 bâtiments'}</small><i class="tp"></i></button>`;
    const tr = treeById(id), n = tr.nodes.filter(x => owned(x.id)).length, d = slots(tr).some(k => garage.dev[k]);
    return `<button data-tab="${id}" class="${id === tab ? 'sel' : ''}" style="--c:${tr.color}"><span class="ti">${ICONS[id]}</span><span class="tn">${id === 'stand' ? 'Pit stop' : tr.name}</span><small>${d ? '⏳ ' : ''}${n}/${tr.nodes.length}</small><i class="tp"><em style="width:${100 * n / tr.nodes.length}%"></em></i></button>`;
  }).join('');
  root.querySelectorAll('[data-tab]').forEach(b => b.onclick = () => { tab = b.dataset.tab; sel = null; render(); });
  root.querySelector('.garage').classList.toggle('infra', tab === 'infra' || zone === 'chefs');
  if (tab === 'infra') return renderInfra(root, sel, id => { sel = id; render(); }, render);
  if (zone === 'chefs') return renderChefs($);
  const t = treeById(tab), chief = chiefOf(tab);
  const free = slots(t).find(k => !garage.dev[k]), money = n => team.budget >= price(n);
  const N = t.nodes;
  $('#gTree').innerHTML = `<svg id="gLines"></svg>` + N.map(n => {
    const w = devOf(n.id);
    const st = owned(n.id) ? 'own' : w ? 'wip' : unlocked(n) ? (free && money(n) ? 'ok' : 'poor') : 'lock';
    const tag = `<span class="pr">${price(n)} M€</span><span>${delay(n)} j</span>`;
    return `<button class="node ${st} ${sel === n.id ? 'pick' : ''}" data-node="${n.id}" style="--c:${t.color};grid-row:${n.at[0]};grid-column:${n.at[1]}">
      <span class="nst">${st === 'own' ? '✓' : st === 'wip' ? '⏳' : st === 'lock' ? '🔒' : '+'}</span><b>${n.name}</b><small>${st === 'own' ? 'Installée' : st === 'wip' ? `prête dans ${Math.max(0, w.left)} j` : tag}</small></button>`;
  }).join('');
  root.querySelectorAll('[data-node]').forEach(b => b.onclick = () => { sel = b.dataset.node; render(); });
  requestAnimationFrame(drawLines);
  // détail de la pièce sélectionnée
  const n = sel && NODES[sel];
  if (n) {
    const reqTxt = n.req ? n.req.map(r => NODES[r].name).join(' + ') : n.reqAny ? n.reqAny.map(r => NODES[r].name).join(' ou ') : '';
    const w = devOf(n.id), can = !owned(n.id) && !w && unlocked(n) && free && money(n), rk = risk(n);
    const why = !unlocked(n) ? 'Pièce verrouillée : développe d’abord les pièces requises.' : !free ? `Le pôle ${t.name} développe déjà : ${slots(t).map(k => NODES[garage.dev[k].id].name).join(', ')}` : !money(n) ? `Budget insuffisant : il manque ${(price(n) - team.budget).toFixed(1)} M€.` : '';
    const reqs = (n.req || n.reqAny || []).map(r => `<span class="${owned(r) ? 'ok' : ''}">${owned(r) ? '✓' : '○'} ${NODES[r].name}</span>`).join(n.reqAny ? '<em>ou</em>' : '');
    $('#gDetail').style.setProperty('--c', t.color);
    $('#gDetail').innerHTML = `<div class="dhead"><span class="dk">${ICONS[tab] || ''} Pôle ${t.name}</span><b>${n.name}</b></div><p class="desc">${n.desc}</p>` +
      `<p class="fx">${Object.entries(n.fx).map(([k, v]) => `<span class="${good(k, v) ? '' : 'bad'}">${good(k, v) ? '▲' : '▼'} ${pct(k, v)} ${FXN[k]}</span>`).join('')}</p>` +
      (reqTxt ? `<p class="reqs"><small>Requiert</small>${reqs}</p>` : '') +
      (owned(n.id) ? `<p class="state done">✓ Pièce installée sur la voiture</p>` : w ? `<p class="state wip">⏳ En recherche · prête dans ${Math.max(0, w.left)} jours${w.rolled ? ' (retardée)' : ''}</p>` :
        `<div class="dcost three"><span><small>Coût</small><b>${price(n)} M€</b></span><span><small>Délai</small><b>${delay(n)} j</b></span><span class="rk"><small>Échec</small><b>${rk.fail} %</b></span></div>` +
        `<p class="chief">👤 ${chief ? `Chef ${t.name} : <b>${chief.name}</b>` : 'Pas de chef pour ce pôle'} · risque de retard ${rk.late} %</p>` +
        (why ? `<p class="why">${why}</p>` : '') + `<button id="gBuy" class="main" ${can ? '' : 'disabled'}>Lancer la recherche</button>`);
    const buy = $('#gBuy');
    if (buy) buy.onclick = () => {
      if (!can) return;
      team.budget = Math.round((team.budget - price(n)) * 10) / 10; saveTeam();
      garage.dev[free] = { id: n.id, left: delay(n), days: 1 }; save(); render();
    };
  } else $('#gDetail').innerHTML = `<p class="hint">Touche une pièce pour voir ses effets. La recherche est payée tout de suite et dure plusieurs jours : elle avance entre les Grands Prix selon le calendrier (une semaine entre deux courses enchaînées, plus pendant les pauses). À la fin des essais, une pièce peut prendre du retard ou échouer : un bon chef de département réduit ces risques.</p>`;
  // statistiques de la voiture
  const u = carUpgrades(), u2 = { ...u };
  if (n && !owned(n.id)) for (const k in n.fx) u2[k] += n.fx[k];
  const bw = (f, max) => Math.max(0, Math.min(100, 100 * f / max));
  $('#gStats').innerHTML = `<h4>Ta voiture${n && !owned(n.id) ? ' <small>▲ avec cette pièce</small>' : ''}</h4>` + STATS.map(([name, f, max]) => {
    const v = f(u), w = bw(v, max), g = Math.max(0, bw(f(u2), max) - w);
    const val = name === 'Fiabilité' ? 50 + u.rel : name === 'Arrêt au stand' ? (v ? '−' + v.toFixed(1) + ' s' : '0 s') : (name === 'Usure des pneus' || name === 'Consommation' ? (v ? '−' : '') : v >= 0 ? '+' : '') + (v * 100).toFixed(1) + ' %';
    return `<div class="${g ? 'up' : ''}"><span>${name}</span><i><em style="width:${w}%"></em>${g ? `<u style="left:${w}%;width:${g}%"></u>` : ''}</i><small>${val}</small></div>`;
  }).join('');
}

// ---------- mercato des chefs de département ----------
function renderChefs($) {
  const dept = tab, t = treeById(dept), cur = chiefOf(dept);
  const bar = (l, v) => `<span class="cb"><small>${l}</small><i><em style="width:${v * 2.8}%"></em></i><b>−${v} %</b></span>`;
  $('#gTree').innerHTML = CHIEFS.filter(c => c.dept === dept).map(c => `<button class="node chef ${cur === c ? 'own' : team.budget >= c.fee ? 'ok' : 'poor'} ${sel === c.id ? 'pick' : ''}" data-chief="${c.id}" style="--c:${t.color}">
    <span class="ch"><span class="av">${c.name.replace(/^[A-Z]\.\s*/, '').slice(0, 3).toUpperCase()}</span><span><b>${c.name}</b><small class="cf">${c.from}</small></span></span>
    ${bar('Coûts', c.cost)}${bar('Délais', c.time)}${bar('Échecs', c.risk)}<small class="fee">${cur === c ? '✓ En poste' : c.fee + ' M€ / saison'}</small></button>`).join('');
  root.querySelectorAll('[data-chief]').forEach(b => b.onclick = () => { sel = b.dataset.chief; render(); });
  const c = CHIEFS.find(x => x.id === sel && x.dept === dept);
  $('#gDetail').style.setProperty('--c', t.color);
  if (c) {
    const can = cur !== c && team.budget >= c.fee;
    $('#gDetail').innerHTML = `<div class="dhead"><span class="dk">${ICONS[dept]} Chef ${t.name}</span><b>${c.name}</b></div><p class="desc">${c.from}. Sur toutes les pièces du pôle ${t.name} :</p>` +
      `<p class="fx"><span>▼ ${c.cost} % de coût</span><span>▼ ${c.time} % de délai</span><span>▼ ${c.risk} % de risque d’échec et de retard</span></p>` +
      (cur && cur !== c ? `<p class="chief">Remplace <b>${cur.name}</b> (${chiefLine(cur).replace(/<[^>]+>/g, ' ').trim()})</p>` : '') +
      (cur === c ? `<p class="state done">✓ En poste</p>` : `<div class="dcost"><span><small>Contrat</small><b>${c.fee} M€</b></span><span><small>Puis chaque saison</small><b>${c.fee} M€</b></span></div>` +
        (team.budget < c.fee ? `<p class="why">Budget insuffisant : il manque ${(c.fee - team.budget).toFixed(1)} M€.</p>` : '') + `<button id="cHire" class="main" ${can ? '' : 'disabled'}>Engager</button>`);
    const h = $('#cHire'); if (h) h.onclick = () => { if (can && hire(c)) render(); };
  } else $('#gDetail').innerHTML = `<p class="hint">Chaque pôle peut avoir un chef. Il réduit le coût des pièces, la durée des recherches ou le risque qu’elles échouent ou prennent du retard. Le contrat se paie à la signature puis à chaque nouvelle saison.</p>`;
  $('#gStats').innerHTML = `<h4>Ton équipe technique</h4>` + Object.entries(DEPTS).map(([d, name]) => { const x = chiefOf(d); return `<p class="istat">${ICONS[d]} ${name} : <b>${x ? x.name : '—'}</b> <small>${x ? `−${x.cost} % · −${x.time} % · −${x.risk} %` : ''}</small></p>`; }).join('');
}

function drawLines() {
  const tree = root.querySelector('#gTree'), svg = root.querySelector('#gLines');
  if (!tree || !svg) return;
  const R = tree.getBoundingClientRect();
  svg.setAttribute('width', R.width); svg.setAttribute('height', tree.scrollHeight);
  const pos = id => { const b = tree.querySelector(`[data-node="${id}"]`).getBoundingClientRect(); return [b.left - R.left + b.width / 2 + tree.scrollLeft, b.top - R.top + tree.scrollTop, b.bottom - R.top + tree.scrollTop]; };
  let h = '';
  const tr = treeById(tab); if (!tr) return;
  svg.style.color = tr.color;
  for (const n of tr.nodes) for (const r of (n.req || n.reqAny || [])) {
    const [x1, , y1] = pos(r), [x2, y2] = pos(n.id), m = (y1 + y2) / 2;
    const cls = owned(r) && owned(n.id) ? 'full' : owned(r) ? 'on' : '';
    h += `<path d="M${x1} ${y1} C${x1} ${m} ${x2} ${m} ${x2} ${y2}" class="${cls}" ${n.reqAny ? 'stroke-dasharray="5 4"' : ''}/>`;
  }
  svg.innerHTML = h;
}
addEventListener('resize', () => root && !root.classList.contains('hidden') && drawLines());
