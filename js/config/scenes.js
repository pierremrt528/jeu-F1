// Cinématiques de l'atelier 3D du QG : plans clés de caméra (t en s, pos et look [x, y, z], l = lumière 0-1).
// La voiture est au centre (plateau tournant de 3,7 m de rayon), ton pilote debout à droite (x ≈ 3,9, z ≈ 2,3).
// actors : personnages (role : 'me' = ton pilote, 'guest' = le pilote ou le chef recruté, 'agent' = son agent),
// pose ('stand' ou 'shake'), on : [début, fin] d'apparition, path : [[t, x, z, angle], …] (angle 0 = face à la caméra).
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
  // rendez-vous au bureau du QG : le pilote et son agent traversent l'atelier
  meet: {
    dur: 5.4,
    keys: [
      { t: 0, pos: [3.2, 1.8, 12.5], look: [-3, 1, -2], l: 0.9 },
      { t: 2.8, pos: [1.6, 1.6, 10.5], look: [-2.2, 1.1, 2.5], l: 1 },
      { t: 5.4, pos: [-0.2, 1.55, 8.4], look: [-0.9, 1.3, 4.7], l: 1 },
    ],
    actors: [
      { role: 'guest', pose: 'stand', on: [0, 99], path: [[0, -7.2, -9, 0.15], [2.4, -5.8, 0.8, 0.3], [4.4, -1.4, 4.7, 0.25]] },
      { role: 'agent', pose: 'stand', on: [0, 99], path: [[0.3, -8.2, -9.6, 0.15], [2.8, -6.6, 1.4, 0.35], [4.8, -0.4, 5.0, -0.05]] },
    ],
  },
  // signature : poignée de main entre ton pilote et la recrue, puis photo officielle devant la voiture
  sign: {
    dur: 6.2, flash: 3,
    keys: [
      { t: 0, pos: [2.95, 1.55, 9.4], look: [2.95, 1.15, 4.6], l: 1 },
      { t: 2.98, pos: [2.95, 1.45, 8.4], look: [2.95, 1.15, 4.6], l: 1 },
      { t: 3, pos: [0, 1.25, 10.5], look: [0, 1.0, 3], l: 1.15 },
      { t: 6.2, pos: [0, 1.2, 9.6], look: [0, 1.0, 3], l: 1.15 },
    ],
    actors: [
      { role: 'me', pose: 'shake', on: [0, 3], path: [[0, 2.4, 4.6, Math.PI / 2]] },
      { role: 'guest', pose: 'shake', on: [0, 3], path: [[0, 3.5, 4.6, -Math.PI / 2]] },
      { role: 'me', pose: 'stand', on: [3, 99], path: [[3, -0.75, 4.6, 0.12]] },
      { role: 'guest', pose: 'stand', on: [3, 99], path: [[3, 0.75, 4.6, -0.12]] },
    ],
  },
};
