// Maillages du circuit et de l'environnement : piste texturée, vibreurs, herbe/gravier, murs publicitaires,
// grillages, tribunes avec public, stands, portique de départ, arbres, montagnes, ciel et soleil.
import * as THREE from '../lib/three.module.min.js';
import { mulberry, pitLane } from './track.js';
import { TEAMS } from './data.js';
import * as TX from './textures.js';

// Petit constructeur de géométrie (triangles non indexés, uv + couleur par sommet)
class Geo {
  constructor() { this.p = []; this.u = []; this.c = []; }
  quad(a, b, c, d, ua, ub, uc, ud, k = 1) {   // a-b sur la tranche i, c-d sur la tranche i+1
    this.p.push(...a, ...b, ...c, ...b, ...d, ...c);
    this.u.push(...ua, ...ub, ...uc, ...ub, ...ud, ...uc);
    const col = Array.isArray(k) ? k : [k, k, k];
    for (let n = 0; n < 6; n++) this.c.push(...col);
  }
  mesh(mat) {
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.Float32BufferAttribute(this.p, 3));
    g.setAttribute('uv', new THREE.Float32BufferAttribute(this.u, 2));
    g.setAttribute('color', new THREE.Float32BufferAttribute(this.c, 3));
    g.computeVertexNormals();
    return new THREE.Mesh(g, mat);
  }
}

// Fusion simple de géométries (non indexées)
export function merge(list) {
  const geos = list.map(g => g.index ? g.toNonIndexed() : g);
  const out = new THREE.BufferGeometry();
  for (const name of ['position', 'normal', 'uv']) {
    const size = geos[0].attributes[name].itemSize;
    let n = 0; for (const g of geos) n += g.attributes[name].count;
    const arr = new Float32Array(n * size); let o = 0;
    for (const g of geos) { arr.set(g.attributes[name].array, o); o += g.attributes[name].array.length; }
    out.setAttribute(name, new THREE.BufferAttribute(arr, size));
  }
  return out;
}
const box = (w, h, d, x, y, z, rx = 0) => { const g = new THREE.BoxGeometry(w, h, d); g.rotateX(rx); g.translate(x, y, z); return g; };

export function buildEnvironment(scene, renderer, track, world = scene) {
  const th = track.def.theme;
  const horizon = new THREE.Color(th.horizon || 0xc9dcea), zenith = new THREE.Color(th.zenith || 0x3f7fd0);
  scene.fog = new THREE.Fog(horizon, th.fog[0], th.fog[1]);
  scene.background = horizon;

  // ciel : sphère à dégradé qui suit la caméra
  const skyGeo = (r) => {
    const g = new THREE.SphereGeometry(r, 32, 16), pos = g.attributes.position, col = [];
    const c = new THREE.Color();
    for (let i = 0; i < pos.count; i++) {
      const t = Math.max(0, pos.getY(i) / r);
      c.copy(horizon).lerp(zenith, Math.pow(t, 0.55)); col.push(c.r, c.g, c.b);
    }
    g.setAttribute('color', new THREE.Float32BufferAttribute(col, 3));
    return g;
  };
  const skyMat = new THREE.MeshBasicMaterial({ vertexColors: true, side: THREE.BackSide, fog: false, depthWrite: false });
  const sky = new THREE.Group();
  const dome = new THREE.Mesh(skyGeo(2600), skyMat); dome.renderOrder = -2; sky.add(dome);
  const sunDir = new THREE.Vector3(...(th.sun || [0.45, 0.42, 0.6])).normalize();
  const sunSprite = new THREE.Sprite(new THREE.SpriteMaterial({ map: TX.sunGlow(), fog: false, depthWrite: false, transparent: true, blending: THREE.AdditiveBlending }));
  sunSprite.scale.set(700, 700, 1); sunSprite.position.copy(sunDir).multiplyScalar(2400); sunSprite.renderOrder = -1;
  sky.add(sunSprite);
  // nuages photographiques par-dessus le dégradé (orientation variable selon le circuit)
  const cl = TX.clouds();
  let cloudMat = null;
  if (cl) {
    cloudMat = new THREE.MeshBasicMaterial({ map: cl.map, alphaMap: cl.alpha, transparent: true, fog: false, depthWrite: false, side: THREE.BackSide });
    const cd = new THREE.Mesh(new THREE.SphereGeometry(2500, 48, 12, 0, Math.PI * 2, 0, Math.PI / 2), cloudMat);
    cd.renderOrder = -1.5; cd.rotation.y = (track.length % 628) / 100; sky.add(cd);
  }
  world.add(sky);

  // éclairage
  const hemi = new THREE.HemisphereLight(0xd8e9ff, 0x6a7448, 1.25); world.add(hemi);
  const sun = new THREE.DirectionalLight(0xfff0d8, 2.6); sun.position.copy(sunDir); world.add(sun);

  // reflets des carrosseries : carte d'environnement tirée du ciel
  const envScene = new THREE.Scene();
  envScene.add(new THREE.Mesh(skyGeo(40), skyMat.clone()));
  if (cloudMat) envScene.add(new THREE.Mesh(new THREE.SphereGeometry(39, 32, 8, 0, Math.PI * 2, 0, Math.PI / 2), cloudMat));
  const ground = new THREE.Mesh(new THREE.CircleGeometry(40, 16), new THREE.MeshBasicMaterial({ color: 0x4a5a3a }));
  ground.rotation.x = -Math.PI / 2; ground.position.y = -2; envScene.add(ground);
  const s2 = new THREE.Sprite(new THREE.SpriteMaterial({ map: sunSprite.material.map, blending: THREE.AdditiveBlending, transparent: true }));
  s2.scale.set(14, 14, 1); s2.position.copy(sunDir).multiplyScalar(35); envScene.add(s2);
  const pm = new THREE.PMREMGenerator(renderer);
  scene.environment = pm.fromScene(envScene, 0.02, 0.1, 100).texture;
  pm.dispose();

  return { sky, hemi, sun, skyMat, cloudMat, sunSprite, fog: { color: scene.fog.color.clone(), near: scene.fog.near, far: scene.fog.far } };
}

