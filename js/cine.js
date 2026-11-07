// Mini cinématiques en 3D, sans coût de rendu supplémentaire (seule la caméra bouge) : survol du
// circuit avant le départ (bandes noires, titre du Grand Prix), tour d'honneur autour de la voiture
// après un podium. Touche l'écran pour passer. Le sacre du champion est une carte animée (champCard).
import * as THREE from '../lib/three.module.min.js';

const ease = t => t * t * (3 - 2 * t);
const lerp = (a, b, t) => a + (b - a) * t;
const w = new THREE.Vector3();
let C = null;   // cinématique en cours
export const cineKind = () => C?.kind;
export const cineT = () => C ? (performance.now() - C.t0) / 1000 : 0;

// point de la piste i, décalé latéralement de lat (m, + = à droite) et en hauteur de h (m)
function at(track, i, lat, h, out) {
  const N = track.N; i = ((Math.round(i) % N) + N) % N;
  return out.set(track.X[i] - track.TZ[i] * lat, track.Y[i] + h, track.Z[i] + track.TX[i] * lat);
}
const COL = ['#ff3d2e', '#ffc83d', '#22d3c5', '#ffffff', '#3aa0ff', '#3ddc84'];
export const confetti = (n = 46) => `<div class="conf">${Array.from({ length: n }, (_, i) => `<i style="--x:${(i * 37) % 100}%;--k:${COL[i % COL.length]};--d:${(0.3 + (i % 9) * 0.12).toFixed(2)}s;--t:${(1.8 + (i % 5) * 0.3).toFixed(2)}s;--r:${(i % 2 ? 1 : -1) * (360 + (i % 4) * 160)}deg"></i>`).join('')}</div>`;

function open(kind, info, extra = '') {
  const e = document.getElementById('cine');
  e.innerHTML = `<div class="lb"></div><div class="lb b"></div>${extra}<div class="ctitle ${kind}"><small>${info.top}</small><b>${info.name}</b><span>${info.sub}</span></div><i class="cskip">Touche pour passer ⏩</i>`;
  e.className = 'cine ' + kind;
  document.body.classList.add('cine');
  e.onpointerdown = () => end(true);
}
// fin de la cinématique ; run = appeler la suite (sinon on l'interrompt simplement)
export function end(run) {
  if (!C) return;
  const f = C.onEnd; C = null;
  const e = document.getElementById('cine');
  e.className = 'cine hidden'; e.innerHTML = ''; document.body.classList.remove('cine');
  if (run) f?.();
}
// survol avant le départ : la course est figée pendant ce temps (info : top, name, sub)
export function startIntro(R, track, info, onEnd) {
  C = { kind: 'intro', t0: performance.now(), dur: 6, R, track, onEnd };
  open('intro', info);
}
// tour d'honneur après un podium (la course continue derrière)
export function startOutro(R, info, onEnd) {
  C = { kind: 'outro', t0: performance.now(), dur: 4.6, R, onEnd, a0: R.player.heading + Math.PI * 0.8 };
  open('outro', info, info.confetti ? confetti() : '');
}

// scène de l'atelier 3D (caméra gérée par showroom.js, plans dans config/scenes.js) : arrivée au QG…
export function startScene(kind, info, onEnd) {
  C = { kind, t0: performance.now(), onEnd };
  open(kind, info);
}
// place la caméra pendant une cinématique ; false s'il n'y en a pas (temps réel : reste calé sur les animations CSS)
export function cineCamera(camera) {
  if (!C) return false;
  const k = (performance.now() - C.t0) / 1000, p = C.R.player;
  if (C.kind === 'intro') {
    const { track } = C, N = track.N;
    if (k < 2.2) {   // plan 1 : drone au-dessus de la ligne droite, la grille au loin
      const u = ease(k / 2.2);
      at(track, 80 - 55 * u, lerp(42, 20, u), lerp(46, 22, u), camera.position);
      camera.lookAt(at(track, N - 28, 0, 0, w));
    } else if (k < 4.1) {   // plan 2 : travelling au ras du sol le long de la grille
      const u = (k - 2.2) / 1.9;
      at(track, N - 1 - u * 64, -8.5, 1.3, camera.position);
      camera.lookAt(at(track, N + 3 - u * 64, 3, 0.5, w));
    } else {   // plan 3 : grue qui descend derrière ta voiture jusqu'à la caméra de course
      const u = ease(Math.min(1, (k - 4.1) / 1.7)), fx = Math.sin(p.heading), fz = Math.cos(p.heading);
      const back = lerp(26, 6.8, u), up = lerp(15, 2.3, u);
      camera.position.set(p.x - fx * back, p.y + up, p.z - fz * back);
      camera.lookAt(p.x + fx * 6, p.y + 0.9, p.z + fz * 6);
    }
  } else {   // tour d'honneur : la caméra tourne autour de la voiture
    const a = C.a0 + k * 0.75, r = 8.5 - Math.min(2.2, k * 0.5);
    camera.position.set(p.x + Math.sin(a) * r, p.y + 2.8 + Math.sin(k * 0.9) * 0.5, p.z + Math.cos(a) * r);   // un peu haut : au-dessus des voitures voisines
    camera.lookAt(p.x, p.y + 1.3, p.z);   // voiture un peu basse dans l'image, le titre au-dessus
  }
  if (k >= C.dur) end(true);
  return true;
}

// carte du sacre en fin de saison (titres : pilotes et/ou écuries)
export function champCard(el, d, onDone) {
  el.innerHTML = `<div class="ncard champ in">${confetti(60)}<div class="trophy">🏆</div><small>Saison ${d.year}</small>
    ${d.titles.map(t => `<b>${t}</b>`).join('')}<p>${d.line}</p><button class="main" id="chOk">Continuer</button></div>`;
  el.classList.remove('hidden');
  el.querySelector('#chOk').onclick = () => { el.classList.add('hidden'); el.innerHTML = ''; onDone(); };
}
