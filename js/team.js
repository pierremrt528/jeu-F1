// État de l'écurie du joueur : moral, budget (M€), réputation, bonus pour la prochaine course,
// confiance de la direction (conf), contrats de sponsors ponctuels (deals : M€ par GP pendant n GP).
// Sauvegarde locale (clé gpm-team).
const KEY = 'gpm-team';
export const team = (() => {
  let s = { moral: 60, budget: 5, rep: 50, boost: {}, boostTxt: '', conf: 60, deals: [] };
  try { s = Object.assign(s, JSON.parse(localStorage.getItem(KEY)) || {}); } catch {}
  return s;
})();
export function resetTeam() { Object.assign(team, { moral: 60, budget: 5, rep: 50, boost: {}, boostTxt: '', conf: 60, deals: [] }); saveTeam(); }
export function saveTeam() { try { localStorage.setItem(KEY, JSON.stringify(team)); } catch {} }

const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
export const FXNAMES = { vtop: 'vitesse de pointe', acc: 'accélération', mu: 'adhérence', brake: 'freinage', kd: 'appui', steer: 'agilité' };

// applique les effets d'une réponse ; perf = bonus/malus pour la prochaine course seulement
export function applyFx(fx) {
  if (fx.moral) team.moral = clamp(team.moral + fx.moral, 0, 100);
  if (fx.budget) team.budget = Math.max(0, Math.round((team.budget + fx.budget) * 10) / 10);
  if (fx.rep) team.rep = clamp(team.rep + fx.rep, 0, 100);
  if (fx.conf) team.conf = clamp((team.conf ?? 60) + fx.conf, 0, 100);
  if (fx.perf) for (const k in fx.perf) team.boost[k] = (team.boost[k] || 0) + fx.perf[k];
  team.boostTxt = describeBoost();
  saveTeam();
}
export function describeBoost() {
  return Object.entries(team.boost).filter(([, v]) => Math.abs(v) > 1e-4)
    .map(([k, v]) => `${v > 0 ? '+' : ''}${(v * 100).toFixed(1).replace('.0', '')} % ${FXNAMES[k] || k}`).join(', ');
}

// Modificateurs de course : moral (±1 %) + bonus de presse ; le bonus est consommé au départ
export function raceModifiers() {
  const m = (team.moral - 50) / 50;   // -1 .. +1
  const mod = { vtop: 0.005 * m, acc: 0.01 * m, mu: 0.004 * m };
  for (const k in team.boost) mod[k] = (mod[k] || 0) + team.boost[k];
  return mod;
}
export function consumeBoost() { team.boost = {}; team.boostTxt = ''; saveTeam(); }

// Gains financiers d'une course (M€), multipliés selon la réputation
const PRIZE = [5, 4, 3.4, 2.8, 2.4, 2.1, 1.8, 1.6, 1.4, 1.2];
export function racePrize(pos, fmt = 1, mult = 1) {
  const f = fmt * mult;
  const p = Math.round(((PRIZE[pos - 1] || 0.8) * f) * 10) / 10;
  team.budget = Math.round((team.budget + p) * 100) / 100;
  // le moral suit les résultats
  team.moral = clamp(team.moral + (pos <= 3 ? 6 : pos <= 10 ? 2 : -3), 0, 100);
  saveTeam();
  return p;
}
export const repFactor = () => 0.8 + team.rep / 250;   // 50 → 1.0, 100 → 1.2
