// Atelier 3D derrière les écrans de gestion (QG, réglages) : la voiture de l'écurie tourne sur un plateau,
// sol miroir, rampes lumineuses, nom de l'écurie au mur. Scène séparée (≈ 11 appels de rendu), dessinée
// seulement quand un de ces écrans est ouvert. frame() place la voiture dans une zone de l'écran.
import * as THREE from '../lib/three.module.min.js';
import { buildCarMesh } from './carmodel.js';
import { TEAMS } from './data.js';
import { onTheme } from './theme.js';
import { logoImage } from './logo.js';

let W = null;
const hex = c => '#' + c.toString(16).padStart(6, '0');
const canvas = (w, h = w) => { const c = document.createElement('canvas'); c.width = w; c.height = h; return [c, c.getContext('2d')]; };
const tex = c => { const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 4; return t; };

// sol : résine sombre, dalles, plus clair sous la voiture
function floorTex() {
  const S = 1024, [cv, g] = canvas(S), c = S / 2;
  const gr = g.createRadialGradient(c, c, 0, c, c, c);
  gr.addColorStop(0, '#30353e'); gr.addColorStop(0.18, '#1f232a'); gr.addColorStop(0.55, '#111419'); gr.addColorStop(1, '#07090c');
  g.fillStyle = gr; g.fillRect(0, 0, S, S);
  g.strokeStyle = 'rgba(255,255,255,.022)'; g.lineWidth = 2;
  for (let k = 0; k <= S; k += S / 24) { g.beginPath(); g.moveTo(k, 0); g.lineTo(k, S); g.moveTo(0, k); g.lineTo(S, k); g.stroke(); }
  return tex(cv);
}
// plateau tournant : métal brossé, anneaux concentriques, repères
function tableTex(acc) {
  const S = 512, [cv, g] = canvas(S), c = S / 2;
  const gr = g.createRadialGradient(c, c, 0, c, c, c);
  gr.addColorStop(0, '#3a3f48'); gr.addColorStop(0.7, '#262a31'); gr.addColorStop(1, '#1a1d22');
  g.fillStyle = gr; g.beginPath(); g.arc(c, c, c, 0, 7); g.fill();
  for (let r = 20; r < c - 6; r += 6) { g.strokeStyle = `rgba(255,255,255,${0.025 + 0.02 * Math.random()})`; g.lineWidth = 1.5; g.beginPath(); g.arc(c, c, r, 0, 7); g.stroke(); }
  g.strokeStyle = 'rgba(255,255,255,.25)'; g.lineWidth = 3;
  for (let k = 0; k < 48; k++) { const a = k / 48 * Math.PI * 2, r1 = c - (k % 4 ? 14 : 26); g.beginPath(); g.moveTo(c + Math.cos(a) * r1, c + Math.sin(a) * r1); g.lineTo(c + Math.cos(a) * (c - 6), c + Math.sin(a) * (c - 6)); g.stroke(); }
  g.strokeStyle = acc; g.lineWidth = 5; g.beginPath(); g.arc(c, c, c - 3, 0, 7); g.stroke();
  return tex(cv);
}
// halo coloré autour du plateau (anneau, mélange additif)
function glowTex(acc) {
  const S = 256, [cv, g] = canvas(S), c = S / 2;
  const gr = g.createRadialGradient(c, c, c * 0.8, c, c, c);
  gr.addColorStop(0, 'rgba(0,0,0,0)'); gr.addColorStop(0.08, acc); gr.addColorStop(0.35, acc + '55'); gr.addColorStop(1, 'rgba(0,0,0,0)');
  g.fillStyle = gr; g.fillRect(0, 0, S, S);
  return tex(cv);
}
// fond : mur sombre, rampes verticales, plinthe lumineuse et nom de l'écurie (mur de 16 m : y monde = 14,5 − 16 × v)
function wallTex(name, acc, logo, abbr) {
  const [cv, g] = canvas(2048, 512), Y = y => (14.5 - y) / 16 * 512;
  const gr = g.createLinearGradient(0, 0, 0, 512);
  gr.addColorStop(0, '#040507'); gr.addColorStop(0.5, '#0d1016'); gr.addColorStop(0.86, '#141820'); gr.addColorStop(1, '#07090c');
  g.fillStyle = gr; g.fillRect(0, 0, 2048, 512);
  for (let x = 0; x < 2048; x += 64) { g.fillStyle = 'rgba(255,255,255,.018)'; g.fillRect(x, Y(9), 2, Y(0) - Y(9)); }
  // rampes verticales de part et d'autre du nom
  for (let k = 0; k < 12; k++) {
    const x = 120 + k * 164; if (Math.abs(x - 1024) < 380) continue;
    const lg = g.createLinearGradient(x - 14, 0, x + 14, 0);
    lg.addColorStop(0, 'rgba(170,200,255,0)'); lg.addColorStop(0.5, 'rgba(170,200,255,.22)'); lg.addColorStop(1, 'rgba(170,200,255,0)');
    g.fillStyle = lg; g.fillRect(x - 14, Y(6.2), 28, Y(0.9) - Y(6.2));
    g.fillStyle = 'rgba(225,235,255,.95)'; g.fillRect(x - 1.5, Y(6.2), 3, Y(0.9) - Y(6.2));
  }
  // plinthe lumineuse
  const lg = g.createLinearGradient(0, 0, 2048, 0);
  lg.addColorStop(0, 'rgba(160,190,255,0)'); lg.addColorStop(0.5, 'rgba(210,225,255,.7)'); lg.addColorStop(1, 'rgba(160,190,255,0)');
  g.fillStyle = lg; g.fillRect(0, Y(0.4), 2048, 3);
  // nom de l'écurie
  g.font = 'italic 900 76px "Roboto Condensed", "Arial Narrow", "DIN Alternate", sans-serif';
  g.textAlign = 'center'; g.textBaseline = 'middle';
  g.fillStyle = 'rgba(255,255,255,.1)'; g.fillText(name.toUpperCase(), 1024, Y(3.9));
  g.fillStyle = acc; g.fillRect(1024 - 70, Y(2.3), 140, 5);
  const t = tex(cv);
  // logo et abréviation au-dessus du nom (image SVG chargée à part)
  if (logo) logoImage(logo, img => {
    g.globalAlpha = 0.9; g.drawImage(img, 1024 - 50, Y(9.6), 100, 100); g.globalAlpha = 1;
    if (abbr) { g.font = 'italic 900 34px "Roboto Condensed", "Arial Narrow", sans-serif'; g.fillStyle = 'rgba(255,255,255,.28)'; g.fillText(abbr, 1024, Y(5.7)); }
    t.needsUpdate = true;
  }); t.wrapS = THREE.RepeatWrapping; t.repeat.x = -1;   // vu de l'intérieur du cylindre
  return t;
}

