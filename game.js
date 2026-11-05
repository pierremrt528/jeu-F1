'use strict';
// Grand Prix Manager — étape 1 : prototype de course vue du dessus.
// Monde en "unités" ; la caméra suit la voiture du joueur, toujours orientée vers le haut.

// ---------- Données (fictives) ----------
const LAPS = 3;
const TRACK_WIDTH = 110;
const CAR_LEN = 34, CAR_WID = 16;
const PLAYER_VMAX = 430;

const TEAMS = [
  { name: 'Écurie Martel',     driver: 'Toi',          color: '#e10600', player: true },
  { name: 'Volta GP',          driver: 'L. Ferrand',   color: '#1e6bff', skill: 0.97 },
  { name: 'Aurora Racing',     driver: 'K. Lindqvist', color: '#00c2a8', skill: 0.95 },
  { name: 'Scuderia Fulmine',  driver: 'M. Bellandi',  color: '#ffcc00', skill: 0.93 },
  { name: 'Nordwind Motorsport', driver: 'J. Halvorsen', color: '#ff7a00', skill: 0.91 },
  { name: 'Kestrel F.Team',    driver: 'D. Okafor',    color: '#b45cff', skill: 0.89 },
];

// Circuit de Valmont : points de contrôle, lissés en spline
const CONTROL = [
  [0, 0], [600, -50], [1100, 0], [1400, 250], [1350, 600], [1000, 750], [800, 1000],
  [900, 1350], [1300, 1500], [1400, 1900], [1100, 2200], [600, 2200], [200, 1950],
  [-100, 1500], [-300, 1000], [-200, 500], [-250, 200],
];

// ---------- Construction du circuit ----------
function buildTrack(ctrl, spacing) {
  const raw = [];
  const n = ctrl.length;
  for (let i = 0; i < n; i++) {
    const p0 = ctrl[(i - 1 + n) % n], p1 = ctrl[i], p2 = ctrl[(i + 1) % n], p3 = ctrl[(i + 2) % n];
    for (let s = 0; s < 30; s++) {
      const t = s / 30, t2 = t * t, t3 = t2 * t;
      const f = (a, b, c, d) => 0.5 * (2 * b + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3);
      raw.push([f(p0[0], p1[0], p2[0], p3[0]), f(p0[1], p1[1], p2[1], p3[1])]);
    }
  }
  // ré-échantillonnage à pas constant
  const pts = [raw[0]];
  let carry = 0;
  for (let i = 1; i <= raw.length; i++) {
    const a = raw[i - 1], b = raw[i % raw.length];
    let seg = Math.hypot(b[0] - a[0], b[1] - a[1]), pos = spacing - carry;
    while (pos <= seg) {
      const t = pos / seg;
      pts.push([a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t]);
      pos += spacing;
    }
    carry = seg - (pos - spacing);
  }
  if (Math.hypot(pts[pts.length - 1][0] - pts[0][0], pts[pts.length - 1][1] - pts[0][1]) < spacing * 0.5) pts.pop();
  const N = pts.length;
  const dir = pts.map((p, i) => { const q = pts[(i + 1) % N]; return Math.atan2(q[1] - p[1], q[0] - p[0]); });
  // courbure devant chaque point (pour la vitesse cible de l'IA)
  const curve = dir.map((d, i) => Math.abs(angDiff(dir[(i + 8) % N], d)));
  return { pts, dir, curve, N };
}

function angDiff(a, b) { let d = a - b; while (d > Math.PI) d -= 2 * Math.PI; while (d < -Math.PI) d += 2 * Math.PI; return d; }
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));

const track = buildTrack(CONTROL, 10);

// ---------- Canvas & entrées ----------
const canvas = document.getElementById('game');
const ctx = canvas.getContext('2d');
let W = 0, H = 0, DPR = 1;
function resize() {
  DPR = Math.min(window.devicePixelRatio || 1, 2);
  W = window.innerWidth; H = window.innerHeight;
  canvas.width = W * DPR; canvas.height = H * DPR;
}
window.addEventListener('resize', resize);
resize();

