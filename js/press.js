// Conférence de presse après la course : questions de journalistes (fictifs) adaptées au résultat,
// réponses à choix avec bonus/malus (moral, budget, réputation, performance à la prochaine course).
import { team, applyFx, FXNAMES } from './team.js';

const OUTLETS = [
  ['Claire Fontanel', 'Le Paddock', '#e5483b'], ['Hugo Marchetti', 'Vitesse Hebdo', '#3aa0ff'],
  ['Inès Rouvière', 'Gazette des Circuits', '#f2b632'], ['Mats Elling', 'Radio Grand Tour', '#3ddc84'],
  ['Sofia Lindgren', 'Grille de Départ TV', '#b06cff'], ['Bastien Kerloc\'h', 'Moteur & Passion', '#ff8a3d'],
];

// q(ctx) renvoie le texte ; when(ctx) filtre ; a = réponses [texte, effets]
const Q = [
  { when: c => c.pos === 1, res: true, q: c => `Victoire à ${c.circuit} ! Qu'est-ce qui a fait la différence aujourd'hui ?`, a: [
    ["Toute l'équipe a fait un travail énorme, cette victoire est la leur.", { moral: 10, rep: 4 }],
    ["J'étais simplement le plus rapide. Les autres devront s'habituer.", { moral: 5, rep: -4, perf: { acc: 0.015 } }],
    ['Nos partenaires nous ont donné les moyens de gagner, merci à eux.', { budget: 1.8, rep: 2 }],
  ] },
  { when: c => c.pos > 1 && c.pos <= 3, res: true, q: c => `Un podium à ${c.circuit}. Frustré de ne pas avoir gagné face à ${c.winner} ?`, a: [
    ['Un podium, ça se savoure. On progresse course après course.', { moral: 6, rep: 3 }],
    [c => `${c.winner} a eu de la chance. La prochaine fois, c'est pour moi.`, { moral: 4, rep: -3, perf: { vtop: 0.008 } }],
    ['Il nous manque encore un peu de vitesse, on va pousser les ingénieurs.', { moral: -2, perf: { acc: 0.012 } }],
  ] },
  { when: c => c.pos > 10, res: true, q: c => `P${c.pos} aujourd'hui. Est-ce que l'écurie traverse une crise ?`, a: [
    ["Pas du tout. On analyse les données et on reviendra plus forts.", { moral: 3, rep: 2 }],
    ["La voiture n'était pas au niveau, il faut que ça change vite.", { moral: -8, rep: -2, perf: { mu: 0.01 } }],
    ["C'est ma faute, j'ai fait trop d'erreurs. Je m'excuse auprès de l'équipe.", { moral: 6, rep: 4, perf: { vtop: -0.004 } }],
  ] },
  { when: c => c.pos > 3 && c.pos <= 10, res: true, q: c => `Dans les points à ${c.circuit}. Satisfait de cette P${c.pos} ?`, a: [
    ['Ce sont des points précieux, on construit notre saison.', { moral: 4, rep: 2 }],
    [c => `Non, je vise le podium. P${c.pos} ne me suffit pas.`, { moral: -2, rep: 3, perf: { acc: 0.01 } }],
    ["On a pris des risques en stratégie, ça n'a pas payé cette fois.", { moral: 1, rep: 1 }],
  ] },
  { when: c => c.gained >= 4, q: c => `Vous avez gagné ${c.gained} places depuis la grille. Quel a été votre plus beau dépassement ?`, a: [
    ['Celui au premier virage, j\'ai freiné très tard.', { moral: 6, rep: 4 }],
    ["Je préfère garder mes secrets pour la prochaine course.", { moral: 3, perf: { brake: 0.01 } }],
    ["Les autres pilotes étaient trop prudents, voilà tout.", { moral: 4, rep: -4, perf: { vtop: 0.006 } }],
  ] },
  { when: c => c.gained <= -4, q: c => `Vous avez perdu ${-c.gained} places en course. Que s'est-il passé ?`, a: [
    ["J'ai eu du mal à trouver le bon rythme, on doit comprendre pourquoi.", { moral: -2, rep: 2, perf: { mu: 0.008 } }],
    ['On a été gênés en piste, certains pilotes conduisent dangereusement.', { moral: 2, rep: -4 }],
    ["Le moteur manquait de puissance, j'attends des réponses de l'usine.", { moral: -4, budget: 1.2 }],
  ] },
  { when: c => !!c.rival, q: c => `Vous avez fini juste derrière ${c.rival}. Est-ce votre grand rival cette saison ?`, a: [
    [c => `${c.rival} est un excellent pilote, j'ai beaucoup de respect pour lui.`, { rep: 5, moral: 1 }],
    [`Un rival ? Il faudrait déjà qu'il arrive à me suivre.`, { rep: -5, moral: 4, perf: { acc: 0.01 } }],
    ["Je ne regarde que ma propre course.", { moral: 2 }],
  ] },
  { when: c => !!c.mate, q: c => `Votre coéquipier ${c.mate} a terminé ${c.mateAhead ? 'devant' : 'derrière'} vous. L'ambiance est-elle bonne dans l'équipe ?`, a: [
    ['Excellente, on se pousse mutuellement et l\'écurie en profite.', { moral: 5, rep: 3 }],
    [c => c.mateAhead ? "Il a eu la meilleure stratégie aujourd'hui, rien de plus." : 'Je suis le leader de cette équipe, les chiffres le montrent.', { moral: 2, rep: -2, perf: { vtop: 0.005 } }],
    ['Je préfère ne pas parler de ça.', { moral: -3 }],
  ] },
  { when: c => c.fastest, q: c => 'Vous signez le meilleur tour en course. La voiture semble avoir un gros potentiel ?', a: [
    ['Oui, et les nouvelles pièces vont encore nous aider.', { moral: 5, rep: 3 }],
    ["Le potentiel est là, mais il faut plus d'investissements.", { budget: 2.1, rep: -1 }],
    ['Un meilleur tour ne rapporte pas de trophée. On veut gagner.', { moral: 3, perf: { acc: 0.01 } }],
  ] },
  { q: c => 'Des rumeurs vous envoient dans une autre écurie la saison prochaine. Info ou intox ?', a: [
    ["Intox. Je suis bien ici et je veux gagner avec cette équipe.", { moral: 8, rep: 2 }],
    ['Je ne ferme aucune porte. Le meilleur projet gagnera.', { moral: -8, budget: 2.4, rep: 2 }],
    ['Pas de commentaire.', { moral: -2 }],
  ] },
  { q: c => 'Un nouveau sponsor serait prêt à rejoindre l\'écurie. Êtes-vous prêt à faire plus d\'événements pour lui ?', a: [
    ['Bien sûr, nos partenaires sont essentiels.', { budget: 3, moral: -4 }],
    ['Je préfère rester concentré sur le pilotage.', { moral: 4, perf: { mu: 0.006 } }],
    ['Seulement si ça finance les nouvelles pièces au garage.', { budget: 1.5, rep: 1 }],
  ] },
  { q: c => 'On dit que votre budget est plus serré que celui des grandes écuries. Est-ce un handicap ?', a: [
    ["L'argent ne fait pas tout. On travaille plus intelligemment.", { moral: 5, rep: 4 }],
    ["Oui, et on a besoin de nouveaux partenaires. L'appel est lancé !", { budget: 2.4, rep: -3 }],
    ["On va prouver sur la piste qu'on mérite plus.", { moral: 3, perf: { vtop: 0.005 } }],
  ] },
  { q: c => `Quel regard portez-vous sur le circuit de ${c.circuit} ?`, a: [
    ["Un tracé magnifique, les fans ici sont incroyables.", { rep: 5 }],
    ["Un peu trop dangereux à mon goût, la sécurité doit progresser.", { rep: -2, moral: 2 }],
    ["Il faut y être parfait au freinage, c'est ce que j'adore.", { moral: 3, perf: { brake: 0.008 } }],
  ] },
  { q: c => 'Les ingénieurs ont-ils une évolution prévue pour la prochaine course ?', a: [
    ["Oui, un nouvel aileron arrive. On a hâte de l'essayer.", { moral: 4, perf: { kd: 0.03 } }],
    ['Je ne peux rien dire, mais on travaille jour et nuit.', { moral: 3, rep: 1 }],
    ['Pas encore, il faut plus de moyens pour développer la voiture.', { budget: 1.5, moral: -3 }],
  ] },
  { q: c => 'Les commissaires ont été critiqués ce week-end. Votre avis ?', a: [
    ['Ils font un métier difficile, je leur fais confiance.', { rep: 5 }],
    ['Certaines décisions étaient incompréhensibles.', { rep: -5, moral: 3 }],
    ["Je préfère parler de pilotage.", { moral: 1 }],
  ] },
  { q: c => 'Comment gérez-vous la pression avant le départ ?', a: [
    ["Je reste avec mes mécaniciens, ça me détend.", { moral: 6 }],
    ["Je n'ai pas de pression, c'est aux autres d'en avoir.", { rep: -3, moral: 3, perf: { acc: 0.008 } }],
    ['Je fais de la méditation et je visualise chaque virage.', { moral: 3, perf: { steer: 0.03 } }],
  ] },
];

