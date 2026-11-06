// Chefs de département (un par pôle de l'usine) : réduisent le coût des pièces, leurs délais de
// développement ou le risque qu'une recherche échoue ou prenne du retard. Mercato des chefs au QG.
// Sauvegarde gpm-staff.
import { team, saveTeam } from './team.js';

export const DEPTS = { moteur: 'Moteur', aero: 'Aéro', chassis: 'Châssis', fiab: 'Fiabilité', stand: 'Stands' };
// cost / time / risk : réductions en % ; fee : M€ à la signature puis à chaque nouvelle saison
const P = (id, dept, name, from, cost, time, risk) => ({ id, dept, name, from, cost, time, risk, fee: Math.round((0.6 + (cost + time + risk) * 0.075) * 10) / 10 });
export const CHIEFS = [
  P('m1', 'moteur', 'R. Valtieri', 'Ex-motoriste', 18, 8, 6), P('m2', 'moteur', 'J. Okafor', 'Ingénieur maison', 5, 15, 12),
  P('m3', 'moteur', 'H. Lindgren', 'Formule B', 8, 8, 8), P('m4', 'moteur', 'D. Morel', 'Vétéran', 10, 22, 30),
  P('a1', 'aero', 'C. Nakamura', 'Soufflerie universitaire', 6, 25, 10), P('a2', 'aero', 'E. Fontaine', 'Ex-aéronautique', 12, 12, 22),
  P('a3', 'aero', 'P. Duarte', 'Jeune talent', 9, 9, 5), P('a4', 'aero', 'S. Brandt', 'Vétéran', 22, 15, 25),
  P('c1', 'chassis', 'L. Marquez', 'Composites', 20, 6, 12), P('c2', 'chassis', 'A. Kovač', 'Ingénieur maison', 8, 18, 8),
  P('c3', 'chassis', 'T. Bello', 'Formule B', 6, 6, 14), P('c4', 'chassis', 'G. Riedel', 'Vétéran', 15, 20, 28),
  P('f1', 'fiab', 'M. Haddad', 'Banc d’essai', 5, 8, 35), P('f2', 'fiab', 'N. Petit', 'Qualité industrielle', 15, 10, 15),
  P('f3', 'fiab', 'Y. Sato', 'Jeune talent', 8, 12, 10), P('f4', 'fiab', 'B. Weller', 'Vétéran', 12, 18, 30),
  P('s1', 'stand', 'O. Lacroix', 'Chef mécanicien', 10, 20, 10), P('s2', 'stand', 'K. Mensah', 'Équipe de rallye', 18, 8, 8),
  P('s3', 'stand', 'V. Ruiz', 'Formule B', 6, 10, 12), P('s4', 'stand', 'F. Aubert', 'Vétéran', 14, 16, 26),
];
const KEY = 'gpm-staff';
export const staff = (() => { let s = { hired: {} }; try { s = Object.assign(s, JSON.parse(localStorage.getItem(KEY)) || {}); } catch {} return s; })();
function save() { try { localStorage.setItem(KEY, JSON.stringify(staff)); } catch {} }
export function resetStaff() { staff.hired = {}; save(); }

export const chiefOf = dept => CHIEFS.find(c => c.id === staff.hired[dept]);
// multiplicateurs appliqués aux pièces d'un pôle
export const chiefMul = (dept, k) => 1 - (chiefOf(dept)?.[k] || 0) / 100;
export function hire(c) {
  if (team.budget < c.fee) return false;
  team.budget = Math.round((team.budget - c.fee) * 10) / 10; saveTeam();
  staff.hired[c.dept] = c.id; save();
  return true;
}
// salaires des chefs à chaque nouvelle saison
export const staffWages = () => Math.round(Object.values(DEPTS).length && Object.keys(staff.hired).reduce((s, d) => s + (chiefOf(d)?.fee || 0), 0) * 10) / 10;
