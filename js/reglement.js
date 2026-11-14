// Grandes refontes du règlement : tous les 3 à 5 ans (surtout 4 ou 5), la forme de la voiture et les arbres
// de développement changent et toutes les pièces repartent de zéro. La voiture de la nouvelle ère se
// développe un an à l'avance, avec les mêmes ateliers et le même budget que la voiture en cours.

// ères (univers fictif) : id = nom du modèle 3D de l'ère (un modèle par ère), mul = poids des effets des pièces,
// shape = forme de la monoplace procédurale (carmodel.js)
export const ERAS = [
  { id: 'fondation', name: 'Fondation', desc: 'Voitures longues, aileron avant à trois éléments, grande dérive.', mul: {}, shape: {} },
  { id: 'effet-sol', name: 'Effet de sol', desc: 'L’appui vient du dessous : fond sculpté, pontons creusés, aileron avant à deux éléments et aileron de poutre.',
    mul: { kd: 1.25, vtop: 0.9 }, shape: { nose: 0.93, fwEl: 2, pod: 0.78, podH: 0.92, fin: 0, rwW: 0.94, beam: 1 } },
  { id: 'hybride', name: 'Hybride', desc: 'Moteurs plus électriques : voitures plus courtes, gros pontons pour refroidir, aileron arrière bas et large.',
    mul: { acc: 1.3, fuel: 1.3, vtop: 1.1, kd: 0.85 }, shape: { nose: 0.86, fwSpan: 0.92, pod: 1.12, podH: 1.06, rwW: 1.14, rwH: -0.12, beam: 0 } },
  { id: 'aero-active', name: 'Aéro active', desc: 'Ailerons mobiles et nez très long : quatre éléments à l’avant, pontons étroits, très grande dérive.',
    mul: { slip: 1.4, kd: 1.1, tyre: 1.2 }, shape: { nose: 1.1, fwEl: 4, fwSpan: 1.04, pod: 0.72, podH: 0.86, fin: 2, rwW: 0.9, rwH: 0.1 } },
  { id: 'securite', name: 'Sécurité', desc: 'Structures renforcées, voitures plus larges et plus lourdes, aileron avant très large et sans dérive.',
    mul: { rel: 1.3, wall: 1.3, mu: 1.15, vtop: 0.95 }, shape: { nose: 0.97, fwEl: 2, fwSpan: 1.1, pod: 1.2, podH: 1.1, fin: 0, rwW: 1.05, rwH: 0.05, beam: 0 } },
];
// nom affiché d'une ère : les ères qui reviennent prennent un numéro (« Effet de sol II »)
export const eraName = e => 'Ère ' + ERAS[e.i].name + (e.gen >= ERAS.length ? ' ' + ['', 'II', 'III', 'IV', 'V'][Math.min(4, Math.floor(e.gen / (ERAS.length - 1)))] : '');
// années entre deux refontes : 3 (20 %), 4 (40 %) ou 5 (40 %)
export const reformGap = () => { const r = Math.random(); return r < 0.2 ? 3 : r < 0.6 ? 4 : 5; };
export const nextEra = cur => { const c = ERAS.map((_, i) => i).filter(i => i && i !== cur); return c[Math.floor(Math.random() * c.length)]; };

