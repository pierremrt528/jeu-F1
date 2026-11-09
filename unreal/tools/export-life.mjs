// Exporte les textes de la vie de l'écurie (press.js, events.js, stories.js) pour Unreal : node unreal/tools/export-life.mjs
// Les textes du jeu web sont des fonctions (c) => `…${c.mate}…` : on les appelle avec un contexte factice qui renvoie
// des repères {mate}, {v.drv.name}… remplacés en C++. Les conditions when(c) sont gardées en texte (petit évaluateur en C++).
// Écrit Content/Data/life.json.
import { writeFileSync, readFileSync, mkdirSync, cpSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { tmpdir } from 'node:os';

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

// copie des modules avec les tableaux internes exportés
const tmp = join(tmpdir(), 'gpm-life-' + process.pid);
mkdirSync(tmp, { recursive: true });
cpSync(join(root, 'js'), join(tmp, 'js'), { recursive: true });
cpSync(join(root, 'lib'), join(tmp, 'lib'), { recursive: true });
const patch = (f, add) => writeFileSync(join(tmp, 'js', f), readFileSync(join(tmp, 'js', f), 'utf8') + '\n' + add + '\n');
patch('press.js', 'export { Q as __Q, OUTLETS as __OUTLETS };');
patch('events.js', 'export { EVENTS as __EVENTS };');
const press = await import(join(tmp, 'js', 'press.js'));
const events = await import(join(tmp, 'js', 'events.js'));
const stories = await import(join(tmp, 'js', 'stories.js'));

// repère : se convertit en « {chemin} », et c.x.y donne {x.y}
const tok = path => new Proxy(function () {}, {
  get: (t, k) => k === Symbol.toPrimitive ? () => `{${path}}` : k === 'toString' ? () => `{${path}}` : k === 'then' ? undefined
    : typeof k === 'symbol' ? undefined : tok(path + '.' + k),
});
const ctxP = prefix => new Proxy({}, { get: (t, k) => typeof k === 'symbol' ? undefined : tok(prefix + k) });
const C = ctxP(''), V = ctxP('v.');
const txt = v => typeof v === 'function' ? String(v(C, V)) : v ?? '';
const when = f => f ? f.toString().replace(/^\s*\(?c\)?\s*=>\s*/, '').trim() : '';
const fxOf = v => {
  if (typeof v !== 'function') return v || {};
  try { return JSON.parse(JSON.stringify(v(C, V), (k, x) => typeof x === 'number' && !isFinite(x) ? 0 : x)); } catch { return {}; }
};
const choice = x => {
  const o = { t: txt(x.t), fx: fxOf(x.fx), out: txt(x.out) };
  if (x.next) o.next = x.next;
  if (x.gap != null) o.gap = x.gap;
  if (x.risk) o.risk = { p: x.risk.p, fx: fxOf(x.risk.fx), out: txt(x.risk.out) };
  if (o.fx.deal && typeof x.fx?.deal?.name === 'function') { try { o.fx.deal.name = String(x.fx.deal.name(V)); } catch { o.fx.deal.name = 'Nouveau partenaire'; } }
  return o;
};
const ev = e => ({ id: e.id, title: txt(e.title), text: txt(e.text), lite: !!e.lite, pre: !!e.pre, when: when(e.when), ch: e.ch.map(choice) });

const out = {
  OUTLETS: press.__OUTLETS,
  PRESS: press.__Q.map(q => ({ q: txt(q.q), when: when(q.when), res: !!q.res, a: q.a.map(([t, fx]) => [txt(t), fx]) })),
  EVENTS: events.__EVENTS.concat(stories.EXTRA).filter(e => !/chief/.test(when(e.when))).map(ev),
  STORIES: stories.STORIES.map(s => ({
    id: s.id, title: s.title, when: when(s.when),
    vars: s.init ? [0, 1, 2, 3].map(() => s.init()) : [{}],
    steps: Object.fromEntries(Object.entries(s.steps).map(([k, st]) => [k, { title: txt(st.title), text: txt(st.text), ch: st.ch.map(choice) }])),
  })),
};
writeFileSync(join(root, 'unreal', 'GrandPrixManager', 'Content', 'Data', 'life.json'), JSON.stringify(out));
console.log(`life.json : ${out.PRESS.length} questions, ${out.EVENTS.length} événements, ${out.STORIES.length} histoires`);