function build(renderer) {
  const t0 = TEAMS[0], acc = getComputedStyle(document.documentElement).getPropertyValue('--acc').trim() || hex(t0.color);
  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0x06080b);
  scene.fog = new THREE.Fog(0x06080b, 30, 75);
  const camera = new THREE.PerspectiveCamera(30, 1, 0.5, 200);

  // reflets : studio avec boîtes à lumière (au-dessus, sur les côtés) et une touche de la couleur de l'écurie
  const env = new THREE.Scene();
  env.add(new THREE.Mesh(new THREE.SphereGeometry(30, 24, 12), new THREE.MeshBasicMaterial({ color: 0x0b0d12, side: THREE.BackSide })));
  const panel = (w, h, pos, col, k) => {
    const m = new THREE.Mesh(new THREE.PlaneGeometry(w, h), new THREE.MeshBasicMaterial({ color: new THREE.Color(col).multiplyScalar(k), side: THREE.DoubleSide }));
    m.position.set(...pos); m.lookAt(0, 0.5, 0); env.add(m);
  };
  panel(14, 5, [0, 12, 0], 0xffffff, 3.2);
  panel(2, 10, [-12, 4, 4], 0xdfe8ff, 2.4); panel(2, 10, [12, 4, -4], 0xdfe8ff, 2.4);
  panel(10, 2, [0, 3, -14], t0.color, 1.6); panel(16, 1, [0, 1, 14], 0xffe2c0, 1.2);
  const pm = new THREE.PMREMGenerator(renderer);
  scene.environment = pm.fromScene(env, 0.035).texture;
  pm.dispose();

  scene.add(new THREE.HemisphereLight(0xcfd8ff, 0x101216, 0.5));
  const key = new THREE.DirectionalLight(0xffffff, 1.6); key.position.set(-6, 9, 8); scene.add(key);
  const rim = new THREE.DirectionalLight(0xbcd0ff, 2.2); rim.position.set(5, 6, -9); scene.add(rim);

  // sol miroir : la voiture est dupliquée à l'envers sous un sol semi-transparent
  const floor = new THREE.Mesh(new THREE.RingGeometry(3.7, 34, 96, 1), new THREE.MeshStandardMaterial({ map: floorTex(), roughness: 0.42, metalness: 0.1, envMapIntensity: 0.35, transparent: true, opacity: 0.8, depthWrite: false }));
  floor.rotation.x = -Math.PI / 2; floor.renderOrder = 1; scene.add(floor);   // percé sous le plateau
  const spin = new THREE.Group(); scene.add(spin);
  const table = new THREE.Mesh(new THREE.CircleGeometry(3.7, 72), new THREE.MeshStandardMaterial({ map: tableTex(acc), roughness: 0.38, metalness: 0.55, transparent: true, opacity: 0.72, depthWrite: false }));
  table.rotation.x = -Math.PI / 2; table.position.y = 0.012; table.renderOrder = 2; spin.add(table);
  const glow = new THREE.Mesh(new THREE.RingGeometry(3.66, 4.9, 96), new THREE.MeshBasicMaterial({ map: glowTex(acc), transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, toneMapped: false }));
  glow.rotation.x = -Math.PI / 2; glow.position.y = 0.014; glow.renderOrder = 3; scene.add(glow);
  const car = buildCarMesh(t0.color, t0.accent, t0.helmet || 0xffc21a); spin.add(car);
  const mirror = buildCarMesh(t0.color, t0.accent, t0.helmet || 0xffc21a);
  mirror.remove(mirror.children[2]); mirror.scale.y = -1; spin.add(mirror);

  // mur du fond
  const wall = new THREE.Mesh(new THREE.CylinderGeometry(26, 26, 16, 48, 1, true, Math.PI * 0.55, Math.PI * 0.9), new THREE.MeshBasicMaterial({ map: wallTex(t0.name, acc, t0.logo, t0.abbr), side: THREE.BackSide }));
  wall.position.y = 6.5; scene.add(wall);

  const view = { d: 14, cx: 0.7, cy: 0.5, k: 0 };
  return { scene, camera, spin, view, rot: rot0 };
}

