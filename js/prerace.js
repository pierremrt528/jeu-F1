// Écran d'avant-course : 3 stratégies recommandées, relais modifiables (gomme, tours, arrêts),
// graphique de l'usure des pneus et du rythme estimé tour par tour, carburant au départ.
import { TYRES, TANK, wxText, makePlans, planInfo, fuelFor, lifeBand } from './strategy.js';

const chip = t => `<b class="tchip" style="--c:${TYRES[t].color}">${t}</b>`;
const mmss = s => `${Math.floor(s / 60)} min ${String(Math.round(s % 60)).padStart(2, '0')}`;
const lapFmt = s => `${Math.floor(s / 60)}:${(s % 60).toFixed(1).padStart(4, '0')}`;
const dt = d => (d < 0.05 ? '' : '+' + d.toFixed(1).replace('.', ',') + ' s');
const BAND = { max: '#3ddc84', bon: '#a6e05a', faible: '#ffb340', nul: '#ff5a4a' };

// barre des relais : largeur proportionnelle aux tours, couleur de la gomme
const barHTML = (st, sel = -1, tag = 'button') => st.map((s, i) => `<${tag} data-st="${i}" class="${i === sel ? 'on' : ''}" style="flex:${s.laps};--c:${TYRES[s.tyre].color}"><b>${s.tyre}</b><span>${s.laps}</span></${tag}>`).join('');

function chartSVG(info, laps) {
  const W = 420, H = 104, x0 = 22, x1 = W - 34, y0 = 6, y1 = H - 16;
  const X = l => x0 + (x1 - x0) * l / laps, Y = r => y1 - (y1 - y0) * Math.max(0, Math.min(1, r));
  let h = '';
  // zones d'adhérence : max, bon, faible, nul
  [[1, 0.85, 'max'], [0.85, 0.6, 'bon'], [0.6, 0.4, 'faible'], [0.4, 0, 'nul']].forEach(([a, b, k]) => {
    h += `<rect x="${x0}" y="${Y(a)}" width="${x1 - x0}" height="${Y(b) - Y(a)}" fill="${BAND[k]}" opacity=".13"/>`;
    if (a < 1) h += `<text x="${x0 - 3}" y="${Y(a) + 3}" class="ax" text-anchor="end">${Math.round(a * 100)}</text>`;
  });
  // rythme estimé (axe de droite)
  const lt = info.laps.map(l => l.lt), lo = Math.min(...lt), hi = Math.max(lo + 1, ...lt);
  const P = v => y0 + 4 + (y1 - y0 - 8) * (v - lo) / (hi - lo);   // plus haut = plus rapide
  h += `<polyline class="pace" points="${info.laps.map(l => `${X(l.lap - 0.5).toFixed(1)},${P(l.lt).toFixed(1)}`).join(' ')}"/>`;
  h += `<text x="${x1 + 3}" y="${y0 + 8}" class="ax">${lapFmt(lo)}</text><text x="${x1 + 3}" y="${y1 - 1}" class="ax">${lapFmt(hi)}</text>`;
  // usure : dents de scie, une couleur par gomme ; arrêts en pointillés
  let start = 0;
  info.stints.forEach((s, i) => {
    const L = info.laps.filter(l => l.stint === i);
    if (i) h += `<line x1="${X(start)}" x2="${X(start)}" y1="${y0}" y2="${y1}" class="stop"/><text x="${X(start)}" y="${H - 4}" class="ax" text-anchor="middle">T${start}</text>`;
    const pts = [[X(start), Y(1)], ...L.map(l => [X(l.lap), Y(l.r)])];
    h += `<polyline class="wear" stroke="${TYRES[s.tyre].color}" points="${pts.map(p => p.map(v => v.toFixed(1)).join(',')).join(' ')}"/>`;
    const e = L[L.length - 1];
    if (e) h += `<circle cx="${X(e.lap)}" cy="${Y(e.r)}" r="3" fill="${BAND[lifeBand(e.r)]}"/>`;
    start += s.laps;
  });
  h += `<text x="${x0}" y="${H - 4}" class="ax">T0</text><text x="${x1}" y="${H - 4}" class="ax" text-anchor="end">T${laps}</text>`;
  return `<svg viewBox="0 0 ${W} ${H}" class="prchart">${h}</svg>`;
}

