// Exporte les données du jeu web (js/) en JSON pour Unreal : node unreal/tools/export-data.mjs
// Écrit dans unreal/GrandPrixManager/Content/Data/. Les fonctions sont ignorées (la logique est portée en C++).
import { writeFileSync, mkdirSync, readFileSync, unlinkSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const out = join(root, 'unreal', 'GrandPrixManager', 'Content', 'Data');
mkdirSync(out, { recursive: true });

// Faux navigateur : tout accès renvoie un objet neutre, pour importer les modules sans DOM ni WebGL.
const fake = new Proxy(function () {}, {
  get: (t, k) => k === Symbol.toPrimitive ? () => 0 : k === 'then' ? undefined : fake,
  apply: () => fake, construct: () => fake, set: () => true,
});
const store = {};
for (const [k, v] of Object.entries({
  window: globalThis, document: fake, navigator: { userAgent: 'node', maxTouchPoints: 0 }, location: { search: '' },
  localStorage: { getItem: k => store[k] ?? null, setItem: (k, v) => { store[k] = v; }, removeItem: k => { delete store[k]; } },
  requestAnimationFrame: () => 0, matchMedia: () => ({ matches: false, addEventListener() {} }),
  addEventListener() {}, Image: fake, AudioContext: fake, HTMLCanvasElement: fake, devicePixelRatio: 1, innerWidth: 844, innerHeight: 390,
})) Object.defineProperty(globalThis, k, { value: v, configurable: true, writable: true });

const MODULES = ['data', 'config/couleurs', 'config/motoristes', 'config/nations', 'config/pilotes', 'config/sponsors', 'config/scenes',
  'career', 'team', 'season', 'garage', 'infra', 'staff', 'strategy', 'tuning', 'events', 'stories', 'press', 'market', 'nego', 'sponsors', 'logo'];
const clean = v => JSON.parse(JSON.stringify(v, (k, x) => typeof x === 'function' ? undefined : x));
const isData = (name, v) => /^[A-Z][A-Z0-9_]*$/.test(name) && v !== null && typeof v !== 'function';

// Tableaux internes (non exportés par le jeu web) : copie temporaire du module avec un export en plus.
const HIDDEN = { events: ['EVENTS'], press: ['Q', 'OUTLETS'] };
async function load(m) {
  const src = join(root, 'js', m + '.js');
  if (!HIDDEN[m]) return import(src);
  const tmp = join(root, 'js', `_export_${m}.js`);
  writeFileSync(tmp, readFileSync(src, 'utf8') + `\nexport { ${HIDDEN[m].map(n => n === 'Q' ? 'Q as QUESTIONS' : n).join(', ')} };\n`);
  try { return await import(tmp); } finally { unlinkSync(tmp); }
}

const index = {};
for (const m of MODULES) {
  let mod;
  try { mod = await load(m); } catch (e) { console.warn(`! ${m} : ${e.message}`); continue; }
  const data = {};
  if (mod.default && typeof mod.default === 'object') data[m.split('/').pop().toUpperCase()] = clean(mod.default);
  for (const [k, v] of Object.entries(mod)) if (isData(k, v)) { try { data[k] = clean(v); } catch {} }
  if (!Object.keys(data).length) continue;
  const file = m.replace('/', '_') + '.json';
  writeFileSync(join(out, file), JSON.stringify(data, null, 1));
  index[file] = Object.keys(data);
  console.log(`${file} : ${Object.keys(data).join(', ')}`);
}

// Circuits calculés : ligne médiane tous les 2 m (x, altitude, z), murs, courbure, vitesse IA, voie des stands.
try {
  const { CIRCUITS } = await import(join(root, 'js', 'data.js'));
  const { buildTrack, pitLane } = await import(join(root, 'js', 'track.js'));
  mkdirSync(join(out, 'tracks'), { recursive: true });
  const r = (a, d = 2) => a ? Array.from(a, x => Math.round(x * 10 ** d) / 10 ** d) : undefined;
  for (const c of CIRCUITS) {
    const t = buildTrack(c);
    let pit; try { const p = pitLane(t); pit = clean({ ...p, X: r(p.X), Z: r(p.Z) }); } catch {}
    const o = { id: c.id, name: c.name, N: t.N, DS: 2, length: t.L ?? t.N * 2, width: c.width,
      X: r(t.X), Y: r(t.Y), Z: r(t.Z), WL: r(t.WL), WR: r(t.WR), C: r(t.C, 5), V: r(t.V ?? t.vmax, 1),
      LX: r(t.LX), LZ: r(t.LZ), pit };
    writeFileSync(join(out, 'tracks', c.id + '.json'), JSON.stringify(clean(o)));
  }
  index.tracks = CIRCUITS.map(c => c.id);
  console.log(`tracks/ : ${CIRCUITS.length} circuits`);
} catch (e) { console.warn('! circuits : ' + e.message); }

writeFileSync(join(out, 'index.json'), JSON.stringify(index, null, 1));
