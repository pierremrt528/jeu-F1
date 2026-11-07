// Illustrations des pièces (dessins au trait originaux, style plan technique, cadre 48 × 48).
// Trait principal : couleur du texte ; class="a" : couleur du pôle ; class="f" : aplat léger.
const r1 = v => Math.round(v * 10) / 10;
function gear(cx, cy, ri, ro, n) {
  let d = '';
  for (let k = 0; k < n * 2; k++) {
    const a0 = k / (n * 2) * Math.PI * 2, a1 = (k + 1) / (n * 2) * Math.PI * 2, r = k % 2 ? ri : ro;
    d += `${k ? 'L' : 'M'}${r1(cx + r * Math.cos(a0 + 0.08))} ${r1(cy + r * Math.sin(a0 + 0.08))}L${r1(cx + r * Math.cos(a1 - 0.08))} ${r1(cy + r * Math.sin(a1 - 0.08))}`;
  }
  return d + 'Z';
}
const holes = (cx, cy, r, n, s) => Array.from({ length: n }, (_, k) => `<circle cx="${r1(cx + r * Math.cos(k / n * 6.283))}" cy="${r1(cy + r * Math.sin(k / n * 6.283))}" r="${s}"/>`).join('');
const CAR = '<path class="f" d="M4 34l12-3 4-3h3l2-5h4l2 4 10 1v3l-8 3H16z"/><path d="M4 34l12-3 4-3h3l2-5h4l2 4 10 1v3M16 35.5h14M1 33v4h8M40 17h6v9h-6zM43 26v2"/><circle cx="11" cy="33" r="5"/><circle cx="36" cy="32" r="6"/><circle cx="36" cy="32" r="2"/>';
const star = (x, y, s) => `<path class="a" d="M${x} ${y - s}l${r1(s * .3)} ${r1(s * .7)} ${r1(s * .7)} ${r1(s * .3)}-${r1(s * .7)} ${r1(s * .3)}-${r1(s * .3)} ${r1(s * .7)}-${r1(s * .3)}-${r1(s * .7)}-${r1(s * .7)}-${r1(s * .3)} ${r1(s * .7)}-${r1(s * .3)}z"/>`;
const WAVE = y => `M10 ${y}c2-2 4 2 6 0s4 2 6 0 4 2 6 0 4 2 6 0`;

