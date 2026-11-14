// Négociations façon mercato : l'agent d'un pilote (ou le chef de pôle en personne) réclame un salaire,
// une durée et un statut. On répond par des offres (salaire, durée, statut, prime à la signature) :
// accord, contre-offre ou agacement (la patience baisse, les discussions peuvent être rompues).
// Pour un pilote d'une écurie rivale, il faut d'abord s'entendre avec l'écurie sur une indemnité de
// transfert. Signature avec une petite cinématique. Fenêtre modale #nego.
import { initials } from './ui.js';
import { sendMail } from './mail.js';
import { startScene } from './cine.js';
import { setCast } from './showroom.js';
import { SKINS, HAIR_COLS } from './pilot3d.js';

const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const hash = s => { let h = 2166136261; for (const ch of String(s)) h = Math.imul(h ^ ch.charCodeAt(0), 16777619) >>> 0; return h; };
const frac = s => (hash(s) % 10007) / 10007;
const r1 = v => Math.round(v * 10) / 10;
export const fmtM = v => r1(v).toFixed(1).replace('.', ',');
export const ans = n => n > 1 ? `${n} ans` : '1 an';
const stepOf = v => Math.abs(v) < 3 ? 0.1 : Math.abs(v) < 8 ? 0.2 : 0.5;
const up = (v, s) => r1(Math.ceil(v / s - 1e-6) * s);

export const PERS = {
  ambitieux: { name: 'Ambitieux', greed: 1, pat: 3, d: 'veut gagner : très sensible aux résultats de l’écurie' },
  mercenaire: { name: 'Mercenaire', greed: 1.15, pat: 2, d: 'l’argent avant tout, peu patient' },
  fidele: { name: 'Fidèle', greed: 0.92, pat: 4, d: 'cherche un projet stable, patient' },
  jeune: { name: 'Jeune loup', greed: 0.95, pat: 3, d: 'veut un volant et du temps pour progresser' },
};
export const ROLES = { n1: 'N°1', eq: 'Égalité', n2: 'N°2' };
// clause libératoire : aucune, élevée (6 saisons de salaire) ou basse (3 saisons) ; payée par l'écurie rivale qui la lève
export const CLAUSES = { none: ['Aucune', 0], high: ['Élevée', 6], low: ['Basse', 3] };
// âge et réputation (fiche du pilote) : fixés par la config, sinon tirés du nom et de l'origine
export const ageOf = s => s.age || (/Formule B|Jeune|réserve/i.test(s.from || '') ? 19 + hash(s.key + 'a') % 4 : /Vétéran/.test(s.from || '') ? 32 + hash(s.key + 'a') % 6 : 24 + hash(s.key + 'a') % 7);
export const repOf = s => s.rep ?? Math.round(clamp((s.r || 70) * 1.6 - 60, 10, 98));
const ROLE_FX = { n1: 'priorité sur toi, +0,3 % de rythme', eq: 'aucune consigne d’équipe', n2: 'te laisse passer en course, −0,3 % de rythme' };

