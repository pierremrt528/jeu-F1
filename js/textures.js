// Textures : photos libres CC0 (Poly Haven, dossier assets/) composées en canvas,
// avec repli sur des textures générées si les images ne se chargent pas.
import * as THREE from '../lib/three.module.min.js';
import { mulberry } from './track.js';

const PHOTOS = ['asphalte', 'asphalte-normal', 'herbe', 'sable', 'gravier', 'nuages', 'nuages-alpha'];
const IMG = {};
await Promise.race([
  Promise.all(PHOTOS.map(n => new Promise(ok => {
    const im = new Image(); im.onload = () => { IMG[n] = im; ok(); }; im.onerror = ok; im.src = `assets/${n}.jpg`;
  }))),
  new Promise(ok => setTimeout(ok, 8000)),
]);
const tile = (c, w, h, im, nx, ny = nx) => { for (let y = 0; y < ny; y++) for (let x = 0; x < nx; x++) c.drawImage(im, x * w / nx, y * h / ny, w / nx + 0.5, h / ny + 0.5); };
const photo = (n, nx = 1, size = 512) => make(size, size, (c, w, h) => tile(c, w, h, IMG[n], nx));
// image brute (cartes de normales, masques) : pas de conversion sRGB
function raw(im, rx = 1, ry = rx) {
  const t = new THREE.Texture(im); t.needsUpdate = true; t.anisotropy = ANISO;
  t.wrapS = t.wrapT = THREE.RepeatWrapping; t.repeat.set(rx, ry); return t;
}
export const asphaltNormal = () => IMG['asphalte-normal'] ? raw(IMG['asphalte-normal'], 4, 4) : null;
// couche de nuages (photo de ciel CC0 « kloppenheim 06 ») : teinte + masque d'opacité
export function clouds() {
  if (!IMG.nuages || !IMG['nuages-alpha']) return null;
  const map = raw(IMG.nuages); map.colorSpace = THREE.SRGBColorSpace; map.wrapT = THREE.ClampToEdgeWrapping;
  const alpha = raw(IMG['nuages-alpha']); alpha.wrapT = THREE.ClampToEdgeWrapping;
  return { map, alpha };
}

let ANISO = 4;
export function setAniso(a) { ANISO = Math.min(8, a); }

function make(w, h, draw, repeat = true) {
  const cv = document.createElement('canvas'); cv.width = w; cv.height = h;
  draw(cv.getContext('2d'), w, h);
  const t = new THREE.CanvasTexture(cv);
  t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = ANISO;
  if (repeat) t.wrapS = t.wrapT = THREE.RepeatWrapping;
  return t;
}
function noise(c, w, h, n, colors, size = 1, rnd = Math.random) {
  for (let i = 0; i < n; i++) { c.fillStyle = colors[(rnd() * colors.length) | 0]; c.fillRect(rnd() * w, rnd() * h, size, size); }
}

// Asphalte : u = travers de la piste (avec lignes blanches de bord), v = longueur
export const asphalt = () => make(IMG.asphalte ? 1024 : 256, IMG.asphalte ? 1024 : 512, (c, w, h) => {
  const r = mulberry(3);
  if (IMG.asphalte) tile(c, w, h, IMG.asphalte, 4);
  else {
    c.fillStyle = '#56585c'; c.fillRect(0, 0, w, h);
    noise(c, w, h, 26000, ['#4a4c50', '#5f6166', '#505256', '#66686c', '#45474b'], 1, r);
    noise(c, w, h, 2500, ['#3e4044', '#6d6f73'], 2, r);
  }
  // zone gommée au centre de la piste
  const g = c.createLinearGradient(0, 0, w, 0);
  g.addColorStop(0, 'rgba(0,0,0,0)'); g.addColorStop(0.35, 'rgba(20,20,22,.18)'); g.addColorStop(0.5, 'rgba(20,20,22,.26)');
  g.addColorStop(0.65, 'rgba(20,20,22,.18)'); g.addColorStop(1, 'rgba(0,0,0,0)');
  c.fillStyle = g; c.fillRect(0, 0, w, h);
  // traces de gomme
  for (let i = 0; i < 14; i++) { c.fillStyle = 'rgba(15,15,15,.12)'; c.fillRect(w * (0.3 + r() * 0.4), r() * h, 3 + r() * 4, 60 + r() * 200); }
  // lignes blanches de bord
  c.fillStyle = '#e9e9e6'; c.fillRect(w * 0.018, 0, w * 0.022, h); c.fillRect(w * 0.96, 0, w * 0.022, h);
  noise(c, w * 0.06, h, 900, ['rgba(80,80,80,.5)'], 1, r);
});