const input = { left: false, right: false, gas: false, brake: false };
const touches = new Map(); // pointerId -> bouton

function buttons() {
  const m = 14, s = Math.min((W - 5 * m) / 4, 96), y = H - s - m - 10;
  return [
    { id: 'left',  x: m,             y, w: s, h: s, label: '◀' },
    { id: 'right', x: m * 2 + s,     y, w: s, h: s, label: '▶' },
    { id: 'brake', x: W - 2 * s - m * 2, y, w: s, h: s, label: 'FREIN' },
    { id: 'gas',   x: W - s - m,     y: y - s * 0.35, w: s, h: s * 1.35, label: 'GAZ' },
  ];
}
function hit(x, y) {
  for (const b of buttons()) if (x >= b.x - 8 && x <= b.x + b.w + 8 && y >= b.y - 8 && y <= b.y + b.h + 8) return b.id;
  return null;
}
function refreshInput() {
  input.left = input.right = input.gas = input.brake = false;
  for (const id of touches.values()) if (id) input[id] = true;
  for (const k of keys) {
    if (k === 'ArrowLeft') input.left = true;
    if (k === 'ArrowRight') input.right = true;
    if (k === 'ArrowUp') input.gas = true;
    if (k === 'ArrowDown') input.brake = true;
  }
}
canvas.addEventListener('pointerdown', e => { touches.set(e.pointerId, hit(e.clientX, e.clientY)); refreshInput(); e.preventDefault(); });
canvas.addEventListener('pointermove', e => { if (touches.has(e.pointerId)) { touches.set(e.pointerId, hit(e.clientX, e.clientY)); refreshInput(); } });
const up = e => { touches.delete(e.pointerId); refreshInput(); };
['pointerup', 'pointercancel', 'pointerleave'].forEach(t => canvas.addEventListener(t, up));
const keys = new Set();
window.addEventListener('keydown', e => { keys.add(e.key); refreshInput(); });
window.addEventListener('keyup', e => { keys.delete(e.key); refreshInput(); });

// ---------- Course ----------
let cars = [], state = 'menu', raceTime = 0, countdown = 0, endTimer = 0;

function newRace() {
  // grille : 2 colonnes derrière la ligne, le joueur part 4e
  const order = [1, 2, 3, 0, 4, 5];
  cars = order.map((t, slot) => {
    const team = TEAMS[t];
    const idx = (track.N - 4 - slot * 5) % track.N;
    const p = track.pts[idx], d = track.dir[idx];
    const side = slot % 2 ? 1 : -1;
    return {
      team, player: !!team.player,
      x: p[0] - Math.sin(d) * side * 22, y: p[1] + Math.cos(d) * side * 22,
      h: d, v: 0, idx, lap: 0, finished: false, finishTime: 0,
      vmax: team.player ? PLAYER_VMAX : PLAYER_VMAX * team.skill,
      lane: (Math.random() - 0.5) * 30, lapStart: 0, bestLap: Infinity,
    };
  });
  raceTime = 0; countdown = 3.5; endTimer = 0; state = 'race';
}

function nearestIdx(car) {
  let best = car.idx, bd = Infinity;
  for (let k = -40; k <= 40; k++) {
    const i = (car.idx + k + track.N) % track.N, p = track.pts[i];
    const d = (p[0] - car.x) ** 2 + (p[1] - car.y) ** 2;
    if (d < bd) { bd = d; best = i; }
  }
  return { i: best, dist: Math.sqrt(bd) };
}

