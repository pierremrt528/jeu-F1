// Réglages de la voiture avant la séance : 5 préréglages, de l'appui maximal à la vitesse maximale, avec un effet réel
// sur la physique (appui u.kd, vitesse de pointe u.vtop). Écarts calibrés sur les 22 circuits, avec des rendements
// décroissants aux extrêmes : chaque réglage est le meilleur quelque part, l'écart entre le meilleur et le pire est de
// 0,1 à 0,8 s au tour. L'ingénieur conseille le réglage qui donne le meilleur tour simulé (lapSim).
// Bandeau de séance : températures (weekendTemps), météo, état de la piste. Parc fermé : le réglage choisi avant les
// qualifications reste pour la course. L'atelier 3D (showroom.js) est dessiné derrière l'écran.
import { SPEC } from './car.js';
import { THEMES } from './data.js';
import { frame } from './showroom.js';

export const SETUPS = [
  { name: 'Appui maxi', kd: 0.0375, vtop: -0.04 },
  { name: 'Appui élevé', kd: 0.025, vtop: -0.02 },
  { name: 'Équilibré', kd: 0, vtop: 0 },
  { name: 'Vitesse élevée', kd: -0.025, vtop: 0.02 },
  { name: 'Vitesse maxi', kd: -0.05, vtop: 0.03 },
];
export function applySetup(up, k) {
  const s = SETUPS[k] || SETUPS[2];
  up.kd = (up.kd ?? 1) * (1 + s.kd); up.vtop = (up.vtop ?? 1) * (1 + s.vtop);
  return up;
}

// tour simulé avec un réglage : vitesse limite en virage (adhérence + appui), freinages puis accélérations.
// full = part du tour à pleine charge (la voiture accélère encore ou tient sa vitesse de pointe)
const DS = 2;
function lapSim(track, up, k, wet, perf = 1) {
  const s = SETUPS[k], { N, C } = track, g = 9.81;
  const mu = SPEC.mu * 0.94 * (up.mu || 1) * (wet ? 0.8 : 1), kd = SPEC.kd * (up.kd || 1) * (1 + s.kd), vt = SPEC.vtop * perf * (up.vtop || 1) * (1 + s.vtop);
  const V = new Float32Array(N);
  for (let i = 0; i < N; i++) {
    let c = 0; for (let j = -3; j <= 3; j++) c = Math.max(c, Math.abs(C[(i + j + N) % N]));
    const den = c - mu * kd;
    V[i] = den > 0 ? Math.min(vt, Math.sqrt(mu * g / den)) : vt;
  }
  for (let pass = 0; pass < 2; pass++) for (let i = N - 1; i >= 0; i--) {
    const vn = V[(i + 1) % N];
    V[i] = Math.min(V[i], Math.sqrt(vn * vn + 2 * 0.8 * (SPEC.brake0 + SPEC.brakeK * vn * vn) * (up.brake || 1) * DS));
  }
  let v = V[0], t = 0, full = 0;
  for (let pass = 0; pass < 2; pass++) for (let i = 0; i < N; i++) {
    const a = SPEC.acc * perf * (up.acc || 1) * Math.max(0, 1 - Math.pow(v / vt, 2.2)), va = Math.sqrt(v * v + 2 * a * DS), cap = V[(i + 1) % N];
    v = Math.min(cap, va);
    if (pass) { t += DS / Math.max(v, 5); if (va <= cap + 0.01) full++; }
  }
  return { t, full: full / N };
}
export const setupLap = (track, up, k, wet, perf) => lapSim(track, up, k, wet, perf).t;
// réglage conseillé (meilleur tour simulé)
export function bestSetup(track, up, wet, perf) {
  const t = SETUPS.map((_, k) => setupLap(track, up, k, wet, perf));
  return t.indexOf(Math.min(...t));
}

// températures du week-end : climat du décor, saison (date du GP), un peu de hasard fixe par GP
const AIR = { campagne: 21, plaine: 22, foret: 18, alpin: 14, automne: 15, desert: 34, cote: 25, ville: 23, port: 24, tropique: 31, sapins: 13,
  canyon: 30, riviera: 26, parc: 20, lac: 19, steppe: 22, vignes: 24, crepuscule: 27, bocage: 16, ile: 29 };
export function weekendTemps(def, date, rain, seed = 0) {
  let h = seed; for (const ch of def.id) h = (h * 31 + ch.charCodeAt(0)) % 9973;
  const r = (h % 100) / 100, m = date ? date.getMonth() : 6, key = Object.keys(THEMES).find(k => THEMES[k] === def.theme);
  const air = Math.round((AIR[key] ?? 21) + 5 * Math.cos((m - 6.5) / 6 * Math.PI) + (r - 0.5) * 4 - (rain ? 5 : 0));
  return { air, track: air + (rain ? 2 : 8 + Math.round(r * 9)) };
}