// Vibreur : bandes rouges/blanches avec relief
export const kerb = () => make(64, 128, (c, w, h) => {
  for (let i = 0; i < 2; i++) {
    c.fillStyle = i ? '#f3f3f0' : '#c8201e'; c.fillRect(0, i * h / 2, w, h / 2);
  }
  for (let y = 0; y < h; y += 8) { c.fillStyle = 'rgba(0,0,0,.12)'; c.fillRect(0, y, w, 2); }
  const g = c.createLinearGradient(0, 0, w, 0); g.addColorStop(0, 'rgba(0,0,0,.25)'); g.addColorStop(0.3, 'rgba(0,0,0,0)'); g.addColorStop(1, 'rgba(0,0,0,.15)');
  c.fillStyle = g; c.fillRect(0, 0, w, h);
  noise(c, w, h, 600, ['rgba(0,0,0,.15)', 'rgba(255,255,255,.1)']);
});

export const grass = () => IMG.herbe ? make(512, 512, (c, w, h) => {
  // photo aérienne un peu sèche : on la ravive vers le vert
  c.drawImage(IMG.herbe, 0, 0, w, h);
  c.globalCompositeOperation = 'color'; c.fillStyle = 'rgba(70,140,40,.45)'; c.fillRect(0, 0, w, h);
  c.globalCompositeOperation = 'source-over';
}) : make(256, 256, (c, w, h) => {
  const r = mulberry(5);
  c.fillStyle = '#5d8f3c'; c.fillRect(0, 0, w, h);
  noise(c, w, h, 30000, ['#4f8034', '#679a44', '#578a39', '#71a34b', '#4a7a30'], 1, r);
  noise(c, w, h, 800, ['#7a8f45', '#3f6e2a'], 3, r);
});

export const gravel = () => IMG.gravier ? photo('gravier', 3) : make(256, 256, (c, w, h) => {
  const r = mulberry(9);
  c.fillStyle = '#b9a98a'; c.fillRect(0, 0, w, h);
  noise(c, w, h, 26000, ['#a8987a', '#cbbd9f', '#9a8b6e', '#d6c9ad', '#8e7f63'], 2, r);
});

// Panneaux publicitaires de marques inventées
const BRANDS = [
  ['VELATRIX', '#0d3b8c', '#ffffff'], ['Nordova', '#ffffff', '#c8102e'], ['KAIRO ÉNERGIE', '#ffcf00', '#111111'],
  ['Brisane', '#0b6e4f', '#ffffff'], ['ORBITEL', '#111111', '#3ee0ff'], ['Maréva', '#e4002b', '#ffffff'],
  ['TALPA PNEUS', '#1d1d1d', '#ffb000'], ['Solenne', '#6a2c91', '#ffffff'],
];
export const adBoards = () => make(2048, 128, (c, w, h) => {
  const pw = w / BRANDS.length;
  BRANDS.forEach(([name, bg, fg], i) => {
    c.fillStyle = bg; c.fillRect(i * pw, 0, pw, h);
    c.fillStyle = fg; c.font = `italic 900 ${h * 0.5}px system-ui, sans-serif`; c.textAlign = 'center'; c.textBaseline = 'middle';
    c.fillText(name, i * pw + pw / 2, h * 0.54, pw * 0.88);
    c.fillStyle = 'rgba(0,0,0,.25)'; c.fillRect(i * pw, h - 10, pw, 10); c.fillRect(i * pw, 0, 3, h);
  });
});

