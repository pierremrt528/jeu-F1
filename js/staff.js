// Chefs de département (un par pôle de l'usine) : réduisent le coût des pièces, leurs délais de
// développement ou le risque qu'une recherche échoue ou prenne du retard. Recrutés au QG par négociation
// (nego.js) : contrat { sal, years, bonus, since, paid }, salaire payé à la signature puis à chaque intersaison.
// Candidats libres qui changent après chaque GP (rotateStaff), chefs des écuries rivales débauchables (indemnité au club).
// Sauvegarde gpm-staff : hired, contracts, talks, extra (chefs générés), free { pôle: [id] }, rivals { écurie: { pôle: id } }.
import { team, saveTeam } from './team.js';
import { season } from './season.js';
import { TEAMS } from './data.js';

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
export const staff = (() => { let s = { hired: {}, contracts: {}, talks: {}, extra: [], fresh: [] }; try { s = Object.assign(s, JSON.parse(localStorage.getItem(KEY)) || {}); } catch {} return s; })();
export function saveStaff() { try { localStorage.setItem(KEY, JSON.stringify(staff)); } catch {} }
export function resetStaff() { staff.hired = {}; staff.contracts = {}; staff.talks = {}; staff.extra = []; staff.fresh = []; delete staff.free; delete staff.rivals; saveStaff(); }
const r1 = v => Math.round(v * 10) / 10, year = () => season?.year || 2027;
const rnd = (a, b) => a + Math.floor(Math.random() * (b - a + 1)), pick = a => a[rnd(0, a.length - 1)];

export const chiefById = id => CHIEFS.find(c => c.id === id) || (staff.extra || []).find(c => c.id === id);
export const chiefOf = dept => chiefById(staff.hired[dept]);
export const quality = c => c.cost + c.time + c.risk;

// ---------- marché des chefs : candidats libres et chefs des écuries rivales ----------
const NAMES = ['Abeyta', 'Barreau', 'Castell', 'Delorme', 'Engström', 'Ferreira', 'Gauthier', 'Hasegawa', 'Ilunga', 'Jansen', 'Kerroux', 'Lemaire', 'Moreau', 'Nyström', 'Oyelaran',
  'Pagani', 'Quintal', 'Rossel', 'Solberg', 'Tavares', 'Urquiza', 'Vidal', 'Wolanski', 'Xhaferi', 'Yamada', 'Zeller', 'Arcand', 'Bianchi', 'Carvalho', 'Dumont'];