export const ART = {
  // moteur
  m1: '<path class="f" d="M7 33h34v7H7z"/><path d="M7 33h34v7H7zM13 33V21c0-3-2-5-2-9h8c0 4-2 6-2 9v12M22 33V21c0-3-2-5-2-9h8c0 4-2 6-2 9v12M31 33V21c0-3-2-5-2-9h8c0 4-2 6-2 9v12"/><path class="a" d="M15 2v6M24 2v6M33 2v6"/>',
  m2: '<circle class="f" cx="20" cy="28" r="12"/><circle cx="20" cy="28" r="12"/><path d="M20 16h18v9h-6"/><circle class="a" cx="20" cy="28" r="4"/><path class="a" d="M20 24c3-2 6-2 8 0M24 28c2 3 2 6 0 8M20 32c-3 2-6 2-8 0M16 28c-2-3-2-6 0-8"/>',
  m3: '<rect class="f" x="12" y="12" width="24" height="24" rx="3"/><rect x="12" y="12" width="24" height="24" rx="3"/><path d="M17 6v6M24 6v6M31 6v6M17 36v6M24 36v6M31 36v6M6 17h6M6 24h6M6 31h6M36 17h6M36 24h6M36 31h6"/><path class="a" d="M16 31c3-9 6-12 9-12 4 0 4 5 7 6"/>',
  m4: '<rect class="f" x="5" y="14" width="33" height="20" rx="3"/><rect x="5" y="14" width="33" height="20" rx="3"/><path d="M38 20h4v8h-4"/><path class="a" d="M24 17l-8 8.5h6.5l-2.5 6 8-8.5h-6.5z"/>',
  m5: '<path class="f" d="M8 34A16 16 0 0 1 40 34z"/><path d="M8 34A16 16 0 0 1 40 34M8 34h3M12.7 22.7l2.1 2.1M24 18v3M35.3 22.7l-2.1 2.1M40 34h-3"/><path class="a" d="M33.5 17.5A19 19 0 0 1 42.4 29.1M24 34l8.4-10"/><circle cx="24" cy="34" r="2.5"/>',
  m6: '<path class="f" d="M13 30V9a4 4 0 0 1 8 0v21a7 7 0 1 1-8 0z"/><path d="M13 30V9a4 4 0 0 1 8 0v21a7 7 0 1 1-8 0zM28 11h13M28 18h13M28 25h13M28 32h13"/><path class="a" d="M17 34V16"/><path class="a" d="M26 41c2-2 4 2 6 0s4 2 6 0 4 2 6 0"/>',
  m7: '<path class="f" d="M19 4h10v9l-2 3v13h-6V16l-2-3z"/><path d="M19 4h10v9l-2 3v13h-6V16l-2-3zM21 29l1 4h4l1-4M19 9h10"/><path class="a" d="M24 36v7M20.5 35.5l-4 6M27.5 35.5l4 6M18 34l-7 3M30 34l7 3"/>',
  m8: '<path class="f" d="M9 22l9-11h12l9 11v15H9z"/><path d="M9 22l9-11h12l9 11v15H9zM9 28h30M15 22h18"/><circle cx="17" cy="32.5" r="2"/><circle cx="24" cy="32.5" r="2"/><circle cx="31" cy="32.5" r="2"/><path class="a" d="M24 2v6M16 4l2.5 4.5M32 4l-2.5 4.5"/>',
  // aéro
  a1: '<path class="f" d="M5 27c7-4 12-5 19-5s12 1 19 5v5H5z"/><path d="M5 17v15M43 17v15M5 32h38M5 27c7-4 12-5 19-5s12 1 19 5M21 32v7h6v-7"/><path class="a" d="M5 21c7-3 12-4 19-4s12 1 19 4"/>',
  a2: '<path class="f" d="M4 20h40v4h-6c-5 0-7 7-14 7s-9-7-14-7H4z"/><path d="M4 20h40M4 24h6c5 0 7 7 14 7s9-7 14-7h6"/><path class="a" d="M5 38h7M16 38h7M27 38h7M38 35l4 3-4 3"/>',
  a3: '<path class="f" d="M4 34c7 0 10-7 18-9s17 1 22 6v3z"/><path d="M4 34c7 0 10-7 18-9s17 1 22 6v3H4z"/><path class="a" d="M6 18c10-3 22-3 36 1M10 11c9-2 18-1 28 2"/>',
  a4: '<circle class="f" cx="30" cy="27" r="13"/><circle cx="30" cy="27" r="13"/><circle cx="30" cy="27" r="4"/><path d="M4 13h12c5 0 8 4 8 9M4 21h9c3 0 5 2 5 5"/><path class="a" d="M1 7h9M1 29h6"/>',
  a5: '<path class="f" d="M4 15h40v7l-8 13H12L4 22z"/><path d="M4 15h40v7l-8 13H12L4 22zM15 22l-2 13M21 22l-1 13M27 22l1 13M33 22l2 13"/><path class="a" d="M12 40l-3 5M24 40v5M36 40l3 5"/>',
  a6: '<path class="f" d="M7 29h34v3H7z"/><path d="M7 8v29M41 8v29M7 29h34M19 29v13M29 29v13"/><path class="a" d="M9 21l30-8"/>',
  a7: '<path class="f" d="M14 44V27l5-5h10l5 5v17z"/><path d="M14 44V27l5-5h10l5 5v17M10 44h28"/><path class="a" d="M18 18c0-6 2-10 4-14M24 18V3M30 18c0-6-2-10-4-14"/>',
  a8: '<path class="f" d="M4 19c8 7 32 7 40 0v7c-8 7-32 7-40 0z"/><path d="M4 19c8 7 32 7 40 0M4 26c8 7 32 7 40 0"/><path class="a" d="M12 4v9M36 4v9M9 10l3 3 3-3M33 10l3 3 3-3"/>',
  a9: CAR + '<path class="a" d="M3 17c12-4 26-4 41 1M8 10c10-2 20-2 31 1"/>',
  a10: CAR + star(9, 13, 6) + star(24, 8, 4.5) + star(34, 13, 3.5),
  // châssis
  c1: '<path d="M24 2v7M24 39v7M15 9h18M15 39h18"/><path class="a" d="M17 13l14 3-14 3 14 3-14 3 14 3-14 3 14 3"/>',
  c2: '<circle class="f" cx="22" cy="26" r="17"/><circle cx="22" cy="26" r="17"/><circle cx="22" cy="26" r="6"/>' + holes(22, 26, 11.5, 8, 1.3) + '<path class="a" d="M33 9c6 3 9 8 9.5 15l-6.5 1c-.5-4-2.5-8-6.5-10z"/>',
  c3: '<path class="f" d="M7 17c0-3 2-5 5-5h24c3 0 5 2 5 5v9c0 6-4 10-10 10H17C11 36 7 32 7 26z"/><path d="M7 17c0-3 2-5 5-5h24c3 0 5 2 5 5v9c0 6-4 10-10 10H17C11 36 7 32 7 26z"/><rect x="17" y="17" width="14" height="9" rx="2"/><path class="a" d="M12 18v9M36 18v9"/><circle cx="20" cy="31" r="1.5"/><circle cx="28" cy="31" r="1.5"/>',
  c4: '<path class="f" d="M8 23c3-8 9-12 16-12s13 4 16 12l-6 2c-2-5-6-8-10-8s-8 3-10 8z"/><path d="M8 23c3-8 9-12 16-12s13 4 16 12l-6 2c-2-5-6-8-10-8s-8 3-10 8z"/><path class="a" d="' + WAVE(33) + WAVE(40) + '"/>',
  c5: '<path class="f" d="M39 5C25 7 15 18 13 34c13-4 22-14 26-29z"/><path d="M39 5C25 7 15 18 13 34c13-4 22-14 26-29zM13 34l-5 9"/><path class="a" d="M21 25l8-2M18 30l7-1M25 18l8-3"/>',
  c6: '<rect class="f" x="18" y="6" width="12" height="17" rx="2"/><rect x="18" y="6" width="12" height="17" rx="2"/><path d="M24 23v14M17 37h14M24 2v4"/><path class="a" d="M2 30h5l2-5 3 10 2-5h4M30 30h4l2-5 3 10 2-5h5"/>',
  c7: '<path class="f" d="M17 4h14l4 15v15l-4 10H17l-4-10V19z"/><path d="M17 4h14l4 15v15l-4 10H17l-4-10V19zM20 21h8v9h-8z"/><path class="a" d="M20 9h8l2.5 10v14l-2.5 6.5h-8L17.5 33V19z"/>',
  // fiabilité
  f1: '<rect class="f" x="8" y="8" width="32" height="28" rx="2"/><rect x="8" y="8" width="32" height="28" rx="2"/><path d="M8 15h32M8 22h32M8 29h32"/><path d="M14 8v28M20 8v28M26 8v28M32 8v28" opacity=".45"/><path class="a" d="' + WAVE(42) + '"/>',
  f2: '<path class="f" d="M4 15h22l14 9-14 9H4z"/><path d="M4 15h22l14 9-14 9H4z"/><path class="a" d="M10 19.5l3 2v4l-3 2-3-2v-4zM19 19.5l3 2v4l-3 2-3-2v-4z"/><path d="M45 13l-3 4M46 24h-4M45 35l-3-4"/>',
  f3: '<path class="f" d="M3 20h42l-4 10H7z"/><path d="M3 20h42l-4 10H7z"/>' + holes(24, 25, 0, 1, 0) + '<circle cx="11" cy="25" r="1.6"/><circle cx="19" cy="25" r="1.6"/><circle cx="29" cy="25" r="1.6"/><circle cx="37" cy="25" r="1.6"/><path class="a" d="M7 36h34M11 41h26"/>',
  f4: `<path class="f" d="${gear(24, 24, 13, 18, 10)}"/><path d="${gear(24, 24, 13, 18, 10)}"/><circle class="a" cx="24" cy="24" r="8"/><circle cx="24" cy="24" r="3"/>`,
  f5: '<rect class="f" x="7" y="5" width="22" height="30" rx="2"/><rect x="7" y="5" width="22" height="30" rx="2"/><path d="M11 13h7M11 19h11M11 25h8"/><circle class="a" cx="31" cy="30" r="8"/><path class="a" d="M37 36l7 7M27.5 30l2.5 2.5 4.5-5"/>',
  f6: '<path class="f" d="M24 4l16 6v12c0 10-7 17-16 22C15 39 8 32 8 22V10z"/><path d="M24 4l16 6v12c0 10-7 17-16 22C15 39 8 32 8 22V10z"/><path class="a" d="M16 23l6 6 10-12"/>',
  // stands
  s1: '<path class="f" d="M5 15h22v11H5z"/><path d="M5 15h22v11H5zM27 18h8v5h-8M35 19h3v3h-3M13 26l-3 14h7l3-14"/><path class="a" d="M41 13l4-3M42 20.5h5M41 28l4 3"/>',
  s2: '<circle class="f" cx="24" cy="24" r="19"/><circle cx="24" cy="24" r="19"/><circle cx="24" cy="24" r="10"/><circle class="a" cx="24" cy="24" r="14.5" stroke-dasharray="3 4"/><circle cx="24" cy="24" r="3"/>',
  s3: '<path class="f" d="M24 4c7 10 13 16 13 24a13 13 0 0 1-26 0c0-8 6-14 13-24z"/><path d="M24 4c7 10 13 16 13 24a13 13 0 0 1-26 0c0-8 6-14 13-24z"/><path class="a" d="M17 29a7 7 0 0 0 7 7"/>',
  s4: '<path d="M4 38h22M10 38v-8h12v8M22 30l17-17M36 10l6 6"/><circle cx="8" cy="41" r="2"/><circle cx="22" cy="41" r="2"/><path class="a" d="M7 23h18v7H7z"/>',
  s5: '<circle class="f" cx="24" cy="31" r="13"/><circle cx="24" cy="31" r="13"/><circle cx="24" cy="31" r="5"/><path d="M9 32a15 15 0 0 1 30 0v3H9z"/><path class="a" d="M15 2c-2 3 2 5 0 8M24 2c-2 3 2 5 0 8M33 2c-2 3 2 5 0 8"/>',
  s6: '<rect class="f" x="5" y="7" width="38" height="26" rx="2"/><rect x="5" y="7" width="38" height="26" rx="2"/><path d="M18 41h12M24 33v8"/><path class="a" d="M10 27l7-8 6 4 8-10 7 5"/>',
  s7: '<circle cx="12" cy="17" r="5"/><circle cx="36" cy="17" r="5"/><path d="M3 39c0-7 4-11 9-11M45 39c0-7-4-11-9-11"/><circle class="a" cx="24" cy="14" r="6"/><path class="a" d="M13 41c0-9 5-14 11-14s11 5 11 14"/>',
  s8: '<path class="f" d="M6 13h18l6 6v8H18l-4 5H6z"/><path d="M6 13h18l6 6v8H18l-4 5H6zM10 32v11"/><path class="a" d="M30 19h10l5 5"/>',
  s9: '<path d="M10 28v-6a14 14 0 0 1 28 0v6"/><rect class="f" x="6" y="26" width="8" height="13" rx="3"/><rect x="6" y="26" width="8" height="13" rx="3"/><rect class="f" x="34" y="26" width="8" height="13" rx="3"/><rect x="34" y="26" width="8" height="13" rx="3"/><path class="a" d="M38 39c0 4-4 6-10 6h-4"/>',
};
// icône d'un pôle (centre des jauges)
export const DEPT_ART = { moteur: 'm8', aero: 'a6', chassis: 'c1', fiab: 'f6', stand: 's1' };
export const art = (id, cls = '') => `<svg class="art ${cls}" viewBox="0 0 48 48" aria-hidden="true">${ART[id] || ''}</svg>`;
