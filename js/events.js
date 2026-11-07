// Vie de l'écurie entre les Grands Prix : événements à choix annoncés en « flash info », objectifs de
// la direction (confiance, licenciement possible), missions proposées par les sponsors avant chaque course.
import { TEAMS } from './data.js';
import { team, applyFx, saveTeam } from './team.js';
import { career, saveCareer, applyCareer, mateContract } from './career.js';
import { chiefOf, chiefContract, DEPTS, loseChief } from './staff.js';
import { shiftDev, devCount } from './garage.js';
import { season, saveSeason, teamStandings } from './season.js';
import { fxText } from './press.js';
import { ROLES } from './nego.js';

const r1 = v => Math.round(v * 10) / 10;
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const pick = a => a[(Math.random() * a.length) | 0];
const fmtM = v => r1(v).toFixed(1).replace('.', ',');
export const EXPECT = { new: 14, mid: 9, top: 4 };   // place attendue par la direction selon le niveau de l'écurie

// ---------- événements ----------
// when(ctx) : conditions ; choix : t (texte), fx (effets), out (conséquence), risk { p, fx, out } (mauvaise surprise)
// effets en plus de moral/budget/rep/perf/conf : mate (stats de l'équipier), wage (salaire de l'équipier), role (statut),
// dev (jours de retard des recherches, négatif = avance), deal (sponsor : gp M€ par GP pendant n GP), chief (départ), grid (pénalité)
const EVENTS = [
  { id: 'fire', icon: '🔥', when: c => c.dev, title: 'Incendie à l’usine', text: () => 'Un court-circuit a déclenché un incendie dans l’atelier composite. Personne n’est blessé, mais les recherches en cours sont menacées.', ch: [
    { t: 'Tout reconstruire tout de suite (6 M€)', fx: { budget: -6, moral: 3 }, out: 'L’atelier tourne de nouveau 48 heures plus tard.' },
    { t: 'Réparer au minimum (2 M€)', fx: { budget: -2, dev: 10, moral: -4 }, out: 'Les recherches prennent dix jours de retard.' },
    { t: 'Attendre l’assurance', fx: { budget: 1, dev: 18, conf: -4 }, out: 'L’assurance paie, mais l’usine reste à l’arrêt près de trois semaines.' },
  ] },
  { id: 'neon', icon: '⚡', title: 'Un sponsor tapageur', text: () => 'La marque de boissons énergisantes Fulgor veut couvrir ta voiture de néon jaune pour le reste de la saison.', ch: [
    { t: 'Accepter : 1,5 M€ par Grand Prix pendant 6 courses', fx: { deal: { name: 'Fulgor', gp: 1.5, n: 6 }, rep: -4 }, out: 'La voiture se voit de loin… les puristes grincent des dents.' },
    { t: 'Négocier une livrée discrète (0,8 M€ par GP)', fx: { deal: { name: 'Fulgor', gp: 0.8, n: 6 }, rep: 1 }, out: 'Un petit logo élégant sur l’aileron arrière.' },
    { t: 'Refuser : notre image avant tout', fx: { rep: 5, conf: -3 }, out: 'Les fans applaudissent, le directeur financier beaucoup moins.' },
  ] },
  { id: 'raise', icon: '💰', when: c => c.mate && c.pos <= 12, title: 'Ton équipier veut plus', text: c => `${c.mate} estime mériter une augmentation après ses dernières courses et laisse entendre qu’il écoute les autres écuries.`, ch: [
    { t: 'Augmenter son salaire de 1,5 M€ par saison', fx: { wage: 1.5, moral: 6 }, out: 'Il se dit « heureux et respecté ».' },
    { t: 'Lui offrir le statut de N°1', fx: { role: 'n1', moral: 3, conf: -2 }, out: 'Il aura la priorité sur toi en course.' },
    { t: 'Refuser : un contrat est un contrat', fx: { moral: -8, mate: { spd: -1 } }, out: 'Il boude et perd un peu de motivation.' },
  ] },
  { id: 'spy', icon: '🕵️', title: 'Des plans à vendre', text: () => 'Un ingénieur d’une écurie rivale propose de te vendre les plans de leur nouvel aileron.', ch: [
    { t: 'Acheter les plans (3 M€)', fx: { budget: -3, perf: { kd: 0.05 } }, out: 'L’aileron copié sera prêt pour le prochain GP.',
      risk: { p: 0.35, fx: { budget: -3, rep: -12, conf: -12 }, out: 'Scandale ! La fédération l’apprend : amende de 3 M€ et réputation au plus bas.' } },
    { t: 'Refuser et prévenir la fédération', fx: { rep: 8, conf: 3 }, out: 'Ton intégrité est saluée dans tout le paddock.' },
    { t: 'Refuser discrètement', fx: { moral: 1 }, out: 'L’affaire en reste là.' },
  ] },
  { id: 'fans', icon: '🎉', title: 'Journée des fans', text: () => 'Le club des supporters veut organiser une grande journée portes ouvertes à l’usine.', ch: [
    { t: 'Ouvrir grand les portes', fx: { rep: 5, moral: 4, dev: 3 }, out: 'Des milliers de fans, des sourires partout, un peu de retard à l’usine.' },
    { t: 'Une visite rapide, l’usine tourne', fx: { rep: 2 }, out: 'Une visite express appréciée.' },
    { t: 'Annuler : on doit travailler', fx: { rep: -4, dev: -2 }, out: 'Les fans sont déçus, les ingénieurs gagnent deux jours.' },
  ] },
  { id: 'poach', icon: '🎯', when: c => c.chief, title: 'Ton chef est convoité', text: c => `Une écurie rivale veut débaucher ${c.chief.name}, ton chef ${c.chiefDept}.`, ch: [
    { t: c => `S’aligner : prime de ${fmtM(c.chiefFee)} M€`, fx: c => ({ budget: -c.chiefFee, moral: 2 }), out: c => `${c.chief.name} reste et remercie l’écurie.` },
    { t: 'Le laisser partir contre une indemnité', fx: c => ({ chief: c.chiefKey, budget: r1(c.chiefFee * 0.6) }), out: c => `${c.chief.name} s’en va. Le poste de chef ${c.chiefDept} est vacant.` },
    { t: 'Lui promettre plus de moyens', fx: { moral: 1 }, out: 'Il accepte de rester… pour l’instant.', risk: { p: 0.5, fx: c => ({ chief: c.chiefKey }), out: c => `Les promesses ne suffisent pas : ${c.chief.name} signe chez le rival.` } },
  ] },
  { id: 'strike', icon: '🪧', title: 'Grogne des mécaniciens', text: () => 'Épuisés par les week-ends enchaînés, les mécaniciens réclament une prime.', ch: [
    { t: 'Verser une prime (2 M€)', fx: { budget: -2, moral: 6 }, out: 'L’ambiance au garage est au beau fixe.' },
    { t: 'Accorder des jours de repos', fx: { dev: 5, moral: 3 }, out: 'Repos mérité, l’usine ralentit un peu.' },
    { t: 'Refuser', fx: { moral: -10, perf: { acc: -0.012 } }, out: 'Les mécaniciens traînent des pieds : le prochain GP sera compliqué.' },
  ] },
  { id: 'docu', icon: '🎬', title: 'Un documentaire', text: () => 'Une plateforme de streaming veut filmer l’écurie pendant toute la saison, coulisses comprises.', ch: [
    { t: 'Accepter : tout filmer', fx: { budget: 4, rep: 6, moral: -6 }, out: 'Les caméras sont partout, même à la cantine.' },
    { t: 'Accepter avec des zones interdites', fx: { budget: 2, rep: 3 }, out: 'Un compromis qui convient à tout le monde.' },
    { t: 'Refuser', fx: { moral: 3 }, out: 'L’équipe garde ses secrets.' },
  ] },
  { id: 'engine', icon: '⚙️', title: 'Moteur fatigué', text: () => 'Le motoriste prévient : ton moteur arrive en fin de vie. En monter un neuf coûte une pénalité sur la grille.', ch: [
    { t: 'Moteur neuf : 5 places de pénalité', fx: { grid: 5, perf: { vtop: 0.006 } }, out: 'Départ plus loin, mais toute la puissance.' },
    { t: 'Garder le moteur usé', fx: { perf: { vtop: -0.01, acc: -0.015 } }, out: 'Il faudra ménager la mécanique au prochain GP.' },
    { t: 'Payer un moteur de rechange hors quota (4 M€)', fx: { budget: -4, conf: -2 }, out: 'Problème réglé, à prix d’or.' },
  ] },
  { id: 'prodigy', icon: '🎓', title: 'Un prodige frappe à la porte', text: () => 'Un jeune champion de karting cherche un volant d’essai et une place à l’académie.', ch: [
    { t: 'L’accueillir comme réserviste (1 M€)', fx: { budget: -1, rep: 3, mate: { spd: 1 } }, out: 'Son énergie pousse ton équipier à se dépasser.' },
    { t: 'Le recommander à une autre écurie', fx: { rep: 2 }, out: 'Le paddock apprécie le geste.' },
    { t: 'Ignorer sa demande', fx: {}, out: 'Il ira voir ailleurs.' },
  ] },
  { id: 'audit', icon: '📑', title: 'Audit financier', text: () => 'La fédération contrôle les dépenses de toutes les écuries. Ton tour est arrivé.', ch: [
    { t: 'Coopérer totalement', fx: { conf: 3, dev: 4 }, out: 'Tout est en règle, mais les ingénieurs ont passé des jours sur la paperasse.' },
    { t: 'Engager des avocats (2 M€)', fx: { budget: -2 }, out: 'Les avocats gèrent, l’usine continue.' },
    { t: 'Cacher quelques factures', fx: { budget: 2 }, out: 'Personne n’a rien vu.', risk: { p: 0.4, fx: { budget: -6, rep: -10, conf: -10 }, out: 'Fraude découverte : 6 M€ d’amende et une réputation entachée.' } },
  ] },
  { id: 'buyout', icon: '🏦', when: c => c.conf < 50, title: 'Un fonds veut entrer au capital', text: () => 'Un fonds d’investissement propose d’injecter de l’argent contre une part de l’écurie. La direction n’apprécie pas.', ch: [
    { t: 'Accepter : +15 M€', fx: { budget: 15, conf: -15 }, out: 'Les caisses sont pleines, la direction fulmine.' },
    { t: 'Refuser poliment', fx: { conf: 5 }, out: 'La direction apprécie ta fidélité.' },
    { t: 'Faire monter les enchères', fx: { budget: 22, conf: -12 }, out: 'Le fonds cède : 22 M€ !', risk: { p: 0.5, fx: { rep: -4, conf: -4 }, out: 'Le fonds se retire, vexé, et la presse s’en mêle.' } },
  ] },
  { id: 'crisis', icon: '📉', when: c => c.dnf || c.pos > 12, title: 'L’écurie à la dérive ?', text: () => 'Après ce résultat, les médias parlent d’une écurie en crise.', ch: [
    { t: 'Conférence de presse offensive', fx: { moral: 5, rep: -3 }, out: 'Le ton monte, l’équipe se resserre.' },
    { t: 'Réunion de crise avec la direction', fx: { moral: 2, conf: 5 }, out: 'La direction apprécie ta lucidité.' },
    { t: 'Remanier l’équipe technique (2 M€)', fx: { budget: -2, dev: -5, moral: -5 }, out: 'Sang neuf à l’usine : les recherches accélèrent.' },
  ] },
  { id: 'party', icon: '🥂', when: c => !c.dnf && c.pos <= 3, title: 'Soirée des partenaires', text: () => 'Les partenaires veulent fêter ce podium avec une soirée géante.', ch: [
    { t: 'Grande fête !', fx: { moral: 8, budget: -1.5, rep: 2, perf: { acc: -0.006 } }, out: 'Mémorable… les lendemains piquent un peu.' },
    { t: 'Petite célébration avec l’équipe', fx: { moral: 4 }, out: 'Un moment simple et chaleureux.' },
    { t: 'On reste concentrés', fx: { moral: -2, perf: { mu: 0.008 } }, out: 'Retour au travail dès ce soir.' },
  ] },
  { id: 'sim', icon: '🕹️', title: 'Simulateur dernier cri', text: () => 'Un fabricant propose un simulateur de pilotage révolutionnaire.', ch: [
    { t: 'L’acheter (5 M€)', fx: { budget: -5, mate: { spd: 2, strat: 1 } }, out: 'Ton équipier y passe ses nuits : il progresse.' },
    { t: 'Le louer pour un GP (1 M€)', fx: { budget: -1, perf: { mu: 0.008, brake: 0.01 } }, out: 'Réglages affinés pour la prochaine course.' },
    { t: 'Non merci', fx: {}, out: 'On garde l’ancien.' },
  ] },
  { id: 'injury', icon: '🤕', when: c => c.mate, title: 'Ton équipier blessé', text: c => `${c.mate} s’est fait mal au poignet en faisant du karting avec des amis.`, ch: [
    { t: 'Il court quand même', fx: { mate: { spd: -2 }, moral: 2 }, out: 'Il serre les dents, mais il a perdu un peu de vitesse.' },
    { t: 'Le meilleur spécialiste (2 M€)', fx: { budget: -2, moral: 3 }, out: 'Guéri en un temps record.' },
    { t: 'Le recadrer publiquement', fx: { moral: -6, rep: 2, mate: { spd: -1 } }, out: 'Le message est passé… l’ambiance en prend un coup.' },
  ] },
  { id: 'tyretest', icon: '🛞', title: 'Pneus prototypes', text: () => 'Le manufacturier de pneus cherche une écurie pour tester ses prototypes entre deux courses.', ch: [
    { t: 'Accepter (+3 M€)', fx: { budget: 3, dev: 4 }, out: 'De l’argent frais, mais les ingénieurs sont mobilisés.' },
    { t: 'Accepter contre des données (+0 M€)', fx: { perf: { mu: 0.01 } }, out: 'Tu connaîtras mieux les pneus au prochain GP.' },
    { t: 'Refuser', fx: {}, out: 'Pas le moment.' },
  ] },
  { id: 'taunt', icon: '🗯️', when: c => c.rival, title: 'Provocation', text: c => `${c.rival} affirme dans la presse que ta voiture est « à la limite du règlement ».`, ch: [
    { t: 'Porter réclamation (1 M€)', fx: { budget: -1, rep: 3 }, out: 'La fédération confirme : ta voiture est conforme.' },
    { t: 'Répondre sur la piste', fx: { moral: 4, perf: { vtop: 0.006 } }, out: 'Toute l’équipe est remontée à bloc.' },
    { t: 'Ignorer', fx: { moral: -2, rep: 2 }, out: 'La polémique s’éteint d’elle-même.' },
  ] },
  { id: 'software', icon: '💻', title: 'Partenaire technologique', text: () => 'Une entreprise de logiciels propose ses outils de simulation aérodynamique.', ch: [
    { t: 'Signer (3 M€)', fx: { budget: -3, dev: -6, conf: 2 }, out: 'Les recherches avancent bien plus vite.' },
    { t: 'Essai gratuit d’un mois', fx: { dev: -2 }, out: 'Un petit coup de pouce.' },
    { t: 'Refuser', fx: {}, out: 'Les ingénieurs gardent leurs habitudes.' },
  ] },
  { id: 'leak', icon: '📰', title: 'Fuite dans la presse', text: () => 'Un document interne sur tes futures pièces a fuité dans un magazine.', ch: [
    { t: 'Trouver le responsable', fx: { moral: -4, rep: 2 }, out: 'Un stagiaire trop bavard est remercié.' },
    { t: 'Accélérer pour garder l’avance (2 M€)', fx: { budget: -2, dev: -5 }, out: 'L’usine passe en 3×8.' },
    { t: 'Démentir', fx: { rep: -2 }, out: 'Personne n’y croit vraiment.' },
  ] },
  { id: 'mainsponsor', icon: '📉', when: c => c.conf < 45 || c.pos > 10, title: 'Le sponsor principal s’impatiente', text: () => 'Ton sponsor principal menace de réduire son soutien si les résultats ne s’améliorent pas.', ch: [
    { t: 'Promettre de meilleurs résultats', fx: { conf: -3, moral: 2 }, out: 'Il te laisse une chance… la pression monte.' },
    { t: 'Lui offrir plus de visibilité', fx: { rep: -3, budget: 1 }, out: 'Son logo grandit sur la voiture.' },
    { t: 'Accepter la baisse de soutien', fx: { budget: -4 }, out: 'Moins d’argent, mais l’honneur est sauf.' },
  ] },
  { id: 'alliance', icon: '🤝', title: 'Alliance technique', text: () => 'Une petite écurie propose de partager ta soufflerie contre de l’argent.', ch: [
    { t: 'Louer la soufflerie (+4 M€)', fx: { budget: 4, dev: 4 }, out: 'De l’argent frais, mais moins de créneaux pour tes pièces.' },
    { t: 'Échanger nos données', fx: { dev: -3, rep: -2 }, out: 'Échange fructueux, même si certains critiquent.' },
    { t: 'Refuser', fx: {}, out: 'Chacun chez soi.' },
  ] },
  { id: 'charity', icon: '💚', title: 'Gala de charité', text: () => 'On te propose de mettre aux enchères un aileron dédicacé pour une association.', ch: [
    { t: 'Donner l’aileron et participer', fx: { rep: 6, moral: 3, budget: -0.5 }, out: 'Un moment fort, salué par tous.' },
    { t: 'Envoyer un représentant', fx: { rep: 2 }, out: 'Un geste apprécié.' },
    { t: 'Décliner', fx: { rep: -3 }, out: 'Certains médias le relèvent.' },
  ] },
  { id: 'rainset', icon: '🌧️', title: 'Pari sur la météo', text: () => 'Les ingénieurs proposent de régler la voiture pour la pluie au prochain GP.', ch: [
    { t: 'Réglages pluie', fx: { perf: { mu: 0.012, vtop: -0.006 } }, out: 'Plus d’adhérence, un peu moins de vitesse.' },
    { t: 'Réglages vitesse', fx: { perf: { vtop: 0.008, mu: -0.006 } }, out: 'Taillée pour les lignes droites.' },
    { t: 'Garder un réglage équilibré', fx: {}, out: 'Pas de pari.' },
  ] },
  // pré-saison (jamais tirés au hasard) : présentation de la voiture, puis essais hivernaux
  { id: 'launch', pre: true, icon: '🚗', title: 'Présentation de la voiture', text: () => `La nouvelle monoplace de ${TEAMS[0].name} est dévoilée devant la presse et les partenaires. Quel message veux-tu faire passer pour la saison ?`, ch: [
    { t: 'Grand show devant les fans (2 M€)', fx: { budget: -2, rep: 5, moral: 4 }, out: 'Jeux de lumière, pilotes sur scène : la livrée fait le tour des réseaux.' },
    { t: 'Annoncer de grandes ambitions', fx: { rep: 3, conf: 6, moral: 3 }, out: 'La direction adore ton discours. Il faudra maintenant l’assumer en piste.',
      risk: { p: 0.3, fx: { rep: -3, conf: -3 }, out: 'Les journalistes se moquent : « Encore une écurie qui promet la lune… »' } },
    { t: 'Une simple vidéo en ligne : l’argent va à l’usine', fx: { budget: 1.5, dev: -4, rep: -2 }, out: 'Présentation discrète : les 1,5 M€ économisés font gagner quatre jours aux recherches.' },
  ] },
  { id: 'tests', pre: true, icon: '🧪', title: 'Essais hivernaux', text: c => `Trois jours de roulage avant le premier Grand Prix${c.dev ? ', avec des pièces encore en développement à l’usine' : ''}. Quel programme pour la voiture ?`, ch: [
    { t: 'Simulations de course : pneus et réglages', fx: { perf: { mu: 0.006 }, moral: 2 }, out: 'Des relais longs sans souci : la voiture sera bien réglée pour le premier GP.' },
    { t: 'Tours rapides à vide pour impressionner', fx: { rep: 4, moral: 3 }, out: 'Meilleur temps de la matinée : la presse s’emballe !',
      risk: { p: 0.3, fx: { budget: -1.5, dev: 6, moral: -3 }, out: 'Tête-à-queue dans le mur : 1,5 M€ de réparations et six jours de retard à l’usine.' } },
    { t: c => c.dev ? 'Valider les pièces en cours de développement' : 'Rouler pour récolter des données', fx: c => c.dev ? { dev: -8 } : { perf: { vtop: 0.004 } },
      out: c => c.dev ? 'Les données des essais font gagner huit jours à l’usine.' : 'Les ingénieurs trouvent un peu de vitesse de pointe pour le premier GP.' },
  ] },
];

