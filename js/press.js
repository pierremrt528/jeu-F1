// Conférence de presse après la course : questions de journalistes (fictifs) adaptées au résultat,
// réponses à choix avec bonus/malus (moral, budget, réputation, performance à la prochaine course).
import { team, applyFx, FXNAMES } from './team.js';
import { TEAMS } from './data.js';

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
  // contexte de la course : abandon, pluie, stratégie, crevaison, voiture de sécurité, pénalité
  { when: c => c.dnf, res: true, q: c => `Abandon à ${c.circuit}. Fiabilité ou erreur de pilotage ?`, a: [
    ["C'est ma faute, j'ai poussé au-delà de la limite.", { moral: 4, rep: 3, conf: 1 }],
    ["La voiture m'a lâché. L'usine doit se remettre en question.", { moral: -6, rep: -2, conf: -3 }],
    ["La malchance fait partie de la course. On repart de l'avant.", { moral: 2 }],
  ] },
  { when: c => c.wet, q: c => 'La pluie a tout chamboulé aujourd’hui. Aimez-vous ces conditions ?', a: [
    ["Sous la pluie, c'est le pilote qui fait la différence. J'adore.", { moral: 4, rep: 2, perf: { mu: 0.006 } }],
    ['Trop dangereux, on aurait dû attendre derrière la voiture de sécurité.', { rep: -2, moral: 1 }],
    ["Le choix des pneus était un vrai casse-tête pour le muret.", { moral: 2 }],
  ] },
  { when: c => c.stops >= 2, q: c => `${c.stops} arrêts aujourd'hui. La stratégie était-elle la bonne ?`, a: [
    ["Oui, des pneus frais au bon moment, c'est ce qui a payé.", { moral: 3, rep: 1 }],
    ["On a été trop prudents. Il faudra oser plus.", { moral: -2, perf: { acc: 0.008 } }],
    ['Le muret a pris les décisions, je fais confiance aux ingénieurs.', { moral: 4, conf: 1 }],
  ] },
  { when: c => c.stops === 0 && !c.dnf, q: c => 'Aucun arrêt au stand : un pari très risqué, non ?', a: [
    ['Qui ne tente rien n’a rien. Les pneus ont tenu.', { moral: 4, rep: 3 }],
    ['On savait que les pneus tiendraient, tout était calculé.', { rep: 2, conf: 1 }],
    ["J'avais les pneus en lambeaux à la fin, plus jamais !", { moral: 2, rep: -1 }],
  ] },
  { when: c => c.flat, q: c => 'Une crevaison a ruiné votre course. Avez-vous trop tiré sur les pneus ?', a: [
    ['Oui, on a voulu aller trop loin. Leçon retenue.', { moral: 2, rep: 3 }],
    ['Les pneus de ce manufacturier ne sont pas fiables.', { rep: -4, moral: 2 }],
    ["C'est la course. On aura plus de chance la prochaine fois.", { moral: 1 }],
  ] },
  { when: c => c.sc, q: c => 'La voiture de sécurité a redistribué les cartes. Chanceux ?', a: [
    ['On a su saisir l’occasion au bon moment, c’est du travail d’équipe.', { moral: 4, rep: 2 }],
    ["Elle m'a volé une meilleure place, c'est frustrant.", { moral: -2, rep: -1 }],
    ['Ça fait partie du spectacle.', { rep: 2 }],
  ] },
  { when: c => c.pen > 0, q: c => `Une pénalité de ${c.pen} secondes aujourd'hui. Justifiée ?`, a: [
    ["Oui, j'ai fait une erreur, je l'assume.", { rep: 4, moral: -1 }],
    ['Non, les commissaires se sont trompés.', { rep: -5, moral: 3 }],
    ["Pas de commentaire, je ne veux pas d'amende.", { moral: -1 }],
  ] },
  // championnat, saison, direction
  { when: c => c.champ === 1 && c.round > 2, q: c => 'Vous êtes en tête du championnat. Pouvez-vous tenir jusqu’au bout ?', a: [
    ['On prend course après course, rien n’est fait.', { moral: 4, rep: 3, conf: 2 }],
    ['Le titre est pour nous, les autres le savent.', { moral: 6, rep: -4, perf: { vtop: 0.006 } }],
    ['La pression est sur les autres, pas sur nous.', { moral: 3, rep: 1 }],
  ] },
  { when: c => c.champ > 1 && c.champ <= 3 && c.round > 3, q: c => `P${c.champ} au championnat : le titre est-il encore possible ?`, a: [
    ['Bien sûr, tant que les maths le permettent on y croit.', { moral: 5, conf: 2 }],
    ['Il faudra des nouvelles pièces rapidement.', { budget: 1.2, moral: -1 }],
    ['On vise d’abord le podium du championnat.', { moral: 2, rep: 2 }],
  ] },
  { when: c => c.mid, q: c => 'La moitié de la saison est passée. Quel bilan faites-vous ?', a: [
    ['Positif, on progresse et l’usine travaille bien.', { moral: 4, conf: 2 }],
    ['Mitigé. On doit frapper fort au mercato.', { moral: -2, rep: 2, conf: -1 }],
    ['Je ne fais les bilans qu’à la fin.', { moral: 1 }],
  ] },
  { when: c => c.round >= c.n - 3 && !c.last, q: c => `Plus que ${c.n - c.round} courses. Quel objectif pour finir la saison ?`, a: [
    ['Finir fort pour préparer l’an prochain.', { moral: 3, conf: 2 }],
    ['Tout donner, il reste des points à prendre.', { moral: 4, perf: { acc: 0.008 } }],
    ['Penser déjà à la voiture de l’an prochain.', { moral: -2, budget: 1 }],
  ] },
  { when: c => c.last, q: c => 'La saison est terminée. Un mot pour votre équipe ?', a: [
    ['Merci à tous, cette saison restera gravée.', { moral: 8, rep: 3 }],
    ["On n'a pas été à la hauteur, il faudra tout changer.", { moral: -6, conf: 3 }],
    ['Vivement l’année prochaine !', { moral: 4 }],
  ] },
  { when: c => c.role === 'n2', q: c => `${c.mate || 'Votre équipier'} a le statut de numéro 2. Est-ce juste ?`, a: [
    ['Les consignes servent l’écurie, il le comprend.', { conf: 2, moral: -1 }],
    ['Il aura sa chance s’il est plus rapide que moi.', { moral: 3, rep: 1 }],
    ['Je préfère ne pas commenter la stratégie interne.', { moral: -1 }],
  ] },
  { when: c => c.role === 'n1', q: c => `${c.mate || 'Votre équipier'} est le numéro 1 de l'écurie. Vous l'acceptez ?`, a: [
    ["Il l'a mérité, je dois faire mieux.", { moral: 3, rep: 3 }],
    ['Sur la piste, il n’y a pas de numéro 1.', { moral: 2, rep: -2, perf: { vtop: 0.005 } }],
    ['Ça changera vite, croyez-moi.', { moral: 4, conf: -2 }],
  ] },
  { when: c => c.conf != null && c.conf < 35, q: c => 'On dit que la direction perd patience. Votre poste est-il menacé ?', a: [
    ['Je sais que les résultats doivent arriver. Ils arriveront.', { conf: 3, moral: 2 }],
    ['La direction devrait plutôt nous donner plus de moyens.', { conf: -6, budget: 2, rep: 1 }],
    ['Je ne lis pas les rumeurs.', { moral: -2 }],
  ] },
  { when: c => c.tier === 'new', q: c => 'Une nouvelle écurie peut-elle vraiment exister face aux géants ?', a: [
    ['Chaque géant a commencé petit. Rendez-vous dans deux ans.', { rep: 4, moral: 3 }],
    ['Il nous faut de nouveaux partenaires, l’appel est lancé.', { budget: 2, rep: -2 }],
    ['On est là pour apprendre, sans pression.', { moral: 3, conf: -1 }],
  ] },
  { when: c => c.tier === 'top', q: c => 'Avec votre budget, tout autre résultat que la victoire est-il un échec ?', a: [
    ['Oui. Nous sommes là pour gagner, point.', { conf: 3, moral: -2, perf: { acc: 0.008 } }],
    ["L'argent ne fait pas tout, la concurrence est rude.", { conf: -3, rep: 2 }],
    ['Nous visons le titre, et nous travaillons pour.', { conf: 2, moral: 1 }],
  ] },
  { when: c => c.reg, q: c => 'Le nouveau règlement va rebattre les cartes. Une chance pour vous ?', a: [
    ['Oui, nos ingénieurs ont déjà des idées.', { moral: 4, conf: 1 }],
    ['C’est un coût énorme pour les petites écuries.', { budget: 1.5, rep: 1 }],
    ['On verra en début de saison prochaine.', { moral: 1 }],
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
  { q: c => 'Les jeunes de Formule B frappent à la porte. Faut-il leur faire confiance ?', a: [
    ['Oui, la jeunesse fait avancer ce sport.', { rep: 4, moral: 1 }],
    ["L'expérience compte plus que tout en course.", { rep: -1, moral: 2 }],
    ["Notre académie va justement en accueillir.", { rep: 2, budget: -0.5, moral: 2 }],
  ] },
  { q: c => 'Vingt-deux Grands Prix dans l’année : le calendrier est-il trop chargé ?', a: [
    ['Les fans en veulent toujours plus, on est là pour eux.', { rep: 4, moral: -2 }],
    ['Les mécaniciens sont épuisés, il faut les protéger.', { moral: 6, rep: 1, conf: -1 }],
    ['Plus de courses, plus de revenus.', { budget: 1, rep: -2 }],
  ] },
  { q: c => 'Les écuries dépensent-elles trop d’argent ?', a: [
    ['Oui, un plafond de dépenses serait juste.', { rep: 3 }],
    ['Chacun investit comme il veut.', { rep: -2, budget: 0.8 }],
    ['Nous, on compte chaque euro.', { conf: 2, moral: 1 }],
  ] },
  { q: c => 'Un message pour vos fans ?', a: [
    ['Merci pour votre soutien, on se bat pour vous.', { rep: 4, moral: 2 }],
    ['Venez nombreux au prochain Grand Prix !', { rep: 2, budget: 0.6 }],
    ['Restez patients, les grands moments arrivent.', { moral: 3 }],
  ] },
  { q: c => 'Quel est votre plus grand défaut en piste ?', a: [
    ["L'impatience. Je veux toujours dépasser trop tôt.", { rep: 3, moral: 1 }],
    ["Je n'en ai pas.", { rep: -4, moral: 4 }],
    ['La gestion des pneus, j’y travaille.', { moral: 1, perf: { mu: 0.005 } }],
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

export const fxText = fx => {
  const out = [];
  if (fx.moral) out.push([`Moral ${fx.moral > 0 ? '+' : ''}${fx.moral}`, fx.moral > 0]);
  if (fx.budget) out.push([`Budget ${fx.budget > 0 ? '+' : ''}${fx.budget.toFixed(1)} M€`, fx.budget > 0]);
  if (fx.rep) out.push([`Réputation ${fx.rep > 0 ? '+' : ''}${fx.rep}`, fx.rep > 0]);
  if (fx.conf) out.push([`Confiance de la direction ${fx.conf > 0 ? '+' : ''}${fx.conf}`, fx.conf > 0]);
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
  `<span class="tn">${TEAMS[0].name}</span><span>Moral <b>${team.moral}</b></span><span>Budget <b>${team.budget.toFixed(1)} M€</b></span><span>Réputation <b>${team.rep}</b></span>`;