// sub : { kind: 'drv' | 'chief', key, name, from, r (note), base (salaire de référence M€), cur (déjà chez toi),
//         club (nom de l'écurie rivale qui l'emploie), sponsor (M€ apportés), color }
// ctx : { win, store, rep, pos (place de l'écurie), mid, feeMult, team, year (1re saison du contrat),
//         avail() budget disponible, cost(offre, indemnité) → { now, parts }, swap (équipier envoyé au club), save() }
export function profile(sub, ctx) {
  const young = /Formule B|Jeune/.test(sub.from || ''), vet = /Vétéran/.test(sub.from || ''), u = frac(sub.key);
  const pers = young ? (u < 0.7 ? 'jeune' : 'ambitieux') : vet ? (u < 0.5 ? 'mercenaire' : 'fidele')
    : ['ambitieux', 'mercenaire', 'fidele', 'ambitieux', 'fidele'][hash(sub.key + 'p') % 5];
  const P = PERS[pers];
  const pref = pers === 'fidele' ? (vet ? 2 : 3) : pers === 'mercenaire' || vet ? 1 : pers === 'jeune' ? (u < 0.35 ? 2 : 3) : 2;
  const wantN1 = sub.kind === 'drv' && (sub.r >= 84 || (pers === 'ambitieux' && sub.r >= 80));
  const w = pers === 'ambitieux' ? 1.6 : pers === 'mercenaire' ? 0.5 : 1;
  let int = 45 + (ctx.rep - 50) * 0.7 * w + (6 - ctx.pos) * 2.5 * w - Math.max(0, sub.r - 78) * 1.2 + (frac(sub.key + '#') - 0.5) * 12;
  if (pers === 'jeune') int += 10;
  if (ctx.car) int += (5.5 - ctx.car) * 2 * w;                                   // performance de la voiture (rang 1-10)
  if (sub.kind === 'drv') int -= Math.max(0, repOf(sub) - ctx.rep) * 0.25;     // une star boude une écurie peu connue
  if (sub.cur) int += pers === 'fidele' ? 20 : 8;
  return { sub, pers, P, pref, wantN1, int: Math.round(clamp(int, 5, 95)) };
}
const askOf = (p, int, ctx) => p.sub.base * p.P.greed * (1 + (55 - int) / 160) * (ctx.mid ? 1.2 : 1);
const thrOf = (p, int, ctx) => 1 - (int - 50) / 500 + (frac(p.sub.key + ctx.win) - 0.5) * 0.04;
const wantsLow = p => p.pers === 'ambitieux' || p.pers === 'jeune';
const clauseTerm = (p, cl) => p.sub.kind !== 'drv' ? 0 : wantsLow(p) ? { none: -0.08, high: -0.03, low: 0.03 }[cl || 'none'] : { none: -0.03, high: 0, low: 0.01 }[cl || 'none'];
// primes de victoire et de podium : valeur espérée sur une saison selon la place de l'écurie
const bonusEV = (o, ctx) => { const k = ctx.pos <= 3 ? 1 : ctx.pos <= 6 ? 0.5 : 0.15; return ((o.win || 0) * 2 + (o.pod || 0) * 5) * k; };
const roleTerm = (p, role) => p.sub.kind !== 'drv' ? 0 : p.wantN1 ? { n1: 0.12, eq: 0, n2: -0.3 }[role] : { n1: 0.05, eq: 0, n2: -0.08 }[role];
// floor : salaire plancher (prolongation d'un contrat en cours : pas de baisse de salaire)
const needOf = (p, int, ctx, o) => Math.max(p.sub.floor || 0, (thrOf(p, int, ctx) - roleTerm(p, o.role) - clauseTerm(p, o.cl) + 0.06 * Math.abs(o.years - p.pref)) * askOf(p, int, ctx) - 0.5 * o.bonus - 0.8 * bonusEV(o, ctx));
function demandOf(p, int, ctx) {
  const d = { years: p.pref, role: p.wantN1 ? 'n1' : 'eq', bonus: 0, win: 0, pod: 0, cl: p.sub.kind === 'drv' ? (wantsLow(p) ? 'low' : 'high') : 'none' };
  d.sal = up(needOf(p, int, ctx, d) * 1.1, stepOf(p.sub.base));
  return d;
}
const feeStep = f => f < 6 ? 0.5 : 1;
const clubFee = (sub, ctx) => up(Math.max(1, sub.base) * 0.8 * (ctx.feeMult || 1), feeStep(sub.base * 0.8 * (ctx.feeMult || 1)));
const stOf = (sub, ctx) => ctx.store.win === ctx.win ? ctx.store.st?.[sub.key] : null;

// aperçu pour les cartes : intérêt, demande de départ, indemnité, discussions rompues
export const INT = [[70, 'très intéressé', 'hi'], [50, 'intéressé', 'ok'], [35, 'hésitant', 'mid'], [20, 'réticent', 'low'], [-1, 'refuse de discuter', 'no']];
const intOf = i => INT.find(x => i >= x[0]);
export function preview(sub, ctx) {
  const p = profile(sub, ctx), st = stOf(sub, ctx), int = st ? st.int : p.int;
  const L = intOf(int);
  if (st?.done) return { tag: 'Contrat signé', cls: 'hi', sal: st.deal.sal };
  if (st?.off) return { tag: 'Discussions rompues', cls: 'no', off: true };
  if (sub.sponsor) return { tag: 'veut signer', cls: 'hi', sal: -sub.sponsor };
  if (int < 20 && !sub.cur) return { tag: L[1], cls: L[2], off: true };
  const d = st?.counter || st?.deal || demandOf(p, int, ctx);
  return { tag: st?.deal ? 'accord trouvé' : L[1], cls: st?.deal ? 'hi' : L[2], sal: d.sal, fee: sub.club ? (st?.fee ?? st?.club?.ask ?? clubFee(sub, ctx)) : 0 };
}
export const previewHTML = v => `<small class="nint ${v.cls}"><i></i>${v.tag}</small>`;