// race : { laps, lapKm, lapTime, wx, title } ; cb : { go(plan), sim(plan), back() }
export function showPreraceScreen($, race, cb) {
  const { laps, lapKm, lapTime, wx } = race, wet = !!wx.rain;
  const plans = makePlans(laps, lapKm, lapTime, wet), ref = plans[0].time;
  let sel = 0, cur, stint = 0, fOff = 0, custom = false;
  const info = () => planInfo(cur, lapKm, lapTime, wet);
  const choose = k => { sel = k; cur = plans[k].stints.map(s => ({ ...s })); stint = 0; fOff = 0; custom = false; };
  choose(0);
  const edit = f => { f(); custom = true; render(); };
  function render() {
    const I = info(), need = fuelFor(cur[0].laps, lapKm), fuel = Math.max(10, Math.min(TANK, need + fOff));
    $('prTitle').textContent = race.title;
    $('prWx').textContent = 'Météo : ' + wxText(wx);
    $('prPlans').innerHTML = plans.map((p, k) => `<button data-plan="${k}" class="${k === sel && !custom ? 'pick' : ''}">
      <span class="pn"><b>${p.best ? '★ ' : ''}${p.name}</b><small>${p.stops ? p.stops + ' arrêt' + (p.stops > 1 ? 's' : '') : 'Sans arrêt'} · ${p.best ? '≈ ' + mmss(p.time) : dt(p.time - ref)}</small></span>
      <span class="mbar">${barHTML(p.stints, -1, 'i')}</span></button>`).join('');
    $('prSel').innerHTML = `<b>${custom ? '✎ Ma stratégie' : plans[sel].name}</b> · ${I.stops ? I.stops + ' arrêt' + (I.stops > 1 ? 's' : '') : 'sans arrêt'} · ≈ ${mmss(I.time)}${I.time - ref >= 0.05 ? ` <i>(${dt(I.time - ref)})</i>` : ''}`;
    $('prBar').innerHTML = barHTML(cur, stint);
    const s = cur[stint], canLaps = cur.length > 1;
    $('prEdit').innerHTML = `<span>Relais ${stint + 1} :</span>` + Object.keys(TYRES).map(t => `<button data-ty="${t}" class="${s.tyre === t ? 'on' : ''}" title="${TYRES[t].name}">${chip(t)}</button>`).join('') +
      `<span class="sep"></span><button data-l="-1" ${canLaps && s.laps > 1 ? '' : 'disabled'}>−</button><b>${s.laps} t</b><button data-l="1" ${canLaps ? '' : 'disabled'}>+</button>` +
      `<span class="sep"></span><button data-add ${s.laps > 1 && cur.length < 5 ? '' : 'disabled'}>＋ arrêt</button><button data-del ${cur.length > 1 ? '' : 'disabled'}>✕</button>`;
    $('prChart').innerHTML = chartSVG(I, laps);
    $('prAdjust').innerHTML = `<span>Carburant au départ</span><button id="prF-">−</button><b>${fuel} kg</b><button id="prF+">+</button>`;
    // alertes : relais trop longs, carburant, pneus inadaptés à la météo
    const warn = [];
    let end = 0;
    I.stints.forEach((st, i) => {
      end += st.laps;
      const r = I.laps[end - 1].r;
      if (r < 0) warn.push(`💥 Relais ${i + 1} : pneus à 0 % avant le tour ${end}, crevaison probable !`);
      else if (r < 0.395) warn.push(`⚠ Relais ${i + 1} : pneus à ${Math.round(r * 100)} % au tour ${end}, presque plus de grip.`);
    });
    if (fuel < need) warn.push(`⚠ Pas assez de carburant pour ${cur[0].laps} tours (il faut ${need} kg).`);
    if (cur.some(x => (x.tyre === 'W') !== wet)) warn.push(wet ? '⚠ Slicks sur piste mouillée : très peu d’adhérence.' : '⚠ Pneus pluie sur le sec : lents et usés très vite.');
    $('prHint').textContent = warn.length ? warn.join(' ') : custom ? 'Touche un relais pour changer sa gomme ou sa durée.' : plans[sel].desc + ' Touche un relais pour le modifier.';
    $('prHint').classList.toggle('warn', warn.length > 0);
    const plan = { name: custom ? 'Ma stratégie' : plans[sel].name, stints: cur.map(x => ({ ...x })), stops: I.stops, time: I.time, fuel };
    $('prGo').onclick = () => cb.go(plan);
    $('prSim').onclick = () => cb.sim(plan);
    bind(plan);
  }
  function bind() {
    const q = s => document.querySelectorAll(s);
    q('[data-plan]').forEach(b => b.onclick = () => { choose(+b.dataset.plan); render(); });
    q('#prBar [data-st]').forEach(b => b.onclick = () => { stint = +b.dataset.st; render(); });
    q('#prEdit [data-ty]').forEach(b => b.onclick = () => edit(() => { cur[stint].tyre = b.dataset.ty; }));
    q('#prEdit [data-l]').forEach(b => b.onclick = () => edit(() => {
      const d = +b.dataset.l, o = stint < cur.length - 1 ? stint + 1 : stint - 1;   // le relais voisin compense
      if (cur[o].laps - d < 1 || cur[stint].laps + d < 1) return;
      cur[stint].laps += d; cur[o].laps -= d;
    }));
    q('#prEdit [data-add]').forEach(b => b.onclick = () => edit(() => {
      const s = cur[stint], a = Math.ceil(s.laps / 2);
      cur.splice(stint + 1, 0, { tyre: s.tyre, laps: s.laps - a }); s.laps = a;
    }));
    q('#prEdit [data-del]').forEach(b => b.onclick = () => edit(() => {
      const [s] = cur.splice(stint, 1); stint = Math.max(0, stint - 1);
      cur[stint].laps += s.laps;
    }));
    $('prF-').onclick = () => { fOff -= 5; render(); };
    $('prF+').onclick = () => { fOff += 5; render(); };
    $('prBack').onclick = cb.back;
  }
  render();
}