// contexte d'un événement (résultat de la course qui vient d'avoir lieu)
export function eventCtx(res) {
  const depts = Object.keys(DEPTS).filter(d => chiefOf(d)), d = depts.length ? pick(depts) : null, c = d && chiefOf(d);
  return { pos: res.pos, dnf: res.dnf, rival: res.rival, mate: career?.mate?.name, conf: team.conf ?? 60, dev: devCount() > 0,
    chief: c ? { name: c.name } : null, chiefKey: d, chiefDept: d ? DEPTS[d] : '', chiefFee: c ? r1(Math.max(1, chiefContract(d).sal * 0.8)) : 0 };
}
export function pickEvent(ctx) {
  const seen = season.seenEv || [];
  const pool = EVENTS.filter(e => !e.pre && !seen.includes(e.id) && (!e.when || e.when(ctx)));
  if (!pool.length) return null;
  const e = pick(pool);
  season.seenEv = [...seen, e.id];
  return { id: e.id, ctx };
}
const val = (v, c) => typeof v === 'function' ? v(c) : v;

// applique les effets étendus d'un choix
export function applyEvent(fx) {
  applyFx(fx);
  if (career) {
    if (fx.mate) for (const k in fx.mate) career.mate[k] = clamp(Math.round((career.mate[k] + fx.mate[k]) * 10) / 10, 40, 99);
    if (fx.wage) { const k = mateContract(); k.sal = r1(k.sal + fx.wage); }
    if (fx.role) mateContract().role = fx.role;
    saveCareer(); applyCareer();
  }
  if (fx.dev) shiftDev(fx.dev);
  if (fx.deal) (team.deals = team.deals || []).push({ ...fx.deal });
  if (fx.chief) loseChief(fx.chief);
  if (fx.grid && season) { season.gridPen = (season.gridPen || 0) + fx.grid; saveSeason(); }
  saveTeam();
}
export function evFxText(fx) {
  const out = [];
  const tag = (t, good) => out.push(`<span class="${good ? 'up' : 'down'}">${t}</span>`);
  if (fx.mate) for (const k in fx.mate) tag(`Équipier : ${{ spd: 'vitesse', strat: 'stratégie', tyre: 'pneus' }[k]} ${fx.mate[k] > 0 ? '+' : ''}${fx.mate[k]}`, fx.mate[k] > 0);
  if (fx.wage) tag(`Salaire de l’équipier +${fmtM(fx.wage)} M€ par saison`, false);
  if (fx.role) tag(`Statut de l’équipier : ${ROLES[fx.role]}`, false);
  if (fx.dev) tag(`Recherches ${fx.dev > 0 ? '+' + fx.dev + ' jours de retard' : 'en avance de ' + -fx.dev + ' jours'}`, fx.dev < 0);
  if (fx.deal) tag(`${fx.deal.name} : +${fmtM(fx.deal.gp)} M€ par GP pendant ${fx.deal.n} GP`, true);
  if (fx.chief) tag('Le poste de chef est vacant', false);
  if (fx.grid) tag(`${fx.grid} places de pénalité sur la grille`, false);
  return fxText(fx) + out.join('');
}

