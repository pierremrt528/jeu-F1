// Exporte le modèle 3D de monoplace du jeu web (js/carmodel.js) pour Unreal : node unreal/tools/export-car.mjs
// Écrit Content/Data/car.json : sommets non indexés (P en m, repère three.js x, y haut, z avant), normales N,
// et pour chaque sommet un indice de couleur K dans la palette (0 = couleur principale, 1 = accent, 2 = casque, puis couleurs fixes).
import { writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const fake = new Proxy(function () {}, {
  get: (t, k) => k === Symbol.toPrimitive ? () => 0 : k === 'then' ? undefined : fake,
  apply: () => fake, construct: () => fake, set: () => true,
});
for (const [k, v] of Object.entries({
  window: globalThis, document: fake, navigator: { userAgent: 'node', maxTouchPoints: 0 }, location: { search: '' },
  localStorage: { getItem: () => null, setItem() {}, removeItem() {} }, requestAnimationFrame: () => 0,
  matchMedia: () => ({ matches: false, addEventListener() {} }), addEventListener() {}, Image: fake, AudioContext: fake,
  HTMLCanvasElement: fake, devicePixelRatio: 1, innerWidth: 844, innerHeight: 390,
})) Object.defineProperty(globalThis, k, { value: v, configurable: true, writable: true });

const { buildCarMesh } = await import(join(root, 'js', 'carmodel.js'));
const MAIN = 0xff0000, ACC = 0x00ff00, HELM = 0x0000ff;
const g = buildCarMesh(MAIN, ACC, HELM);
const pal = ['main', 'accent', 'helmet'], key = new Map([['ff0000', 0], ['00ff00', 1], ['0000ff', 2]]);
const P = [], N = [], K = [];
const r3 = x => Math.round(x * 1000) / 1000, r2 = x => Math.round(x * 100) / 100;
function push(geo, fixed) {
  if (geo.index) geo = geo.toNonIndexed();
  const p = geo.attributes.position.array, n = geo.attributes.normal.array, c = geo.attributes.color?.array;
  for (let i = 0; i < p.length / 3; i++) {
    P.push(r3(p[i * 3]), r3(p[i * 3 + 1]), r3(p[i * 3 + 2]));
    N.push(r2(n[i * 3]), r2(n[i * 3 + 1]), r2(n[i * 3 + 2]));
    let hex = fixed;
    if (!hex) hex = [0, 1, 2].map(j => Math.round(c[i * 3 + j] * 255).toString(16).padStart(2, '0')).join('');
    if (!key.has(hex)) { key.set(hex, pal.length); pal.push(hex); }
    K.push(key.get(hex));
  }
}
push(g.children[0].geometry);          // carrosserie
push(g.children[1].geometry, '1a1a1c'); // roues
const R = g.userData.ranges || {};   // ailerons (fw, rw) : [premier sommet, nombre] pour les dégâts
writeFileSync(join(root, 'unreal', 'GrandPrixManager', 'Content', 'Data', 'car.json'), JSON.stringify({ palette: pal, P, N, K, R }));
console.log(`car.json : ${P.length / 3} sommets, ${pal.length} couleurs`);
