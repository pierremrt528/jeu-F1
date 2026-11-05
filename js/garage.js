// Garage : arbres de développement (moteur, aéro, châssis, fiabilité), points de R&D gagnés en course,
// effets sur la voiture du joueur, sauvegarde locale et écran du garage.
import { NO_UP } from './car.js';
import { team, saveTeam } from './team.js';

// fx : bonus additionnels (0.02 = +2 %). rel : fiabilité (utilisée à l'étape « dégâts »).
// req : toutes les pièces requises ; reqAny : au moins une.
export const TREES = [
  { id: 'moteur', name: 'Moteur', color: '#e5483b', nodes: [
    { id: 'm1', name: 'Admission optimisée', desc: 'Meilleur remplissage des cylindres.', cost: 15, fx: { vtop: 0.008, acc: 0.02 } },
    { id: 'm2', name: 'Turbo grand débit', desc: 'Plus de puissance à haut régime.', cost: 25, req: ['m1'], fx: { vtop: 0.012 } },
    { id: 'm3', name: 'Cartographie couple', desc: 'Des relances plus franches en sortie de virage.', cost: 25, req: ['m1'], fx: { acc: 0.05 } },
    { id: 'm4', name: "Récupération d'énergie", desc: "L'énergie du freinage est rendue à l'accélération.", cost: 40, reqAny: ['m2', 'm3'], fx: { acc: 0.04, vtop: 0.006 } },
    { id: 'm5', name: 'Mode attaque', desc: 'Une cartographie agressive pour les lignes droites.', cost: 40, req: ['m2'], fx: { vtop: 0.01 } },
    { id: 'm6', name: 'Bloc Évolution', desc: 'Nouveau moteur complet, plus puissant et plus léger.', cost: 70, req: ['m4', 'm5'], fx: { vtop: 0.012, acc: 0.04 } },
  ] },
  { id: 'aero', name: 'Aéro', color: '#3aa0ff', nodes: [
    { id: 'a1', name: 'Aileron avant affiné', desc: "Plus d'appui sur le train avant.", cost: 15, fx: { kd: 0.04 } },
    { id: 'a2', name: 'Fond plat à effet de sol', desc: 'La voiture est plaquée au sol dans les courbes rapides.', cost: 25, req: ['a1'], fx: { kd: 0.07 } },
    { id: 'a3', name: 'Carrosserie basse traînée', desc: "Plus de vitesse de pointe, un peu moins d'appui.", cost: 25, req: ['a1'], fx: { vtop: 0.01, kd: -0.02 } },
    { id: 'a4', name: 'Diffuseur double', desc: "Beaucoup d'appui supplémentaire.", cost: 40, req: ['a2'], fx: { kd: 0.08 } },
    { id: 'a5', name: 'Aileron arrière mobile', desc: "L'aspiration derrière une voiture devient bien plus efficace.", cost: 40, req: ['a3'], fx: { slip: 0.5 } },
    { id: 'a6', name: 'Pack aéro intégral', desc: 'Toute la carrosserie revue en soufflerie.', cost: 70, req: ['a4', 'a5'], fx: { kd: 0.06, vtop: 0.006 } },
  ] },
  { id: 'chassis', name: 'Châssis', color: '#f2b632', nodes: [
    { id: 'c1', name: 'Suspensions raidies', desc: 'Plus d\'adhérence mécanique dans les virages lents.', cost: 15, fx: { mu: 0.02 } },
    { id: 'c2', name: 'Freins carbone', desc: 'Des freinages plus courts.', cost: 25, req: ['c1'], fx: { brake: 0.08 } },
    { id: 'c3', name: 'Direction rapide', desc: 'La voiture tourne plus vite et plus précisément.', cost: 25, req: ['c1'], fx: { steer: 0.1 } },
    { id: 'c4', name: 'Allègement du châssis', desc: 'Moins de poids : meilleure accélération et adhérence.', cost: 40, reqAny: ['c2', 'c3'], fx: { acc: 0.03, mu: 0.02 } },
    { id: 'c5', name: 'Étriers refroidis', desc: 'Les freins restent efficaces toute la course.', cost: 40, req: ['c2'], fx: { brake: 0.07 } },
    { id: 'c6', name: 'Monocoque nouvelle génération', desc: 'Un châssis plus rigide et plus léger.', cost: 70, req: ['c4', 'c5'], fx: { mu: 0.03, brake: 0.04 } },
  ] },
  { id: 'fiab', name: 'Fiabilité', color: '#3ddc84', nodes: [
    { id: 'f1', name: 'Radiateurs renforcés', desc: 'Le moteur chauffe moins (moins de casses à venir).', cost: 15, fx: { rel: 10 } },
    { id: 'f2', name: 'Structure anti-choc', desc: 'Les contacts avec les murs coûtent moins de vitesse.', cost: 25, req: ['f1'], fx: { wall: -0.25 } },
    { id: 'f3', name: 'Fond plat renforcé', desc: 'On perd moins de vitesse dans l\'herbe et le gravier.', cost: 25, req: ['f1'], fx: { off: 0.15 } },
    { id: 'f4', name: 'Boîte de vitesses durcie', desc: 'Des passages de rapports plus rapides et plus sûrs.', cost: 40, reqAny: ['f2', 'f3'], fx: { rel: 15, acc: 0.01 } },
    { id: 'f5', name: 'Contrôle qualité', desc: 'Chaque pièce est vérifiée avant la course.', cost: 40, req: ['f2'], fx: { rel: 15, wall: -0.15 } },
    { id: 'f6', name: 'Fiabilité totale', desc: 'La voiture est quasi incassable.', cost: 70, req: ['f4', 'f5'], fx: { rel: 20, off: 0.1 } },
  ] },
];
const NODES = Object.fromEntries(TREES.flatMap(t => t.nodes.map(n => [n.id, n])));