// fenêtre « flash info » : le joueur doit trancher ; onDone() une fois la conséquence affichée
export function showEvent(ev, onDone) {
  const E = EVENTS.find(e => e.id === ev.id), c = ev.ctx, el = document.getElementById('nego');
  if (!E) return onDone();
  el.innerHTML = `<div class="ncard evc in" style="--c:#ff3d2e">
    <div class="flash"><b>Flash info</b><span>${E.title}</span></div>
    <div class="evb"><span class="evi">${E.icon}</span><p>${E.text(c)}</p></div>
    <div class="evch">${E.ch.map((x, i) => `<button data-c="${i}">${val(x.t, c)}</button>`).join('')}</div>
    <div class="evout hidden"></div></div>`;
  el.classList.remove('hidden');
  el.querySelectorAll('[data-c]').forEach(b => b.onclick = () => {
    const x = E.ch[+b.dataset.c], bad = x.risk && Math.random() < x.risk.p;
    const fx = val(bad ? x.risk.fx : x.fx, c), out = val(bad ? x.risk.out : x.out, c);
    applyEvent(fx);
    el.querySelectorAll('[data-c]').forEach(y => { y.disabled = true; if (y !== b) y.classList.add('off'); });
    b.classList.add('chosen');
    const o = el.querySelector('.evout');
    o.innerHTML = `<p class="${bad ? 'bad' : ''}">${bad ? '⚠️ ' : ''}${out}</p><div class="pfx">${evFxText(fx) || '<span>Aucun effet</span>'}</div><button class="main" id="evOk">Continuer</button>`;
    o.classList.remove('hidden');
    el.querySelector('#evOk').onclick = () => { el.classList.add('hidden'); el.innerHTML = ''; onDone(); };
  });
}