function aiControl(c) {
  const N = track.N;
  const look = 10 + Math.floor(c.v / 35);
  const ti = (c.idx + look) % N, tp = track.pts[ti], td = track.dir[ti];
  const tx = tp[0] - Math.sin(td) * c.lane, ty = tp[1] + Math.cos(td) * c.lane;
  const steer = clamp(angDiff(Math.atan2(ty - c.y, tx - c.x), c.h) * 2.5, -1, 1);
  let curve = 0;
  for (let k = 0; k < 35; k += 3) curve = Math.max(curve, track.curve[(c.idx + k) % N] * (1 - k / 70));
  const target = c.vmax * clamp(1 - curve * 0.95, 0.38, 1);
  return { steer, gas: c.v < target, brake: c.v > target + 25 };
}

function physics(c, ctl, dt) {
  const near = nearestIdx(c);
  const grass = near.dist > TRACK_WIDTH / 2;
  const vmax = grass ? c.vmax * 0.45 : c.vmax;
  if (ctl.gas) c.v += 300 * (1 - c.v / vmax) * dt;
  else c.v -= 70 * dt;
  if (ctl.brake) c.v -= 520 * dt;
  if (c.v > vmax) c.v -= 400 * dt;
  c.v = Math.max(0, c.v);
  const turn = 2.6 * (1 - 0.4 * c.v / PLAYER_VMAX) * Math.min(1, c.v / 90);
  c.h += ctl.steer * turn * dt;
  c.x += Math.cos(c.h) * c.v * dt;
  c.y += Math.sin(c.h) * c.v * dt;
  // passage de ligne
  const N = track.N, prev = c.idx;
  c.idx = nearestIdx(c).i;
  if (prev > N * 0.85 && c.idx < N * 0.15) {
    c.lap++;
    if (c.lap > 1) c.bestLap = Math.min(c.bestLap, raceTime - c.lapStart);
    c.lapStart = raceTime;
    if (c.lap > LAPS && !c.finished) { c.finished = true; c.finishTime = raceTime; }
  } else if (prev < N * 0.15 && c.idx > N * 0.85) c.lap--;
}

function collide() {
  for (let i = 0; i < cars.length; i++) for (let j = i + 1; j < cars.length; j++) {
    const a = cars[i], b = cars[j], dx = b.x - a.x, dy = b.y - a.y, d = Math.hypot(dx, dy), min = 26;
    if (d > 0 && d < min) {
      const push = (min - d) / 2, nx = dx / d, ny = dy / d;
      a.x -= nx * push; a.y -= ny * push; b.x += nx * push; b.y += ny * push;
      a.v *= 0.985; b.v *= 0.985;
    }
  }
}

const progress = c => c.finished ? 1e9 - c.finishTime : c.lap * track.N + c.idx;
const standings = () => [...cars].sort((a, b) => progress(b) - progress(a));

function update(dt) {
  if (state !== 'race') return;
  if (countdown > 0) { countdown -= dt; return; }
  raceTime += dt;
  for (const c of cars) {
    let ctl;
    if (c.finished) ctl = { steer: aiControl(c).steer, gas: false, brake: c.v > 120 };
    else if (c.player) ctl = { steer: (input.right ? 1 : 0) - (input.left ? 1 : 0), gas: input.gas, brake: input.brake };
    else ctl = aiControl(c);
    physics(c, ctl, dt);
  }
  collide();
  const me = cars.find(c => c.player);
  if (me.finished) {
    endTimer += dt;
    if (cars.every(c => c.finished) || endTimer > 20) showResults();
  }
}

