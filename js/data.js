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

// Thèmes de décor (ciel, sol, végétation, relief à l'horizon, ville, mer)
const MOUNT = { far: [160, 520, 0x9db3c2, 0xb9cad6], near: [60, 260, 0x6f8a78, 0x8ea596] };
export const THEMES = {
  campagne: { horizon: 0xc6d9e6, zenith: 0x3b78c9, sun: [0.5, 0.45, 0.55], fog: [250, 1500], surface: 'grass', trees: 1100, treeKind: 'mix', mountains: MOUNT },
  plaine:   { horizon: 0xd3e2ec, zenith: 0x4a86d4, sun: [-0.4, 0.5, 0.6], fog: [300, 1700], surface: 'grass', trees: 380, treeKind: 'mix', mountains: { far: [40, 140, 0xa9bccb, 0xbfcfdb], near: [10, 50, 0x7d9a7c, 0x93ad8f] } },
  foret:    { horizon: 0xc2d4dc, zenith: 0x3d74b8, sun: [0.3, 0.5, -0.6], fog: [160, 1100], surface: 'grass', trees: 2600, treeKind: 'pine', mountains: { far: [200, 600, 0x8ea6b4, 0xaebfcb], near: [100, 340, 0x4f6b55, 0x6a8570] } },
  alpin:    { horizon: 0xd9e6f0, zenith: 0x2c6cc6, sun: [0.6, 0.55, 0.3], fog: [260, 1700], surface: 'grass', trees: 1300, treeKind: 'pine', mountains: { far: [550, 1150, 0x8a9fb5, 0xf4f7fa], near: [220, 520, 0x5d7464, 0x93a596] } },
  automne:  { horizon: 0xe2dccd, zenith: 0x4a7cc0, sun: [-0.5, 0.4, 0.5], fog: [220, 1400], surface: 'grass', trees: 1500, treeKind: 'mix', treeHue: [0.02, 0.14], mountains: { far: [380, 820, 0x9aa5b4, 0xe9edf2], near: [140, 380, 0x7b6d55, 0x9a8b6d] } },
  desert:   { horizon: 0xf0dcc0, zenith: 0x4f8fd8, sun: [0.4, 0.6, 0.5], fog: [320, 1900], surface: 'sand', trees: 90, treeKind: 'palm', mountains: { far: [140, 420, 0xd9b58f, 0xe8cfb0], near: [40, 160, 0xc9955f, 0xd9ac7a] } },
  cote:     { horizon: 0xd4e6f2, zenith: 0x3a86d6, sun: [0.5, 0.4, -0.6], fog: [280, 1700], surface: 'grass', trees: 650, treeKind: 'palmmix', sea: true, mountains: { far: [120, 380, 0xa6b9c6, 0xc0d0dc], near: [50, 180, 0x7c9478, 0x95ab90] } },
  ville:    { horizon: 0xcfd8e0, zenith: 0x4b7fbf, sun: [0.55, 0.4, 0.45], fog: [220, 1300], surface: 'concrete', trees: 140, treeKind: 'mix', city: { count: 420, hmin: 18, hmax: 140 }, mountains: { far: [60, 200, 0xaab6c2, 0xc2ccd6], near: null } },
  port:     { horizon: 0xd6e4ee, zenith: 0x3f84d0, sun: [0.45, 0.45, -0.55], fog: [220, 1400], surface: 'concrete', trees: 220, treeKind: 'palm', city: { count: 260, hmin: 10, hmax: 45 }, sea: true, mountains: { far: [200, 520, 0x9fb2c0, 0xbfcdd8], near: null } },
};

