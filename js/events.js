// Vie de l'écurie entre les Grands Prix : événements à choix annoncés en « flash info », objectifs de
// la direction (confiance, licenciement possible), missions proposées par les sponsors avant chaque course.
import { TEAMS, CIRCUITS } from './data.js';
import { team, applyFx, saveTeam } from './team.js';
import { career, saveCareer, applyCareer, mateContract } from './career.js';
import { chiefOf, chiefContract, DEPTS, loseChief } from './staff.js';
import { shiftDev, devCount } from './garage.js';
import { season, saveSeason, teamStandings, nRounds } from './season.js';
import { STORIES, EXTRA } from './stories.js';
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
    { t: 'Acheter les plans (3 M€)', fx: { budget: -3, perf: { kd: 0.05 }, flag: 'spied' }, out: 'L’aileron copié sera prêt pour le prochain GP.',
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
    { t: 'Accepter : tout filmer', fx: { budget: 4, rep: 6, moral: -6, flag: 'docu' }, out: 'Les caméras sont partout, même à la cantine.' },
    { t: 'Accepter avec des zones interdites', fx: { budget: 2, rep: 3, flag: 'docu' }, out: 'Un compromis qui convient à tout le monde.' },
    { t: 'Refuser', fx: { moral: 3 }, out: 'L’équipe garde ses secrets.' },
  ] },
  { id: 'engine', icon: '⚙️', title: 'Moteur fatigué', text: () => 'Le motoriste prévient : ton moteur arrive en fin de vie. En monter un neuf coûte une pénalité sur la grille.', ch: [
    { t: 'Moteur neuf : 5 places de pénalité', fx: { grid: 5, perf: { vtop: 0.006 } }, out: 'Départ plus loin, mais toute la puissance.' },
    { t: 'Garder le moteur usé', fx: { perf: { vtop: -0.01, acc: -0.015 } }, out: 'Il faudra ménager la mécanique au prochain GP.' },
    { t: 'Payer un moteur de rechange hors quota (4 M€)', fx: { budget: -4, conf: -2 }, out: 'Problème réglé, à prix d’or.' },
  ] },
  { id: 'prodigy', icon: '🎓', title: 'Un prodige frappe à la porte', text: () => 'Un jeune champion de karting cherche un volant d’essai et une place à l’académie.', ch: [
    { t: 'L’accueillir comme réserviste (1 M€)', fx: { budget: -1, rep: 3, mate: { spd: 1 }, flag: 'prodigy' }, out: 'Son énergie pousse ton équipier à se dépasser.' },
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
  // ---- brèves : petits événements à effet marginal, c'est leur cumul qui compte ----
  { id: 'cantine', lite: true, icon: '🍝', title: 'Révolution à la cantine', text: () => 'Le nouveau chef cuisinier de l’usine veut imposer un menu « performance » sans dessert.', ch: [
    { t: 'Menu performance pour tous', fx: { moral: -2, perf: { mu: 0.003 } }, out: 'Les ingénieurs râlent, les pilotes s’affûtent.' },
    { t: 'Garder les desserts le vendredi', fx: { moral: 2 }, out: 'Compromis sucré, tout le monde est content.' },
    { t: 'Laisser chacun choisir', fx: { moral: 1, budget: -0.2 }, out: 'Deux menus, un peu plus cher.' },
  ] },
  { id: 'influ', lite: true, icon: '📱', title: 'Un influenceur au garage', text: c => `${c.star}, star des réseaux avec des millions d’abonnés, veut tourner une vidéo dans ton garage pendant le prochain Grand Prix.`, ch: [
    { t: 'Accès total au garage', fx: { rep: 3, moral: -2, flag: 'influ' }, out: 'La vidéo cartonne… et montre un peu trop de choses.' },
    { t: 'Une interview dans le motorhome', fx: { rep: 1, budget: 0.3 }, out: 'Une vidéo sympa, sans risque.' },
    { t: 'Refuser', fx: { moral: 1 }, out: 'Les mécaniciens travaillent tranquilles.' },
  ] },
  { id: 'influbad', lite: true, icon: '🙈', when: c => c.flags.influ, title: 'La vidéo fait jaser', text: c => `Dans la vidéo de ${c.star}, on aperçoit un écran avec des données de télémétrie. Les rivaux se frottent les mains.`, ch: [
    { t: 'Demander de flouter l’image', fx: { rep: -1, dev: 1 }, out: 'Trop tard pour certains, mais l’essentiel est caché.' },
    { t: 'En rire publiquement', fx: { rep: 2, moral: 1 }, out: '« Ce ne sont que des données d’il y a un mois ! » Le paddock sourit.' },
    { t: 'Changer tous les mots de passe', fx: { dev: 2, moral: -1 }, out: 'Une journée perdue en informatique, mais l’usine est blindée.' },
  ] },
  { id: 'livery', lite: true, icon: '🎨', title: 'Livrée spéciale', text: c => `Les fans votent pour une livrée spéciale au Grand Prix de ${c.next}. Le design gagnant est… très original.`, ch: [
    { t: 'L’adopter (0,4 M€)', fx: { budget: -0.4, rep: 3 }, out: 'Les fans adorent, les photos font le tour du monde.' },
    { t: 'L’adapter aux couleurs du sponsor', fx: { budget: 0.4, rep: 1 }, out: 'Tout le monde y trouve son compte.' },
    { t: 'Garder notre livrée', fx: { rep: -1, moral: 1 }, out: 'Fidèle à l’identité de l’écurie.' },
  ] },
  { id: 'storm', lite: true, icon: '🌪️', title: 'Tempête sur le fret', text: () => 'Une tempête retarde l’avion qui transporte les caisses de pièces de rechange.', ch: [
    { t: 'Affréter un camion express (0,6 M€)', fx: { budget: -0.6 }, out: 'Les caisses arrivent à temps.' },
    { t: 'Faire avec les pièces sur place', fx: { perf: { kd: -0.004 }, moral: -1 }, out: 'On roulera avec l’ancien aileron.' },
    { t: 'Fabriquer sur place avec les mécaniciens', fx: { moral: 2, dev: 1 }, out: 'Nuit blanche, mais l’équipe en sort soudée.' },
  ] },
  { id: 'interns', lite: true, icon: '🧑‍🎓', title: 'Stagiaires ingénieurs', text: () => 'Une école d’ingénieurs propose cinq stagiaires brillants pour la saison.', ch: [
    { t: 'Les accueillir', fx: { dev: -2, moral: -1 }, out: 'Ils apportent des idées fraîches… et quelques erreurs.' },
    { t: 'En prendre un seul', fx: { dev: -1 }, out: 'Le meilleur des cinq rejoint l’usine.' },
    { t: 'Pas cette année', fx: {}, out: 'L’école ira voir ailleurs.' },
  ] },
  { id: 'coffee', lite: true, icon: '☕', title: 'Panne de machine à café', text: () => 'La machine à café de l’usine a rendu l’âme. Les ingénieurs sont au bord de la crise.', ch: [
    { t: 'Machine haut de gamme (0,1 M€)', fx: { budget: -0.1, moral: 3 }, out: 'Le meilleur café du paddock !' },
    { t: 'Une cafetière en attendant', fx: { moral: -1 }, out: 'Le café est tiède, l’ambiance aussi.' },
    { t: 'Une boîte de thé', fx: { moral: -2, perf: { steer: 0.004 } }, out: 'Zénitude forcée…' },
  ] },
  { id: 'radioleak', lite: true, icon: '🎙️', title: 'Message radio diffusé', text: c => `Un message radio un peu vif de ${c.mate || 'ton équipier'} a été diffusé à la télévision.`, ch: [
    { t: 'L’excuser : la tension de la course', fx: { moral: 2, rep: -1 }, out: 'L’équipier apprécie ton soutien.' },
    { t: 'Le recadrer en interne', fx: { moral: -1, conf: 1 }, out: 'Le message est passé, discrètement.' },
    { t: 'En faire un produit dérivé', fx: { budget: 0.5, rep: 1 }, out: 'Les t-shirts se vendent comme des petits pains.' },
  ] },
  { id: 'kart', lite: true, icon: '🏎️', title: 'Course de karting caritative', text: () => 'Les pilotes du plateau organisent une course de karting au profit d’une association.', ch: [
    { t: 'Participer à fond', fx: { rep: 2, moral: 2 }, out: 'Victoire au karting et sourires partout.',
      risk: { p: 0.15, fx: { rep: 1, moral: -2, perf: { steer: -0.004 } }, out: 'Petite chute : une épaule douloureuse pour le prochain GP.' } },
    { t: 'Faire un don (0,2 M€)', fx: { budget: -0.2, rep: 2 }, out: 'Un geste apprécié.' },
    { t: 'Décliner', fx: { rep: -1 }, out: 'Repos bien mérité.' },
  ] },
  { id: 'gym', lite: true, icon: '🏋️', title: 'Nouveau préparateur physique', text: () => 'Un préparateur physique réputé propose un programme intensif.', ch: [
    { t: 'Programme intensif', fx: { moral: -1, perf: { steer: 0.006 } }, out: 'Courbatures, mais des réflexes affûtés.' },
    { t: 'Programme doux', fx: { moral: 1, perf: { steer: 0.002 } }, out: 'Progrès tranquilles.' },
    { t: 'Garder l’ancien', fx: {}, out: 'On ne change pas une équipe qui roule.' },
  ] },
  { id: 'merch', lite: true, icon: '🧢', title: 'Ruée sur la boutique', text: () => 'Les casquettes de l’écurie sont en rupture de stock.', ch: [
    { t: 'Commander en urgence (0,2 M€)', fx: { budget: 0.4, rep: 1 }, out: 'Stock refait : les ventes repartent.' },
    { t: 'Édition limitée numérotée', fx: { budget: 0.6, rep: -1 }, out: 'Les collectionneurs paient cher, certains fans râlent.' },
    { t: 'Attendre la prochaine production', fx: { rep: -1 }, out: 'Les fans patienteront.' },
  ] },
  { id: 'heat', lite: true, icon: '🌡️', title: 'Canicule annoncée', text: c => `Une vague de chaleur est attendue au Grand Prix de ${c.next}.`, ch: [
    { t: 'Refroidissement renforcé', fx: { perf: { vtop: -0.003, mu: 0.004 } }, out: 'Moins de vitesse, mais les pneus et le moteur respirent.' },
    { t: 'Garder le réglage standard', fx: {}, out: 'On verra sur place.' },
    { t: 'Gilets réfrigérants pour l’équipe (0,1 M€)', fx: { budget: -0.1, moral: 2 }, out: 'L’équipe garde la tête froide.' },
  ] },
  { id: 'rookieday', lite: true, icon: '🔰', title: 'Journée des jeunes pilotes', text: () => 'La fédération demande à chaque écurie de faire rouler un jeune pilote lors des essais libres.', ch: [
    { t: 'Faire rouler un jeune prometteur', fx: { rep: 2, perf: { mu: -0.002 } }, out: 'Il impressionne, mais tu perds une séance de réglages.' },
    { t: 'Faire rouler le pilote de réserve', fx: { moral: 1 }, out: 'Séance tranquille.' },
    { t: 'Le jeune d’un sponsor (+0,5 M€)', fx: { budget: 0.5, rep: -1 }, out: 'Il paie bien… et part en tête-à-queue.' },
  ] },
  { id: 'paint', lite: true, icon: '🖌️', title: 'Peinture plus légère', text: () => 'Le fournisseur propose une nouvelle peinture qui allège la voiture de quelques centaines de grammes.', ch: [
    { t: 'L’essayer (0,3 M€)', fx: { budget: -0.3, perf: { acc: 0.004 } }, out: 'Quelques grammes de gagnés : chaque millième compte.' },
    { t: 'Peindre seulement les ailerons', fx: { budget: -0.1, perf: { acc: 0.002 } }, out: 'Petit gain, petit prix.' },
    { t: 'Non merci', fx: {}, out: 'La livrée reste comme elle est.' },
  ] },
  { id: 'podcast', lite: true, icon: '🎧', title: 'Invitation dans un podcast', text: () => 'Un podcast très écouté veut t’interviewer sur ta vision de l’écurie.', ch: [
    { t: 'Tout raconter, sans filtre', fx: { rep: 3, conf: -2 }, out: 'Interview passionnante… la direction a moins aimé certains passages.' },
    { t: 'Langue de bois', fx: { conf: 1 }, out: 'Rien à signaler.' },
    { t: 'Envoyer le chef technique', fx: { rep: 1, dev: 1 }, out: 'Le chef parle aéro pendant deux heures.' },
  ] },
  { id: 'scout', lite: true, icon: '🔭', title: 'Rapport d’espionnage légal', text: c => `Tes ingénieurs ont analysé les photos de la voiture de ${c.rivalTeam}. Ils pensent avoir compris une astuce.`, ch: [
    { t: 'Essayer l’idée à l’usine', fx: { dev: 2, perf: { kd: 0.004 } }, out: 'L’idée marche à moitié, mais ça valait le coup.' },
    { t: 'Garder l’idée pour plus tard', fx: { dev: -1 }, out: 'Les ingénieurs restent concentrés sur leur plan.' },
    { t: 'Signaler un doute à la fédération', fx: { rep: -1, conf: 1 }, out: 'La fédération jettera un œil.' },
  ] },
  { id: 'hotel', lite: true, icon: '🏨', title: 'Hôtel surbooké', text: c => `L’hôtel de l’équipe à ${c.next} a vendu les chambres deux fois.`, ch: [
    { t: 'Hôtel de luxe en urgence (0,4 M€)', fx: { budget: -0.4, moral: 2 }, out: 'Piscine et spa pour tout le monde.' },
    { t: 'Camping-cars au paddock', fx: { moral: -2, budget: 0.1 }, out: 'Ambiance colonie de vacances…' },
    { t: 'Dormir chez les fans', fx: { rep: 2, moral: -1 }, out: 'Les fans n’en reviennent pas !' },
  ] },
  { id: 'tyrebet', lite: true, icon: '🎲', title: 'Allocation de pneus', text: () => 'Le manufacturier propose une allocation de pneus différente pour le prochain GP : plus de tendres ou plus de durs.', ch: [
    { t: 'Plus de tendres', fx: { perf: { mu: 0.004, vtop: -0.002 } }, out: 'De quoi attaquer en qualifications.' },
    { t: 'Plus de durs', fx: { perf: { vtop: 0.003 } }, out: 'Une course plus sereine.' },
    { t: 'L’allocation standard', fx: {}, out: 'Pas de surprise.' },
  ] },
  { id: 'mechbday', lite: true, icon: '🎂', title: 'Anniversaire au garage', text: () => 'Le chef mécanicien fête ses 30 ans de carrière dans l’écurie.', ch: [
    { t: 'Fête surprise à l’usine (0,1 M€)', fx: { budget: -0.1, moral: 4, dev: 1 }, out: 'Émotion et gâteau géant.' },
    { t: 'Un cadeau de toute l’équipe', fx: { moral: 2 }, out: 'Il a la larme à l’œil.' },
    { t: 'Un simple message', fx: { moral: -1 }, out: 'Il aurait aimé un peu plus.' },
  ] },
  { id: 'eco', lite: true, icon: '🌱', title: 'Bilan carbone', text: () => 'Une association pointe l’empreinte carbone de l’écurie.', ch: [
    { t: 'Panneaux solaires à l’usine (0,8 M€)', fx: { budget: -0.8, rep: 3, conf: 1 }, out: 'L’écurie se met au vert.' },
    { t: 'Compenser par des plantations', fx: { budget: -0.2, rep: 1 }, out: 'Un geste symbolique.' },
    { t: 'Ignorer', fx: { rep: -2 }, out: 'La polémique dure quelques jours.' },
  ] },
  { id: 'vip', lite: true, icon: '🎟️', title: 'Invités de marque', text: c => `Le patron de ${c.brand} veut visiter le garage pendant les qualifications.`, ch: [
    { t: 'Visite VIP complète', fx: { budget: 0.6, moral: -1 }, out: 'Il repart conquis… les mécaniciens un peu moins.' },
    { t: 'Visite après la séance', fx: { budget: 0.3 }, out: 'Tout le monde est content.' },
    { t: 'Refuser poliment', fx: { moral: 1, rep: -1 }, out: 'Concentration maximale.' },
  ] },
  { id: 'fuelsup', lite: true, icon: '⛽', title: 'Nouveau carburant', text: () => 'Le pétrolier partenaire propose un carburant expérimental un peu plus énergétique.', ch: [
    { t: 'L’utiliser dès le prochain GP', fx: { perf: { vtop: 0.004 } }, out: 'Quelques chevaux en plus.',
      risk: { p: 0.15, fx: { perf: { vtop: -0.004 }, rep: -1 }, out: 'Le carburant encrasse le moteur : moins de puissance.' } },
    { t: 'Le tester au banc d’abord', fx: { dev: 1 }, out: 'Prudence d’ingénieur.' },
    { t: 'Garder le carburant actuel', fx: {}, out: 'On ne touche à rien.' },
  ] },
  { id: 'wind', lite: true, icon: '🌬️', title: 'Données de soufflerie faussées', text: () => 'Un capteur de la soufflerie était mal calibré depuis deux semaines.', ch: [
    { t: 'Refaire tous les essais', fx: { dev: 3 }, out: 'Trois jours de retard, mais des données fiables.' },
    { t: 'Corriger par calcul', fx: { dev: 1, perf: { kd: -0.002 } }, out: 'Presque juste.' },
    { t: 'Faire venir un expert (0,3 M€)', fx: { budget: -0.3, dev: 1 }, out: 'Réglé en une journée.' },
  ] },
  { id: 'fanletter', lite: true, icon: '✉️', title: 'Lettre d’un jeune fan', text: () => 'Un enfant hospitalisé a écrit à l’écurie : son rêve est de voir la voiture de près.', ch: [
    { t: 'Amener la voiture à l’hôpital', fx: { rep: 3, moral: 3, budget: -0.2 }, out: 'Un moment inoubliable pour tout le monde.' },
    { t: 'L’inviter au prochain GP', fx: { rep: 2, moral: 2 }, out: 'Il a passé la journée au muret.' },
    { t: 'Envoyer un casque dédicacé', fx: { rep: 1 }, out: 'Il ne le quitte plus.' },
  ] },
  { id: 'gearbox', lite: true, icon: '🔩', title: 'Boîte de vitesses suspecte', text: () => 'La télémétrie montre des vibrations anormales sur la boîte de vitesses.', ch: [
    { t: 'La changer (0,5 M€)', fx: { budget: -0.5 }, out: 'Boîte neuve, esprit tranquille.' },
    { t: 'Ménager les passages de rapports', fx: { perf: { acc: -0.004 } }, out: 'Un peu moins vif au prochain GP.' },
    { t: 'Croiser les doigts', fx: {}, out: 'Ça devrait tenir…',
      risk: { p: 0.3, fx: { perf: { acc: -0.008 }, moral: -2 }, out: 'Elle accroche les rapports : prochain GP compliqué.' } },
  ] },
  { id: 'rivalpraise', lite: true, icon: '👏', when: c => c.pos <= 8 && !c.dnf, title: 'Compliments d’un rival', text: c => `Le directeur de ${c.rivalTeam} a salué publiquement les progrès de ton écurie.`, ch: [
    { t: 'Le remercier chaleureusement', fx: { rep: 2 }, out: 'Le paddock apprécie la classe des deux camps.' },
    { t: '« Il a raison d’avoir peur »', fx: { moral: 2, rep: -1 }, out: 'Les mécaniciens adorent la réplique.' },
    { t: 'Se méfier : il prépare quelque chose', fx: { conf: 1 }, out: 'Prudence, prudence…' },
  ] },
  { id: 'nightshift', lite: true, when: c => c.dev, icon: '🌙', title: 'Équipe de nuit', text: () => 'Le chef de l’usine propose de faire tourner une équipe de nuit cette semaine.', ch: [
    { t: 'Oui (0,5 M€)', fx: { budget: -0.5, dev: -2, moral: -1 }, out: 'Les pièces avancent, les yeux se ferment.' },
    { t: 'Seulement deux nuits', fx: { budget: -0.2, dev: -1 }, out: 'Un petit coup d’accélérateur.' },
    { t: 'Non, on respecte les horaires', fx: { moral: 2 }, out: 'Les ingénieurs dorment bien.' },
  ] },
  { id: 'photo', lite: true, icon: '📸', title: 'Photo virale', text: () => 'Une photo de ta voiture prise au coucher du soleil devient virale.', ch: [
    { t: 'En faire une affiche officielle', fx: { budget: 0.3, rep: 2 }, out: 'L’affiche se vend partout.' },
    { t: 'Retrouver et créditer le photographe', fx: { rep: 3 }, out: 'Le photographe devient un ami de l’écurie.' },
    { t: 'Ne rien faire', fx: {}, out: 'Le buzz retombe.' },
  ] },
  { id: 'simrace', lite: true, icon: '🎮', title: 'Course virtuelle', text: () => 'Une ligue de course virtuelle veut créer une équipe à tes couleurs.', ch: [
    { t: 'Créer l’équipe officielle (0,3 M€)', fx: { budget: -0.3, rep: 3 }, out: 'Des milliers de jeunes fans découvrent l’écurie.' },
    { t: 'Prêter seulement le nom (+0,3 M€)', fx: { budget: 0.3, rep: 1 }, out: 'Facile et rentable.' },
    { t: 'Refuser', fx: {}, out: 'Les jeux vidéo, ce n’est pas pour nous.' },
  ] },
  { id: 'brakes', lite: true, icon: '🛑', title: 'Nouveaux disques de frein', text: () => 'Un fournisseur propose des disques de frein plus endurants à tester.', ch: [
    { t: 'Les monter au prochain GP', fx: { perf: { brake: 0.006 } }, out: 'Freinages plus tardifs en vue.',
      risk: { p: 0.2, fx: { perf: { brake: -0.004 } }, out: 'Ils chauffent trop : il faudra freiner plus tôt.' } },
    { t: 'Les tester en essais libres seulement', fx: { perf: { brake: 0.002 } }, out: 'Un petit gain sans risque.' },
    { t: 'Garder nos freins', fx: {}, out: 'On reste sur du connu.' },
  ] },
  { id: 'umbrella', lite: true, icon: '☂️', title: 'Parapluies pour les fans', text: () => 'Pluie annoncée dans les tribunes : un sponsor propose d’offrir des parapluies aux couleurs de l’écurie.', ch: [
    { t: 'Accepter (+0,3 M€)', fx: { budget: 0.3, rep: 2 }, out: 'Une marée de parapluies à tes couleurs.' },
    { t: 'Les payer nous-mêmes (0,2 M€)', fx: { budget: -0.2, rep: 3 }, out: 'Les fans s’en souviendront.' },
    { t: 'Non', fx: {}, out: 'Ils ont des capuches.' },
  ] },
  // suites d'événements (débloquées par un choix précédent)
  { id: 'docuair', lite: true, icon: '📺', when: c => c.flags.docu, title: 'Le documentaire est diffusé', text: () => 'Le premier épisode du documentaire sur l’écurie vient de sortir. Une scène de dispute au muret fait beaucoup parler.', ch: [
    { t: 'Assumer : c’est la passion', fx: { rep: 3, moral: -1 }, out: 'Le public adore cette sincérité.' },
    { t: 'Demander de couper la scène', fx: { rep: -1, moral: 2 }, out: 'L’équipe apprécie, la plateforme beaucoup moins.' },
    { t: 'Organiser une avant-première', fx: { budget: 0.5, rep: 2 }, out: 'Tapis rouge et partenaires ravis.' },
  ] },
  { id: 'prodigywin', lite: true, icon: '🏆', when: c => c.flags.prodigy, title: 'Ton protégé gagne', text: () => 'Le jeune pilote accueilli à l’académie a remporté sa première course en Formule B !', ch: [
    { t: 'Le féliciter publiquement', fx: { rep: 3, moral: 2 }, out: 'L’académie de l’écurie gagne en prestige.' },
    { t: 'Lui offrir un roulage en essais (0,3 M€)', fx: { budget: -0.3, rep: 2, mate: { spd: 1 } }, out: 'Il pousse ton équipier dans ses retranchements.' },
    { t: 'Garder ça discret', fx: { conf: 1 }, out: 'Pas de pression sur ses épaules.' },
  ] },
  { id: 'spyprobe', lite: true, icon: '🔎', when: c => c.flags.spied, title: 'Questions gênantes', text: () => 'Un journaliste enquête sur l’origine de ton nouvel aileron.', ch: [
    { t: 'Nier en bloc', fx: { rep: -1 }, out: 'L’affaire s’essouffle… pour l’instant.',
      risk: { p: 0.25, fx: { rep: -4, conf: -3 }, out: 'L’article sort : le paddock murmure.' } },
    { t: 'Offrir une exclusivité sur un autre sujet', fx: { budget: -0.2, rep: 1 }, out: 'Le journaliste change de piste.' },
    { t: 'Retirer discrètement l’aileron', fx: { perf: { kd: -0.004 }, rep: 1 }, out: 'Plus de preuve, un peu moins d’appui.' },
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
  const rivals = TEAMS.slice(1), nx = CIRCUITS.find(x => x.id === (season?.cal || [])[season?.round]);
  return { pos: res.pos, dnf: res.dnf, rival: res.rival, mate: career?.mate?.name, conf: team.conf ?? 60, dev: devCount() > 0,
    flags: { ...(season?.evFlags || {}) }, star: pick(STARS), brand: pick(SPONSORS), rivalTeam: pick(rivals).name, next: nx?.name || 'la prochaine manche',
    chief: c ? { name: c.name } : null, chiefKey: d, chiefDept: d ? DEPTS[d] : '', chiefFee: c ? r1(Math.max(1, chiefContract(d).sal * 0.8)) : 0 };
}
// après chaque GP : un événement presque toujours, parfois une brève en plus (enchaînées dans la même fenêtre).
// Les suites débloquées par un choix précédent passent en priorité. Une histoire à épisodes peut démarrer,
// ses épisodes passent avant le reste. Historique d'une partie à l'autre (gpm-ev-hist) : on évite ce qu'on a déjà vu.
const HIST = 'gpm-ev-hist';
const hist = () => { try { return JSON.parse(localStorage.getItem(HIST)) || []; } catch { return []; } };
const remember = id => { try { localStorage.setItem(HIST, JSON.stringify([id, ...hist().filter(x => x !== id)].slice(0, 70))); } catch {} };
const ALL = () => EVENTS.concat(EXTRA);
// tirage qui préfère ce qui n'a pas été vu récemment (même dans une partie précédente)
function fresh(pool) { const h = hist(), f = pool.filter(e => !h.includes(e.id)); return f.length && Math.random() < 0.85 ? f : pool; }
function storyEv(ctx) {
  const st = season.story;
  if (st && st.due <= season.round) return { story: st.id, step: st.step, ctx, v: st.v, ep: st.ep, k: jit() };
  if (st || !career || season.round < 1 || season.round > nRounds() - 7 || Math.random() > 0.5) return null;
  const done = season.seenSt || [], pool = STORIES.filter(x => !done.includes(x.id) && (!x.when || x.when(ctx)));
  if (!pool.length) return null;
  const S = pick(fresh(pool)), first = Object.keys(S.steps)[0];
  season.seenSt = [...done, S.id]; remember(S.id);
  season.story = { id: S.id, step: first, due: season.round, v: S.init ? S.init() : {}, ep: 1 };
  return { story: S.id, step: first, ctx, v: season.story.v, ep: 1, k: jit() };
}
export function pickEvent(ctx, n = Math.random() < 0.4 ? 2 : 1) {
  const seen = season.seenEv || [];
  let pool = ALL().filter(e => !e.pre && !seen.includes(e.id) && (!e.when || e.when(ctx)));
  const sev = storyEv(ctx);
  const add = ev => { if (n > 1) { pool = fresh(pool.filter(x => x.lite && x.id !== ev.id)); if (pool.length) { const f = pick(pool); season.seenEv.push(f.id); remember(f.id); ev.next = { id: f.id, ctx, k: jit() }; } } return ev; };
  season.seenEv = seen.slice();
  if (sev) return add(sev);
  if (!pool.length) return null;
  const follow = pool.filter(e => e.when && /flags\./.test(e.when.toString()));
  const e = follow.length && Math.random() < 0.6 ? pick(follow) : pick(fresh(pool));
  season.seenEv.push(e.id); remember(e.id);
  return add({ id: e.id, ctx, k: jit() });
}
// chaque partie est différente : l'ampleur des effets varie de ±25 % (moral, réputation, confiance, performance)
const jit = () => r1(0.75 + Math.random() * 0.5) || 1;
function jitter(fx, k) {
  if (!k || k === 1) return fx;
  const o = { ...fx }, rd = v => Math.sign(v) * Math.max(1, Math.round(Math.abs(v) * k));
  for (const key of ['moral', 'rep', 'conf']) if (o[key]) o[key] = rd(o[key]);
  if (o.perf) { o.perf = { ...o.perf }; for (const p in o.perf) o.perf[p] = Math.round(o.perf[p] * k * 1e4) / 1e4; }
  return o;
}
const val = (v, c, sv) => typeof v === 'function' ? v(c, sv) : v;

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
  // histoire : le pilote de l'histoire devient l'équipier (l'ancien rejoint les pilotes libres)
  if (fx.sign?.d && career) {
    const y = season?.year || 2027, k = fx.sign;
    (career.free = career.free || []).push({ ...career.mate, from: 'Ex-équipier' });
    career.mate = { ...k.d }; career.contract = { sal: k.sal, years: k.years, role: k.role, bonus: 0, since: y, paid: y };
    saveCareer(); applyCareer();
  }
  if (fx.grid && season) { season.gridPen = (season.gridPen || 0) + fx.grid; saveSeason(); }
  if (fx.flag && season) { (season.evFlags = season.evFlags || {})[fx.flag] = season.round; saveSeason(); }
  saveTeam();
}
export function evFxText(fx) {
  const out = [];
  const tag = (t, good) => out.push(`<span class="${good ? 'up' : 'down'}">${t}</span>`);
  if (fx.mate) for (const k in fx.mate) tag(`Équipier : ${{ spd: 'vitesse', strat: 'stratégie', tyre: 'pneus' }[k]} ${fx.mate[k] > 0 ? '+' : ''}${fx.mate[k]}`, fx.mate[k] > 0);
  if (fx.wage) tag(`Salaire de l’équipier +${fmtM(fx.wage)} M€ par saison`, false);
  if (fx.role) tag(`Statut de l’équipier : ${ROLES[fx.role]}`, false);
  if (fx.dev) tag(`Recherches ${fx.dev > 0 ? '+' + fx.dev + ' jours de retard' : 'en avance de ' + -fx.dev + ' jours'}`, fx.dev < 0);
  if (fx.deal) tag(fx.deal.gp >= 0 ? `${fx.deal.name} : +${fmtM(fx.deal.gp)} M€ par GP pendant ${fx.deal.n} GP` : `${fx.deal.name} : −${fmtM(-fx.deal.gp)} M€ par GP pendant ${fx.deal.n} GP`, fx.deal.gp >= 0);
  if (fx.sign?.d) tag(`Nouvel équipier : ${fx.sign.d.name} (${fx.sign.years} saison${fx.sign.years > 1 ? 's' : ''})`, true);
  if (fx.chief) tag('Le poste de chef est vacant', false);
  if (fx.grid) tag(`${fx.grid} places de pénalité sur la grille`, false);
  return fxText(fx) + out.join('');
}

// fenêtre « flash info » : le joueur doit trancher ; onDone() une fois la conséquence affichée
export function showEvent(ev, onDone) {
  const SD = ev.story && STORIES.find(x => x.id === ev.story), E = SD ? SD.steps[ev.step] : ALL().find(e => e.id === ev.id);
  const c = ev.ctx, sv = ev.v || {}, el = document.getElementById('nego');
  if (!E) { if (SD && season) { season.story = null; saveSeason(); } return onDone(); }
  const head = SD ? `<b>Histoire · épisode ${ev.ep || 1}</b><span>${SD.icon} ${SD.title} — ${E.title}</span>` : `<b>${E.lite ? 'En bref' : 'Flash info'}</b><span>${E.title}</span>`;
  el.innerHTML = `<div class="ncard evc in ${SD ? 'story' : ''}" style="--c:${SD ? '#b06cff' : '#ff3d2e'}">
    <div class="flash">${head}</div>
    <div class="evb"><span class="evi">${E.icon}</span><p>${E.text(c, sv)}</p></div>
    <div class="evch">${E.ch.map((x, i) => `<button data-c="${i}">${val(x.t, c, sv)}</button>`).join('')}</div>
    <div class="evout hidden"></div></div>`;
  el.classList.remove('hidden');
  el.querySelectorAll('[data-c]').forEach(b => b.onclick = () => {
    const x = E.ch[+b.dataset.c], bad = x.risk && Math.random() < x.risk.p;
    let fx = jitter(val(bad ? x.risk.fx : x.fx, c, sv), ev.k);
    const out = val(bad ? x.risk.out : x.out, c, sv);
    if (typeof fx.deal?.name === 'function') fx = { ...fx, deal: { ...fx.deal, name: fx.deal.name(sv) } };
    if (fx.sign) fx = { ...fx, sign: { ...fx.sign, d: sv[fx.sign.drv] } };
    applyEvent(fx);
    // histoire : épisode suivant dans quelques GP, ou fin
    let end = '';
    if (SD && season) {
      const nx = x.next && SD.steps[x.next];
      if (nx) season.story = { ...season.story, step: x.next, due: season.round + (x.gap ?? 1 + ((Math.random() * 2) | 0)), ep: (ev.ep || 1) + 1 };
      else { season.story = null; end = `<p class="stend">📕 Fin de l’histoire « ${SD.title} ».</p>`; }
      saveSeason();
    }
    el.querySelectorAll('[data-c]').forEach(y => { y.disabled = true; if (y !== b) y.classList.add('off'); });
    b.classList.add('chosen');
    const o = el.querySelector('.evout');
    o.innerHTML = `<p class="${bad ? 'bad' : ''}">${bad ? '⚠️ ' : ''}${out}</p>${SD && !end ? '<p class="stend">📖 À suivre…</p>' : end}<div class="pfx">${evFxText(fx) || '<span>Aucun effet</span>'}</div><button class="main" id="evOk">Continuer</button>`;
    o.classList.remove('hidden');
    el.querySelector('#evOk').onclick = () => { el.classList.add('hidden'); el.innerHTML = ''; ev.next ? showEvent(ev.next, onDone) : onDone(); };
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
const STARS = ['Lily Vroom', 'TurboTom', 'Max Drift', 'Zoé Paddock', 'Kenji Apex', 'Nina Slipstream'];
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
