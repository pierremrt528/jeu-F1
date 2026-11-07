// Couleurs de l'écurie du joueur appliquées partout : voiture (TEAMS[0]), atelier 3D du QG, menus et HUD (--acc, --acc2).
// colors = { p, s } : id de la palette (config/couleurs.js). Une paire trop proche de celle d'une écurie rivale est refusée.
import PALETTE from './config/couleurs.js';
import { TEAMS } from './data.js';

export { PALETTE };
export const colorOf = id => PALETTE.find(c => c.id === id) || PALETTE[0];
export const DEFAULT_COLORS = { p: 'rouge', s: 'blanc' };
const css = n => '#' + n.toString(16).padStart(6, '0');
const rgb = n => [(n >> 16) & 255, (n >> 8) & 255, n & 255];
export const lum = n => { const [r, g, b] = rgb(n); return (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255; };
// éclaircit une couleur sombre (même teinte) pour qu'elle reste lisible sur les menus sombres
function bright(n, min = 0.5) {
  let [r, g, b] = rgb(n).map(v => v / 255);
  const mx = Math.max(r, g, b), mn = Math.min(r, g, b), l = (mx + mn) / 2, d = mx - mn;
  if (l >= min) return n;
  const s = d ? d / (1 - Math.abs(2 * l - 1)) : 0;
  let h = 0;
  if (d) h = mx === r ? ((g - b) / d) % 6 : mx === g ? (b - r) / d + 2 : (r - g) / d + 4;
  const L = min, C = (1 - Math.abs(2 * L - 1)) * Math.min(1, s * 1.1), X = C * (1 - Math.abs((h % 2) - 1)), m = L - C / 2;
  [r, g, b] = [[C, X, 0], [X, C, 0], [0, C, X], [0, X, C], [X, 0, C], [C, 0, X]][Math.floor(((h % 6) + 6) % 6)];
  return (Math.round((r + m) * 255) << 16) | (Math.round((g + m) * 255) << 8) | Math.round((b + m) * 255);
}

// écurie rivale qui a déjà cette paire de couleurs (dans un sens ou dans l'autre), sinon null
export function clash(p, s) {
  return TEAMS.slice(1).find(t => t.pal && ((t.pal[0].includes(p) && t.pal[1].includes(s)) || (t.pal[1].includes(p) && t.pal[0].includes(s)))) || null;
}

// couleurs d'interface : la plus lisible des deux sert d'accent principal
export function uiColors(c) {
  const P = colorOf(c.p).hex, S = colorOf(c.s).hex;
  const swap = lum(P) < 0.1 && lum(S) >= 0.1;
  const a = swap ? S : P, b = swap ? P : S;
  const acc = lum(a) < 0.1 ? bright(a, 0.5) : a, acc2 = bright(b, lum(b) < 0.1 ? 0.55 : 0.4);
  return { acc: css(acc), accB: css(bright(acc, Math.min(0.75, (lum(acc) > 0.6 ? 0.8 : 0.62)))), acc2: css(acc2), txt: lum(acc) > 0.55 ? '#0a0c10' : '#fff' };
}

let listeners = [];
export const onTheme = f => listeners.push(f);
// applique les couleurs (null : couleurs par défaut du jeu)
export function applyTheme(c) {
  const k = c || DEFAULT_COLORS, root = document.documentElement.style;
  TEAMS[0].color = colorOf(k.p).hex; TEAMS[0].accent = colorOf(k.s).hex;
  if (c) {
    const u = uiColors(c);
    root.setProperty('--acc', u.acc); root.setProperty('--acc-b', u.accB); root.setProperty('--acc2', u.acc2); root.setProperty('--acct', u.txt);
  } else ['--acc', '--acc-b', '--acc2', '--acct'].forEach(v => root.removeProperty(v));
  listeners.forEach(f => f());
}
