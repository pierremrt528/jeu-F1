// Logos d'écurie : 20 emblèmes en SVG (viewBox 64 × 64), chacun en 2 couleurs de la palette.
// o : contour rempli en couleur principale ; i : motif en couleur secondaire, découpé par le contour (règle pair-impair).
// logo = { shape (indice), p, s (id de couleurs, config/couleurs.js) }
import PALETTE from './config/couleurs.js';

const disc = (r, cx = 32, cy = 32) => `M${cx} ${cy - r}a${r} ${r} 0 1 0 .01 0z`;
const shield = 'M32 4L56 12V30C56 46 45 56 32 60C19 56 8 46 8 30V12Z';
const rsq = 'M14 4h36a10 10 0 0 1 10 10v36a10 10 0 0 1-10 10H14A10 10 0 0 1 4 50V14A10 10 0 0 1 14 4z';
const checker = (x0, y0, n, c) => { let d = ''; for (let i = 0; i < n; i++) for (let j = 0; j < n; j++) if ((i + j) % 2) d += `M${x0 + i * c} ${y0 + j * c}h${c}v${c}h-${c}z`; return d; };
const blades = n => Array.from({ length: n }, (_, k) => {
  const a = k * 2 * Math.PI / n, p = (r, da) => `${(32 + r * Math.sin(a + da)).toFixed(1)} ${(32 - r * Math.cos(a + da)).toFixed(1)}`;
  return `M${p(4, -0.9)}L${p(24, -0.22)}Q${p(27, 0)} ${p(24, 0.22)}L${p(4, 0.9)}z`;
}).join('') + disc(5);

export const SHAPES = [
  { name: 'Bouclier', o: shield, i: 'M26 0h12v64H26z' },
  { name: 'Médaille', o: disc(28), i: disc(23) + disc(17) },
  { name: 'Hexagone', o: 'M32 4L56 18V46L32 60L8 46V18Z', i: 'M32 16L46 24V40L32 48L18 40V24Z' },
  { name: 'Losange', o: 'M32 3L61 32L32 61L3 32Z', i: 'M0 26h64v12H0z' },
  { name: 'Étoile', o: disc(28), i: 'M32 10l6.5 14 15 1.5-11.3 10 3.3 15L32 43l-13.5 7.5 3.3-15-11.3-10 15-1.5z' },
  { name: 'Éclair', o: rsq, i: 'M37 8L16 36h13l-4 20 22-30H34z' },
  { name: 'Chevrons', o: disc(28), i: 'M8 16l24 14 24-14v9L32 39 8 25zM8 31l24 14 24-14v9L32 54 8 40z' },
  { name: 'Aile', o: 'M2 20C18 20 26 16 32 6c6 10 14 14 30 14-4 12-14 18-24 20l-6 18-6-18C16 38 6 32 2 20z', i: 'M0 24h64v5H0zM0 33h64v4H0z' },
  { name: 'Damier', o: 'M6 6h52v52H6z', i: checker(6, 6, 4, 13) },
  { name: 'Couronne', o: 'M6 52L9 16l13 14 10-20 10 20 13-14 3 36z', i: 'M0 42h64v10H0z' },
  { name: 'Flamme', o: 'M32 3c4 12 18 18 18 35a18 18 0 0 1-36 0c0-8 4-14 8-18 0 6 2 10 6 12-2-12 2-21 4-29z', i: 'M32 29c3 6 9 10 9 18a9 9 0 0 1-18 0c0-6 5-11 9-18z' },
  { name: 'Flèche', o: 'M32 4L58 58L32 45L6 58Z', i: 'M29 0h6v64h-6z' },
  { name: 'Volant', o: disc(28), i: disc(24) + disc(18) + 'M29 14h6v16h-6zM14 30h36v6H14z' },
  { name: 'Faucon', o: disc(28), i: 'M8 40c12-2 18-10 24-24 2 9 9 15 24 15-11 3-18 8-22 17-5-7-14-9-26-8z' },
  { name: 'Écu', o: shield, i: 'M-4 46L46 -4h14L-4 60z' },
  { name: 'Triangle', o: 'M32 5L60 56H4Z', i: 'M32 25L46 50H18Z' },
  { name: 'Ovale', o: 'M4 32a28 21 0 1 0 56 0a28 21 0 1 0-56 0z', i: 'M0 28h64v8H0z' },
  { name: 'Drapeau', o: 'M5 4h4v56H5zM9 8c12-4 20 4 32 0s11-2 16-2v34c-5 0-6 2-16 2s-20-4-32 0z', i: checker(9, 4, 6, 8) + 'M0 0h9v64H0z' },
  { name: 'Hélice', o: disc(28), i: blades(3) },
  { name: 'Cible', o: disc(28), i: disc(22) + disc(15) + disc(9) },
];

const css = n => '#' + n.toString(16).padStart(6, '0');
export const colHex = id => css((PALETTE.find(c => c.id === id) || PALETTE[0]).hex);

// SVG d'un logo (chaîne) ; size en px, ou 100 % de la boîte si absent
export function logoSVG(logo, size) {
  const k = Math.max(0, Math.min(SHAPES.length - 1, logo?.shape | 0)), S = SHAPES[k];
  const dim = size ? ` width="${size}" height="${size}"` : '';
  return `<svg class="logo" viewBox="0 0 64 64"${dim} xmlns="http://www.w3.org/2000/svg"><defs><clipPath id="lgc${k}"><path d="${S.o}"/></clipPath></defs>`
    + `<path d="${S.o}" fill="${colHex(logo?.p)}"/><path d="${S.i}" fill="${colHex(logo?.s || 'blanc')}" fill-rule="evenodd" clip-path="url(#lgc${k})"/>`
    + `<path d="${S.o}" fill="none" stroke="rgba(0,0,0,.45)" stroke-width="1.6"/></svg>`;
}
// image du logo (pour les textures en canvas), appelle cb(img) une fois chargée
export function logoImage(logo, cb) {
  const img = new Image();
  img.onload = () => cb(img);
  img.src = 'data:image/svg+xml;charset=utf-8,' + encodeURIComponent(logoSVG(logo, 256));
}

// abréviation de 3 lettres tirée du nom : premier mot significatif (sans accents)
const SKIP = /^(écurie|ecurie|team|racing|gp|motorsport|engineering|corse|scuderia|de|du|des|la|le|les|the|f1)$/i;
export function autoAbbr(name) {
  const w = String(name || '').normalize('NFD').replace(/[̀-ͯ]/g, '').replace(/[^A-Za-z ]/g, ' ').split(/\s+/).filter(Boolean);
  const m = w.find(x => !SKIP.test(x)) || w[0] || 'GPM';
  return (m + (w[1] || '') + 'XXX').slice(0, 3).toUpperCase();
}
export const cleanAbbr = s => String(s || '').normalize('NFD').replace(/[̀-ͯ]/g, '').replace(/[^A-Za-z]/g, '').slice(0, 3).toUpperCase();