// ---------- objectifs de la direction ----------
const OBJ = { new: [['cpos', 8], ['pts', 25]], mid: [['cpos', 5], ['pod', 1]], top: [['cpos', 2], ['win', 3]] };
export function objectives() {
  if (!career || !season) return [];
  const st = teamStandings(), me = st.findIndex(t => t.player) + 1, pts = st[me - 1].pts, res = season.results || [];
  return (OBJ[career.tier] || OBJ.mid).map(([k, v]) => {
    const cur = k === 'cpos' ? me : k === 'pts' ? pts : res.filter(r => r.pos <= (k === 'win' ? 1 : 3)).length;
    const ok = k === 'cpos' ? res.length > 0 && cur <= v : cur >= v;
    const txt = k === 'cpos' ? `Top ${v} des écuries` : k === 'pts' ? `${v} points pour l’écurie` : k === 'pod' ? `${v} podium${v > 1 ? 's' : ''}` : `${v} victoire${v > 1 ? 's' : ''}`;
    const prog = k === 'cpos' ? (res.length ? `P${cur}` : 'P—') : `${cur}/${v}`;
    return { k, v, cur, ok, txt, prog };
  });
}
// après chaque course : la direction juge le résultat par rapport au niveau de l'écurie
export function boardAfterRace(pos, dnf) {
  const exp = EXPECT[career?.tier] || 9;
  const d = dnf ? -4 : clamp(Math.round((exp - pos) * 0.6), -5, 5);
  team.conf = clamp((team.conf ?? 60) + d, 0, 100); saveTeam();
  return d;
}
// bilan de fin de saison : objectifs atteints ou non ; renvoie le bilan (appliqué une seule fois)
export function seasonReview() {
  if (season.review) return season.review;
  const obj = objectives(), met = obj.filter(o => o.ok).length;
  const d = met * 10 - (obj.length - met) * 15;
  team.conf = clamp((team.conf ?? 60) + d, 0, 100);
  const bonus = team.conf >= 80 ? r1(5 + met * 3) : 0;
  team.budget = r1(team.budget + bonus); saveTeam();
  season.review = { met, n: obj.length, d, bonus, fired: team.conf < 25 }; saveSeason();
  return season.review;
}
export const confTxt = c => c >= 75 ? 'excellente' : c >= 55 ? 'bonne' : c >= 35 ? 'fragile' : c >= 20 ? 'en crise' : 'rompue';
export const isFired = () => !!career && season && (season.review?.fired || (team.conf ?? 60) <= 5);
export function boardHTML() {
  const c = Math.round(team.conf ?? 60), obj = objectives();
  return `<div class="board ${c < 35 ? 'low' : ''}"><b>🏛️ Direction</b><span class="bconf"><i><em style="width:${c}%"></em></i>Confiance ${c} % · ${confTxt(c)}</span>` +
    obj.map(o => `<span class="obj ${o.ok ? 'ok' : ''}">${o.ok ? '✓' : '○'} ${o.txt} <small>(${o.prog})</small></span>`).join('') + '</div>';
}