// Grillage (transparence par alphaTest) : mailles fines, poteau tous les 3 m, lisse en haut
export const fence = () => make(128, 128, (c, w, h) => {
  c.clearRect(0, 0, w, h); c.strokeStyle = 'rgba(190,196,202,1)'; c.lineWidth = 1.6;
  c.beginPath();
  for (let k = -w; k <= w; k += w / 6) { c.moveTo(k, 0); c.lineTo(k + h, h); c.moveTo(k + h, 0); c.lineTo(k, h); }
  c.stroke();
  c.fillStyle = '#8d949b'; c.fillRect(0, 0, w, 4); c.fillRect(0, h - 3, w, 3); c.fillRect(0, 0, 5, h);
});

// Public des tribunes : rangées de spectateurs (tête, buste aux couleurs variées) et sièges vides
export const crowd = () => make(1024, 512, (c, w, h) => {
  const r = mulberry(11), rows = 16, rh = h / rows;
  const shirts = ['#c8242b', '#1f4fa8', '#eeeeee', '#f2c230', '#2d8a3e', '#e8731c', '#20242b', '#7d8590', '#c43d7a', '#159fb0', '#f4f4f0', '#a32020', '#203a6b', '#dcd6c8'];
  const skins = ['#f1c7a3', '#e0ac85', '#c68b5f', '#8d5a3b', '#5e3b26'], hairs = ['#2a1d14', '#4a3222', '#121212', '#8a6a3a', '#bfbfbf'];
  c.fillStyle = '#3b4048'; c.fillRect(0, 0, w, h);
  for (let row = 0; row < rows; row++) {
    const y0 = row * rh;
    c.fillStyle = '#9da3aa'; c.fillRect(0, y0 + rh - 5, w, 5);   // marche en béton
    c.fillStyle = '#5a6068'; c.fillRect(0, y0 + rh - 6, w, 1);
    for (let x = 0; x < w; x += 21) {
      c.fillStyle = '#1d4f9c'; c.fillRect(x + 3, y0 + 10, 15, rh - 16);   // siège
      if (r() < 0.12) continue;
      const px = x + 10 + (r() - 0.5) * 5, sh = shirts[(r() * shirts.length) | 0];
      c.fillStyle = sh; c.beginPath(); c.roundRect(px - 8, y0 + 13, 16, rh - 17, 5); c.fill();
      c.fillStyle = 'rgba(0,0,0,.25)'; c.fillRect(px - 8, y0 + rh - 9, 16, 5);
      if (r() < 0.12) { c.fillStyle = sh; c.fillRect(px - 9, y0 - 2, 3, 15); c.fillRect(px + 6, y0 - 2, 3, 15); }   // bras levés
      c.fillStyle = skins[(r() * skins.length) | 0]; c.beginPath(); c.ellipse(px, y0 + 9, 5, 6, 0, 0, 7); c.fill();
      if (r() < 0.3) { c.fillStyle = shirts[(r() * shirts.length) | 0]; c.fillRect(px - 6, y0 + 2, 12, 4); }   // casquette
      else { c.fillStyle = hairs[(r() * hairs.length) | 0]; c.beginPath(); c.ellipse(px, y0 + 5, 5, 3, 0, Math.PI, 0); c.fill(); }
    }
  }
});

// Façade des stands
export const pitWall = () => make(256, 128, (c, w, h) => {
  c.fillStyle = '#e4e7eb'; c.fillRect(0, 0, w, h);
  c.fillStyle = '#2b3440'; c.fillRect(0, h * 0.12, w, h * 0.22);
  for (let x = 0; x < w; x += 64) { c.fillStyle = '#9aa3ad'; c.fillRect(x + 6, h * 0.45, 52, h * 0.55); c.fillStyle = 'rgba(0,0,0,.25)'; for (let y = h * 0.45; y < h; y += 6) c.fillRect(x + 6, y, 52, 1); }
});

