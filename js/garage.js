// Garage : arbres de développement (moteur, aéro, châssis, fiabilité). Une pièce coûte des M€ et met
// 1 à 3 GP à être livrée ; une seule pièce en développement par pôle. Sauvegarde locale et écran.
import { NO_UP } from './car.js';
import { team, saveTeam } from './team.js';
import { engineOf } from './career.js';
import { partDiscount, partDelay, parallelParts, pitBonus, renderInfra } from './infra.js';

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
export const price = n => Math.round(n.cost / 5 * partDiscount() * 10) / 10;            // M€ (Centre de R&D)
export const delay = n => partDelay(n.cost <= 20 ? 1 : n.cost <= 55 ? 2 : 3);       // GP avant livraison (Soufflerie)

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
// Après chaque GP : les pièces en cours avancent d'une course ; renvoie les pièces livrées
export function tickDevelopment() {
  const done = [];
  for (const k in garage.dev) {
    const d = garage.dev[k];
    if (--d.left <= 0) { garage.owned.push(d.id); done.push(NODES[d.id].name); delete garage.dev[k]; }
  }
  save();
  return done;
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

let tab = 0, sel = null, root = null;
const ICONS = ['⚡', '🌀', '🔩', '🛡️', '🔧'];
export function openGarage(el, onClose) {
  root = el; root.querySelector('#garageClose').onclick = onClose;
  render();
}

function render() {
  const $ = s => root.querySelector(s);
  $('#gPts').textContent = team.budget.toFixed(1);
  $('#gTabs').innerHTML = TREES.map((tr, k) => {
    const n = tr.nodes.filter(x => owned(x.id)).length, d = slots(tr).some(k => garage.dev[k]);
    return `<button data-tab="${k}" class="${k === tab ? 'sel' : ''}" style="--c:${tr.color}"><span class="ti">${ICONS[k] || '•'}</span>${tr.name}<small>${n}/${tr.nodes.length}${d ? ' ⏳' : ''}</small><i class="tp"><em style="width:${100 * n / tr.nodes.length}%"></em></i></button>`;
  }).join('') + `<button data-tab="${TREES.length}" class="${tab === TREES.length ? 'sel' : ''}" style="--c:#c06cff"><span class="ti">🏗️</span>Infra<small>bâtiments</small><i class="tp"></i></button>`;
  root.querySelectorAll('[data-tab]').forEach(b => b.onclick = () => { tab = +b.dataset.tab; sel = null; render(); });
  root.querySelector('.garage').classList.toggle('infra', tab === TREES.length);
  if (tab === TREES.length) return renderInfra(root, sel, id => { sel = id; render(); }, render);
  const t = TREES[tab];
  const free = slots(t).find(k => !garage.dev[k]), money = n => team.budget >= price(n);
  // arbre : 6 étages (1 / 2 / 2 / 1 / 2 / 1)
  const N = t.nodes;
  $('#gTree').innerHTML = `<svg id="gLines"></svg>` + N.map((n, k) => {
    const w = devOf(n.id);
    const st = owned(n.id) ? 'own' : w ? 'wip' : unlocked(n) ? (free && money(n) ? 'ok' : 'poor') : 'lock';
    const tag = `${price(n)} M€ · ${delay(n)} GP`;
    return `<button class="node ${st} ${sel === n.id ? 'pick' : ''}" data-node="${n.id}" style="--c:${t.color};grid-row:${n.at[0]};grid-column:${n.at[1]}">
      <span class="nst">${st === 'own' ? '✓' : st === 'wip' ? '⏳' : st === 'lock' ? '🔒' : k + 1}</span><b>${n.name}</b><small>${st === 'own' ? 'Installé' : st === 'wip' ? `livrée dans ${w.left} GP` : tag}</small></button>`;
  }).join('');
  root.querySelectorAll('[data-node]').forEach(b => b.onclick = () => { sel = b.dataset.node; render(); });
  requestAnimationFrame(drawLines);
  // détail de la pièce sélectionnée
  const n = sel && NODES[sel];
  if (n) {
    const reqTxt = n.req ? n.req.map(r => NODES[r].name).join(' + ') : n.reqAny ? n.reqAny.map(r => NODES[r].name).join(' ou ') : '';
    const w = devOf(n.id), can = !owned(n.id) && !w && unlocked(n) && free && money(n);
    const why = !unlocked(n) ? 'Pièce verrouillée' : !free ? `Le pôle ${t.name} développe déjà : ${slots(t).map(k => NODES[garage.dev[k].id].name).join(', ')}` : !money(n) ? `Budget insuffisant (${price(n)} M€)` : `Développer : ${price(n)} M€ · livrée dans ${delay(n)} GP`;
    $('#gDetail').style.setProperty('--c', t.color);
    $('#gDetail').innerHTML = `<span class="dk">${ICONS[tab] || ''} Pôle ${t.name}</span><b>${n.name}</b><p>${n.desc}</p><p class="fx">${Object.entries(n.fx).map(([k, v]) => `<span>${pct(k, v)} ${FXN[k]}</span>`).join('')}</p>` +
      (reqTxt ? `<p class="req">Requiert : ${reqTxt}</p>` : '') +
      (owned(n.id) ? `<p class="done">✓ Pièce installée sur la voiture</p>` : w ? `<p class="done">⏳ En développement, livrée dans ${w.left} GP</p>` :
        `<button id="gBuy" class="main" ${can ? '' : 'disabled'}>${why}</button>`);
    const buy = $('#gBuy');
    if (buy) buy.onclick = () => {
      if (!can) return;
      team.budget = Math.round((team.budget - price(n)) * 10) / 10; saveTeam();
      garage.dev[free] = { id: n.id, left: delay(n) }; save(); render();
    };
  } else $('#gDetail').innerHTML = `<p class="hint">Touche une pièce pour voir ses effets. Chaque pôle développe une pièce à la fois (deux avec la Soufflerie niveau 3) ; elle est payée tout de suite et livrée après 1 à 3 Grands Prix. L’onglet Infra permet de construire des bâtiments.</p>`;
  // statistiques de la voiture
  const u = carUpgrades();
  $('#gStats').innerHTML = STATS.map(([name, f, max]) => {
    const v = f(u), w = Math.max(0, Math.min(100, 100 * v / max));
    const val = name === 'Fiabilité' ? 50 + u.rel : name === 'Arrêt au stand' ? (v ? '−' + v.toFixed(1) + ' s' : '0 s') : (name === 'Usure des pneus' || name === 'Consommation' ? (v ? '−' : '') : v >= 0 ? '+' : '') + (v * 100).toFixed(1) + ' %';
    return `<div><span>${name}</span><i><em style="width:${w}%"></em></i><small>${val}</small></div>`;
  }).join('');
}

function drawLines() {
  const tree = root.querySelector('#gTree'), svg = root.querySelector('#gLines');
  if (!tree || !svg) return;
  const R = tree.getBoundingClientRect();
  svg.setAttribute('width', R.width); svg.setAttribute('height', tree.scrollHeight);
  const pos = id => { const b = tree.querySelector(`[data-node="${id}"]`).getBoundingClientRect(); return [b.left - R.left + b.width / 2, b.top - R.top + tree.scrollTop, b.bottom - R.top + tree.scrollTop]; };
  let h = '';
  for (const n of TREES[tab].nodes) for (const r of (n.req || n.reqAny || [])) {
    const a = pos(r), b = pos(n.id);
    h += `<line x1="${a[0]}" y1="${a[2]}" x2="${b[0]}" y2="${b[1]}" class="${owned(r) ? 'on' : ''}" ${n.reqAny ? 'stroke-dasharray="5 4"' : ''}/>`;
  }
  svg.innerHTML = h;
}
addEventListener('resize', () => root && !root.classList.contains('hidden') && drawLines());