const ORIGINS = ['Ingénieur maison', 'Formule B', 'Jeune talent', 'Vétéran', 'Endurance', 'Université', 'Industrie automobile', 'Équipe de rallye', 'Ex-aéronautique', 'Banc d’essai'];
// nouveau chef : q = somme des réductions, répartie au hasard entre coûts, délais et risques
function gen(dept, q, from) {
  const w = [Math.random() + 0.3, Math.random() + 0.3, Math.random() + 0.3], sw = w[0] + w[1] + w[2];
  const [cost, time, risk] = w.map(x => Math.max(3, Math.min(35, Math.round(q * x / sw))));
  const used = new Set([...CHIEFS, ...staff.extra].map(c => c.name));
  let name; do name = 'ABCDEFGHJKLMNOPRSTVWY'[rnd(0, 20)] + '. ' + pick(NAMES); while (used.has(name));
  staff.seq = (staff.seq || 0) + 1;
  const c = P('g' + staff.seq, dept, name, from, cost, time, risk);
  staff.extra.push(c);
  return c;
}
const rivalQ = t => Math.round(26 + Math.max(0, Math.min(1, (t.perf - 0.95) / 0.04)) * 26 + rnd(-6, 6));
// première ouverture (ou ancienne sauvegarde) : candidats d'origine libres, un chef par pôle dans chaque écurie rivale
export function ensureStaff() {
  staff.extra = staff.extra || []; staff.fresh = staff.fresh || [];
  let ch = false;
  if (!staff.free) { ch = true; staff.free = {}; for (const d of Object.keys(DEPTS)) staff.free[d] = CHIEFS.filter(c => c.dept === d && staff.hired[d] !== c.id).map(c => c.id); }
  staff.rivals = staff.rivals || {};
  for (const t of TEAMS.slice(1)) if (!staff.rivals[t.name]) {
    ch = true; staff.rivals[t.name] = {};
    for (const d of Object.keys(DEPTS)) staff.rivals[t.name][d] = gen(d, rivalQ(t), pick(ORIGINS.slice(0, 6))).id;
  }
  if (ch) saveStaff();
}
export const freeChiefs = dept => { ensureStaff(); return (staff.free[dept] || []).map(chiefById).filter(Boolean); };
export const rivalChiefs = dept => { ensureStaff(); return TEAMS.slice(1).map((t, ti) => ({ c: chiefById(staff.rivals[t.name]?.[dept]), t, ti: ti + 1 })).filter(x => x.c); };
// rythme de développement d'une écurie rivale selon ses chefs (≈ 1 en moyenne, 0,7 sans aucun chef)
export function rivalDevMul(name) {
  ensureStaff();
  const R = staff.rivals[name];
  if (!R) return 1;
  return 0.7 + Object.values(R).reduce((s, id) => s + (chiefById(id) ? quality(chiefById(id)) / 45 : 0), 0) * 0.06;
}
// on oublie les chefs générés qui ne sont plus nulle part
function prune() {
  const keep = new Set([...Object.values(staff.hired), ...Object.values(staff.free).flat(), ...Object.values(staff.rivals).flatMap(o => Object.values(o))]);
  staff.extra = staff.extra.filter(c => keep.has(c.id));
}
// après chaque GP (et à chaque étape de la pré-saison) : des candidats libres partent, d'autres arrivent,
// et parfois une écurie rivale en recrute un. Renvoie le nombre de nouveaux candidats.
export function rotateStaff() {
  ensureStaff();
  staff.fresh = [];
  for (const d of Object.keys(DEPTS)) {
    const F = staff.free[d] = (staff.free[d] || []).filter(id => id !== staff.hired[d]);
    if (F.length && Math.random() < 0.65) {
      const id = F.splice(rnd(0, F.length - 1), 1)[0];
      if (Math.random() < 0.4) {   // embauché par une écurie rivale, dont l'ancien chef devient libre
        const t = pick(TEAMS.slice(1)), R = staff.rivals[t.name];
        if (R[d]) F.push(R[d]);
        R[d] = id;
      }
    }
    const want = Math.random() < 0.3 ? 5 : 4;
    while (F.length < want) {
      const from = pick(ORIGINS), c = gen(d, from === 'Vétéran' ? rnd(38, 55) : from === 'Jeune talent' ? rnd(18, 30) : rnd(22, 46), from);
      F.push(c.id); staff.fresh.push(c.id);
    }
  }
  prune(); saveStaff();
  return staff.fresh.length;
}
const toFree = id => { if (id) { const c = chiefById(id); if (c) (staff.free[c.dept] = staff.free[c.dept] || []).push(id); } };
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
// club : écurie rivale qui le libère (elle récupère ton ancien chef, sinon un intérimaire)
export function signChief(c, k, cost, extend, club) {
  ensureStaff();
  team.budget = r1(team.budget - cost); saveTeam();
  const old = staff.hired[c.dept];
  if (!extend) {
    staff.free[c.dept] = (staff.free[c.dept] || []).filter(id => id !== c.id);
    if (club && staff.rivals[club]) staff.rivals[club][c.dept] = old || gen(c.dept, rnd(14, 24), 'Intérim').id;
    else toFree(old);
  }
  staff.hired[c.dept] = c.id;
  staff.contracts = staff.contracts || {};
  staff.contracts[c.dept] = { sal: k.sal, years: k.years, bonus: k.bonus, since: year() + (extend ? 1 : 0), paid: year() };
  saveStaff();
}
// un chef débauché par une écurie rivale : le poste est libéré
export function loseChief(dept) {
  ensureStaff();
  const id = staff.hired[dept], t = pick(TEAMS.slice(1)), R = staff.rivals[t.name];
  if (id) { toFree(R[dept]); R[dept] = id; }
  delete staff.hired[dept]; if (staff.contracts) delete staff.contracts[dept]; saveStaff();
}
// intersaison : salaires des contrats qui continuent ; un contrat terminé libère le poste
export function renewStaff(ny) {
  let total = 0;
  const gone = [];
  for (const d of Object.keys(staff.hired)) {
    const c = chiefOf(d), k = chiefContract(d);
    if (!c) continue;
    if (chiefUntil(k) < ny) { gone.push(`${c.name} (${DEPTS[d]})`); ensureStaff(); toFree(c.id); delete staff.hired[d]; delete staff.contracts[d]; continue; }
    if (k.paid < ny) { total += k.sal; k.paid = ny; }
  }
  saveStaff();
  return { total: r1(total), gone };
}