const TXT = {
  drv: {
    who: s => `Agent de ${s.name}`,
    open: d => `Mon client demande ${fmtM(d.sal)} M€ par saison sur ${ans(d.years)}${d.role === 'n1' ? ', avec le statut de pilote n°1' : ''}.`,
    pers: { ambitieux: 'Il veut se battre devant.', mercenaire: 'Il ira là où l’offre est la meilleure.', fidele: 'Il cherche un projet stable.', jeune: 'Il veut un volant et du temps pour progresser.' },
    cur: 'Il se sent bien chez vous, mais il a ses exigences.', raise: f => `Pour prolonger, il ne descendra pas sous ${fmtM(f)} M€.`,
    refuse: t => `Mon client ne souhaite pas rejoindre ${t} pour le moment. Revenez quand l’écurie aura fait ses preuves.`,
    yes: ['Marché conclu ! Mon client est ravi.', 'C’est une belle offre : nous acceptons.', 'Affaire conclue, on peut signer.'],
    counter: [c => `On s’en approche. Mon client signerait pour ${fmtM(c.sal)} M€ par saison sur ${ans(c.years)}${c.role !== 'eq' ? `, statut ${ROLES[c.role]}` : ''}.`,
      c => `Faites encore un effort : ${fmtM(c.sal)} M€ par saison sur ${ans(c.years)} et c’est signé.`],
    low: ['C’est une plaisanterie ? Mon client vaut bien plus que ça.', 'Vous nous faites perdre notre temps.', 'Avec une offre pareille, on va voir ailleurs.'],
    n2: 'Le statut de n°2 ? Hors de question pour un pilote de ce calibre.',
    years: n => `Il préfère un contrat de ${ans(n)}.`,
    off: 'Nous avons d’autres propositions. Les discussions sont terminées.',
    pay: s => `Mon client apporte ${fmtM(s)} M€ de sponsors par saison. Il signe tout de suite pour un volant.`,
  },
  chief: {
    who: s => s.name,
    open: d => `Je demande ${fmtM(d.sal)} M€ par saison sur ${ans(d.years)}.`,
    pers: { ambitieux: 'Je veux un projet qui gagne.', mercenaire: 'Je vais au plus offrant.', fidele: 'Je cherche une écurie où m’installer.', jeune: 'Je veux faire mes preuves.' },
    cur: 'Je me plais ici, mais mes conditions ont changé.', raise: f => `Pour prolonger, pas moins de ${fmtM(f)} M€.`,
    refuse: t => `Votre projet ne m’intéresse pas pour l’instant. Revenez quand ${t} jouera devant.`,
    yes: ['Marché conclu, je commence lundi !', 'Vous m’avez convaincu : j’accepte.', 'D’accord, topez là.'],
    counter: [c => `Je signerais pour ${fmtM(c.sal)} M€ par saison sur ${ans(c.years)}.`, c => `Montez à ${fmtM(c.sal)} M€ par saison et c’est d’accord.`],
    low: ['Vous plaisantez ? Je vaux bien plus que ça.', 'Je ne me déplace pas pour ce prix-là.'],
    years: n => `Je préfère un contrat de ${ans(n)}.`,
    off: 'J’ai d’autres offres. Restons-en là.',
  },
  club: {
    open: (club, name, f, swap) => `${club} demande ${fmtM(f)} M€ d’indemnité pour libérer ${name}.${swap ? ` ${swap} partirait chez nous pour le remplacer.` : ''}`,
    yes: club => `Marché conclu avec ${club}. À vous de convaincre son agent.`,
    counter: [f => `Nous pourrions descendre à ${fmtM(f)} M€, pas moins.`, f => `${fmtM(f)} M€ et l’affaire est faite.`],
    low: name => `Vous plaisantez ? ${name} n’est pas à brader.`,
    off: club => `${club} met fin aux discussions.`,
  },
};
const any = (a, s) => a[hash(s + Date.now()) % a.length];