// Un circuit = soit des points de contrôle [x, z, altitude] en mètres, soit un tracé « layout »
// (S = ligne droite, L/R = virage à gauche/droite : rayon, angle) avec un profil d'altitude « elev ».
export const CIRCUITS = [
  {
    id: 'valmont', name: 'Circuit de Valmont', type: 'Campagne vallonnée', width: 14, theme: THEMES.campagne,
    points: [
      [0, 0, 0], [300, 0, 0], [600, 0, 0], [850, 10, 1], [950, 80, 2], [940, 200, 4],
      [850, 260, 6], [700, 280, 8], [550, 330, 10], [480, 450, 12], [540, 560, 14], [650, 620, 15],
      [800, 640, 15], [1000, 640, 14], [1200, 660, 12], [1320, 740, 10], [1340, 880, 8], [1260, 980, 6],
      [1100, 1000, 5], [950, 960, 5], [820, 1000, 5], [700, 1100, 4], [560, 1150, 3], [380, 1120, 2],
      [250, 1020, 1], [200, 880, 1], [120, 760, 0], [-20, 700, 0], [-160, 620, 0], [-230, 480, 0],
      [-220, 300, 0], [-160, 130, 0], [-90, 30, 0],
    ],
  },
  {
    id: 'portazur', name: 'Port-Azur', type: 'Urbain · bord de mer', width: 12, runoff: 3.5, theme: THEMES.port,
    layout: [['S',900],['R',30,90],['S',465],['R',25,90],['S',205],['L',25,90],['S',225],['R',30,90],['S',450],['R',25,90],['S',375],['L',30,90],['S',150],['R',30,90],['S',300],['R',40,90]],
    elev: [[0, 0], [0.3, 3], [0.55, 9], [0.8, 4]],
  },
  {
    id: 'sierra', name: 'Sierra Alta', type: 'Montagne · fort dénivelé', width: 13, runoff: 7, theme: THEMES.alpin,
    layout: [['S',400],['R',50,90],['S',350],['L',38,180],['S',305],['R',38,180],['S',350],['L',45,90],['S',200],['L',60,90],['S',700],['L',80,90],['S',807],['L',70,90],['S',230],['L',50,90]],
    elev: [[0, 0], [0.09, 3], [0.17, 28], [0.25, 54], [0.33, 80], [0.43, 94], [0.55, 86], [0.7, 52], [0.85, 18], [0.95, 3]],
  },
  {
    id: 'kerlan', name: 'Forêt de Kerlan', type: 'Très sinueux · forêt', width: 13, runoff: 7, theme: THEMES.foret,
    layout: [['S',300],['R',40,90],['S',80],['L',45,54],['S',120],['R',30,120],['S',60],['L',50,84],['S',100],['R',55,70],['S',70],['L',35,34],['S',150],['R',25,150],['S',90],['L',45,114],['S',180],['R',40,80],['S',60],['L',40,44],['S',321],['R',60,60],['S',50],['L',30,24],['S',90],['R',35,110],['S',80],['L',45,74],['S',120],['R',50,75],['S',60],['L',40,39],['S',100],['R',30,95],['S',90],['L',55,59],['S',150],['R',70,66],['S',60],['L',50,30]],
    elev: [[0, 0], [0.2, 12], [0.4, 4], [0.6, 18], [0.8, 7]],
  },
  {
    id: 'sahrane', name: 'Dunes de Sahrane', type: 'Rapide · désert', width: 15, runoff: 12, theme: THEMES.desert,
    layout: [['S',1253],['R',45,120],['S',551],['L',25,50],['R',25,100],['L',25,50],['S',600],['R',60,120],['S',450],['R',30,40],['L',30,80],['R',30,40],['S',700],['R',45,120]],
    elev: [[0, 0], [0.5, 2]],
  },
  {
    id: 'veldmar', name: 'Polder de Veldmar', type: 'Plat · campagne', width: 14, theme: THEMES.plaine,
    layout: [['S',700],['R',70,100],['S',530],['R',40,80],['S',534],['L',80,90],['S',200],['R',35,120],['S',400],['R',90,60],['S',500],['R',50,90],['S',250],['L',60,90],['R',60,90]],
    elev: [[0, 0]],
  },
  {
    id: 'neokyara', name: 'Neo-Kyara', type: 'Urbain · gratte-ciel', width: 13, runoff: 3.5, theme: THEMES.ville,
    layout: [['S',735],['R',20,90],['S',1095],['R',20,90],['S',250],['L',20,90],['S',400],['R',25,180],['S',400],['L',20,90],['S',150],['R',30,90],['S',650],['L',25,90],['S',200],['R',30,90],['S',360],['R',40,90]],
    elev: [[0, 0], [0.4, 5], [0.7, 2]],
  },
  {
    id: 'solenza', name: 'Cap Solenza', type: 'Côte · courbes rapides', width: 14, runoff: 10, theme: THEMES.cote,
    layout: [['S',391],['R',120,60],['S',200],['R',80,70],['L',90,50],['S',300],['R',50,100],['S',400],['R',150,45],['S',424],['R',40,85],['S',250],['L',70,60],['R',70,110]],
    elev: [[0, 0], [0.25, 10], [0.5, 22], [0.75, 8]],
  },
  {
    id: 'varenne', name: 'Crêtes de Varenne', type: 'Montagne · sinueux', width: 12, runoff: 6, theme: THEMES.automne,
    layout: [['S',350],['R',30,120],['L',40,90],['S',150],['R',45,100],['S',120],['L',30,140],['R',50,90],['S',250],['R',35,110],['S',367],['L',40,60],['R',40,60],['S',200],['R',60,90],['S',541],['R',45,80]],
    elev: [[0, 0], [0.2, 26], [0.4, 58], [0.55, 68], [0.75, 34], [0.9, 9]],
  },
  {
    id: 'brekka', name: 'Anneau de Brekka', type: 'Très rapide · lignes droites', width: 15, runoff: 12, theme: THEMES.plaine,
    layout: [['S',1171],['R',150,90],['S',200],['R',150,90],['S',500],['L',25,45],['R',25,90],['L',25,45],['S',600],['R',150,90],['S',200],['R',150,90]],
    elev: [[0, 0]],
  },
];
