// Données du jeu : tout est fictif (écuries, pilotes, circuits).

// perf : performance de la voiture (vitesse de pointe, accélération)
// skill : niveau de chaque pilote IA (fraction de la vitesse limite qu'il ose prendre)
export const TEAMS = [
  { name: 'Écurie Martel',       color: 0xd81e1e, accent: 0xffffff, perf: 0.975, drivers: [{ name: 'Toi', player: true }, { name: 'A. Rivière', skill: 0.935 }] },
  { name: 'Volta GP',            color: 0x1e5bff, accent: 0xffd400, perf: 0.99,  drivers: [{ name: 'L. Ferrand', skill: 0.975 }, { name: 'T. Mbeki', skill: 0.955 }] },
  { name: 'Aurora Racing',       color: 0x00b89c, accent: 0x111111, perf: 0.985, drivers: [{ name: 'K. Lindqvist', skill: 0.97 }, { name: 'R. Okonkwo', skill: 0.95 }] },
  { name: 'Fulmine Corse',       color: 0xffc400, accent: 0x1a1a1a, perf: 0.98,  drivers: [{ name: 'M. Bellandi', skill: 0.965 }, { name: 'S. Varga', skill: 0.94 }] },
  { name: 'Nordwind Motorsport', color: 0xff7a00, accent: 0x0d1b3d, perf: 0.975, drivers: [{ name: 'J. Halvorsen', skill: 0.96 }, { name: 'E. Duval', skill: 0.935 }] },
  { name: 'Kestrel Engineering', color: 0x8e44ff, accent: 0xffffff, perf: 0.97,  drivers: [{ name: 'D. Okafor', skill: 0.955 }, { name: 'P. Navarro', skill: 0.93 }] },
  { name: 'Halcyon Racing',      color: 0x0f6b3a, accent: 0xe8c547, perf: 0.965, drivers: [{ name: 'Y. Tanabe', skill: 0.95 }, { name: 'C. Moreau', skill: 0.925 }] },
  { name: 'Meridia GP',          color: 0xff4fa3, accent: 0x14306b, perf: 0.96,  drivers: [{ name: 'I. Petrov', skill: 0.945 }, { name: 'B. Achterberg', skill: 0.92 }] },
  { name: 'Atlas Velocity',      color: 0xb8bec6, accent: 0x111111, perf: 0.955, drivers: [{ name: 'G. Castellano', skill: 0.94 }, { name: 'N. Haddad', skill: 0.915 }] },
  { name: 'Tempesta Racing',     color: 0x0b2a5b, accent: 0x35e0ff, perf: 0.95,  drivers: [{ name: 'F. Lemaire', skill: 0.935 }, { name: 'O. Brandt', skill: 0.91 }] },
];

// Un circuit = des points de contrôle [x, z, altitude] en mètres (lissés en spline),
// une largeur, et un thème de décor. L'étape 2 ajoutera 9 autres circuits sur ce modèle.
export const CIRCUITS = [
  {
    id: 'valmont',
    name: 'Circuit de Valmont',
    type: 'Campagne vallonnée',
    width: 14,
    theme: {
      sky: 0x9ccbef, fog: [220, 900], grass: 0x4f8f3a, grassFar: 0x447c33, ground: 0x467f35,
      trees: 900, hills: 26, treeColor: 0x2f6a2a,
    },
    points: [
      [0, 0, 0], [300, 0, 0], [600, 0, 0], [850, 10, 1], [950, 80, 2], [940, 200, 4],
      [850, 260, 6], [700, 280, 8], [550, 330, 10], [480, 450, 12], [540, 560, 14], [650, 620, 15],
      [800, 640, 15], [1000, 640, 14], [1200, 660, 12], [1320, 740, 10], [1340, 880, 8], [1260, 980, 6],
      [1100, 1000, 5], [950, 960, 5], [820, 1000, 5], [700, 1100, 4], [560, 1150, 3], [380, 1120, 2],
      [250, 1020, 1], [200, 880, 1], [120, 760, 0], [-20, 700, 0], [-160, 620, 0], [-230, 480, 0],
      [-220, 300, 0], [-160, 130, 0], [-90, 30, 0],
    ],
  },
];