// personnages des scènes 3D : la recrue (combinaison de son écurie actuelle) et son agent
const hexOf = c => /^#[0-9a-f]{6}$/i.test(c || '') ? parseInt(c.slice(1), 16) : 0x8d96a3;
function castOf(sub) {
  const h = hash(sub.key), look = k => ({ skin: (h >>> k) % SKINS.length, hair: (h >>> (k + 3)) % 4, hc: (h >>> (k + 6)) % HAIR_COLS.length, beard: (h >>> (k + 9)) % 3 === 0 ? 1 : 0 });
  return { guest: { look: look(0), suit: sub.kind === 'drv' ? { p: hexOf(sub.color), s: 0xf2f4f7 } : { p: 0x2b303a, s: hexOf(sub.color) }, helmet: sub.kind === 'drv' ? { m: h % 6, p: hexOf(sub.color), s: 0xf2f4f7 } : null },
    agent: sub.kind === 'drv' && !sub.sponsor ? { look: look(12), suit: { p: 0x262a32, s: 0xd9dde3 } } : null };
}
// ouvre la négociation ; cb.done(contrat { sal, years, role, bonus, win, pod, clause }, indemnité) à la signature, cb.close() sinon.
// Premier rendez-vous : petite scène 3D (le pilote et son agent arrivent au QG), puis la table de négociation.
export function openNego(sub, ctx, cb) {
  const fresh = !(ctx.store.win === ctx.win && ctx.store.st?.[sub.key]);
  const v = preview(sub, ctx);
  if (!fresh || v.off || sub.cur) return talks(sub, ctx, cb);
  setCast(castOf(sub));
  startScene('meet', { top: sub.kind === 'drv' ? 'Rendez-vous au QG' : 'Entretien au QG', name: sub.name, sub: sub.kind === 'drv' && !sub.sponsor ? `arrive avec son agent · ${sub.from || ''}` : sub.from || '' }, () => talks(sub, ctx, cb));
}
function talks(sub, ctx, cb) {
  const el = document.getElementById('nego'), V = TXT[sub.kind === 'chief' ? 'chief' : 'drv'];
  const store = ctx.store;
  if (store.win !== ctx.win) { store.win = ctx.win; store.st = {}; }
  store.st = store.st || {};
  const p = profile(sub, ctx), drv = sub.kind === 'drv';
  const broke = club => sendMail({ tag: 'mercato', from: club || (drv && !sub.sponsor ? `Agent de ${sub.name}` : sub.name), subject: `Discussions rompues : ${sub.name}`,
    body: club ? `${club} met fin aux discussions sur le transfert de ${sub.name}.` : `${drv ? 'Mon client' : 'Je'} ne donne${drv ? '' : 's'} pas suite pour le moment.`, });
  const signedMail = k => sendMail({ tag: 'mercato', from: drv ? 'Cellule recrutement' : 'Ressources humaines', subject: `${sub.cur ? 'Prolongation' : 'Contrat signé'} : ${sub.name}`,
    body: `${sub.name} ${sub.cur ? 'prolonge chez' : 'rejoint'} ${ctx.team}.<br>Salaire : <b>${k.sal} M€</b> par saison · ${k.years} saison${k.years > 1 ? 's' : ''}${drv && k.role ? ` · statut ${ROLES[k.role] || k.role}` : ''}${k.bonus ? ` · prime à la signature ${k.bonus} M€` : ''}${k.win ? ` · ${k.win} M€ par victoire` : ''}${k.pod ? ` · ${k.pod} M€ par podium` : ''}${k.clause ? ` · clause libératoire ${k.clause} M€` : ''}` });
  let st = store.st[sub.key];
  const say = (who, t) => { st.log.push([who, t]); if (st.log.length > 12) st.log.shift(); };
  const ask = () => askOf(p, st.int, ctx), thr = () => thrOf(p, st.int, ctx);
  const need = o => needOf(p, st.int, ctx, o);
  const score = o => { const s = o.sal / ask() + 0.5 * o.bonus / ask() + 0.8 * bonusEV(o, ctx) / ask() + roleTerm(p, o.role) + clauseTerm(p, o.cl) - 0.06 * Math.abs(o.years - p.pref); return o.sal < (sub.floor || 0) ? Math.min(s, thr() - 0.001) : s; };
  const open = () => { st.dem = demandOf(p, st.int, ctx); say('a', V.open(st.dem) + ' ' + (sub.cur ? (sub.floor ? V.raise(sub.floor) : V.cur) : V.pers[p.pers])); };
  if (!st) {
    st = store.st[sub.key] = { pat: p.P.pat, int: p.int, log: [] };
    if (sub.sponsor) { st.deal = { sal: -sub.sponsor, years: p.pref, role: 'eq', bonus: 0 }; say('a', V.pay(sub.sponsor)); }
    else if (p.int < 20 && !sub.cur) { st.refuse = true; say('a', V.refuse(ctx.team)); }
    else if (sub.club) {
      const f0 = clubFee(sub, ctx);
      st.club = { pat: 3, ask: f0, f0, min: r1(f0 * (0.82 + frac(sub.key + ctx.win + 'c') * 0.13)) };
      say('c', TXT.club.open(sub.club, sub.name, f0, ctx.swap));
    } else open();
    ctx.save?.();
  }
  const sstep = stepOf(sub.base), bstep = r1(sstep * 2);
  let o = { win: 0, pod: 0, cl: 'none', ...(st.counter || st.dem || { sal: 0, years: p.pref, role: 'eq', bonus: 0 }) };
  let fee = st.club?.ask ?? 0;
  const fstep = feeStep(st.club?.f0 || 1);
  const phase = () => st.done ? 'done' : st.off ? 'off' : st.refuse ? 'refuse' : st.deal ? 'deal' : st.club && !st.club.ok ? 'club' : 'agent';
  const costOf = (k, f) => ctx.cost(k, f);
  const close = () => { el.classList.add('hidden'); el.innerHTML = ''; };
  let seen = -1;   // n'animer que la carte à l'ouverture et les nouvelles bulles

  function render() {
    const ph = phase(), L = intOf(st.int), maxPat = st.club && !st.club.ok ? 3 : p.P.pat, pat = st.club && !st.club.ok ? st.club.pat : st.pat;
    const who = w => w === 'm' ? 'Toi' : w === 'c' ? sub.club : V.who(sub);
    const chat = st.log.map(([w, t], i) => `<p class="nb ${w} ${seen >= 0 && i >= seen ? 'new' : ''}"><small>${who(w)}</small>${t}</p>`).join('');
    const first = seen < 0; seen = st.log.length;
    let h = '';
    const avail = ctx.avail();
    if (ph === 'club') {
      h = `<p class="nlbl">Indemnité de transfert pour ${sub.club}</p>
        <div class="nrow"><span>Indemnité</span><button data-f="-1">−</button><b>${fmtM(fee)} M€</b><button data-f="1">+</button></div>
        <p class="ncost">Budget disponible <b>${fmtM(avail)} M€</b>${ctx.swap ? ` · ${ctx.swap} part chez ${sub.club}` : ''}</p>
        <div class="nbtn"><button class="main" id="nFee" ${fee > avail ? 'disabled' : ''}>Proposer ${fmtM(fee)} M€</button><button data-q>Quitter</button></div>`;
    } else if (ph === 'agent') {
      const c = costOf(o, st.fee || 0), ok = c.now <= avail + 1e-6 && o.sal > 0;
      const cc = st.counter && costOf(st.counter, st.fee || 0), cOk = cc && cc.now <= avail + 1e-6;
      h = `<div class="nrow"><span>Salaire <small>/ saison</small></span><button data-s="-1">−</button><b>${fmtM(o.sal)} M€</b><button data-s="1">+</button></div>
        <div class="nrow"><span>Durée</span><div class="nseg">${[1, 2, 3].map(n => `<button data-y="${n}" class="${o.years === n ? 'sel' : ''}">${ans(n)}</button>`).join('')}</div></div>` +
        (drv ? `<div class="nrow"><span>Statut</span><div class="nseg">${Object.entries(ROLES).map(([k, l]) => `<button data-r="${k}" class="${o.role === k ? 'sel' : ''}">${l}</button>`).join('')}</div></div><p class="nfx">${ROLE_FX[o.role]}</p>` : '') +
        `<div class="nrow"><span>Prime <small>à la signature</small></span><button data-b="-1">−</button><b>${fmtM(o.bonus)} M€</b><button data-b="1">+</button></div>` +
        (drv ? `<div class="nrow two"><span>Primes <small>victoire · podium</small></span><button data-w="-1">−</button><b>${fmtM(o.win)}</b><button data-w="1">+</button><button data-p="-1">−</button><b>${fmtM(o.pod)}</b><button data-p="1">+</button></div>
        <div class="nrow"><span>Clause <small>libératoire</small></span><div class="nseg">${Object.entries(CLAUSES).map(([k, [l, m]]) => `<button data-cl="${k}" class="${o.cl === k ? 'sel' : ''}">${l}${m ? ` <small>${fmtM(m * o.sal)}</small>` : ''}</button>`).join('')}</div></div>` : '') +
        `
        <p class="ncost">Coût maintenant <b class="${ok ? '' : 'bad'}">${fmtM(c.now)} M€</b> <small>(${c.parts.filter(x => x[1]).map(x => `${x[0]} ${fmtM(x[1])}`).join(' + ') || '—'})</small> · budget ${fmtM(avail)} M€<br>
        puis ${fmtM(o.sal)} M€ par saison jusqu’en ${ctx.year + o.years - 1}</p>
        <div class="nbtn"><button class="main" id="nGo" ${ok ? '' : 'disabled'}>Faire l’offre</button>${st.counter ? `<button id="nAcc" class="acc" ${cOk ? '' : 'disabled'}>✓ Accepter</button>` : ''}<button data-q>Quitter</button></div>`;
    } else if (ph === 'deal') {
      const k = st.deal, c = costOf(k, st.fee || 0);
      h = `<p class="ndeal">🤝 Accord trouvé : <b>${ans(k.years)}</b> · <b>${fmtM(k.sal)} M€</b> par saison${drv ? ` · statut <b>${ROLES[k.role]}</b>` : ''}${k.bonus ? ` · prime ${fmtM(k.bonus)} M€` : ''}${k.win || k.pod ? ` · ${fmtM(k.win || 0)} M€ / victoire, ${fmtM(k.pod || 0)} M€ / podium` : ''}${drv ? ` · clause ${CLAUSES[k.cl || 'none'][0].toLowerCase()}` : ''}${st.fee ? ` · indemnité ${fmtM(st.fee)} M€` : ''}</p>
        <p class="ncost">Coût maintenant <b>${fmtM(c.now)} M€</b> · budget ${fmtM(avail)} M€ · jusqu’en ${ctx.year + k.years - 1}</p>
        <div class="nbtn"><button class="main" id="nSign" ${c.now <= avail + 1e-6 ? '' : 'disabled'}>✍️ Signer le contrat</button><button data-q>Plus tard</button></div>`;
    } else {
      const why = ph === 'done' ? 'Contrat signé.' : ph === 'off' ? 'Les discussions sont rompues jusqu’à la prochaine fenêtre de transferts.' : 'Améliore ta réputation et tes résultats pour attirer les meilleurs.';
      h = `<p class="nlbl">${why}</p><div class="nbtn"><button class="main" data-q>Fermer</button></div>`;
    }
    el.innerHTML = `<div class="ncard ${first ? 'in' : ''}" style="--c:${sub.color || 'var(--acc)'}">
      <div class="nhead"><span class="av">${initials(sub.name)}</span>
        <span class="nn"><b>${sub.name}</b><small>${sub.from || ''}${sub.r ? ` · note ${Math.round(sub.r)}` : ''}${drv ? ` · ${ageOf(sub)} ans · réputation ${repOf(sub)}` : ''} · ${p.P.name}</small></span>
        <span class="ngauge ${L[2]}"><small>Intérêt</small><i><em style="width:${st.int}%"></em></i><b>${L[1]}</b></span>
        <span class="npat"><small>Patience</small><span>${Array.from({ length: maxPat }, (_, k) => `<i class="${k < pat ? 'on' : ''}"></i>`).join('')}</span></span>
        <button class="nx" data-q aria-label="Fermer">✕</button></div>
      <p class="npers">${p.P.d}${st.dem ? ` · <b>attentes</b> : ${fmtM(st.dem.sal)} M€ / saison, ${ans(p.pref)}${drv ? `, statut ${ROLES[st.dem.role]}, clause ${CLAUSES[st.dem.cl][0].toLowerCase()}` : ''}` : ` · préfère ${ans(p.pref)}`}</p>
      <div class="nbody"><div class="nchat">${chat}</div><div class="noffer">${h}</div></div></div>`;
    el.classList.remove('hidden');
    const ch = el.querySelector('.nchat'); ch.scrollTop = ch.scrollHeight;
    bind();
  }
  function counter() {
    const c = { years: o.years, role: o.role, bonus: o.bonus, win: o.win, pod: o.pod, cl: o.cl };
    const extra = [];
    if (p.wantN1 && o.role === 'n2') { c.role = 'eq'; extra.push(V.n2); }
    if (Math.abs(o.years - p.pref) >= 2) { c.years = p.pref; extra.push(V.years(p.pref)); }
    c.sal = up(Math.max(o.sal + sstep, need(c) * 1.04), sstep);
    st.counter = c;
    say('a', [...extra, any(V.counter, sub.key)(c)].join(' '));
    o = { ...c };
  }
  function offer() {
    say('m', `Je propose ${fmtM(o.sal)} M€ par saison sur ${ans(o.years)}${drv ? `, statut ${ROLES[o.role]}` : ''}${o.bonus ? `, plus ${fmtM(o.bonus)} M€ de prime` : ''}${o.win || o.pod ? `, primes ${fmtM(o.win)} M€ par victoire et ${fmtM(o.pod)} M€ par podium` : ''}${drv ? `, clause ${CLAUSES[o.cl][0].toLowerCase()}` : ''}.`);
    const s = score(o), T = thr();
    if (s >= T) { st.deal = { ...o }; st.counter = null; say('a', any(V.yes, sub.key)); }
    else if (s < 0.75 * T) {
      st.pat -= 2; st.int = Math.max(5, st.int - 8);
      if (st.pat <= 0) { st.off = true; say('a', V.off); broke(); } else say('a', any(V.low, sub.key));
    } else {
      st.pat -= 1;
      if (st.pat <= 0) { st.off = true; say('a', V.off); broke(); } else counter();
    }
  }
  function offerFee() {
    const c = st.club;
    say('m', `Je propose ${fmtM(fee)} M€ d’indemnité.`);
    if (fee >= c.min - 1e-6) { c.ok = true; st.fee = fee; say('c', TXT.club.yes(sub.club)); open(); o = { ...st.dem }; }
    else if (fee < 0.6 * c.f0) { c.pat -= 2; if (c.pat <= 0) { st.off = true; say('c', TXT.club.off(sub.club)); broke(sub.club); } else say('c', TXT.club.low(sub.name)); }
    else {
      c.pat -= 1;
      if (c.pat <= 0) { st.off = true; say('c', TXT.club.off(sub.club)); broke(sub.club); }
      else { c.ask = Math.max(up(c.min, fstep), up((fee + c.ask) / 2, fstep)); fee = c.ask; say('c', any(TXT.club.counter, sub.key)(c.ask)); }
    }
  }
  function bind() {
    const q = s => el.querySelectorAll(s), act = f => () => { f(); ctx.save?.(); render(); };
    q('[data-q]').forEach(b => b.onclick = () => { close(); cb.close?.(); });
    q('[data-s]').forEach(b => b.onclick = () => { o.sal = Math.max(sstep, r1(o.sal + b.dataset.s * sstep)); render(); });
    q('[data-b]').forEach(b => b.onclick = () => { o.bonus = Math.max(0, r1(o.bonus + b.dataset.b * bstep)); render(); });
    q('[data-y]').forEach(b => b.onclick = () => { o.years = +b.dataset.y; render(); });
    q('[data-w]').forEach(b => b.onclick = () => { o.win = Math.max(0, r1(o.win + b.dataset.w * 0.1)); render(); });
    q('[data-p]').forEach(b => b.onclick = () => { o.pod = Math.max(0, r1(o.pod + b.dataset.p * 0.1)); render(); });
    q('[data-cl]').forEach(b => b.onclick = () => { o.cl = b.dataset.cl; render(); });
    q('[data-r]').forEach(b => b.onclick = () => { o.role = b.dataset.r; render(); });
    q('[data-f]').forEach(b => b.onclick = () => { fee = Math.max(fstep, r1(fee + b.dataset.f * fstep)); render(); });
    const g = el.querySelector('#nGo'); if (g) g.onclick = act(offer);
    const f = el.querySelector('#nFee'); if (f) f.onclick = act(offerFee);
    const a = el.querySelector('#nAcc'); if (a) a.onclick = act(() => { st.deal = { ...st.counter }; st.counter = null; say('m', 'J’accepte vos conditions.'); say('a', any(V.yes, sub.key)); });
    // signature : poignée de main et photo devant la voiture (3D), puis la carte du contrat
    const s = el.querySelector('#nSign'); if (s) s.onclick = () => {
      const k = { ...st.deal, clause: drv ? r1(CLAUSES[st.deal.cl || 'none'][1] * st.deal.sal) : 0 };
      const card = () => signing(sub, k, ctx, () => { signedMail(k); st.done = true; ctx.save?.(); close(); cb.done(k, st.fee || 0); });
      close(); setCast(castOf(sub));
      startScene('sign', { top: sub.cur ? 'Prolongation' : 'Signature officielle', name: sub.name, sub: `${sub.cur ? 'prolonge chez' : 'rejoint'} ${ctx.team} · photo devant la voiture` }, card);
    };
  }
  render();
}