const pick = a => a[(Math.random() * a.length) | 0];
function shuffle(a) { for (let i = a.length - 1; i > 0; i--) { const j = (Math.random() * (i + 1)) | 0; [a[i], a[j]] = [a[j], a[i]]; } return a; }

export function buildQuestions(ctx, n = 3) {
  const special = shuffle(Q.filter(q => q.when && q.when(ctx)));
  const general = shuffle(Q.filter(q => !q.when));
  const list = [];
  // une question sur le résultat d'abord, puis du contexte et du général
  const res = special.find(q => q.res);
  if (res) list.push(res);
  for (const q of special) if (list.length < n - 1 && !list.includes(q) && !q.res) list.push(q);
  for (const q of general) if (list.length < n) list.push(q);
  const people = shuffle(OUTLETS.slice());
  return list.map((q, k) => ({ text: q.q(ctx), who: people[k], answers: q.a.map(([t, fx]) => [typeof t === 'function' ? t(ctx) : t, fx]) }));
}

const fxText = fx => {
  const out = [];
  if (fx.moral) out.push([`Moral ${fx.moral > 0 ? '+' : ''}${fx.moral}`, fx.moral > 0]);
  if (fx.budget) out.push([`Budget ${fx.budget > 0 ? '+' : ''}${fx.budget.toFixed(1)} M€`, fx.budget > 0]);
  if (fx.rep) out.push([`Réputation ${fx.rep > 0 ? '+' : ''}${fx.rep}`, fx.rep > 0]);
  if (fx.perf) for (const k in fx.perf) out.push([`${fx.perf[k] > 0 ? '+' : ''}${(fx.perf[k] * 100).toFixed(1).replace('.0', '')} % ${FXNAMES[k]} (prochaine course)`, fx.perf[k] > 0]);
  return out.map(([t, good]) => `<span class="${good ? 'up' : 'down'}">${t}</span>`).join('');
};