// silhouette de profil : aileron arrière plus ou moins incliné, flèches d'appui ou traits de vitesse
function carSVG(k) {
  const a = [-26, -19, -12, -6, -1][k], down = [4, 3, 2, 1, 0][k], fast = [0, 1, 2, 3, 4][k];
  let ind = '';
  for (let j = 0; j < down; j++) { const x = 30 + j * 12; ind += `M${x} 1v6M${x - 3} 4.5l3 3 3-3`; }
  for (let j = 0; j < fast; j++) { const y = 13 + j * 5.5; ind += `M${100 + (j % 2) * 3} ${y}h${10 - (j % 2) * 3}`; }
  return `<svg class="art tcar" viewBox="0 0 112 40" aria-hidden="true"><path class="f" d="M4 33l22-5 10-3 6-2 6-7h6l16 8h14v6l-8 3H30z"/>` +
    `<path d="M4 33l22-5 10-3 6-2 6-7h6l16 8h14v6l-8 3H30zM1 34h13M1 31v4M38 23c2-4 7-5 10-3"/>` +
    `<circle cx="20" cy="31" r="5.5"/><circle cx="76" cy="30" r="6.5"/><circle cx="76" cy="30" r="2"/>` +
    `<path d="M84 13v13M94 13v11"/><path class="a" d="M83 15h12" transform="rotate(${a} 89 15)"/>` +
    (ind ? `<path class="a" d="${ind}"/>` : '') + '</svg>';
}
const HEADSET = '<svg class="art" viewBox="0 0 48 48" aria-hidden="true"><path d="M10 29v-5a14 14 0 0 1 28 0v5"/><path class="f" d="M7 28h7v11H7zM34 28h7v11h-7z"/>' +
  '<path d="M7 28h7v11H7zM34 28h7v11h-7z"/><path class="a" d="M37 39c0 4-4 6-10 6h-2"/></svg>';
const lapFmt = t => `${Math.floor(t / 60)}:${(t % 60).toFixed(3).padStart(6, '0')}`;
const gap = t => '+' + t.toFixed(3).replace('.', ',') + ' s';
const pc = v => v ? `${v > 0 ? '+' : '−'}${Math.round(Math.abs(v) * 100)} %` : '=';

// voiture de l'atelier : à droite en paysage, en haut en portrait
const place = () => innerHeight > innerWidth ? frame(0.08, 0.92, 0.1, 0.25) : frame(0.52, 0.99, 0.07, 0.53);
addEventListener('resize', () => { if (!document.getElementById('tuning')?.classList.contains('hidden')) place(); });

// d = { session, sub, track, up, perf, wet, base (temps au tour estimé, réglage équilibré), info: [[libellé, valeur, classe]],
//       k (réglage déjà choisi), goLabel, simLabel, parc } ; cb = { go(k), sim(k), back() }
export function showTuning($, d, cb) {
  const sims = SETUPS.map((_, i) => lapSim(d.track, d.up, i, d.wet, d.perf)), times = sims.map(s => s.t);
  const best = Math.min(...times), rec = times.indexOf(best), share = Math.round(sims[2].full * 100), spread = Math.max(...times) - best;
  let k = d.k ?? rec;
  const est = i => d.base * times[i] / times[2];
  const why = [
    `Circuit sinueux, peu de lignes droites (${share} % du tour à fond) : ailerons au maximum, le temps se gagne en courbe.`,
    `Beaucoup de virages rapides (${share} % du tour à fond) : on gagnera plus en courbe avec de l’appui qu’on ne perdra en ligne droite.`,
    `Circuit équilibré (${share} % du tour à fond) : un réglage polyvalent sera le plus rapide.`,
    `Les lignes droites comptent ici (${share} % du tour à fond) : on enlève un peu d’appui pour la vitesse de pointe.`,
    `Circuit très rapide (${share} % du tour à fond) : ailerons presque à plat, la vitesse de pointe avant tout.`,
  ][rec] + (d.wet ? ' Piste mouillée : j’ai refait le calcul avec moins d’adhérence.' : '') +
    ` Entre le meilleur et le pire réglage : ${spread.toFixed(1).replace('.', ',')} s au tour.`;
  $('tnSub').textContent = d.sub;
  $('tnPf').innerHTML = d.parc ? '🔒 Parc fermé : ce réglage restera pour la course' : '';
  $('tnBar').innerHTML = d.info.map(([l, v, c]) => `<span class="st"><small>${l}</small><b class="${c || ''}">${v}</b></span>`).join('');
  $('tnGo').textContent = d.goLabel; $('tnSim').textContent = d.simLabel;
  const render = () => {
    $('tnList').innerHTML = SETUPS.map((s, i) => `<button class="tcard${i === k ? ' sel' : ''}${i === rec ? ' rec' : ''}" data-k="${i}" style="--p:${i / 4}">` +
      (i === rec ? '<span class="tbadge">Conseil de l’ingénieur</span>' : '') +
      `<small class="tno">Réglage ${i + 1}${i === k ? ' · choisi' : ''}</small><b class="tnm">${s.name}</b>${carSVG(i)}` +
      `<span class="tbal"><i></i></span><span class="tlab"><small>Appui</small><small>Vitesse</small></span>` +
      `<em class="${i === rec ? 'best' : ''}">${i === rec ? 'Meilleur tour' : gap(times[i] - best)}</em></button>`).join('');
    document.querySelectorAll('#tnList [data-k]').forEach(b => b.onclick = () => { k = +b.dataset.k; render(); });
    const s = SETUPS[k], vmax = SPEC.vtop * (d.perf || 1) * (d.up.vtop || 1) * 3.6, dv = Math.round(vmax * s.vtop);
    $('tnEng').innerHTML = `<span class="eav">${HEADSET}</span><div class="eq"><small>Ingénieur de piste</small><p>« ${why} »</p></div>` +
      `<div class="ginfo tfx"><span class="st"><small>Appui aéro</small><b>${pc(s.kd)}</b></span>` +
      `<span class="st"><small>Vitesse max</small><b>${Math.round(vmax * (1 + s.vtop))} km/h${dv ? `<i>${dv > 0 ? '+' : '−'}${Math.abs(dv)}</i>` : ''}</b></span>` +
      `<span class="st"><small>Tour estimé</small><b class="${k === rec ? 'ok' : ''}">${lapFmt(est(k))}${k === rec ? '' : `<i>${gap(times[k] - best)}</i>`}</b></span></div>`;
  };
  render(); place();
  $('tnGo').onclick = () => cb.go(k);
  $('tnSim').onclick = () => cb.sim(k);
  $('tnBack').onclick = () => cb.back();
}
