// Infrastructures de l'écurie : bâtiments à construire (3 niveaux) qui accélèrent le développement,
// attirent les sponsors ou améliorent les arrêts. Un seul chantier à la fois. Sauvegarde gpm-infra.
import { team, saveTeam } from './team.js';

export const INFRA = [
  { id: 'rd', name: 'Centre de R&D', icon: '🔬', desc: 'Les pièces du garage coûtent moins cher.', lv: ['−10 % sur les pièces', '−20 % sur les pièces', '−30 % sur les pièces'] },
  { id: 'wind', name: 'Soufflerie', icon: '🌬️', desc: 'Les pièces sont livrées plus vite.', lv: ['Pièces de 3 GP livrées en 2', 'Toutes les pièces 1 GP plus tôt', '2 pièces en même temps par pôle'] },
  { id: 'hosp', name: 'Espace partenaires', icon: '🤝', desc: 'Attire plus de sponsors : revenus à chaque Grand Prix.', lv: ['+1 M€ par GP', '+2 M€ par GP', '+3,5 M€ par GP'] },
  { id: 'media', name: 'Centre médias', icon: '📺', desc: 'Plus de visibilité : primes de course et réputation.', lv: ['Primes +10 %', 'Primes +20 %', 'Primes +30 %'] },
  { id: 'acad', name: 'Académie de pilotes', icon: '🎓', desc: 'Ton équipier progresse plus vite, les salaires baissent.', lv: ['Progression ×1,5 · salaires −5 %', 'Progression ×2 · salaires −10 %', 'Progression ×2,5 · salaires −15 %'] },
  { id: 'pitc', name: 'Centre des mécaniciens', icon: '🔧', desc: 'Des arrêts au stand plus rapides.', lv: ['Arrêts −0,3 s', 'Arrêts −0,6 s', 'Arrêts −1 s'] },
];
const COST = [8, 16, 30], TIME = [1, 2, 3];   // M€ et GP de chantier par niveau
export const buildCost = (b, l) => COST[l] * (b.id === 'hosp' || b.id === 'media' ? 1.2 : 1);

const KEY = 'gpm-infra';
export const infra = (() => {
  let s = { lv: {}, build: null };
  try { s = Object.assign(s, JSON.parse(localStorage.getItem(KEY)) || {}); } catch {}
  return s;
})();
function save() { try { localStorage.setItem(KEY, JSON.stringify(infra)); } catch {} }
export function resetInfra() { infra.lv = {}; infra.build = null; save(); }
export const level = id => infra.lv[id] || 0;

// effets
export const partDiscount = () => 1 - 0.1 * level('rd');
export const partDelay = d => Math.max(1, level('wind') >= 2 ? d - 1 : level('wind') === 1 && d >= 3 ? d - 1 : d);
export const parallelParts = () => level('wind') >= 3 ? 2 : 1;
export const sponsorIncome = () => Math.round((0.6 + team.rep / 100 + [0, 1, 2, 3.5][level('hosp')]) * 10) / 10;
export const prizeBonus = () => 1 + 0.1 * level('media');
export const mateGrowth = () => 1 + 0.5 * level('acad');
export const salaryCut = () => 1 - 0.05 * level('acad');
export const pitBonus = () => -[0, 0.3, 0.6, 1][level('pitc')];

// après chaque GP : le chantier avance ; renvoie le bâtiment terminé (ou null)
export function tickInfra() {
  const b = infra.build;
  if (!b) return null;
  if (--b.left > 0) { save(); return null; }
  infra.lv[b.id] = level(b.id) + 1; infra.build = null;
  if (b.id === 'media' || b.id === 'hosp') { team.rep = Math.min(100, team.rep + 4); saveTeam(); }
  save();
  const d = INFRA.find(x => x.id === b.id);
  return `${d.name} niveau ${infra.lv[b.id]}`;
}

// ---------- rendu dans l'écran du garage (onglet Infrastructures) ----------
export function renderInfra(root, sel, onPick, refresh) {
  const $ = s => root.querySelector(s);
  const b = infra.build;
  $('#gTree').innerHTML = INFRA.map(x => {
    const l = level(x.id), busy = b?.id === x.id;
    const st = busy ? 'wip' : l >= 3 ? 'own' : b ? 'poor' : team.budget >= buildCost(x, l) ? 'ok' : 'poor';
    return `<button class="node infra ${st} ${sel === x.id ? 'pick' : ''}" data-infra="${x.id}" style="--c:#c06cff">
      <b>${x.icon} ${x.name}</b><small>${busy ? `⏳ niveau ${l + 1} dans ${b.left} GP` : l >= 3 ? '✓ Niveau max' : `Niveau ${l}/3 · ${buildCost(x, l)} M€`}</small></button>`;
  }).join('');
  root.querySelectorAll('[data-infra]').forEach(e => e.onclick = () => onPick(e.dataset.infra));
  const x = INFRA.find(i => i.id === sel);
  if (x) {
    const l = level(x.id), cost = l < 3 ? buildCost(x, l) : 0, can = l < 3 && !b && team.budget >= cost;
    const why = l >= 3 ? '' : b ? `Chantier en cours : ${INFRA.find(i => i.id === b.id).name}` : team.budget < cost ? `Budget insuffisant (${cost} M€)` : `Construire le niveau ${l + 1} : ${cost} M€ · ${TIME[l]} GP`;
    $('#gDetail').innerHTML = `<b>${x.icon} ${x.name}</b><p>${x.desc}</p>` +
      x.lv.map((t, k) => `<p class="${k < l ? 'done' : 'req'}">${k < l ? '✓' : '○'} Niveau ${k + 1} : ${t}</p>`).join('') +
      (l >= 3 ? '' : b?.id === x.id ? `<p class="done">⏳ En construction, prêt dans ${b.left} GP</p>` : `<button id="iBuy" class="main" ${can ? '' : 'disabled'}>${why}</button>`);
    const btn = $('#iBuy');
    if (btn) btn.onclick = () => {
      if (!can) return;
      team.budget = Math.round((team.budget - cost) * 10) / 10; saveTeam();
      infra.build = { id: x.id, left: TIME[l] }; save(); refresh();
    };
  } else $('#gDetail').innerHTML = `<p class="hint">Touche un bâtiment. Un seul chantier à la fois ; il est payé tout de suite et terminé après 1 à 3 Grands Prix.</p>`;
  $('#gStats').innerHTML = `<p class="istat">🤝 Sponsors : <b>+${sponsorIncome()} M€</b> par GP</p><p class="istat">📺 Primes de course : <b>×${prizeBonus().toFixed(1)}</b></p>` +
    `<p class="istat">🔬 Prix des pièces : <b>${Math.round(partDiscount() * 100)} %</b></p><p class="istat">🔧 Arrêts : <b>${pitBonus() ? pitBonus() + ' s' : 'normaux'}</b></p>`;
}