// cinématique de signature : la carte se retourne, tampon « SIGNÉ », signature à l'encre, confettis
export function signing(sub, k, ctx, onEnd) {
  const el = document.getElementById('nego');
  const col = ['#ff3d2e', '#ffc83d', '#22d3c5', '#ffffff', '#3aa0ff', '#3ddc84'];
  const conf = Array.from({ length: 46 }, (_, i) => `<i style="--x:${(i * 37) % 100}%;--k:${col[i % col.length]};--d:${(1.05 + (i % 9) * 0.07).toFixed(2)}s;--t:${(1.4 + (i % 5) * 0.25).toFixed(2)}s;--r:${(i % 2 ? 1 : -1) * (360 + (i % 4) * 160)}deg"></i>`).join('');
  const until = ctx.year + k.years - 1;
  const head = sub.cur ? `✍️ ${sub.name} prolonge jusqu’en ${until} !` : `📰 ${sub.name} rejoint ${ctx.team} !`;
  el.innerHTML = `<div class="ncard sgn" style="--c:${sub.color || 'var(--acc)'}"><div class="conf">${conf}</div>
    <p class="sghead">${head}</p>
    <div class="sgcard"><span class="av">${initials(sub.name)}</span><b>${sub.name}</b><small>${sub.from || ''}</small>
      <span class="sgk">${ans(k.years)} · ${fmtM(k.sal)} M€ / saison${sub.kind === 'drv' ? ` · ${ROLES[k.role]}` : ''}${k.win || k.pod ? `<br>primes ${fmtM(k.win || 0)} M€ / victoire · ${fmtM(k.pod || 0)} M€ / podium` : ''}${k.clause ? `<br>clause libératoire ${fmtM(k.clause)} M€` : ''}</span>
      <svg class="sig" viewBox="0 0 200 40"><path d="M6 30 C 22 4, 34 40, 52 20 S 80 4, 92 26 S 124 36, 138 14 S 170 8, 194 22"/></svg>
      <i class="stamp">SIGNÉ</i></div>
    <button class="main" id="sgOk">Continuer</button></div>`;
  el.classList.remove('hidden');
  el.querySelector('#sgOk').onclick = onEnd;
}
