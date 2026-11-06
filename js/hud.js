// HUD en DOM : position, tour, chronos, vitesse, rapport, aspiration, feux, mini-carte, tour des écarts.
import { DS } from './track.js';
import { SPEC } from './car.js';
import { TYRES, MODES, fuelLaps } from './strategy.js';

// éléments mis en cache et écritures seulement si la valeur change (évite des recalculs de mise en page)
const EL = {}, last = {};
const $ = id => EL[id] || (EL[id] = document.getElementById(id));
const txt = (id, v) => { if (last[id] !== v) { last[id] = v; $(id).textContent = v; } };
const html = (id, v) => { if (last['h' + id] !== v) { last['h' + id] = v; $(id).innerHTML = v; } };
const tog = (id, c, on) => { const k = id + '.' + c; if (last[k] !== on) { last[k] = on; $(id).classList.toggle(c, on); } };
let mm, mctx, mapImg, mapTf, frame = 0;

export const fmt = t => {
  if (!t || t < 0) return '--:--.---';
  const m = Math.floor(t / 60), s = t - m * 60;
  return m + ':' + (s < 10 ? '0' : '') + s.toFixed(3);
};

export function initHud(track) {
  mm = $('minimap'); mctx = mm.getContext('2d');
  const S = mm.width = mm.height = 140;
  let minX = Infinity, maxX = -Infinity, minZ = Infinity, maxZ = -Infinity;
  for (let i = 0; i < track.N; i++) { minX = Math.min(minX, track.X[i]); maxX = Math.max(maxX, track.X[i]); minZ = Math.min(minZ, track.Z[i]); maxZ = Math.max(maxZ, track.Z[i]); }
  const sc = (S - 16) / Math.max(maxX - minX, maxZ - minZ);
  // x vers la gauche pour que la carte corresponde à la vue (z vers le haut)
  mapTf = (x, z) => [S - (8 + (x - minX) * sc) - (S - 16 - (maxX - minX) * sc) / 2, S - 8 - (z - minZ) * sc - (S - 16 - (maxZ - minZ) * sc) / 2];
  mapImg = document.createElement('canvas'); mapImg.width = mapImg.height = S;
  const c = mapImg.getContext('2d');
  c.lineJoin = 'round'; c.strokeStyle = 'rgba(255,255,255,.85)'; c.lineWidth = 4; c.beginPath();
  for (let i = 0; i <= track.N; i += 4) { const [x, y] = mapTf(track.X[i % track.N], track.Z[i % track.N]); i ? c.lineTo(x, y) : c.moveTo(x, y); }
  c.closePath(); c.stroke();
  const [sx, sy] = mapTf(track.X[0], track.Z[0]); c.fillStyle = '#fff'; c.fillRect(sx - 3, sy - 3, 6, 6);
}

export function updateHud(R, fps) {
  const p = R.player, n = R.cars.length;
  const pos = R.order.indexOf(p) + 1;
  frame++;
  txt('spd', Math.round(Math.abs(p.v) * 3.6));
  txt('gear', p.gear);
  const f = Math.max(0, p.v) / (SPEC.vtop * p.perf), rw = p.gear === 'N' ? 15 : Math.round(40 + 60 * ((f * 9) % 1));
  if (last.rpm !== rw) { last.rpm = rw; $('rpm').style.transform = `scaleX(${rw / 100})`; }
  tog('slip', 'show', p.slipstream > 0.25 && R.phase === 'race');
  const lc = R.phase === 'lights' ? 'n' + R.lightsOn : (R.t - R.goAt < 1 ? 'go' : 'off');
  if (last.lights !== lc) { last.lights = lc; $('lights').className = lc; }
  const radioOn = R.radioT > 0 && !!R.radioMsg;
  if (radioOn) txt('radio', R.radioMsg);
  tog('radio', 'show', radioOn);
  tog('scBanner', 'show', !!R.sc);
  if (frame % 6) return;
  // pneus, météo, dégâts, stand
  const T = TYRES[p.tyre];
  html('tyreChip', `<b style="--c:${T.color}">${p.tyre}</b>${Math.max(0, Math.round(100 - p.wear * 100))} %` + (p.box && !p.pit ? ` → <b style="--c:${TYRES[p.box.tyre].color}">${p.box.tyre}</b>` : ''));
  txt('fuelIco', R.solo ? '' : `⛽ ${Math.round(p.fuel)} kg · ${fuelLaps(R, p).toFixed(1)} t` + (p.mode !== 'norm' ? ` · ${MODES[p.mode].name}` : ''));
  tog('fuelIco', 'low', !R.solo && fuelLaps(R, p) < 1.5);
  tog('tyreChip', 'box', !!p.box);
  txt('wxIco', R.raining ? '🌧️' : R.wet > 0.1 ? '💧' : '');
  txt('dmgIco', p.dmg > 0.12 ? `⚠ aileron ${Math.round(p.dmg * 100)} %` : p.powerLoss ? '⚠ moteur' : '');
  tog('boxBtn', 'hidden', !!R.solo); tog('radioBtn', 'hidden', !!R.solo);
  tog('boxBtn', 'on', !!p.box || !!p.pit);
  txt('boxBtn', p.pit ? '🔧 AU STAND' : p.box ? '🔧 BOX ✓' : '🔧 STAND');
  html('pos', `P${pos}<small>/${n}</small>`);
  txt('lap', R.solo ? (p.lap < 1 ? 'Tour de lancement' : 'Tour chrono') : `Tour ${Math.max(1, Math.min(p.lap, R.laps))}/${R.laps}`);
  txt('cur', R.phase === 'race' && !p.finished ? fmt(R.t - p.lapStart) : fmt(0));
  txt('best', 'Meilleur ' + fmt(p.bestLap));
  txt('fps', fps || '');
  // tour des écarts : 2 devant, 2 derrière
  const k0 = Math.max(0, Math.min(n - 5, pos - 3));
  let h = '';
  for (let k = k0; k < Math.min(n, k0 + 5); k++) {
    const c = R.order[k];
    let gap = '';
    if (c !== p) {
      const d = ((c.lap * R.track.N + c.idx) - (p.lap * R.track.N + p.idx)) * DS;
      gap = (d > 0 ? '+' : '−') + (Math.abs(d) / Math.max(30, Math.abs(p.v) || 30)).toFixed(1);
    }
    h += `<div class="${c === p ? 'me' : ''}"><b>${k + 1}</b><i style="background:#${c.team.color.toString(16).padStart(6, '0')}"></i>${c.name}<span>${gap}</span></div>`;
  }
  html('tower', h);
  // mini-carte
  mctx.clearRect(0, 0, 140, 140); mctx.drawImage(mapImg, 0, 0);
  for (const c of R.cars) {
    if (c === p) continue;
    const [x, y] = mapTf(c.x, c.z);
    mctx.fillStyle = '#' + c.team.color.toString(16).padStart(6, '0'); mctx.fillRect(x - 2.5, y - 2.5, 5, 5);
  }
  if (R.sc) { const [sx, sy] = mapTf(R.sc.car.x, R.sc.car.z); mctx.fillStyle = '#ffb300'; mctx.fillRect(sx - 3.5, sy - 3.5, 7, 7); }
  const [x, y] = mapTf(p.x, p.z);
  mctx.fillStyle = '#fff'; mctx.beginPath(); mctx.arc(x, y, 4.5, 0, 7); mctx.fill();
  mctx.fillStyle = '#e10600'; mctx.beginPath(); mctx.arc(x, y, 3, 0, 7); mctx.fill();
}
