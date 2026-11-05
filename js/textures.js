// Textures générées au lancement (canvas) : aucun fichier image à télécharger.
import * as THREE from '../lib/three.module.min.js';
import { mulberry } from './track.js';

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
export const asphalt = () => make(256, 512, (c, w, h) => {
  const r = mulberry(3);
  c.fillStyle = '#56585c'; c.fillRect(0, 0, w, h);
  noise(c, w, h, 26000, ['#4a4c50', '#5f6166', '#505256', '#66686c', '#45474b'], 1, r);
  noise(c, w, h, 2500, ['#3e4044', '#6d6f73'], 2, r);
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

export const grass = () => make(256, 256, (c, w, h) => {
  const r = mulberry(5);
  c.fillStyle = '#5d8f3c'; c.fillRect(0, 0, w, h);
  noise(c, w, h, 30000, ['#4f8034', '#679a44', '#578a39', '#71a34b', '#4a7a30'], 1, r);
  noise(c, w, h, 800, ['#7a8f45', '#3f6e2a'], 3, r);
});

export const gravel = () => make(256, 256, (c, w, h) => {
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

// Grillage (transparence par alphaTest)
export const fence = () => make(64, 64, (c, w, h) => {
  c.clearRect(0, 0, w, h); c.strokeStyle = 'rgba(200,205,210,1)'; c.lineWidth = 2;
  c.beginPath(); c.moveTo(0, 0); c.lineTo(w, h); c.moveTo(w, 0); c.lineTo(0, h); c.stroke();
  c.fillStyle = '#9aa0a6'; c.fillRect(0, 0, w, 3);
});

// Public des tribunes
export const crowd = () => make(512, 256, (c, w, h) => {
  const r = mulberry(11);
  c.fillStyle = '#2a2d33'; c.fillRect(0, 0, w, h);
  const cols = ['#d33', '#36c', '#eee', '#fc3', '#2a2', '#f80', '#a3d', '#333', '#f6c', '#1bb', '#c96', '#888'];
  for (let row = 0; row < 16; row++) {
    c.fillStyle = 'rgba(255,255,255,.08)'; c.fillRect(0, row * 16 + 13, w, 3);
    for (let x = 0; x < w; x += 6) {
      if (r() < 0.12) continue;
      c.fillStyle = cols[(r() * cols.length) | 0]; c.fillRect(x + r() * 2, row * 16 + 5, 4, 8);
      c.fillStyle = r() < 0.5 ? '#e0b48f' : '#8a5a3c'; c.fillRect(x + 1 + r(), row * 16 + 1, 3, 4);
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