// ---------- état sauvegardé ----------
const KEY = 'gpm-garage';
export const garage = (() => {
  let s = { pts: 0, owned: [] };
  try { s = Object.assign(s, JSON.parse(localStorage.getItem(KEY)) || {}); } catch {}
  return s;
})();
const params = new URLSearchParams(location.search);
if (+params.get('rd') > garage.pts) garage.pts = +params.get('rd');
function save() { try { localStorage.setItem(KEY, JSON.stringify(garage)); } catch {} }

const owned = id => garage.owned.includes(id);
const unlocked = n => (!n.req || n.req.every(owned)) && (!n.reqAny || n.reqAny.some(owned));

export function carUpgrades() {
  const up = { ...NO_UP };
  for (const id of garage.owned) {
    const fx = NODES[id]?.fx || {};
    for (const k in fx) up[k] += fx[k];
  }
  return up;
}

// Points de R&D d'une course : place, places gagnées, meilleur tour, longueur de course
const BASE = [40, 32, 27, 23, 20, 18, 16, 14, 12, 11, 10, 9, 8, 7, 6, 5, 5, 4, 4, 3];
export function awardPoints(pos, grid, laps, fastest, mult = 1) {
  const f = (laps <= 1 ? 0.5 : laps >= 5 ? 1.4 : 1) * mult;
  const parts = [[`P${pos}`, Math.round(BASE[pos - 1] * f)]];
  if (grid > pos) parts.push([`${grid - pos} place${grid - pos > 1 ? 's' : ''} gagnée${grid - pos > 1 ? 's' : ''}`, 2 * (grid - pos)]);
  if (fastest) parts.push(['meilleur tour', 5]);
  const total = parts.reduce((a, p) => a + p[1], 0);
  garage.pts += total; save();
  return { total, parts };
}

// ---------- écran ----------
const STATS = [
  ['Vitesse de pointe', u => u.vtop - 1, 0.065], ['Accélération', u => u.acc - 1, 0.19], ['Appui aéro', u => u.kd - 1, 0.23],
  ['Adhérence', u => u.mu - 1, 0.07], ['Freinage', u => u.brake - 1, 0.19], ['Agilité', u => u.steer - 1, 0.1],
  ['Fiabilité', u => u.rel / 60, 1],
];
const pct = (k, v) => k === 'rel' ? `+${v}` : `${v > 0 ? '+' : ''}${(v * 100).toFixed(v * 100 % 1 ? 1 : 0)} %`;
const FXN = { vtop: 'vitesse de pointe', acc: 'accélération', kd: 'appui', mu: 'adhérence', brake: 'freinage', steer: 'agilité', slip: 'aspiration', off: 'hors piste', wall: 'perte aux chocs', rel: 'fiabilité' };

