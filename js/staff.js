// Chefs de département (un par pôle de l'usine) : réduisent le coût des pièces, leurs délais de
// développement ou le risque qu'une recherche échoue ou prenne du retard. Recrutés au QG par négociation
// (nego.js) : contrat { sal, years, bonus, since, paid }, salaire payé à la signature puis à chaque intersaison.
// Sauvegarde gpm-staff.
import { team, saveTeam } from './team.js';
import { season } from './season.js';

export const DEPTS = { moteur: 'Moteur', aero: 'Aéro', chassis: 'Châssis', fiab: 'Fiabilité', stand: 'Stands' };
// cost / time / risk : réductions en % ; fee : salaire de référence (M€ par saison)
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
export const staff = (() => { let s = { hired: {}, contracts: {}, talks: {} }; try { s = Object.assign(s, JSON.parse(localStorage.getItem(KEY)) || {}); } catch {} return s; })();
export function saveStaff() { try { localStorage.setItem(KEY, JSON.stringify(staff)); } catch {} }
export function resetStaff() { staff.hired = {}; staff.contracts = {}; staff.talks = {}; saveStaff(); }
const r1 = v => Math.round(v * 10) / 10, year = () => season?.year || 2027;

export const chiefOf = dept => CHIEFS.find(c => c.id === staff.hired[dept]);
// multiplicateurs appliqués aux pièces d'un pôle
export const chiefMul = (dept, k) => 1 - (chiefOf(dept)?.[k] || 0) / 100;
// contrat du chef en poste ; anciennes sauvegardes : une saison au salaire de référence
export function chiefContract(dept) {
  const c = chiefOf(dept);
  if (!c) return null;
  staff.contracts = staff.contracts || {};
  if (!staff.contracts[dept]) staff.contracts[dept] = { sal: c.fee, years: 1, bonus: 0, since: year(), paid: year() };
  return staff.contracts[dept];
}
export const chiefUntil = k => k.since + k.years - 1;
// libérer un chef encore sous contrat : la moitié des saisons restantes
export function chiefRelease(dept) {
  const k = chiefContract(dept);
  if (!k) return 0;
  const n = chiefUntil(k) - year();
  return n > 0 ? r1(0.5 * k.sal * n) : 0;
}
// signature (cost : payé tout de suite) ; prolongation du chef en poste : le nouveau contrat démarre la saison suivante
export function signChief(c, k, cost, extend) {
  team.budget = r1(team.budget - cost); saveTeam();
  staff.hired[c.dept] = c.id;
  staff.contracts = staff.contracts || {};
  staff.contracts[c.dept] = { sal: k.sal, years: k.years, bonus: k.bonus, since: year() + (extend ? 1 : 0), paid: year() };
  saveStaff();
}
// intersaison : salaires des contrats qui continuent ; un contrat terminé libère le poste
export function renewStaff(ny) {
  let total = 0;
  const gone = [];
  for (const d of Object.keys(staff.hired)) {
    const c = chiefOf(d), k = chiefContract(d);
    if (!c) continue;
    if (chiefUntil(k) < ny) { gone.push(`${c.name} (${DEPTS[d]})`); delete staff.hired[d]; delete staff.contracts[d]; continue; }
    if (k.paid < ny) { total += k.sal; k.paid = ny; }
  }
  saveStaff();
  return { total: r1(total), gone };
}