// pièces possibles de chaque pôle : [nom, description, poids des effets, illustration]
const POOL = {
  moteur: [
    ['Chambre de combustion compacte', 'Une combustion plus rapide et plus complète.', { vtop: 1, acc: 0.5 }, 'm1'],
    ['Turbo à géométrie variable', 'De la puissance sans temps de réponse.', { vtop: 1 }, 'm2'],
    ['Volant moteur allégé', 'Le moteur prend ses tours plus vite.', { acc: 1 }, 'm3'],
    ['Batterie haute densité', 'Plus d’énergie stockée à chaque freinage.', { acc: 0.8, vtop: 0.3 }, 'm4'],
    ['Moteur électrique avant', 'Une poussée de plus en sortie de virage.', { acc: 1 }, 'm4'],
    ['Cartographie qualification', 'Un mode tout pour la vitesse sur un tour.', { vtop: 1 }, 'm5'],
    ['Refroidissement direct', 'Le moteur tourne plus frais : il consomme et casse moins.', { fuel: 1, rel: 0.6 }, 'm6'],
    ['Allumage à préchambre', 'Plus de rendement pour chaque goutte de carburant.', { fuel: 0.8, vtop: 0.4 }, 'm7'],
    ['Échappement accordé', 'Les gaz sortent mieux : plus de puissance en haut.', { vtop: 1 }, 'm2'],
    ['Embrayage sans coupure', 'Des relances sans à-coups.', { acc: 1 }, 'm3'],
    ['Récupération à l’échappement', 'La chaleur perdue devient de la puissance.', { acc: 0.5, vtop: 0.5 }, 'm4'],
    ['Bloc monobloc', 'Plus rigide, plus léger et plus fiable.', { rel: 1, acc: 0.3 }, 'm6'],
    ['Moteur nouvelle génération', 'Le sommet du savoir-faire de l’écurie.', { vtop: 1, acc: 0.6 }, 'm8'],
  ],
  aero: [
    ['Fond à canaux Venturi', 'L’air accélère sous la voiture et la plaque au sol.', { kd: 1 }, 'a2'],
    ['Aileron avant épuré', 'Moins de turbulences pour la voiture qui suit.', { kd: 0.5, slip: 0.5 }, 'a1'],
    ['Pontons creusés', 'L’air file vers l’arrière sans freiner la voiture.', { vtop: 1 }, 'a3'],
    ['Lame de fond flexible', 'Un fond qui se plaque à haute vitesse.', { kd: 0.8, vtop: 0.3 }, 'a8'],
    ['Aileron arrière cuillère', 'Peu de traînée pour les lignes droites.', { vtop: 1, kd: -0.3 }, 'a3'],
    ['Volets mobiles', 'L’aileron s’ouvre en ligne droite derrière un rival.', { slip: 1 }, 'a6'],
    ['Déflecteurs de roues', 'Le sillage des pneus est canalisé.', { kd: 0.7 }, 'a4'],
    ['Écopes de freins soufflées', 'Les freins sont mieux refroidis.', { brake: 1 }, 'a4'],
    ['Diffuseur à triple étage', 'Énormément d’appui à l’arrière.', { kd: 1 }, 'a5'],
    ['Bord de plancher sculpté', 'Le fond reste efficace même en virage.', { kd: 0.8 }, 'a2'],
    ['Capot moteur affiné', 'Moins de surface frontale, plus de vitesse.', { vtop: 0.8, kd: 0.2 }, 'a9'],
    ['Sillage maîtrisé', 'La voiture souffre moins dans l’air sale.', { slip: 0.6, kd: 0.4 }, 'a7'],
    ['Concept aéro intégral', 'Toute la voiture pensée comme une seule aile.', { kd: 1, vtop: 0.4 }, 'a10'],
  ],
  chassis: [
    ['Suspension à tirants', 'Un centre de gravité plus bas.', { mu: 1 }, 'c1'],
    ['Disques ventilés', 'Des freinages plus courts.', { brake: 1 }, 'c2'],
    ['Crémaillère rapide', 'La voiture tourne plus vite.', { steer: 1 }, 'c3'],
    ['Étriers six pistons', 'Un freinage plus fort et plus constant.', { brake: 1 }, 'c4'],
    ['Composite tressé', 'Un châssis plus léger et plus rigide.', { acc: 0.5, mu: 0.6 }, 'c5'],
    ['Amortisseurs reliés', 'Les pneus restent collés à la piste.', { mu: 0.7, tyre: 0.6 }, 'c6'],
    ['Géométrie anti-plongée', 'La voiture reste à plat au freinage.', { brake: 0.6, mu: 0.5 }, 'c3'],
    ['Barres antiroulis réglables', 'Un équilibre ajusté à chaque circuit.', { steer: 0.6, mu: 0.5 }, 'c1'],
    ['Moyeux allégés', 'Moins de masse non suspendue.', { acc: 0.4, steer: 0.6 }, 'c2'],
    ['Répartiteur de freinage actif', 'Le freinage s’adapte à chaque virage.', { brake: 0.8, tyre: 0.3 }, 'c4'],
    ['Lest réparti', 'Le poids est placé au mieux.', { mu: 0.8 }, 'c5'],
    ['Monocoque de nouvelle ère', 'Le châssis le plus abouti de l’écurie.', { mu: 1, steer: 0.5, brake: 0.4 }, 'c7'],
  ],
  fiab: [
    ['Faisceaux renforcés', 'Moins de pannes électriques.', { rel: 1 }, 'f1'],
    ['Cellule anti-intrusion', 'Les chocs coûtent moins de vitesse.', { wall: 1 }, 'f2'],
    ['Patins de fond en titane', 'Moins de perte dans l’herbe et le gravier.', { off: 1 }, 'f3'],
    ['Rapports renforcés', 'Des passages de rapports sûrs.', { rel: 0.8, acc: 0.2 }, 'f4'],
    ['Capteurs de surveillance', 'Les problèmes sont repérés avant la casse.', { rel: 1 }, 'f5'],
    ['Radiateurs surdimensionnés', 'Le moteur ne surchauffe plus.', { rel: 1 }, 'f1'],
    ['Ailerons renforcés', 'Les contacts abîment moins la voiture.', { wall: 1, rel: 0.3 }, 'f2'],
    ['Fond plat blindé', 'On perd moins de vitesse hors piste.', { off: 1 }, 'f3'],
    ['Nez absorbeur', 'Les chocs à l’avant font moins de dégâts.', { wall: 1 }, 'f2'],
    ['Banc d’endurance', 'Chaque pièce roule 5 000 km au banc.', { rel: 1, off: 0.3 }, 'f6'],
    ['Contrôle qualité total', 'Zéro défaut à la sortie de l’usine.', { rel: 1, wall: 0.5 }, 'f5'],
  ],
  stand: [
    ['Écrous captifs', 'Les roues se changent en un geste.', { pit: 1 }, 's1'],
    ['Couvertures pilotées', 'Des pneus prêts dès la sortie des stands.', { tyre: 1 }, 's5'],
    ['Logiciel de consommation', 'Chaque goutte est comptée.', { fuel: 1 }, 's3'],
    ['Cric à levée rapide', 'La voiture est levée en un éclair.', { pit: 1 }, 's4'],
    ['Analyse de l’usure', 'Des relais calculés au tour près.', { tyre: 1 }, 's2'],
    ['Feux de sortie automatiques', 'Plus d’hésitation au départ du box.', { pit: 1 }, 's7'],
    ['Simulateur de course', 'La stratégie est jouée mille fois avant le départ.', { tyre: 0.5, fuel: 0.5 }, 's6'],
    ['Entraînement quotidien', 'Des mécaniciens réglés comme une horloge.', { pit: 1 }, 's7'],
    ['Mélange carburant optimisé', 'Le même plein pour plus de tours.', { fuel: 1 }, 's3'],
    ['Équipe de stand d’élite', 'La meilleure équipe de stand du plateau.', { pit: 1, tyre: 0.4 }, 's9'],
    ['Ravitaillement éclair', 'Le plein en un éclair.', { pit: 0.8 }, 's8'],
  ],
};
// effet d'une pièce de poids 1 pour 40 de coût (en racine du coût ensuite)
const UNIT = { vtop: 0.011, acc: 0.03, kd: 0.055, mu: 0.018, brake: 0.06, steer: 0.07, slip: 0.35, rel: 10, wall: -0.2, off: 0.12, tyre: -0.05, fuel: -0.05, pit: -0.3 };
const LEVEL = { moteur: 1.25, aero: 1, chassis: 1.05, fiab: 0.55, stand: 0.6 };   // prix relatif des pôles