// ---------- Rendu ----------
function drawCar(c) {
  ctx.save();
  ctx.translate(c.x, c.y); ctx.rotate(c.h);
  const L = CAR_LEN, Wd = CAR_WID;
  ctx.fillStyle = '#111';
  for (const [x, y] of [[-L * 0.32, -Wd * 0.62], [-L * 0.32, Wd * 0.62], [L * 0.28, -Wd * 0.58], [L * 0.28, Wd * 0.58]])
    ctx.fillRect(x - 4.5, y - 3, 9, 6);
  ctx.fillStyle = c.team.color;
  ctx.fillRect(-L * 0.45, -Wd * 0.62, 5, Wd * 1.24);                // aileron arrière
  ctx.fillRect(L * 0.42, -Wd * 0.6, 4, Wd * 1.2);                   // aileron avant
  ctx.beginPath();
  ctx.moveTo(-L * 0.42, -Wd * 0.32); ctx.lineTo(L * 0.1, -Wd * 0.38);
  ctx.lineTo(L * 0.5, -Wd * 0.1); ctx.lineTo(L * 0.5, Wd * 0.1);
  ctx.lineTo(L * 0.1, Wd * 0.38); ctx.lineTo(-L * 0.42, Wd * 0.32);
  ctx.closePath(); ctx.fill();
  ctx.fillStyle = '#0b0d10'; ctx.beginPath(); ctx.arc(-L * 0.02, 0, 3.6, 0, Math.PI * 2); ctx.fill(); // cockpit
  ctx.restore();
}

function trackPath() {
  ctx.beginPath();
  track.pts.forEach((p, i) => i ? ctx.lineTo(p[0], p[1]) : ctx.moveTo(p[0], p[1]));
  ctx.closePath();
}

function render() {
  ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
  ctx.fillStyle = '#2f7a35'; ctx.fillRect(0, 0, W, H);
  const me = cars.find(c => c.player) || { x: track.pts[0][0], y: track.pts[0][1], h: track.dir[0] };
  const zoom = Math.min(W, H) / 430;
  ctx.save();
  ctx.translate(W / 2, H * 0.62);
  ctx.scale(zoom, zoom);
  ctx.rotate(-me.h - Math.PI / 2);
  ctx.translate(-me.x, -me.y);
  ctx.lineJoin = 'round'; ctx.lineCap = 'round';
  // vibreurs
  trackPath(); ctx.lineWidth = TRACK_WIDTH + 12; ctx.strokeStyle = '#eee'; ctx.stroke();
  ctx.lineCap = 'butt'; ctx.setLineDash([16, 16]); ctx.strokeStyle = '#d22'; ctx.stroke(); ctx.setLineDash([]); ctx.lineCap = 'round';
  trackPath(); ctx.lineWidth = TRACK_WIDTH; ctx.strokeStyle = '#4a4d52'; ctx.stroke();
  // ligne de départ en damier
  const p = track.pts[0], d = track.dir[0];
  ctx.save(); ctx.translate(p[0], p[1]); ctx.rotate(d);
  for (let r = 0; r < 2; r++) for (let k = 0; k < 11; k++) {
    ctx.fillStyle = (r + k) % 2 ? '#fff' : '#111';
    ctx.fillRect(r * 8 - 8, -TRACK_WIDTH / 2 + k * TRACK_WIDTH / 11, 8, TRACK_WIDTH / 11);
  }
  ctx.restore();
  for (const c of cars) drawCar(c);
  ctx.restore();
  if (state === 'race') drawHud();
}

function fmt(t) {
  if (!isFinite(t)) return '—';
  const m = Math.floor(t / 60), s = (t % 60).toFixed(2).padStart(5, '0');
  return `${m}:${s}`;
}