// ---------- missions des sponsors (une par GP, à accepter ou non) ----------
const SPONSORS = ['Lumora', 'Kestrel Tech', 'Novacel', 'Brisa Mobile', 'Atlasbank', 'Orvane', 'Pixalto', 'Verdania'];
export function makeMission(rival) {
  const exp = EXPECT[career?.tier] || 9, sp = pick(SPONSORS), k = Math.random();
  let m;
  if (k < 0.3) { const t = clamp(exp - 1 - ((Math.random() * 3) | 0), 1, 16); m = { kind: 'top', v: t, txt: `Termine dans le top ${t}`, gain: r1(1.2 + (exp - t) * 0.35) }; }
  else if (k < 0.48) { const g = 3 + ((Math.random() * 3) | 0); m = { kind: 'gain', v: g, txt: `Gagne au moins ${g} places pendant la course`, gain: r1(1 + g * 0.3) }; }
  else if (k < 0.6) m = { kind: 'fast', txt: 'Signe le meilleur tour en course', gain: 2.5 };
  else if (k < 0.78 && rival) m = { kind: 'beat', who: rival, txt: `Termine devant ${rival}`, gain: 1.8 };
  else if (k < 0.9) { const t = Math.min(10, exp + 1); m = { kind: 'both', v: t, txt: `Les deux voitures dans le top ${t}`, gain: r1(2 + (10 - t) * 0.2) }; }
  else m = { kind: 'finish', v: exp + 3, txt: `Termine la course sans abandon, dans le top ${Math.min(20, exp + 3)}`, gain: 1 };
  return { ...m, sp, acc: false };
}
// r : { pos, dnf, gained, fastest, matePos, rivalPos }
export function checkMission(m, r) {
  if (r.dnf) return false;
  switch (m.kind) {
    case 'top': return r.pos <= m.v;
    case 'gain': return r.gained >= m.v;
    case 'fast': return r.fastest;
    case 'beat': return r.rivalPos > r.pos;
    case 'both': return r.pos <= m.v && r.matePos <= m.v;
    default: return r.pos <= m.v;
  }
}
export function missionHTML(m) {
  if (!m) return '';
  return `<div class="mission ${m.acc ? 'on' : ''}"><span>🤝 <b>Mission ${m.sp}</b> : ${m.txt}. Prime <b>+${fmtM(m.gain)} M€</b> · en cas d’échec : réputation −3.</span>` +
    `<button id="szMis" ${m.acc ? 'disabled' : ''}>${m.acc ? '✓ Acceptée' : 'Accepter'}</button></div>`;
}
// sponsors ponctuels : paiement après chaque GP
export function payDeals() {
  let s = 0;
  team.deals = (team.deals || []).filter(d => { s += d.gp; d.n--; return d.n > 0; });
  team.budget = r1(team.budget + s); saveTeam();
  return r1(s);
}