function rng(seed) { let a = seed * 2654435761 >>> 0; return () => { a = a + 0x6D2B79F5 >>> 0; let t = a; t = Math.imul(t ^ t >>> 15, t | 1); t ^= t + Math.imul(t ^ t >>> 7, t | 61); return ((t ^ t >>> 14) >>> 0) / 4294967296; }; }
const shuffle = (a, r) => { for (let k = a.length - 1; k > 0; k--) { const j = Math.floor(r() * (k + 1)); [a[k], a[j]] = [a[j], a[k]]; } return a; };

// arbres d'une ère : chaque pôle reprend la forme (prérequis, rangées) d'un autre pôle de base, parfois
// en miroir, avec de nouvelles pièces tirées de son catalogue ; ids uniques par génération (« g3m4 »)
export function eraTrees(e, base) {
  if (!e.gen) return base;
  const r = rng(e.gen * 97 + e.i), mul = ERAS[e.i].mul;
  // formes : permutation des 5 arbres de base (un catalogue doit contenir assez de pièces)
  let tpl;
  do tpl = shuffle(base.slice(), r); while (tpl.some((t, k) => t.nodes.length > POOL[base[k].id].length) || tpl.every((t, k) => t === base[k]));
  return base.map((b, k) => {
    const T = tpl[k], mirror = r() < 0.5, pick = shuffle(POOL[b.id].slice(0, -1), r).slice(0, T.nodes.length - 1).concat([POOL[b.id].at(-1)]);
    const ord = T.nodes.slice().sort((x, y) => x.at[0] - y.at[0] || x.cost - y.cost);   // la dernière pièce du catalogue au sommet
    const ids = Object.fromEntries(ord.map((n, j) => [n.id, `g${e.gen}${b.id[0]}${j + 1}`]));
    const nodes = ord.map((n, j) => {
      const [name, desc, w, art] = pick[j], cost = Math.max(6, Math.round(n.cost / LEVEL[T.id] * LEVEL[b.id] * (0.9 + r() * 0.2)));
      const fx = {};
      for (const key in w) {
        const v = UNIT[key] * w[key] * Math.sqrt(cost / LEVEL[b.id] / 40) * (mul[key] || 1);
        fx[key] = key === 'rel' ? Math.round(v) : key === 'pit' ? Math.round(v * 10) / 10 : Math.round(v * 1000) / 1000;
      }
      const o = { id: ids[n.id], at: [n.at[0], mirror ? 4 - n.at[1] : n.at[1]], name, desc, cost, fx, art };
      if (n.req) o.req = n.req.map(q => ids[q]);
      if (n.reqAny) o.reqAny = n.reqAny.map(q => ids[q]);
      return o;
    });
    return { ...b, nodes };
  });
}