let tab = 0, sel = null, root = null;
export function openGarage(el, onClose) {
  root = el; root.querySelector('#garageClose').onclick = onClose;
  render();
}

function render() {
  const t = TREES[tab];
  const $ = s => root.querySelector(s);
  $('#gPts').textContent = garage.pts;
  const bb = $('#gBuyPts');
  bb.innerHTML = `Acheter 10 pts <small>1 M€ · budget ${team.budget.toFixed(1)} M€</small>`;
  bb.disabled = team.budget < 1;
  bb.onclick = () => { if (team.budget < 1) return; team.budget = Math.round((team.budget - 1) * 100) / 100; saveTeam(); garage.pts += 10; save(); render(); };
  $('#gTabs').innerHTML = TREES.map((tr, k) => {
    const n = tr.nodes.filter(x => owned(x.id)).length;
    return `<button data-tab="${k}" class="${k === tab ? 'sel' : ''}" style="--c:${tr.color}">${tr.name}<small>${n}/${tr.nodes.length}</small></button>`;
  }).join('');
  root.querySelectorAll('[data-tab]').forEach(b => b.onclick = () => { tab = +b.dataset.tab; sel = null; render(); });
  // arbre : 4 étages (1 / 2 / 2 / 1)
  const N = t.nodes, slot = ['c', 'l', 'r', 'l', 'r', 'c'];
  $('#gTree').innerHTML = `<svg id="gLines"></svg>` + N.map((n, k) => {
    const st = owned(n.id) ? 'own' : unlocked(n) ? (garage.pts >= n.cost ? 'ok' : 'poor') : 'lock';
    return `<button class="node ${st} ${slot[k]} ${sel === n.id ? 'pick' : ''}" data-node="${n.id}" style="--c:${t.color};grid-row:${[1, 2, 2, 3, 3, 4][k]}">
      <b>${n.name}</b><small>${st === 'own' ? '✓ Installé' : st === 'lock' ? '🔒 ' + n.cost + ' pts' : n.cost + ' pts'}</small></button>`;
  }).join('');
  root.querySelectorAll('[data-node]').forEach(b => b.onclick = () => { sel = b.dataset.node; render(); });
  requestAnimationFrame(drawLines);
  // détail de la pièce sélectionnée
  const n = sel && NODES[sel];
  if (n) {
    const reqTxt = n.req ? n.req.map(r => NODES[r].name).join(' + ') : n.reqAny ? n.reqAny.map(r => NODES[r].name).join(' ou ') : '';
    const can = !owned(n.id) && unlocked(n) && garage.pts >= n.cost;
    $('#gDetail').innerHTML = `<b>${n.name}</b><p>${n.desc}</p><p class="fx">${Object.entries(n.fx).map(([k, v]) => `${pct(k, v)} ${FXN[k]}`).join(' · ')}</p>` +
      (reqTxt ? `<p class="req">Requiert : ${reqTxt}</p>` : '') +
      (owned(n.id) ? `<p class="done">✓ Pièce installée sur la voiture</p>` :
        `<button id="gBuy" class="main" ${can ? '' : 'disabled'}>${unlocked(n) ? (garage.pts >= n.cost ? `Développer (${n.cost} pts)` : `Il manque ${n.cost - garage.pts} pts`) : 'Pièce verrouillée'}</button>`);
    const buy = $('#gBuy');
    if (buy) buy.onclick = () => { if (!can) return; garage.pts -= n.cost; garage.owned.push(n.id); save(); render(); };
  } else $('#gDetail').innerHTML = `<p class="hint">Touche une pièce pour voir ses effets. Gagne des points de R&D en course : place à l'arrivée, places gagnées, meilleur tour.</p>`;
  // statistiques de la voiture
  const u = carUpgrades();
  $('#gStats').innerHTML = STATS.map(([name, f, max]) => {
    const v = f(u), w = Math.max(0, Math.min(100, 100 * v / max));
    return `<div><span>${name}</span><i><em style="width:${w}%"></em></i><small>${name === 'Fiabilité' ? 50 + u.rel : (v >= 0 ? '+' : '') + (v * 100).toFixed(1) + ' %'}</small></div>`;
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