// Flanc de pneu : bande rouge, jante sombre
export const tyreSide = () => make(128, 128, (c, w, h) => {
  const cx = w / 2;
  c.fillStyle = '#141414'; c.fillRect(0, 0, w, h);
  const ring = (r, col) => { c.fillStyle = col; c.beginPath(); c.arc(cx, cx, r, 0, 7); c.fill(); };
  ring(62, '#1b1b1b'); ring(54, '#d8141a'); ring(50, '#1b1b1b'); ring(40, '#2d2f33'); ring(36, '#17181a');
  c.strokeStyle = '#3a3c40'; c.lineWidth = 4;
  for (let k = 0; k < 10; k++) { const a = k * Math.PI / 5; c.beginPath(); c.moveTo(cx, cx); c.lineTo(cx + Math.cos(a) * 36, cx + Math.sin(a) * 36); c.stroke(); }
  ring(8, '#8b8f96');
}, false);

export const tyreTread = () => make(64, 64, (c, w, h) => {
  c.fillStyle = '#181818'; c.fillRect(0, 0, w, h);
  noise(c, w, h, 500, ['#222', '#101010', '#262626']);
});

// Ombre douce sous les voitures
export const blob = () => make(64, 128, (c, w, h) => {
  const g = c.createRadialGradient(w / 2, h / 2, 4, w / 2, h / 2, w / 2);
  g.addColorStop(0, 'rgba(0,0,0,.85)'); g.addColorStop(0.6, 'rgba(0,0,0,.55)'); g.addColorStop(1, 'rgba(0,0,0,0)');
  c.save(); c.scale(1, h / w); c.fillStyle = g; c.fillRect(0, 0, w, w); c.restore();
}, false);

// Lueur du soleil
export const sunGlow = () => make(128, 128, (c, w, h) => {
  const g = c.createRadialGradient(w / 2, h / 2, 0, w / 2, h / 2, w / 2);
  g.addColorStop(0, 'rgba(255,255,240,1)'); g.addColorStop(0.12, 'rgba(255,250,225,.95)'); g.addColorStop(0.3, 'rgba(255,240,200,.3)'); g.addColorStop(1, 'rgba(255,240,200,0)');
  c.fillStyle = g; c.fillRect(0, 0, w, h);
}, false);

export const sand = () => IMG.sable ? photo('sable') : make(256, 256, (c, w, h) => {
  const r = mulberry(13);
  c.fillStyle = '#d9b98c'; c.fillRect(0, 0, w, h);
  noise(c, w, h, 24000, ['#d0ae80', '#e2c498', '#c9a676', '#e8cda4', '#bf9c6c'], 1, r);
  for (let i = 0; i < 40; i++) { c.fillStyle = 'rgba(150,110,60,.08)'; c.fillRect(0, r() * h, w, 2 + r() * 3); }
});

export const concrete = () => make(256, 256, (c, w, h) => {
  const r = mulberry(17);
  c.fillStyle = '#9a9a96'; c.fillRect(0, 0, w, h);
  noise(c, w, h, 22000, ['#8f8f8b', '#a5a5a1', '#949490', '#adada9', '#878783'], 1, r);
  c.fillStyle = 'rgba(60,60,60,.35)'; c.fillRect(0, 0, w, 2); c.fillRect(0, 0, 2, h); c.fillRect(0, h / 2, w, 1); c.fillRect(w / 2, 0, 1, h);
});

// Façade d'immeuble (teintée par couleur de sommet) : 3 × 3 fenêtres par tuile de 10 m
export const facade = () => make(128, 128, (c, w, h) => {
  const r = mulberry(19);
  c.fillStyle = '#e4e4e4'; c.fillRect(0, 0, w, h);
  for (let y = 0; y < 3; y++) for (let x = 0; x < 3; x++) {
    const g = c.createLinearGradient(0, y * 42 + 8, 0, y * 42 + 36);
    const lit = r() < 0.15;
    g.addColorStop(0, lit ? '#f3e2a8' : '#5d7a99'); g.addColorStop(1, lit ? '#c9b47a' : '#24364a');
    c.fillStyle = g; c.fillRect(x * 42 + 7, y * 42 + 8, 30, 28);
    c.fillStyle = 'rgba(255,255,255,.18)'; c.fillRect(x * 42 + 7, y * 42 + 8, 30, 3);
  }
});