// zone de l'écran (fractions 0-1) où la voiture doit apparaître ; transition douce entre deux cadrages
let box = [0.45, 1, 0.1, 0.9];
// couleurs, nom ou logo changés : la scène est reconstruite au prochain affichage (même angle du plateau)
let rot0 = 0.6;
function refreshRoom() {
  if (!W) return;
  rot0 = W.rot;
  W.scene.traverse(o => { o.geometry?.dispose(); [].concat(o.material || []).forEach(m => { m.map?.dispose(); m.dispose(); }); });
  W.scene.environment?.dispose();
  W = null;
}
onTheme(refreshRoom);
export function frame(x0, x1, y0, y1) { box = [x0, x1, y0, y1]; }

export function renderRoom(renderer, dt) {
  if (!W) W = build(renderer);
  const { scene, camera, spin, view } = W;
  const v = renderer.getSize(new THREE.Vector2()), aspect = v.x / Math.max(1, v.y);
  if (camera.aspect !== aspect) camera.aspect = aspect;
  // distance pour que la voiture (sphère de 3,3 m) tienne dans la zone demandée
  const [x0, x1, y0, y1] = box;
  const fit = Math.min(y1 - y0, (x1 - x0) * aspect), dist = 3.3 / (fit * Math.tan(THREE.MathUtils.degToRad(camera.fov / 2)));
  const k = view.k ? 1 - Math.exp(-dt * 4) : 1; view.k = 1;
  view.d += (dist - view.d) * k; view.cx += ((x0 + x1) / 2 - view.cx) * k; view.cy += ((y0 + y1) / 2 - view.cy) * k;
  const el = THREE.MathUtils.degToRad(11);
  camera.position.set(0, 0.55 + Math.sin(el) * view.d, Math.cos(el) * view.d);
  camera.lookAt(0, 0.55, 0);
  camera.far = view.d + 60;
  scene.fog.near = Math.max(30, view.d + 10); scene.fog.far = Math.max(75, view.d + 55);   // petite zone = caméra plus loin : le brouillard recule aussi
  camera.setViewOffset(v.x, v.y, (0.5 - view.cx) * v.x, (0.5 - view.cy) * v.y, v.x, v.y);
  camera.updateProjectionMatrix();
  W.rot += dt * 0.2; spin.rotation.y = W.rot;
  renderer.render(scene, camera);
}
