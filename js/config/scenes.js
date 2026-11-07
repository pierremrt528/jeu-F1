// Cinématiques de l'atelier 3D du QG : plans clés de caméra (t en s, pos et look [x, y, z], l = lumière 0-1)
// et textes. La voiture est au centre (plateau tournant), le pilote debout à droite (x ≈ 3,9, z ≈ 2,3).
// Pour enrichir : ajouter des plans, ou de nouvelles scènes jouées par startScene(nom, …) dans cine.js.
export default {
  qg: {
    dur: 9,
    keys: [
      { t: 0, pos: [0, 1.4, 30], look: [0, 1.2, 0], l: 0.05 },
      { t: 2.2, pos: [0, 1.0, 13], look: [0, 0.7, 0], l: 0.7 },
      { t: 4.4, pos: [-7.5, 1.3, 3.5], look: [0, 0.5, 0], l: 1 },
      { t: 6.4, pos: [-2.5, 2.6, 9.5], look: [1.2, 0.8, 0.6], l: 1 },
      { t: 9, pos: [2.4, 1.5, 7.4], look: [2.8, 1.0, 1.4], l: 1 },
    ],
  },
};