// Écran : une question à la fois ; onDone appelé à la fin
export function runPress(el, questions, onDone) {
  let k = 0;
  const step = () => {
    const q = questions[k];
    el.querySelector('#pressBody').innerHTML = `
      <div class="pq"><i style="background:${q.who[2]}">${q.who[0].split(' ').map(w => w[0]).join('')}</i>
        <div><small>${q.who[0]} · ${q.who[1]} — question ${k + 1}/${questions.length}</small><p>« ${q.text} »</p></div></div>
      <div class="pans">${q.answers.map((a, i) => `<button data-a="${i}">${a[0]}</button>`).join('')}</div>`;
    el.querySelectorAll('[data-a]').forEach(b => b.onclick = () => {
      const fx = q.answers[+b.dataset.a][1];
      applyFx(fx);
      el.querySelectorAll('[data-a]').forEach(x => { x.disabled = true; if (x !== b) x.classList.add('off'); });
      b.classList.add('chosen');
      b.insertAdjacentHTML('beforeend', `<div class="pfx">${fxText(fx)}</div>`);
      el.querySelector('#pressState').innerHTML = teamLine();
      setTimeout(() => { k++; k < questions.length ? step() : finish(); }, 1600);
    });
  };
  const finish = () => {
    el.querySelector('#pressBody').innerHTML = `<p class="pend">Fin de la conférence de presse.</p>` +
      (team.boostTxt ? `<p class="pend">Pour la prochaine course : <b>${team.boostTxt}</b></p>` : '') +
      `<button class="main" id="pressDone">Continuer</button>`;
    el.querySelector('#pressDone').onclick = onDone;
  };
  el.querySelector('#pressState').innerHTML = teamLine();
  step();
}

export const teamLine = () =>
  `<span>Moral <b>${team.moral}</b></span><span>Budget <b>${team.budget.toFixed(1)} M€</b></span><span>Réputation <b>${team.rep}</b></span>`;