function drawHud() {
  const me = cars.find(c => c.player), st = standings(), pos = st.indexOf(me) + 1;
  ctx.fillStyle = 'rgba(10,12,16,.75)';
  ctx.fillRect(10, 10, 150, 86);
  ctx.fillStyle = '#fff'; ctx.font = 'italic 800 26px system-ui';
  ctx.fillText(`P${pos}/${cars.length}`, 20, 40);
  ctx.font = '600 15px system-ui';
  ctx.fillText(`Tour ${clamp(me.lap, 1, LAPS)}/${LAPS}`, 20, 62);
  ctx.fillText(`${Math.round(me.v * 0.75)} km/h · ${fmt(raceTime)}`, 20, 84);
  drawMinimap(W - 110, 10, 100, 130);
  if (countdown > 0) {
    const n = Math.ceil(countdown - 0.5);
    ctx.fillStyle = 'rgba(10,12,16,.75)'; ctx.fillRect(W / 2 - 90, H * 0.28, 180, 60);
    for (let i = 0; i < 3; i++) {
      ctx.fillStyle = n <= 0 ? '#2c2' : (3 - i <= n ? '#333' : '#e10600');
      ctx.beginPath(); ctx.arc(W / 2 - 50 + i * 50, H * 0.28 + 30, 18, 0, Math.PI * 2); ctx.fill();
    }
  }
  for (const b of buttons()) {
    ctx.fillStyle = input[b.id] ? 'rgba(255,255,255,.4)' : 'rgba(255,255,255,.16)';
    ctx.beginPath(); ctx.roundRect(b.x, b.y, b.w, b.h, 16); ctx.fill();
    ctx.fillStyle = '#fff'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    ctx.font = `800 ${b.label.length > 2 ? 17 : 30}px system-ui`;
    ctx.fillText(b.label, b.x + b.w / 2, b.y + b.h / 2);
    ctx.textAlign = 'start'; ctx.textBaseline = 'alphabetic';
  }
}

let mini = null;
function drawMinimap(x, y, w, h) {
  if (!mini) {
    const xs = track.pts.map(p => p[0]), ys = track.pts.map(p => p[1]);
    mini = { minX: Math.min(...xs), minY: Math.min(...ys), sw: Math.max(...xs) - Math.min(...xs), sh: Math.max(...ys) - Math.min(...ys) };
  }
  const s = Math.min((w - 16) / mini.sw, (h - 16) / mini.sh);
  const tx = px => x + 8 + (px - mini.minX) * s, ty = py => y + 8 + (py - mini.minY) * s;
  ctx.fillStyle = 'rgba(10,12,16,.75)'; ctx.fillRect(x, y, w, h);
  ctx.beginPath();
  track.pts.forEach((p, i) => i ? ctx.lineTo(tx(p[0]), ty(p[1])) : ctx.moveTo(tx(p[0]), ty(p[1])));
  ctx.closePath(); ctx.strokeStyle = '#999'; ctx.lineWidth = 3; ctx.stroke();
  for (const c of cars) {
    ctx.fillStyle = c.team.color; ctx.beginPath();
    ctx.arc(tx(c.x), ty(c.y), c.player ? 5 : 3.5, 0, Math.PI * 2); ctx.fill();
    if (c.player) { ctx.strokeStyle = '#fff'; ctx.lineWidth = 1.5; ctx.stroke(); }
  }
}

// ---------- Écrans ----------
const $ = id => document.getElementById(id);
function showResults() {
  state = 'results';
  const st = standings(), winner = st[0];
  $('resultsTable').innerHTML = st.map((c, i) => {
    const gap = !c.finished ? 'n.c.' : i === 0 ? fmt(c.finishTime) : '+' + (c.finishTime - winner.finishTime).toFixed(2) + 's';
    return `<tr class="${c.player ? 'me' : ''}"><td class="pos">${i + 1}</td>
      <td><span class="swatch" style="background:${c.team.color}"></span>${c.team.driver}<span class="team">${c.team.name}</span></td>
      <td class="time">${gap}<br><small>MT ${fmt(c.bestLap)}</small></td></tr>`;
  }).join('');
  $('results').classList.remove('hidden');
}
$('startBtn').onclick = () => { $('menu').classList.add('hidden'); newRace(); };
$('againBtn').onclick = () => { $('results').classList.add('hidden'); newRace(); };

// ---------- Boucle ----------
let last = performance.now();
function loop(now) {
  const dt = Math.min(0.05, (now - last) / 1000); last = now;
  update(dt); render();
  requestAnimationFrame(loop);
}
requestAnimationFrame(loop);