export function buildTrackMeshes(track, scene) {
  const { N, X, Y, Z, TX: tx, TZ: tz, C, width: w, def } = track;
  const th = def.theme;
  const DSm = track.length / N;
  const group = new THREE.Group();
  const L = (o) => new THREE.MeshLambertMaterial(o);
  const pt = (j, off, dy) => [X[j] - tz[j] * off, Y[j] + dy, Z[j] + tx[j] * off];
  const wall = w / 2 + track.runoff, WL = track.WL, WR = track.WR, Wmax = i => Math.max(WL[i], WR[i]);
  const P = pitLane(track), pitK = i => P.k(i, N), inPit = i => pitK(i) >= P.e0 - 2 && pitK(i) <= P.x1 + 2;
  const surf = th.surface || 'grass', isGrass = surf === 'grass';
  const surfTex = () => surf === 'sand' ? TX.sand() : surf === 'concrete' ? TX.concrete() : TX.grass();
  // courbure large pour savoir de quel côté est l'extérieur des virages
  const Cw = new Float32Array(N);
  for (let i = 0; i < N; i++) { let s = 0; for (let k = -15; k <= 15; k++) s += C[(i + k + N) % N]; Cw[i] = s / 31; }

  // tracés qui se croisent (pont) : grille des points pour trouver les autres portions de piste proches
  const tg = new Map(), tc = 50, far2 = 40;
  for (let i = 0; i < N; i += 2) { const k = Math.floor(X[i] / tc) + ',' + Math.floor(Z[i] / tc); if (!tg.has(k)) tg.set(k, []); tg.get(k).push(i); }
  const others = (x, z, i, r, f) => {
    const gx = Math.floor(x / tc), gz = Math.floor(z / tc), n = Math.ceil(r / tc);
    for (let a = -n; a <= n; a++) for (let b = -n; b <= n; b++) for (const k of tg.get((gx + a) + ',' + (gz + b)) || []) {
      const di = Math.abs(k - i), ad = Math.min(di, N - di);
      if (ad > far2 && (X[k] - x) ** 2 + (Z[k] - z) ** 2 < r * r && f(k)) return true;
    }
    return false;
  };
  const over = new Uint8Array(N), nearLow = new Uint8Array(N);
  for (let i = 0; i < N; i++) {
    over[i] = others(X[i], Z[i], i, Wmax(i) + 7, k => Y[i] - Y[k] > 4) ? 1 : 0;
    nearLow[i] = others(X[i], Z[i], i, 95, k => Y[i] - Y[k] > 2.5) ? 1 : 0;
  }
  // zones de gravier dessinées (def.gravel : x, z tous les 10 m)
  let gravelAt = null;
  if (def.gravel) {
    const gs = new Set(); for (let k = 0; k < def.gravel.length; k += 2) gs.add(def.gravel[k] + ',' + def.gravel[k + 1]);
    gravelAt = (p) => { const x = Math.round(p[0] / 10) * 10, z = Math.round(p[2] / 10) * 10; return gs.has(x + ',' + z); };
  }
  const deck = new Geo();
  const road = new Geo(), kerbs = new Geo(), grass = new Geo(), gravel = new Geo(), walls = new Geo(), fences = new Geo();
  for (let i = 0; i < N; i++) {
    const j = (i + 1) % N, v0 = i * DSm, v1 = (i + 1) * DSm;
    road.quad(pt(i, -w / 2, 0.02), pt(i, w / 2, 0.02), pt(j, -w / 2, 0.02), pt(j, w / 2, 0.02), [0, v0 / 16], [1, v0 / 16], [0, v1 / 16], [1, v1 / 16]);
    const kerb = Math.abs(C[i]) > 1 / 220;
    const stripe = isGrass ? (Math.floor(v0 / 12) % 2 ? 1 : 0.86) : 1;
    for (const s of [-1, 1]) {
      const A = (o, dy) => pt(i, s * o, dy), B = (o, dy) => pt(j, s * o, dy);
      // gauche/droite : on garde l'ordre intérieur→extérieur pour des normales vers le haut
      const q = (geo, o1, o2, y1, y2, u1, u2, va, vb, k) => s > 0
        ? geo.quad(A(o1, y1), A(o2, y2), B(o1, y1), B(o2, y2), [u1, va], [u2, va], [u1, vb], [u2, vb], k)
        : geo.quad(A(o2, y2), A(o1, y1), B(o2, y2), B(o1, y1), [u2, va], [u1, va], [u2, vb], [u1, vb], k);
      let edge = w / 2;
      if (kerb) { q(kerbs, w / 2 - 0.1, w / 2 + 1.4, 0.035, 0.09, 0, 1, v0 / 3, v1 / 3, 1); edge = w / 2 + 1.4; }
      // distance du mur de ce côté (variable selon le circuit)
      const Wa = s > 0 ? WR[i] : WL[i], Wb = s > 0 ? WR[j] : WL[j];
      const wq = (geo, d1, d2, y1, y2, uv, k) => geo.quad(A(Wa + d1, y1), A(Wa + d2, y2), B(Wb + d1, y1), B(Wb + d2, y2), ...uv, k);
      const outside = gravelAt ? gravelAt(pt(i, s * (edge + Math.min(Wa, edge + 30)) / 2, 0)) : surf !== 'concrete' && Math.abs(Cw[i]) > 1 / 170 && s === (Cw[i] > 0 ? 1 : -1);
      // dégagement : gravier à l'extérieur des virages, herbe ailleurs
      const zone = outside ? gravel : grass;
      const uvw = (k, o) => { const p = pt(k, s * o, 0); return [p[0] / 7, p[2] / 7]; };
      const pit = s > 0 && inPit(i), fa = pit ? wall + 30 : Wa + 3, fb = pit ? wall + 30 : Wb + 3;   // côté stands : terrain plat jusqu'aux garages
      const qa = A(edge, 0), qb = A(fa, -0.05), qc = B(edge, 0), qd = B(fb, -0.05);
      if (over[i]) {   // pont : tablier en béton au lieu du terrain
        wq(deck, 0.6, 0.6, 0, -1.6, [[0, 0], [1, 0], [0, 1], [1, 1]], 0.75);
        deck.quad(A(edge, 0), A(Wa + 0.6, 0), B(edge, 0), B(Wb + 0.6, 0), [0, 0], [1, 0], [0, 1], [1, 1], 0.9);
        deck.quad(A(0, -1.6), A(Wa + 0.6, -1.6), B(0, -1.6), B(Wb + 0.6, -1.6), [0, 0], [1, 0], [0, 1], [1, 1], 0.6);
        if (!pit) wq(walls, 0, 0, 0, 1.15, [[v0 / 48, 0], [v0 / 48, 1], [v1 / 48, 0], [v1 / 48, 1]]);
        continue;
      }
      if (s > 0) zone.quad(qa, qb, qc, qd, uvw(i, edge), uvw(i, fa), uvw(j, edge), uvw(j, fb), outside && !pit ? 1 : stripe);
      else zone.quad(qb, qa, qd, qc, uvw(i, fa), uvw(i, edge), uvw(j, fb), uvw(j, edge), outside ? 1 : stripe);
      // talus jusqu'au sol (mur de soutènement près d'une piste plus basse)
      const fall = (k, f) => { const d = nearLow[i] ? f : Math.max(75, f + 25); return [X[k] - tz[k] * s * d, track.ground, Z[k] + tx[k] * s * d]; };
      const Fa = fall(i, fa), Fb = fall(j, fb), fu = (p) => [p[0] / 7, p[2] / 7];
      if (nearLow[i]) deck.quad(qb, Fa, qd, Fb, [v0 / 8, 0], [v0 / 8, (Y[i] - track.ground) / 8], [v1 / 8, 0], [v1 / 8, (Y[j] - track.ground) / 8], 0.8);
      else if (s > 0) grass.quad(qb, Fa, qd, Fb, uvw(i, fa), fu(Fa), uvw(j, fb), fu(Fb), isGrass ? 0.8 : 0.92);
      else grass.quad(Fa, qb, Fb, qd, fu(Fa), uvw(i, fa), fu(Fb), uvw(j, fb), isGrass ? 0.8 : 0.92);
      // mur publicitaire puis grillage
      if (pit) continue;   // remplacé par le muret des stands
      wq(walls, 0, 0, 0, 1.15, [[v0 / 48, 0], [v0 / 48, 1], [v1 / 48, 0], [v1 / 48, 1]]);
      wq(fences, 0.1, 0.1, 1.15, 4.2, [[v0 / 3, 0], [v0 / 3, 1], [v1 / 3, 0], [v1 / 3, 1]]);
    }
  }
  const roadMesh = road.mesh(L({ map: TX.asphalt(), normalMap: TX.asphaltNormal() })); roadMesh.name = 'road'; group.add(roadMesh);
  // voie des stands : bande d'asphalte qui quitte la piste, muret, lignes blanches, emplacements aux couleurs des écuries
  {
    const lane = new Geo(), paint = new Geo(), hw = 3.6;
    const ip = k => (k + N) % N;
    for (let k = P.e0; k < P.x1; k++) {
      const i = ip(k), j = ip(k + 1), la = P.latAt(k), lb = P.latAt(k + 1), v0 = k * DSm, v1 = (k + 1) * DSm;
      lane.quad(pt(i, la - hw, 0.04), pt(i, la + hw, 0.04), pt(j, lb - hw, 0.04), pt(j, lb + hw, 0.04), [0, v0 / 16], [0.45, v0 / 16], [0, v1 / 16], [0.45, v1 / 16]);
      for (const e of [-hw, hw]) paint.quad(pt(i, la + e - 0.12, 0.06), pt(i, la + e + 0.12, 0.06), pt(j, lb + e - 0.12, 0.06), pt(j, lb + e + 0.12, 0.06), [0, 0], [1, 0], [0, 1], [1, 1]);
    }
    // emplacements : rectangle à la couleur de chaque écurie, côté garages
    TEAMS.forEach((t, n) => {
      const c = new THREE.Color(t.color), k0 = P.box(n) - 2;
      for (let k = k0; k < k0 + 4; k++) {
        const i = ip(k), j = ip(k + 1);
        paint.quad(pt(i, P.lat + 0.6, 0.065), pt(i, P.lat + hw - 0.3, 0.065), pt(j, P.lat + 0.6, 0.065), pt(j, P.lat + hw - 0.3, 0.065), [0, 0], [1, 0], [0, 1], [1, 1], [c.r, c.g, c.b]);
      }
    });
    // muret des stands entre la piste et la voie, avec grillage
    const pw = new Geo();
    for (let k = P.e1 - 6; k < P.x0 + 6; k++) {
      const i = ip(k), j = ip(k + 1), A = (o, y) => pt(i, o, y), B = (o, y) => pt(j, o, y);
      pw.quad(A(wall, 0), A(wall, 1.1), B(wall, 0), B(wall, 1.1), [0, 0], [0, 1], [1, 0], [1, 1]);
      pw.quad(A(wall, 1.1), A(wall + 0.5, 1.1), B(wall, 1.1), B(wall + 0.5, 1.1), [0, 0], [0, 1], [1, 0], [1, 1]);
      fences.quad(A(wall + 0.1, 1.1), A(wall + 0.1, 3.4), B(wall + 0.1, 1.1), B(wall + 0.1, 3.4), [k * DSm / 3, 0], [k * DSm / 3, 1], [(k + 1) * DSm / 3, 0], [(k + 1) * DSm / 3, 1]);
    }
    group.add(lane.mesh(L({ map: TX.asphalt(), color: 0xc4c8cc, polygonOffset: true, polygonOffsetFactor: -2 })));
    group.add(paint.mesh(new THREE.MeshBasicMaterial({ vertexColors: true, polygonOffset: true, polygonOffsetFactor: -3 })));
    group.add(pw.mesh(L({ color: 0xd9dde2, side: THREE.DoubleSide })));
  }
  if (deck.p.length) group.add(deck.mesh(L({ map: TX.concrete(), vertexColors: true, side: THREE.DoubleSide })));
  group.add(kerbs.mesh(L({ map: TX.kerb(), polygonOffset: true, polygonOffsetFactor: -1 })));
  group.add(grass.mesh(L({ map: surfTex(), vertexColors: true })));
  group.add(gravel.mesh(L({ map: TX.gravel(), vertexColors: true })));
  const adMat = L({ map: TX.adBoards(), side: THREE.DoubleSide });
  group.add(walls.mesh(adMat));
  const fenceMesh = fences.mesh(L({ map: TX.fence(), alphaTest: 0.5, side: THREE.DoubleSide, transparent: false }));
  group.add(fenceMesh);

  // Sol au-delà des talus
  let minX = Infinity, maxX = -Infinity, minZ = Infinity, maxZ = -Infinity;
  for (let i = 0; i < N; i++) { minX = Math.min(minX, X[i]); maxX = Math.max(maxX, X[i]); minZ = Math.min(minZ, Z[i]); maxZ = Math.max(maxZ, Z[i]); }
  const cx = (minX + maxX) / 2, cz = (minZ + maxZ) / 2, size = Math.max(maxX - minX, maxZ - minZ) + 3000;
  const gTex = surfTex(); gTex.repeat.set(size / 7, size / 7);
  const groundMesh = new THREE.Mesh(new THREE.PlaneGeometry(size, size), L({ map: gTex, color: isGrass ? 0xb8c8a8 : 0xd0d0d0 }));
  groundMesh.rotation.x = -Math.PI / 2; groundMesh.position.set(cx, track.ground - 0.05, cz);
  group.add(groundMesh);
  // mer au sud du circuit
  const seaZ = minZ - 140;
  if (th.sea) {
    const sea = new THREE.Mesh(new THREE.PlaneGeometry(size, size / 2), new THREE.MeshStandardMaterial({ color: 0x1f5f8a, roughness: 0.12, metalness: 0.2 }));
    sea.rotation.x = -Math.PI / 2; sea.position.set(cx, track.ground + 0.1, seaZ - size / 4);
    group.add(sea);
    const beach = new THREE.Mesh(new THREE.PlaneGeometry(size, 50), L({ map: (() => { const t = TX.sand(); t.repeat.set(size / 7, 7); return t; })() }));
    beach.rotation.x = -Math.PI / 2; beach.position.set(cx, track.ground + 0.05, seaZ + 20);
    group.add(beach);
  }
  const inSea = (z) => th.sea && z < seaZ + 45;

  // Ligne de départ en damier et grille
  {
    const cv = document.createElement('canvas'); cv.width = 64; cv.height = 8;
    const c = cv.getContext('2d');
    for (let x = 0; x < 16; x++) for (let y = 0; y < 2; y++) { c.fillStyle = (x + y) % 2 ? '#111' : '#f4f4f4'; c.fillRect(x * 4, y * 4, 4, 4); }
    const t = new THREE.CanvasTexture(cv); t.magFilter = THREE.NearestFilter; t.colorSpace = THREE.SRGBColorSpace;
    const m = new THREE.Mesh(new THREE.PlaneGeometry(w - 0.6, 1.6), L({ map: t, polygonOffset: true, polygonOffsetFactor: -2 }));
    m.rotation.order = 'YXZ'; m.rotation.set(-Math.PI / 2, track.H[0], 0);
    m.position.set(X[0], Y[0] + 0.04, Z[0]); group.add(m);
    const gm = L({ color: 0xeeeeee, polygonOffset: true, polygonOffsetFactor: -2 });
    for (let k = 0; k < 20; k++) {
      const i = (N - 5 - k * 4) % N, s = k % 2 ? 1 : -1;
      const g = new THREE.Mesh(new THREE.PlaneGeometry(2.4, 0.25), gm);
      g.rotation.order = 'YXZ'; g.rotation.set(-Math.PI / 2, track.H[i], 0);
      const p = pt(i, s * 3, 0.04); g.position.set(p[0], p[1], p[2]); group.add(g);
    }
  }

  // Portique de départ avec 5 feux
  const lights = [];
  {
    const h = track.H[0], rx = -tz[0], rz = tx[0];
    const dark = new THREE.MeshStandardMaterial({ color: 0x23262b, metalness: 0.5, roughness: 0.5 });
    for (const s of [-1, 1]) {
      const post = new THREE.Mesh(new THREE.BoxGeometry(0.7, 8.5, 0.7), dark);
      post.position.set(X[0] + rx * s * (w / 2 + 1.5), Y[0] + 4.25, Z[0] + rz * s * (w / 2 + 1.5)); group.add(post);
    }
    const beam = new THREE.Mesh(new THREE.BoxGeometry(w + 3.6, 1.6, 0.9), dark);
    beam.position.set(X[0], Y[0] + 7.6, Z[0]); beam.rotation.y = h; group.add(beam);
    for (let k = 0; k < 5; k++) {
      const l = new THREE.Mesh(new THREE.CircleGeometry(0.42, 16), new THREE.MeshBasicMaterial({ color: 0x330000 }));
      const off = (k - 2) * 1.5;
      l.position.set(X[0] + rx * off - tx[0] * 0.46, Y[0] + 7.6, Z[0] + rz * off - tz[0] * 0.46); l.rotation.y = h + Math.PI;
      group.add(l); lights.push(l);
    }
  }

  // Tribunes avec public (côté gauche de la ligne droite + extérieur de quelques virages)
  const crowdTex = TX.crowd();
  const concrete = L({ color: 0xb4b8bd }), roofMat = L({ color: 0xe6e9ec, emissive: 0x4a4f56, side: THREE.DoubleSide });
  const crowdMat = L({ map: crowdTex, color: 0xc4c4c4 });
  const stand = (i, s, len, off = (s > 0 ? WR[i] : WL[i]) + 3.5) => {
    const g = new THREE.Group();
    const seats = new THREE.PlaneGeometry(len, Math.hypot(14, 9));
    const uv = seats.attributes.uv; for (let k = 0; k < uv.count; k++) uv.setXY(k, uv.getX(k) * len / 24, uv.getY(k));
    seats.rotateX(-Math.PI / 2 + Math.atan2(9, 14)); seats.translate(0, 6, -7);
    g.add(new THREE.Mesh(seats, crowdMat));
    const parts = [box(len, 1.6, 0.4, 0, 0.8, 0), box(len + 0.8, 12, 0.5, 0, 6, -14.3), box(0.5, 11, 14.5, -len / 2, 5.5, -7), box(0.5, 11, 14.5, len / 2, 5.5, -7)];
    for (let x = -len / 2; x <= len / 2 + 0.1; x += len / Math.round(len / 15)) parts.push(box(0.5, 16, 0.5, x, 8, -14.6));
    g.add(new THREE.Mesh(merge(parts), concrete));
    g.add(new THREE.Mesh(box(len + 3, 0.25, 19, 0, 15.6, -6.5, -0.12), roofMat));
    const p = pt(i, s * off, 0);
    g.position.set(p[0], Y[i] - 0.3, p[2]); g.rotation.y = track.H[i] + s * Math.PI / 2;
    group.add(g);
  };
  const placedStands = [];
  if (def.stands) {   // tribunes placées sur le dessin du circuit : [x, z, longueur]
    for (const [x, z, len] of def.stands) {
      let i = 0, bd = Infinity;
      for (let k = 0; k < N; k++) { const d = (X[k] - x) ** 2 + (Z[k] - z) ** 2; if (d < bd) { bd = d; i = k; } }
      const lat = (x - X[i]) * -tz[i] + (z - Z[i]) * tx[i], s = lat >= 0 ? 1 : -1;
      const n = Math.max(1, Math.round(len / 80)), seg = len / n;
      for (let m = 0; m < n; m++) {
        const k = (i + Math.round((m - (n - 1) / 2) * seg / DSm) + N) % N;
        const wk = s > 0 ? WR[k] : WL[k];
        let off = Math.min(Math.max(Math.abs(lat), wk + 3.5), wk + 40);
        if (s > 0 && inPit(k)) off = Math.max(off, P.lat + 24);
        const sp = pt(k, s * (off + 7), 0);
        if (over[k] || others(sp[0], sp[2], k, 7 + w / 2 + 4, () => true)) continue;   // pas de tribune sur une autre portion de piste
        const c = pt(k, s * (off + 7), 0);
        if (placedStands.some(q => (q[0] - c[0]) ** 2 + (q[2] - c[2]) ** 2 < 900)) continue;   // pas deux tribunes l'une sur l'autre
        placedStands.push(c); stand(k, s, seg - 4, off);
      }
    }
  } else for (let k = -2; k <= 3; k++) stand((k * 33 + N) % N, -1, 62);
  // virages les plus serrés : tribune à l'extérieur
  const corners = [];
  for (let i = 0; i < N; i++) if (Math.abs(Cw[i]) > 1 / 120) corners.push(i);
  const picked = [];
  for (const i of corners.sort((a, b) => Math.abs(Cw[b]) - Math.abs(Cw[a]))) {
    if (picked.every(p => Math.min(Math.abs(p - i), N - Math.abs(p - i)) > 150) && Math.min(i, N - i) > 250) picked.push(i);
    if (picked.length >= 4) break;
  }
  if (!def.stands) for (const i of picked) stand(i, Cw[i] > 0 ? 1 : -1, 70);

  // Bâtiment des stands (côté droit de la ligne droite)
  {
    const wm = L({ color: 0xdfe3e8 }), gm = new THREE.MeshStandardMaterial({ color: 0x1d2a38, metalness: 0.6, roughness: 0.2 });
    for (let k = -4; k <= 1; k++) {
      const i = (k * 31 + 8 + N) % N;
      const t = TX.pitWall(); t.repeat.set(4, 1);
      const b = new THREE.Mesh(new THREE.BoxGeometry(62, 9, 16), [wm, wm, wm, wm, L({ map: t }), wm]);
      const p = pt(i, P.lat + 3.6 + 1.5 + 8, 0);
      b.position.set(p[0], Y[i] + 4.5, p[2]); b.rotation.y = track.H[i] + Math.PI / 2;
      group.add(b);
      const glass = new THREE.Mesh(new THREE.BoxGeometry(60, 2.4, 13), gm);
      glass.position.set(p[0], Y[i] + 10.2, p[2]); glass.rotation.y = b.rotation.y; group.add(glass);
    }
  }

  // Arbres instanciés (feuillus texturés + conifères), loin de la piste
  {
    const grid = new Map(), cell = 40;
    for (let i = 0; i < N; i += 2) {
      const key = Math.floor(X[i] / cell) + ',' + Math.floor(Z[i] / cell);
      if (!grid.has(key)) grid.set(key, []); grid.get(key).push(i);
    }
    const nearest = (x, z) => {
      let bd = Infinity, bi = 0; const gx = Math.floor(x / cell), gz = Math.floor(z / cell);
      for (let a = -2; a <= 2; a++) for (let b = -2; b <= 2; b++) {
        const l = grid.get((gx + a) + ',' + (gz + b)); if (!l) continue;
        for (const i of l) { const d = (X[i] - x) ** 2 + (Z[i] - z) ** 2; if (d < bd) { bd = d; bi = i; } }
      }
      return [Math.sqrt(bd), bi];
    };
    const rnd = mulberry(7);
    // feuillage : touffes de feuilles ombrées (claires en haut à gauche, sombres en bas)
    const leafTex = (() => {
      const S = 256, cv = document.createElement('canvas'); cv.width = cv.height = S; const c = cv.getContext('2d');
      c.fillStyle = '#2c5220'; c.fillRect(0, 0, S, S);
      for (let k = 0; k < 900; k++) {
        const x = rnd() * S, y = rnd() * S, r = 3 + rnd() * 7, l = rnd();
        for (const [dx, dy, col] of [[1.5, 1.5, '#1c3a15'], [0, 0, l < 0.33 ? '#3d6d2a' : l < 0.66 ? '#4c8034' : '#5f9640'], [-1.2, -1.2, l < 0.5 ? '#6ea84a' : '#7fb955']]) {
          c.fillStyle = col; c.beginPath(); c.ellipse(x + dx, y + dy, r * (dx < 0 ? 0.45 : 1), r * 0.7 * (dx < 0 ? 0.45 : 1), rnd() * 3, 0, 7); c.fill();
        }
      }
      const t = new THREE.CanvasTexture(cv); t.colorSpace = THREE.SRGBColorSpace; t.wrapS = t.wrapT = THREE.RepeatWrapping; return t;
    })();
    // couronne bosselée (plusieurs lobes fusionnés) et sapin à étages, ombrés par sommet (dessous plus sombre)
    const shade = (g, y0, y1, lo = 0.45) => {
      const p = g.attributes.position, col = [];
      for (let k = 0; k < p.count; k++) { const t = Math.min(1, Math.max(0, (p.getY(k) - y0) / (y1 - y0))); const v = lo + (1 - lo) * t; col.push(v, v, v); }
      g.setAttribute('color', new THREE.Float32BufferAttribute(col, 3)); return g;
    };
    const lumpy = (() => {
      const rr = mulberry(3), lobes = [];
      for (const [x, y, z, r] of [[0, 0.15, 0, 0.82], [0.45, -0.1, 0.25, 0.62], [-0.45, -0.05, 0.1, 0.62], [0.05, -0.1, -0.48, 0.6], [0.05, 0.6, 0.05, 0.55]]) {
        const g = new THREE.IcosahedronGeometry(r, 1), p = g.attributes.position;
        for (let k = 0; k < p.count; k++) { const f = 1 + (rr() - 0.5) * 0.22; p.setXYZ(k, p.getX(k) * f + x, p.getY(k) * f + y, p.getZ(k) * f + z); }
        g.computeVertexNormals(); lobes.push(g);
      }
      const g = merge(lobes); g.computeVertexNormals(); return shade(g, -0.7, 1.0, 0.4);
    })();
    const firGeo = (() => {
      const tiers = [];
      for (const [y, r, h] of [[-0.42, 1, 0.42], [-0.12, 0.78, 0.38], [0.15, 0.56, 0.34], [0.38, 0.34, 0.24]]) {
        const g = new THREE.ConeGeometry(r, h, 8, 1, true); g.translate(0, y + h / 2, 0);
        const p = g.attributes.position; for (let k = 0; k < p.count; k++) if (p.getY(k) < y + 0.01) p.setY(k, y - 0.04);   // bords tombants
        tiers.push(g);
      }
      const g = merge(tiers); g.computeVertexNormals(); return shade(g, -0.5, 0.62, 0.35);
    })();
    // Immeubles (circuits urbains) : une seule géométrie fusionnée, UV à l'échelle réelle
    const blocked = [];
    if (th.city) {
      const parts = [], cols = [], cc = new THREE.Color();
      const pal = [0xd8d2c4, 0xb9c2cc, 0x9aa8b8, 0xe6e0d4, 0x8f9aa6, 0xc7b299, 0xa7b7c4, 0xd9c7b0];
      let n = 0, tries = 0;
      while (n < th.city.count && tries++ < th.city.count * 40) {
        const x = minX - 250 + rnd() * (maxX - minX + 500), z = minZ - 250 + rnd() * (maxZ - minZ + 500);
        if (inSea(z)) continue;
        const bw = 14 + rnd() * 24, bd = 12 + rnd() * 20;
        const [d, i] = nearest(x, z);
        if (d < Wmax(i) + 6 + Math.max(bw, bd) * 0.62) continue;
        if (Math.min(i, N - i) < 200 && d < 90) continue; // stands, voie des stands et tribunes
        const far = Math.min(1, d / 220);
        const hgt = th.city.hmin + (th.city.hmax - th.city.hmin) * Math.pow(rnd(), 1.6) * (0.4 + 0.6 * far);
        const g = new THREE.BoxGeometry(bw, hgt, bd);
        const uv = g.attributes.uv;
        const dims = [[bd, hgt], [bd, hgt], [0, 0], [0, 0], [bw, hgt], [bw, hgt]];
        for (let f = 0; f < 6; f++) for (let k = 0; k < 4; k++) {
          const id = f * 4 + k;
          if (f === 2 || f === 3) uv.setXY(id, 0.01, 0.99); else uv.setXY(id, uv.getX(id) * dims[f][0] / 10, uv.getY(id) * dims[f][1] / 10);
        }
        g.rotateY(track.H[i]); g.translate(x, Math.min(Y[i], track.ground + 2) + hgt / 2 - 1, z);
        parts.push(g); cc.setHex(pal[(rnd() * pal.length) | 0]);
        for (let k = 0; k < 36; k++) cols.push(cc.r, cc.g, cc.b);
        blocked.push([x, z, Math.max(bw, bd) * 0.7]);
        n++;
      }
      if (parts.length) {
        const geo = merge(parts);
        geo.setAttribute('color', new THREE.Float32BufferAttribute(cols, 3));
        group.add(new THREE.Mesh(geo, L({ map: TX.facade(), vertexColors: true })));
      }
    }

    // Arbres : feuillus, conifères ou palmiers selon le thème
    const nT = th.trees || 0, kind = th.treeKind || 'mix', hue = th.treeHue || [0.24, 0.32];
    const leafMat = L({ map: leafTex, vertexColors: true });
    const crown = new THREE.InstancedMesh(lumpy, leafMat, nT);
    const crownFar = new THREE.InstancedMesh(shade(new THREE.IcosahedronGeometry(1, 1), -0.9, 1, 0.45), leafMat, nT);   // loin de la piste : couronne simple
    const pine = new THREE.InstancedMesh(firGeo, L({ map: leafTex, color: 0x8fae8c, vertexColors: true, side: THREE.DoubleSide }), nT);
    const pineFar = new THREE.InstancedMesh(shade(new THREE.ConeGeometry(0.8, 1, 7).translate(0, 0.05, 0), -0.45, 0.55, 0.4), pine.material, nT);
    // palmier : 9 palmes arquées (bandes effilées qui retombent)
    const palmGeo = (() => {
      const pos = [], uv = [], S = 5;
      for (let f = 0; f < 9; f++) {
        const a = f / 9 * Math.PI * 2 + (f % 2) * 0.2, ca = Math.cos(a), sa = Math.sin(a), lift = f % 2 ? 0.35 : 0.15;
        const P = (t, side) => { const wd = 0.16 * Math.sin(Math.PI * Math.min(1, t * 1.15 + 0.08)) * side, r = t; return [ca * r - sa * wd, lift * Math.sin(t * 2.2) - 0.9 * t * t + 0.25, sa * r + ca * wd]; };
        for (let k = 0; k < S; k++) {
          const t0 = k / S, t1 = (k + 1) / S, A = P(t0, -1), B = P(t0, 1), C = P(t1, -1), D = P(t1, 1);
          pos.push(...A, ...B, ...C, ...B, ...D, ...C); uv.push(0, t0, 1, t0, 0, t1, 1, t0, 1, t1, 0, t1);
        }
      }
      const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); g.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
      g.computeVertexNormals(); return shade(g, -0.7, 0.4, 0.5);
    })();
    const palm = new THREE.InstancedMesh(palmGeo, L({ map: leafTex, color: 0xd8f0b0, vertexColors: true, side: THREE.DoubleSide }), nT);
    const trunk = new THREE.InstancedMesh(new THREE.CylinderGeometry(0.25, 0.4, 1, 5), L({ color: 0x5b4330 }), nT * 2);
    const m = new THREE.Matrix4(), q = new THREE.Quaternion(), e = new THREE.Euler(), col = new THREE.Color();
    const V3 = (a, b, c) => new THREE.Vector3(a, b, c);
    let nc = 0, nf = 0, np = 0, npf = 0, nl = 0, nt = 0, tries = 0;
    while (nc + nf + np + npf + nl < nT && tries++ < nT * 30) {
      const x = minX - 400 + rnd() * (maxX - minX + 800), z = minZ - 400 + rnd() * (maxZ - minZ + 800);
      if (inSea(z)) continue;
      const [d, i] = nearest(x, z);
      if (d < Wmax(i) + 7) continue;
      if (Math.min(i, N - i) < 200 && d < 70) continue;
      if (blocked.some(b => (b[0] - x) ** 2 + (b[1] - z) ** 2 < b[2] * b[2])) continue;
      const t = Math.min(1, Math.max(0, (d - (Wmax(i) + 3)) / (Math.max(75, Wmax(i) + 28) - (Wmax(i) + 3))));
      const y = d < 300 ? Y[i] + (track.ground - Y[i]) * t : track.ground;
      const sc = 0.75 + rnd() * 0.7;
      e.set(0, rnd() * 6.28, 0); q.setFromEuler(e);
      col.setHSL(hue[0] + rnd() * (hue[1] - hue[0]), 0.35 + rnd() * 0.3, 0.4 + rnd() * 0.25);
      const r = rnd();
      const k = kind === 'pine' ? (r < 0.85 ? 'p' : 'c') : kind === 'palm' ? 'l' : kind === 'palmmix' ? (r < 0.55 ? 'l' : 'c') : (r < 0.62 ? 'c' : 'p');
      if (k === 'c') {
        m.compose(V3(x, y + 6.5 * sc, z), q, V3(4.2 * sc, 4.6 * sc, 4.2 * sc));
        if (d < 160) { crown.setMatrixAt(nc, m); crown.setColorAt(nc++, col); } else { crownFar.setMatrixAt(nf, m); crownFar.setColorAt(nf++, col); }
        m.compose(V3(x, y + 1.6 * sc, z), q, V3(sc, 3.2 * sc, sc)); trunk.setMatrixAt(nt++, m);
      } else if (k === 'p') {
        m.compose(V3(x, y + 8 * sc, z), q, V3(3.2 * sc, 13 * sc, 3.2 * sc)); if (d < 160) { pine.setMatrixAt(np, m); pine.setColorAt(np++, col); } else { pineFar.setMatrixAt(npf, m); pineFar.setColorAt(npf++, col); }
        m.compose(V3(x, y + 1 * sc, z), q, V3(sc, 2 * sc, sc)); trunk.setMatrixAt(nt++, m);
      } else {
        m.compose(V3(x, y + 10.3 * sc, z), q, V3(4.2 * sc, 2.6 * sc, 4.2 * sc)); palm.setMatrixAt(nl, m); palm.setColorAt(nl++, col);
        m.compose(V3(x, y + 5 * sc, z), q, V3(0.7 * sc, 10.5 * sc, 0.7 * sc)); trunk.setMatrixAt(nt++, m);
      }
    }
    crown.count = nc; crownFar.count = nf; pine.count = np; pineFar.count = npf; palm.count = nl; trunk.count = nt;
    for (const im of [crown, crownFar, pine, pineFar, palm, trunk]) if (im.count) group.add(im);

    // Montagnes à l'horizon (non affectées par le brouillard, couleurs déjà « brumeuses »)
    const ridge = (R, hMin, hMax, cBase, cTop, seed) => {
      const r = mulberry(seed), segs = 160, pos = [], cols = [], idx = [];
      const ph = [r() * 6, r() * 6, r() * 6, r() * 6];
      const cb = new THREE.Color(cBase), ct = new THREE.Color(cTop), c = new THREE.Color();
      for (let k = 0; k <= segs; k++) {
        const a = k / segs * Math.PI * 2;
        const n = 0.5 + 0.25 * Math.sin(a * 3 + ph[0]) + 0.15 * Math.sin(a * 7 + ph[1]) + 0.07 * Math.sin(a * 17 + ph[2]) + 0.04 * Math.sin(a * 31 + ph[3]);
        const hgt = hMin + (hMax - hMin) * n;
        pos.push(cx + Math.cos(a) * R, track.ground - 30, cz + Math.sin(a) * R, cx + Math.cos(a) * (R - 300), track.ground + hgt, cz + Math.sin(a) * (R - 300));
        c.copy(cb); cols.push(c.r, c.g, c.b); c.copy(cb).lerp(ct, Math.min(1, n * 1.1)); cols.push(c.r, c.g, c.b);
        if (k < segs) { const o = k * 2; idx.push(o, o + 2, o + 1, o + 1, o + 2, o + 3); }
      }
      const g = new THREE.BufferGeometry();
      g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); g.setAttribute('color', new THREE.Float32BufferAttribute(cols, 3)); g.setIndex(idx);
      const mesh = new THREE.Mesh(g, new THREE.MeshBasicMaterial({ vertexColors: true, fog: false, side: THREE.DoubleSide }));
      mesh.renderOrder = -1; return mesh;
    };
    const RR = Math.min(2350, size / 2 + 600);
    const mt = th.mountains || {};
    if (mt.far) group.add(ridge(RR, ...mt.far, 21));
    if (mt.near) group.add(ridge(RR - 450, ...mt.near, 33));
  }

  scene.add(group);
  return { group, lights };
}
