// Histoires à épisodes (5-6 événements liés, étalés sur plusieurs GP) et événements supplémentaires.
// Une histoire : steps { clé: { icon, title, text(c, v), ch: [{ t, fx, out, next: clé suivante (absente = fin), gap: GP d'attente }] } }.
// v : variables de l'histoire (tirées par init au début : noms…). Effets : ceux de events.js, plus
// sign (le pilote de l'histoire devient l'équipier, contrat { sal, years, role }), deal à gp négatif (coût par GP).
const rnd = (a, b) => a + Math.floor(Math.random() * (b - a + 1));
const pick = a => a[(Math.random() * a.length) | 0];
const FIRST = ['N.', 'L.', 'T.', 'É.', 'A.', 'M.', 'J.', 'R.', 'S.', 'K.'];
const LAST = ['Vasseur', 'Okoro', 'Lindahl', 'Marchal', 'Ferraz', 'Ivanova', 'Delcourt', 'Sato', 'Brunel', 'Kovač', 'Aldana', 'Ruiz'];
const nm = () => pick(FIRST) + ' ' + pick(LAST);

export const STORIES = [
  // 1. Le réserviste (l'exemple du joueur) : équipier malade → remplaçant → séance incroyable → 3e pilote → contrat
  { id: 'reserve', title: 'Le réserviste', icon: '🧑‍🚀', when: c => c.mate,
    init: () => ({ drv: { name: nm(), from: 'Pilote de réserve', spd: rnd(74, 82), strat: rnd(62, 72), tyre: rnd(64, 76), pot: rnd(84, 95) }, vet: nm() }),
    steps: {
      a: { icon: '🤒', title: 'Équipier malade', text: (c, v) => `${c.mate} est cloué au lit par une forte fièvre à deux jours des essais. Qui fait rouler la deuxième voiture ?`, ch: [
        { t: (c, v) => `Le jeune réserviste ${v.drv.name}`, fx: { rep: 1 }, out: (c, v) => `${v.drv.name} n’en revient pas : il va piloter en Grand Prix.`, next: 'b', gap: 1 },
        { t: (c, v) => `Le vétéran ${v.vet} (0,5 M€)`, fx: { budget: -0.5, conf: 1 }, out: 'Un choix prudent : il ramènera la voiture entière.' },
        { t: (c, v) => `${c.mate} roule malgré tout`, fx: { mate: { spd: -1 }, moral: -1 }, out: 'Il serre les dents, mais il n’est pas à 100 %.' },
      ] },
      b: { icon: '⏱️', title: 'Une séance incroyable', text: (c, v) => `Stupeur au muret : en essais libres, ${v.drv.name} a signé un temps à trois dixièmes de ${c.mate} sur sa voiture habituelle. Les journalistes ne parlent que de lui.`, ch: [
        { t: 'Le refaire rouler en essais dans deux GP', fx: { perf: { mu: -0.002 }, rep: 2 }, out: 'Rendez-vous est pris : on veut confirmer.', next: 'c', gap: 2 },
        { t: 'Le recruter tout de suite comme 3e pilote (0,3 M€ par GP)', fx: { deal: { name: 'Contrat 3e pilote', gp: -0.3, n: 8 }, moral: 1 }, out: 'Il signe un contrat de troisième pilote, il suivra l’équipe partout.', next: 'd', gap: 2 },
        { t: 'Le remercier : ce n’était qu’un dépannage', fx: { moral: 1 }, out: (c, v) => `${v.drv.name} retourne en Formule B, un peu déçu.` },
      ] },
      c: { icon: '🔁', title: 'Deuxième chance', text: (c, v) => `${v.drv.name} roule de nouveau en essais libres… et termine dans le top 8 de la séance. ${c.mate} commence à regarder le chrono de travers.`, ch: [
        { t: 'Lui offrir un contrat de 3e pilote (0,3 M€ par GP)', fx: { deal: { name: 'Contrat 3e pilote', gp: -0.3, n: 8 }, rep: 1 }, out: 'Il fait désormais partie de l’écurie.', next: 'd', gap: 2 },
        { t: (c) => `Rassurer ${c.mate}`, fx: { moral: 2 }, out: 'L’équipier reste le titulaire, le jeune patiente.', next: 'e', gap: 3 },
        { t: 'Le prêter à une petite écurie (+0,8 M€)', fx: { budget: 0.8 }, out: 'Il part se faire les dents ailleurs.' },
      ] },
      d: { icon: '📈', title: 'Le 3e pilote progresse', text: (c, v) => `Au simulateur, ${v.drv.name} apporte des réglages précieux. Mais ${c.rivalTeam} lui fait les yeux doux.`, ch: [
        { t: 'Lui confier le développement au simulateur', fx: { perf: { kd: 0.004 }, dev: -1 }, out: 'Ses retours accélèrent l’usine.', next: 'e', gap: 2 },
        { t: 'Le laisser courir en Formule B le week-end', fx: { rep: 2 }, out: 'Il gagne en Formule B et fait parler de l’écurie.', next: 'e', gap: 2 },
        { t: (c) => `Le laisser partir chez ${c.rivalTeam} (+1 M€)`, fx: { budget: 1 }, out: 'Une indemnité correcte… pour un futur rival.' },
      ] },
      e: { icon: '✍️', title: 'L’heure du choix', text: (c, v) => `${v.drv.name} veut une réponse : son agent exige un volant de titulaire, sinon il signe ailleurs. ${c.mate} sent la pression.`, ch: [
        { t: (c, v) => `Signer ${v.drv.name} comme titulaire (2 saisons)`, fx: { sign: { drv: 'drv', sal: 1.5, years: 2, role: 'eq' }, rep: 2, moral: -1 }, out: (c, v) => `${v.drv.name} devient ton équipier ! ${c.mate} part rejoindre les pilotes libres.` },
        { t: 'Le garder comme réserviste de luxe (1 M€)', fx: { budget: -1, mate: { spd: 1 } }, out: (c) => `Il reste dans l’ombre et pousse ${c.mate} à se dépasser.` },
        { t: (c) => `Garder ${c.mate} et le laisser partir`, fx: { moral: 3, rep: -1 }, out: 'L’équipier apprécie ta fidélité.' },
      ] },
    } },
  // 2. Le sponsor mystère
  { id: 'mystery', title: 'Le sponsor mystère', icon: '🕶️',
    init: () => ({ firm: pick(['Obsidia Capital', 'Nexvault', 'Aurum Prime', 'Celestra Group']) }),
    steps: {
      a: { icon: '💼', title: 'Une offre trop belle', text: (c, v) => `Une société inconnue, ${v.firm}, propose 1 M€ par Grand Prix sans rien demander en échange. Son patron reste introuvable.`, ch: [
        { t: 'Accepter sans poser de questions', fx: { deal: { name: 'Sponsor mystère', gp: 1, n: 4 } }, out: 'L’argent arrive dès le lendemain.', next: 'b', gap: 2 },
        { t: 'Enquêter avant de signer (0,2 M€)', fx: { budget: -0.2 }, out: 'Un détective privé se met au travail.', next: 'c', gap: 1 },
        { t: 'Refuser', fx: { conf: 1 }, out: 'Mieux vaut éviter les ennuis.' },
      ] },
      b: { icon: '📸', title: 'Photos compromettantes', text: (c, v) => `Un journal publie des photos du logo ${v.firm} sur ta voiture… à côté d’un article sur une enquête financière.`, ch: [
        { t: 'Retirer le logo immédiatement', fx: { rep: 1, budget: -1 }, out: 'Tu rends une partie de l’argent, l’image est sauve.', next: 'd', gap: 2 },
        { t: 'Défendre le partenaire', fx: { rep: -3 }, out: 'La polémique enfle.', next: 'd', gap: 2 },
        { t: 'Ne rien dire', fx: { rep: -1 }, out: 'Le silence est interprété comme un aveu.', next: 'd', gap: 2 },
      ] },
      c: { icon: '🔍', title: 'Le rapport du détective', text: (c, v) => `Le détective a trouvé : ${v.firm} appartient à un ancien pilote qui veut racheter une écurie en secret.`, ch: [
        { t: 'Le rencontrer', fx: { moral: 1 }, out: 'Rendez-vous discret dans un hôtel de luxe.', next: 'd', gap: 1 },
        { t: 'Prévenir la direction', fx: { conf: 3 }, out: 'La direction te remercie de ta loyauté.' },
        { t: 'Vendre l’info à la presse (+0,5 M€)', fx: { budget: 0.5, rep: -2 }, out: 'Scoop à la une, mais le paddock n’aime pas les mouchards.' },
      ] },
      d: { icon: '🤝', title: 'Le vrai visage', text: (c, v) => `Le patron de ${v.firm} sort de l’ombre : il veut devenir partenaire officiel… contre un siège au conseil de l’écurie.`, ch: [
        { t: 'Accepter : gros contrat, moins de pouvoir', fx: { deal: { name: v => v.firm, gp: 1.2, n: 6 }, conf: -4 }, out: 'Les caisses se remplissent, la direction grince.', next: 'e', gap: 3 },
        { t: 'Contrat simple, sans siège', fx: { deal: { name: 'Partenaire discret', gp: 0.5, n: 6 } }, out: 'Un compromis raisonnable.' },
        { t: 'Rompre tout lien', fx: { rep: 2, conf: 2 }, out: 'Page tournée.' },
      ] },
      e: { icon: '⚖️', title: 'Le dénouement', text: (c, v) => `La justice blanchit finalement ${v.firm}. Son patron propose de prolonger… ou de racheter carrément l’écurie.`, ch: [
        { t: 'Prolonger le partenariat', fx: { deal: { name: 'Partenaire fidèle', gp: 0.8, n: 6 }, rep: 2 }, out: 'Une belle histoire qui finit bien.' },
        { t: 'Refuser le rachat, garder l’indépendance', fx: { conf: 4, moral: 2 }, out: 'L’écurie reste maîtresse de son destin.' },
        { t: 'Demander une prime de fidélité (+2 M€)', fx: { budget: 2, rep: -1 }, out: 'Il paie, un peu agacé.' },
      ] },
    } },
  // 3. La taupe
  { id: 'mole', title: 'La taupe', icon: '🐀',
    init: () => ({ who: nm() }),
    steps: {
      a: { icon: '📂', title: 'Des données envolées', text: c => `Les ingénieurs sont formels : ${c.rivalTeam} a monté un fond plat presque identique au tien. Quelqu’un parle.`, ch: [
        { t: 'Lancer une enquête interne', fx: { moral: -2 }, out: 'L’ambiance devient pesante à l’usine.', next: 'b', gap: 1 },
        { t: 'Changer tous les accès informatiques', fx: { dev: 2 }, out: 'Deux jours perdus, mais les fuites cessent… pour l’instant.', next: 'b', gap: 2 },
        { t: 'Ignorer : on va plus vite qu’eux', fx: { moral: 1 }, out: 'Tu préfères regarder devant.' },
      ] },
      b: { icon: '🕵️', title: 'Un suspect', text: (c, v) => `Les soupçons se portent sur ${v.who}, un ingénieur vu à dîner avec le directeur technique de ${c.rivalTeam}.`, ch: [
        { t: 'Lui tendre un piège avec de fausses données', fx: {}, out: 'De faux plans circulent dans l’usine.', next: 'c', gap: 2 },
        { t: 'Le confronter directement', fx: { moral: -1 }, out: 'Il nie tout, les larmes aux yeux.', next: 'd', gap: 1 },
        { t: 'Le muter loin des projets sensibles', fx: { dev: 1 }, out: 'Plus de risque, un ingénieur de moins sur les pièces.' },
      ] },
      c: { icon: '🪤', title: 'Le piège fonctionne', text: c => `${c.rivalTeam} vient de tester un aileron absurde… celui des faux plans ! La taupe est démasquée.`, ch: [
        { t: 'Le renvoyer et porter plainte', fx: { rep: 3, conf: 2 }, out: 'Justice est faite, le paddock salue ta fermeté.', next: 'e', gap: 2 },
        { t: 'En faire un agent double', fx: { perf: { kd: 0.004 } }, out: 'Il renseigne désormais sur les plans du rival…', next: 'e', gap: 2 },
        { t: 'Rire de l’aileron rival en public', fx: { rep: 2, moral: 2 }, out: 'Les réseaux s’amusent, le rival fulmine.' },
      ] },
      d: { icon: '💔', title: 'Innocent ?', text: (c, v) => `Rebondissement : ${v.who} était innocent. La vraie fuite venait d’un prestataire informatique.`, ch: [
        { t: 'Présenter des excuses publiques', fx: { moral: 3, rep: 1 }, out: 'L’équipe apprécie l’honnêteté.', next: 'e', gap: 2 },
        { t: 'Lui offrir une promotion', fx: { moral: 2, dev: -1, budget: -0.3 }, out: 'Il devient l’un de tes plus fidèles ingénieurs.', next: 'e', gap: 2 },
        { t: 'Passer à autre chose', fx: { moral: -2 }, out: 'Le malaise reste.' },
      ] },
      e: { icon: '⚖️', title: 'Affaire classée', text: c => `La fédération inflige une amende à ${c.rivalTeam} pour usage de données volées et te propose une compensation.`, ch: [
        { t: 'Accepter la compensation (+1,5 M€)', fx: { budget: 1.5 }, out: 'Dossier clos, chèque encaissé.' },
        { t: 'Demander des heures de soufflerie en plus', fx: { dev: -3 }, out: 'Trois jours d’avance pour l’usine.' },
        { t: 'Renoncer avec élégance', fx: { rep: 4 }, out: 'Ton fair-play marque les esprits.' },
      ] },
    } },
  // 4. Le génie incompris
  { id: 'genius', title: 'Le génie incompris', icon: '💡',
    init: () => ({ who: nm() }),
    steps: {
      a: { icon: '📐', title: 'Une idée folle', text: (c, v) => `${v.who}, jeune ingénieur discret, dépose sur ton bureau le plan d’un aileron « à double fente mobile ». Les anciens se moquent de lui.`, ch: [
        { t: 'Lui donner une semaine en soufflerie', fx: { dev: 1 }, out: 'Il travaille jour et nuit.', next: 'b', gap: 1 },
        { t: 'Lui demander de convaincre le chef aéro', fx: { moral: 1 }, out: 'Le chef accepte d’y jeter un œil.', next: 'b', gap: 2 },
        { t: 'Classer le projet', fx: {}, out: 'Le plan finit dans un tiroir.' },
      ] },
      b: { icon: '🌬️', title: 'Les chiffres parlent', text: (c, v) => `En soufflerie, l’aileron de ${v.who} donne des chiffres étonnants… mais il vibre à haute vitesse.`, ch: [
        { t: 'Construire un prototype (0,6 M€)', fx: { budget: -0.6 }, out: 'Le prototype sera prêt dans deux courses.', next: 'c', gap: 2 },
        { t: 'Corriger les vibrations d’abord', fx: { dev: 2 }, out: 'Plus long, mais plus sûr.', next: 'c', gap: 3 },
        { t: 'Trop risqué, on arrête', fx: { moral: -2 }, out: (c, v) => `${v.who} est effondré.` },
      ] },
      c: { icon: '🧪', title: 'Premier roulage', text: (c, v) => `Le prototype de ${v.who} est prêt. Le monter en course est permis… mais personne ne l’a testé en vraie condition.`, ch: [
        { t: 'Le monter pour le prochain GP', fx: { perf: { kd: 0.008 } }, out: 'La voiture colle à la piste !', risk: { p: 0.3, fx: { perf: { kd: -0.004 }, budget: -0.4 }, out: 'L’aileron casse en essais : réparation et prochain GP compliqué.' }, next: 'd', gap: 2 },
        { t: 'Le tester en essais libres seulement', fx: { perf: { kd: 0.003 } }, out: 'Des données précieuses, sans risque.', next: 'd', gap: 2 },
        { t: 'Le garder pour la saison prochaine', fx: { conf: 1 }, out: 'L’idée mûrira.' },
      ] },
      d: { icon: '📞', title: 'Le génie convoité', text: (c, v) => `${c.rivalTeam} a remarqué ${v.who} et lui offre le double de son salaire.`, ch: [
        { t: 'Le promouvoir chef de projet (0,5 M€)', fx: { budget: -0.5, dev: -2, moral: 1 }, out: 'Il reste et prend du galon.', next: 'e', gap: 3 },
        { t: 'Lui promettre un projet à son nom', fx: { moral: 2 }, out: 'Il reste… pour l’instant.', risk: { p: 0.4, fx: { dev: 2, moral: -2 }, out: (c, v) => `${v.who} part chez ${c.rivalTeam}. Son départ désorganise l’usine.` }, next: 'e', gap: 3 },
        { t: 'Le laisser partir', fx: { budget: 0.3 }, out: 'Bonne chance à lui.' },
      ] },
      e: { icon: '🏅', title: 'La consécration', text: (c, v) => `Le paddock entier copie désormais le concept de ${v.who}. Un magazine veut le sacrer « ingénieur de l’année ».`, ch: [
        { t: 'Le mettre en avant', fx: { rep: 4, moral: 2 }, out: 'L’écurie gagne une image d’innovatrice.' },
        { t: 'Garder le secret de la prochaine évolution', fx: { dev: -3 }, out: 'Il travaille déjà sur la suite.' },
        { t: 'Breveter l’idée (+1 M€)', fx: { budget: 1, rep: -1 }, out: 'Les rivaux paient pour l’utiliser.' },
      ] },
    } },
  // 5. Guerre des pilotes
  { id: 'feud', title: 'La guerre des pilotes', icon: '⚔️', when: c => c.mate,
    steps: {
      a: { icon: '😤', title: 'Tension dans le garage', text: c => `${c.mate} s’est plaint en interne que tu aurais eu les meilleures pièces ce week-end.`, ch: [
        { t: 'Montrer les données : tout est égal', fx: { moral: 1 }, out: 'Il se calme, à moitié convaincu.', next: 'b', gap: 2 },
        { t: 'Lui rappeler qui est le patron', fx: { moral: -2, conf: 1 }, out: 'Il ravale sa colère.', next: 'b', gap: 1 },
        { t: 'Lui donner la prochaine pièce en priorité', fx: { mate: { spd: 1 }, perf: { vtop: -0.003 } }, out: 'Il est content, tu perds un peu de vitesse.' },
      ] },
      b: { icon: '📰', title: 'Interview incendiaire', text: c => `Dans un magazine, ${c.mate} déclare : « Dans cette écurie, il y a un favori. »`, ch: [
        { t: 'Répondre publiquement', fx: { rep: -1, moral: -1 }, out: 'Le feuilleton fait vendre du papier.', next: 'c', gap: 1 },
        { t: 'Réunion à huis clos', fx: { moral: 1 }, out: 'Les choses sont dites, en privé.', next: 'c', gap: 2 },
        { t: 'Lui retirer une séance au simulateur', fx: { mate: { strat: -1 } }, out: 'Sanction discrète.', next: 'c', gap: 1 },
      ] },
      c: { icon: '💥', title: 'Accrochage fratricide', text: c => `Selon les ingénieurs, ${c.mate} a défendu trop fort contre toi en course et vous avez failli vous toucher.`, ch: [
        { t: 'Imposer des consignes d’équipe', fx: { role: 'n2', moral: -2 }, out: 'Il sera numéro 2 : il devra te laisser passer.', next: 'd', gap: 2 },
        { t: 'Laisser courir : que le meilleur gagne', fx: { moral: 2, rep: 1 }, out: 'Le duel continue… pour le plus grand plaisir des fans.', next: 'd', gap: 2 },
        { t: 'Médiation avec un psychologue (0,2 M€)', fx: { budget: -0.2, moral: 3 }, out: 'Les deux pilotes finissent par en rire.', next: 'e', gap: 3 },
      ] },
      d: { icon: '📢', title: 'Le paddock s’en mêle', text: c => `Le directeur de ${c.rivalTeam} assure que ${c.mate} « serait bien mieux traité chez eux ».`, ch: [
        { t: 'Lui proposer une prolongation', fx: { wage: 0.5, moral: 2 }, out: 'Il se sent enfin respecté.', next: 'e', gap: 3 },
        { t: 'Le laisser écouter les offres', fx: { moral: -2 }, out: 'Il reste… mais la tête ailleurs.', next: 'e', gap: 3 },
        { t: 'Exiger une indemnité si on le débauche', fx: { conf: 2 }, out: 'Le rival recule.', next: 'e', gap: 3 },
      ] },
      e: { icon: '🤝', title: 'La réconciliation ?', text: c => `Après des semaines de tension, ${c.mate} propose de faire une vidéo ensemble pour les fans.`, ch: [
        { t: 'Accepter avec le sourire', fx: { rep: 3, moral: 3 }, out: 'Une vidéo hilarante qui enterre la hache de guerre.' },
        { t: 'Accepter, mais garder ses distances', fx: { rep: 1 }, out: 'Une paix froide.' },
        { t: 'Refuser', fx: { moral: -2, mate: { spd: 1 } }, out: 'Il sera encore plus motivé pour te battre.' },
      ] },
    } },
  // 6. Le milliardaire et son fils
  { id: 'tycoon', title: 'Le milliardaire', icon: '🛥️',
    init: () => ({ boss: nm(), son: { name: nm(), from: 'Pilote payant', spd: rnd(62, 70), strat: rnd(55, 65), tyre: rnd(56, 66), pot: rnd(70, 85) } }),
    steps: {
      a: { icon: '🥂', title: 'Une visite remarquée', text: (c, v) => `${v.boss}, magnat de l’immobilier, assiste au Grand Prix depuis ton garage. Il semble très intéressé par l’écurie.`, ch: [
        { t: 'Lui faire visiter le garage', fx: { moral: -1 }, out: 'Il pose des questions très précises.', next: 'b', gap: 1 },
        { t: 'Le confier au service partenaires', fx: { budget: 0.2 }, out: 'Il repart avec une casquette et un dossier.', next: 'b', gap: 2 },
        { t: 'L’ignorer poliment', fx: {}, out: 'Il ne reviendra pas.' },
      ] },
      b: { icon: '👦', title: 'Une demande paternelle', text: (c, v) => `${v.boss} propose 2 M€… si son fils ${v.son.name} roule en essais libres au prochain Grand Prix.`, ch: [
        { t: 'Accepter (+2 M€)', fx: { budget: 2, perf: { mu: -0.003 } }, out: 'Le fils roulera, tes réglages en pâtiront.', next: 'c', gap: 1 },
        { t: 'Proposer le simulateur à la place (+0,5 M€)', fx: { budget: 0.5 }, out: 'Le fils s’amuse au simulateur.', next: 'd', gap: 2 },
        { t: 'Refuser fermement', fx: { conf: 2 }, out: 'Le volant ne s’achète pas.' },
      ] },
      c: { icon: '🌀', title: 'Le fils en piste', text: (c, v) => `${v.son.name} a fait deux tête-à-queue… puis un dernier tour honorable. Son père est aux anges.`, ch: [
        { t: 'Le féliciter devant son père', fx: { rep: -1 }, out: 'Le père sourit jusqu’aux oreilles.', next: 'd', gap: 2 },
        { t: 'Dire la vérité : il n’est pas prêt', fx: { rep: 2, moral: 1 }, out: 'Le père encaisse, vexé.', next: 'd', gap: 2 },
        { t: 'Lui offrir une place à l’académie (0,3 M€)', fx: { budget: -0.3 }, out: 'Le fils va apprendre le métier.', next: 'd', gap: 2 },
      ] },
      d: { icon: '🏦', title: 'L’offre de rachat', text: (c, v) => `${v.boss} propose d’entrer au capital : 5 M€ tout de suite, mais il veut son mot à dire sur les pilotes.`, ch: [
        { t: 'Accepter (+5 M€)', fx: { budget: 5, conf: -5 }, out: 'L’argent coule à flots, la direction n’aime pas ça.', next: 'e', gap: 3 },
        { t: 'Contre-proposer un simple partenariat', fx: { deal: { name: v => 'Groupe ' + v.boss.split(' ')[1], gp: 0.5, n: 6 } }, out: 'Il accepte un rôle de sponsor.' },
        { t: 'Refuser', fx: { conf: 3 }, out: 'La direction apprécie ta loyauté.' },
      ] },
      e: { icon: '🎯', title: 'L’ultimatum', text: (c, v) => `${v.boss} exige que ${v.son.name} devienne titulaire la saison prochaine, sinon il retire ses fonds.`, ch: [
        { t: (c, v) => `Signer ${v.son.name} (il apporte un sponsor)`, fx: { sign: { drv: 'son', sal: 0.3, years: 1, role: 'n2' }, budget: 2 }, out: (c, v) => `${v.son.name} devient ton équipier. ${c.mate || 'L’ancien équipier'} est libéré.` },
        { t: 'Refuser et rembourser (−3 M€)', fx: { budget: -3, conf: 4, rep: 2 }, out: 'L’écurie reprend sa liberté.' },
        { t: 'Gagner du temps', fx: { conf: -2 }, out: 'Il patiente, l’œil mauvais.' },
      ] },
    } },
  // 7. Le retour de la légende
  { id: 'legend', title: 'Le retour de la légende', icon: '🦁',
    init: () => ({ who: pick(['V. Moranne', 'H. Castellane', 'J.-P. Arnaud', 'R. Steinmar']) }),
    steps: {
      a: { icon: '🏆', title: 'Un ancien champion', text: (c, v) => `${v.who}, triple champion retraité, propose ses services de consultant à l’écurie.`, ch: [
        { t: 'L’engager (0,3 M€ par GP)', fx: { deal: { name: 'Consultant', gp: -0.3, n: 6 }, rep: 3 }, out: 'Sa présence électrise le garage.', next: 'b', gap: 1 },
        { t: 'Un essai d’un week-end', fx: { budget: -0.2 }, out: 'On verra ce qu’il apporte.', next: 'b', gap: 1 },
        { t: 'Non merci', fx: {}, out: 'Il ira conseiller une autre écurie.' },
      ] },
      b: { icon: '🗣️', title: 'Des conseils qui piquent', text: (c, v) => `${v.who} trouve ta trajectoire « trop scolaire » et le dit devant les mécaniciens.`, ch: [
        { t: 'L’écouter attentivement', fx: { perf: { brake: 0.005 }, moral: -1 }, out: 'Vexant, mais tu freines plus tard.', next: 'c', gap: 2 },
        { t: 'Lui demander de parler en privé', fx: { moral: 1 }, out: 'Il comprend et adoucit le ton.', next: 'c', gap: 2 },
        { t: 'Le recadrer', fx: { rep: -1 }, out: 'L’ambiance se refroidit.', next: 'c', gap: 2 },
      ] },
      c: { icon: '🏎️', title: 'L’envie de rouler', text: (c, v) => `${v.who} rêve de refaire quelques tours dans une monoplace moderne lors d’une démonstration.`, ch: [
        { t: 'Organiser la démonstration (0,4 M€)', fx: { budget: -0.4, rep: 4 }, out: 'Le public est debout dans les tribunes.', next: 'd', gap: 2 },
        { t: 'Lui prêter le simulateur', fx: { moral: 1 }, out: 'Il bat le record de ton équipier au simulateur !', next: 'd', gap: 2 },
        { t: 'Trop dangereux', fx: {}, out: 'Il est déçu.', next: 'd', gap: 2 },
      ] },
      d: { icon: '🎓', title: 'Le mentor', text: (c, v) => `${v.who} veut prendre ${c.mate || 'ton équipier'} sous son aile pour le reste de la saison.`, ch: [
        { t: 'Bonne idée', fx: { mate: { spd: 1, strat: 1 } }, out: 'L’équipier progresse à vue d’œil.', next: 'e', gap: 3 },
        { t: 'Je préfère qu’il travaille avec moi', fx: { perf: { steer: 0.005 } }, out: 'Tu en profites toi-même.', next: 'e', gap: 3 },
        { t: 'Qu’il travaille avec les ingénieurs', fx: { dev: -2 }, out: 'Ses retours accélèrent l’usine.', next: 'e', gap: 3 },
      ] },
      e: { icon: '📜', title: 'Un rôle officiel ?', text: (c, v) => `${v.who} propose de devenir ambassadeur de l’écurie pour plusieurs saisons.`, ch: [
        { t: 'Contrat d’ambassadeur', fx: { rep: 5, budget: -0.5 }, out: 'Son nom est désormais associé à l’écurie.' },
        { t: 'Rester amis', fx: { rep: 2, moral: 1 }, out: 'Il passera de temps en temps.' },
        { t: 'Le remercier pour tout', fx: { moral: 1 }, out: 'Une belle page se tourne.' },
      ] },
    } },
  // 8. La nouvelle usine
  { id: 'factory', title: 'La nouvelle usine', icon: '🏗️',
    init: () => ({ town: pick(['Bellerive', 'Montclar', 'Varennes-sur-Lys', 'Saint-Odran']) }),
    steps: {
      a: { icon: '🏛️', title: 'Un terrain offert', text: (c, v) => `La ville de ${v.town} offre un terrain pour agrandir l’usine, à condition d’y créer des emplois.`, ch: [
        { t: 'Accepter le projet', fx: { rep: 2 }, out: 'Les architectes sont au travail.', next: 'b', gap: 2 },
        { t: 'Négocier des aides en plus', fx: { budget: 0.5 }, out: 'La ville accepte de participer.', next: 'b', gap: 2 },
        { t: 'Refuser, on reste chez nous', fx: { moral: 1 }, out: 'Pas de déménagement.' },
      ] },
      b: { icon: '🚧', title: 'Chantier retardé', text: () => 'Les fondations ont révélé une ancienne source souterraine : le chantier prend du retard.', ch: [
        { t: 'Payer des travaux en urgence (1 M€)', fx: { budget: -1 }, out: 'Le chantier reprend.', next: 'c', gap: 2 },
        { t: 'Attendre', fx: { moral: -1 }, out: 'Le projet patine.', next: 'c', gap: 3 },
        { t: 'Transformer la source en atout : refroidissement naturel', fx: { rep: 2, budget: -0.3 }, out: 'Une usine écologique : les médias adorent.', next: 'c', gap: 2 },
      ] },
      c: { icon: '👷', title: 'Le recrutement local', text: (c, v) => `La mairie de ${v.town} rappelle la promesse : 50 emplois locaux.`, ch: [
        { t: 'Embaucher des jeunes du coin et les former', fx: { dev: 2, rep: 3 }, out: 'Ils apprennent vite, mais il faut du temps.', next: 'd', gap: 2 },
        { t: 'Embaucher des techniciens expérimentés (0,8 M€)', fx: { budget: -0.8, dev: -1 }, out: 'Opérationnels tout de suite.', next: 'd', gap: 2 },
        { t: 'Gagner du temps', fx: { rep: -2 }, out: 'Les élus s’impatientent.', next: 'd', gap: 2 },
      ] },
      d: { icon: '🎀', title: 'L’inauguration', text: (c, v) => `L’usine de ${v.town} est prête. Comment l’inaugurer ?`, ch: [
        { t: 'Grande fête avec les habitants (0,3 M€)', fx: { budget: -0.3, rep: 3, moral: 3 }, out: 'Toute la ville est là.', next: 'e', gap: 3 },
        { t: 'Inauguration sobre', fx: { moral: 1 }, out: 'On coupe le ruban et on travaille.', next: 'e', gap: 3 },
        { t: 'Inviter les sponsors (+0,5 M€)', fx: { budget: 0.5 }, out: 'Les partenaires sont impressionnés.', next: 'e', gap: 3 },
      ] },
      e: { icon: '⚙️', title: 'L’usine tourne', text: () => 'Quelques semaines plus tard, la nouvelle usine tourne à plein régime. À quoi la consacrer ?', ch: [
        { t: 'Aux pièces de la voiture', fx: { dev: -3 }, out: 'Les livraisons s’accélèrent.' },
        { t: 'À la voiture de l’an prochain', fx: { conf: 3 }, out: 'La direction apprécie la vision à long terme.' },
        { t: 'À la sous-traitance pour d’autres écuries (+1,5 M€)', fx: { budget: 1.5 }, out: 'L’usine rapporte de l’argent.' },
      ] },
    } },
  // 9. L'affaire du carburant
  { id: 'fuelcase', title: 'L’affaire du carburant', icon: '⛽',
    steps: {
      a: { icon: '🧾', title: 'Contrôle surprise', text: () => 'Les commissaires ont prélevé ton carburant après la course : un additif inhabituel a été détecté.', ch: [
        { t: 'Coopérer totalement', fx: { conf: 1 }, out: 'Les échantillons partent au laboratoire.', next: 'b', gap: 1 },
        { t: 'Engager un avocat (0,4 M€)', fx: { budget: -0.4 }, out: 'Ton avocat prépare la défense.', next: 'b', gap: 1 },
        { t: 'Accuser le pétrolier', fx: { rep: -1 }, out: 'Le pétrolier n’apprécie pas.', next: 'c', gap: 1 },
      ] },
      b: { icon: '🔬', title: 'Résultats du laboratoire', text: () => 'Le laboratoire confirme : l’additif est dans une zone grise du règlement.', ch: [
        { t: 'Le retirer de nous-mêmes', fx: { perf: { vtop: -0.003 }, rep: 2 }, out: 'Un peu de puissance en moins, beaucoup d’honnêteté en plus.', next: 'd', gap: 2 },
        { t: 'Le garder : c’est légal', fx: { perf: { vtop: 0.003 } }, out: 'L’audience aura lieu dans deux courses.', next: 'd', gap: 2 },
        { t: 'Demander l’avis officiel de la fédération', fx: {}, out: 'Réponse attendue sous peu.', next: 'd', gap: 2 },
      ] },
      c: { icon: '🔥', title: 'Le pétrolier contre-attaque', text: () => 'Le pétrolier menace de rompre le contrat si tu ne retires pas tes accusations.', ch: [
        { t: 'Présenter des excuses', fx: { rep: -1, conf: 1 }, out: 'Les relations se réchauffent.', next: 'd', gap: 1 },
        { t: 'Maintenir tes propos', fx: { budget: -0.5, rep: 1 }, out: 'Le pétrolier réduit son soutien.', next: 'd', gap: 1 },
        { t: 'Chercher un nouveau pétrolier', fx: { dev: 1 }, out: 'Les ingénieurs doivent tout recalibrer.', next: 'd', gap: 2 },
      ] },
      d: { icon: '🏛️', title: 'L’audience', text: () => 'L’audience devant le tribunal de la fédération commence. Quelle ligne de défense ?', ch: [
        { t: 'Plaider la bonne foi', fx: { rep: 1 }, out: 'Le tribunal écoute.', next: 'e', gap: 1 },
        { t: 'Démontrer la faille du règlement', fx: {}, out: 'Les juges sont piqués au vif.', risk: { p: 0.4, fx: { grid: 3 }, out: 'Le tribunal n’apprécie pas : 3 places de pénalité sur la grille.' }, next: 'e', gap: 1 },
        { t: 'Proposer un accord amiable (0,5 M€)', fx: { budget: -0.5 }, out: 'Affaire réglée hors tribunal.' },
      ] },
      e: { icon: '🔨', title: 'Le verdict', text: () => 'Verdict : l’écurie est blanchie, mais le règlement sera modifié. Les autres écuries demandent des comptes.', ch: [
        { t: 'Partager les données avec tout le monde', fx: { rep: 4 }, out: 'Le paddock salue ta transparence.' },
        { t: 'Rester discret', fx: { conf: 2 }, out: 'L’affaire s’oublie peu à peu.' },
        { t: 'Réclamer le remboursement des frais (+0,6 M€)', fx: { budget: 0.6, rep: -1 }, out: 'La fédération paie, sans plaisir.' },
      ] },
    } },
  // 10. La voiture du fondateur
  { id: 'founder', title: 'La voiture du fondateur', icon: '🏚️',
    steps: {
      a: { icon: '🔦', title: 'Une découverte', text: () => 'Un fermier a retrouvé dans sa grange la toute première voiture de l’écurie, disparue depuis quarante ans.', ch: [
        { t: 'La racheter (0,3 M€)', fx: { budget: -0.3, rep: 2 }, out: 'La voiture rentre à la maison.', next: 'b', gap: 1 },
        { t: 'Négocier un prix d’ami', fx: { rep: -1 }, out: 'Le fermier finit par céder… sans le sourire.', next: 'b', gap: 2 },
        { t: 'La laisser à un musée', fx: { rep: 1 }, out: 'Le musée te remercie.' },
      ] },
      b: { icon: '🔧', title: 'La restauration', text: () => 'Les vieux mécaniciens de l’écurie se proposent de la restaurer sur leur temps libre.', ch: [
        { t: 'Les laisser faire', fx: { moral: 3, dev: 1 }, out: 'L’atelier sent l’huile et la nostalgie.', next: 'c', gap: 2 },
        { t: 'Confier ça à un spécialiste (0,5 M€)', fx: { budget: -0.5 }, out: 'Une restauration parfaite.', next: 'c', gap: 2 },
        { t: 'Mobiliser les apprentis', fx: { moral: 1, rep: 1 }, out: 'Les jeunes apprennent l’histoire de l’écurie.', next: 'c', gap: 2 },
      ] },
      c: { icon: '📜', title: 'Un secret dans la voiture', text: () => 'Sous le siège, les mécaniciens trouvent un carnet du fondateur rempli d’idées de réglages.', ch: [
        { t: 'Étudier le carnet', fx: { perf: { mu: 0.004 } }, out: 'Une vieille astuce de réglage fonctionne encore !', next: 'd', gap: 2 },
        { t: 'L’exposer au QG', fx: { moral: 2 }, out: 'Il inspire toute l’équipe.', next: 'd', gap: 2 },
        { t: 'Le publier en livre (+0,4 M€)', fx: { budget: 0.4, rep: 2 }, out: 'Le livre se vend bien.', next: 'd', gap: 2 },
      ] },
      d: { icon: '🏁', title: 'Le tour de démonstration', text: () => 'La voiture restaurée peut faire un tour de démonstration avant un Grand Prix.', ch: [
        { t: 'Tu la pilotes toi-même', fx: { rep: 3, moral: 2 }, out: 'Frissons garantis dans les tribunes.', next: 'e', gap: 2 },
        { t: 'Faire piloter le plus vieux mécanicien', fx: { moral: 4 }, out: 'Il pleure de joie en sortant du baquet.', next: 'e', gap: 2 },
        { t: 'Une exposition statique suffit', fx: { rep: 1 }, out: 'Les fans font la queue pour la voir.', next: 'e', gap: 2 },
      ] },
      e: { icon: '💎', title: 'Une offre de collectionneur', text: () => 'Un collectionneur propose 3 M€ pour la voiture du fondateur.', ch: [
        { t: 'Vendre (+3 M€)', fx: { budget: 3, rep: -3, moral: -2 }, out: 'L’argent est là, l’âme un peu moins.' },
        { t: 'Refuser : elle reste au QG', fx: { rep: 3, moral: 2 }, out: 'Elle trône à l’entrée de l’usine.' },
        { t: 'La prêter à un musée contre une rente', fx: { deal: { name: 'Musée', gp: 0.2, n: 8 }, rep: 1 }, out: 'Elle voyage, et rapporte un peu.' },
      ] },
    } },
];

// événements supplémentaires (s'ajoutent à ceux de events.js)
export const EXTRA = [
  { id: 'sticker', icon: '🏷️', lite: true, title: 'Autocollant oublié', text: () => 'Un mécanicien a oublié de coller le logo d’un petit sponsor sur la voiture.', ch: [
    { t: 'S’excuser et offrir une visite VIP', fx: { budget: -0.1, rep: 1 }, out: 'Le sponsor est ravi.' },
    { t: 'Doubler la taille du logo au prochain GP', fx: { rep: -1 }, out: 'Le logo est énorme… et un peu ridicule.' },
    { t: 'Faire comme si de rien n’était', fx: { budget: -0.3 }, out: 'Le sponsor réduit son chèque.' },
  ] },
  { id: 'lost', icon: '🧳', lite: true, title: 'Bagages perdus', text: () => 'Les combinaisons de course ont été envoyées dans le mauvais pays.', ch: [
    { t: 'En faire fabriquer en urgence (0,2 M€)', fx: { budget: -0.2 }, out: 'Prêtes juste à temps.' },
    { t: 'Emprunter celles de l’an dernier', fx: { rep: -1, moral: 1 }, out: 'Les fans repèrent l’ancien design.' },
    { t: 'Rouler avec les combinaisons d’essai', fx: { moral: -1 }, out: 'Pas très élégant.' },
  ] },
  { id: 'rivalboss', icon: '☕', title: 'Café avec un rival', text: c => `Le directeur de ${c.rivalTeam} t’invite à boire un café pour « discuter de l’avenir du sport ».`, ch: [
    { t: 'Accepter et l’écouter', fx: { rep: 1 }, out: 'Il propose une alliance pour faire baisser les coûts.' },
    { t: 'Lui soutirer des infos', fx: { perf: { vtop: 0.002 }, rep: -1 }, out: 'Quelques confidences utiles…' },
    { t: 'Décliner', fx: {}, out: 'Pas de fraternisation.' },
  ] },
  { id: 'drone', icon: '🛸', lite: true, title: 'Drone au-dessus du garage', text: () => 'Un drone non identifié survole ton garage pendant les essais.', ch: [
    { t: 'Bâcher la voiture', fx: { moral: -1 }, out: 'Plus rien à voir.' },
    { t: 'Montrer de fausses pièces', fx: { moral: 2 }, out: 'Les espions vont s’arracher les cheveux.' },
    { t: 'Prévenir la sécurité', fx: { conf: 1 }, out: 'Le drone disparaît.' },
  ] },
  { id: 'sleep', icon: '😴', lite: true, title: 'Décalage horaire', text: () => 'Le décalage horaire assomme toute l’équipe à l’arrivée.', ch: [
    { t: 'Repos imposé le premier jour', fx: { moral: 2, perf: { mu: -0.002 } }, out: 'Frais, mais moins de préparation.' },
    { t: 'Café et travail', fx: { moral: -2, perf: { mu: 0.002 } }, out: 'Les yeux piquent, la voiture est prête.' },
    { t: 'Faire venir un spécialiste du sommeil (0,1 M€)', fx: { budget: -0.1, moral: 1 }, out: 'Une astuce de luminothérapie qui marche.' },
  ] },
  { id: 'helmet', icon: '⛑️', lite: true, title: 'Casque spécial', text: () => 'Un artiste propose de peindre un casque unique pour le prochain GP.', ch: [
    { t: 'Accepter, et le vendre aux enchères ensuite', fx: { budget: 0.3, rep: 2 }, out: 'Une œuvre d’art à 300 km/h.' },
    { t: 'Le garder en souvenir', fx: { moral: 2 }, out: 'Il trônera dans ton salon.' },
    { t: 'Garder mon casque habituel', fx: {}, out: 'Superstitieux ?' },
  ] },
  { id: 'kids', icon: '🧒', lite: true, title: 'Classe en visite', text: () => 'Une école primaire demande à visiter l’usine.', ch: [
    { t: 'Grande visite guidée', fx: { rep: 2, dev: 1 }, out: 'Trente futurs ingénieurs repartent ravis.' },
    { t: 'Une vidéo pour la classe', fx: { rep: 1 }, out: 'Les enfants adorent.' },
    { t: 'Pas le temps', fx: { rep: -1 }, out: 'Dommage.' },
  ] },
  { id: 'cooling', icon: '❄️', lite: true, title: 'Surchauffe des freins', text: () => 'Les ingénieurs proposent d’ouvrir davantage les écopes de frein.', ch: [
    { t: 'Ouvrir les écopes', fx: { perf: { brake: 0.004, vtop: -0.002 } }, out: 'Freins frais, un peu de traînée.' },
    { t: 'Garder la configuration', fx: {}, out: 'On reste sur le réglage connu.' },
    { t: 'Fermer pour la vitesse', fx: { perf: { vtop: 0.003, brake: -0.003 } }, out: 'Attention aux longs freinages.' },
  ] },
  { id: 'translator', icon: '🌍', lite: true, title: 'Problème de traduction', text: () => 'Une interview traduite de travers te fait dire que la voiture « est une brouette ».', ch: [
    { t: 'Démentir avec humour', fx: { rep: 2 }, out: 'La « brouette » devient un mème sympathique.' },
    { t: 'Exiger un correctif officiel', fx: { rep: 1, moral: -1 }, out: 'Le journal publie un rectificatif.' },
    { t: 'Ignorer', fx: { moral: -1 }, out: 'Les ingénieurs sont vexés.' },
  ] },
  { id: 'pitcrew', icon: '🏃', title: 'Concours d’arrêt au stand', text: () => 'Un sponsor organise un concours d’arrêts au stand entre écuries, retransmis à la télé.', ch: [
    { t: 'Participer à fond', fx: { moral: 3, rep: 2 }, out: 'Deuxième place, à un centième de la victoire !', risk: { p: 0.3, fx: { moral: -2, rep: 1 }, out: 'Une roue mal serrée en direct… fous rires garantis.' } },
    { t: 'Envoyer l’équipe B', fx: { moral: 1 }, out: 'Elle s’amuse sans pression.' },
    { t: 'Décliner', fx: { rep: -1 }, out: 'Les mécaniciens auraient bien aimé.' },
  ] },
  { id: 'prototype', icon: '🔋', title: 'Batterie expérimentale', text: () => 'Le motoriste propose une batterie plus légère mais encore peu testée.', ch: [
    { t: 'La monter', fx: { perf: { acc: 0.005 } }, out: 'Une meilleure relance en sortie de virage.', risk: { p: 0.2, fx: { perf: { acc: -0.004 } }, out: 'Elle surchauffe : le moteur bride la puissance.' } },
    { t: 'Attendre la version finale', fx: {}, out: 'Prudence.' },
    { t: 'Participer au développement (0,3 M€)', fx: { budget: -0.3, dev: -1 }, out: 'Tes ingénieurs apprennent beaucoup.' },
  ] },
  { id: 'mascot', icon: '🐕', lite: true, title: 'Une mascotte', text: () => 'Un chien errant s’est installé dans le garage et refuse de partir.', ch: [
    { t: 'L’adopter comme mascotte', fx: { moral: 3, rep: 2 }, out: 'Il a son propre badge du paddock.' },
    { t: 'Lui trouver une famille', fx: { rep: 1 }, out: 'Un mécanicien l’adopte.' },
    { t: 'Appeler la fourrière', fx: { moral: -2, rep: -1 }, out: 'Personne n’est fier.' },
  ] },
  { id: 'payrise', icon: '💶', title: 'Revendication des ingénieurs', text: () => 'Les ingénieurs aérodynamiques demandent à être alignés sur les salaires des grandes écuries.', ch: [
    { t: 'Augmenter les salaires (1 M€)', fx: { budget: -1, moral: 3, dev: -1 }, out: 'Ils restent et travaillent avec entrain.' },
    { t: 'Primes aux résultats', fx: { moral: 1 }, out: 'Un compromis motivant.' },
    { t: 'Refuser', fx: { moral: -3 }, out: 'Deux ingénieurs envoient leur CV ailleurs.', risk: { p: 0.3, fx: { moral: -2, dev: 2 }, out: 'Un ingénieur clé part chez un rival.' } },
  ] },
  { id: 'track', icon: '🗺️', lite: true, title: 'Reconnaissance à vélo', text: c => `L’équipe propose de faire le tour du circuit de ${c.next} à vélo pour repérer les bosses.`, ch: [
    { t: 'Y aller avec les ingénieurs', fx: { perf: { brake: 0.003 }, moral: 1 }, out: 'Une bosse au freinage du virage 3 est notée.' },
    { t: 'Y aller en trottinette', fx: { moral: 2 }, out: 'Fous rires et peu de données.' },
    { t: 'Rester au motorhome', fx: {}, out: 'Les données suffiront.' },
  ] },
  { id: 'loan', icon: '🏦', title: 'Proposition de la banque', text: () => 'La banque de l’écurie propose un prêt à taux réduit.', ch: [
    { t: 'Emprunter 3 M€ (remboursés 0,5 M€ par GP)', fx: { budget: 3, deal: { name: 'Remboursement du prêt', gp: -0.55, n: 6 } }, out: 'De l’argent tout de suite, à rembourser ensuite.' },
    { t: 'Emprunter 1 M€', fx: { budget: 1, deal: { name: 'Remboursement du prêt', gp: -0.2, n: 6 } }, out: 'Un petit coup de pouce.' },
    { t: 'Refuser', fx: { conf: 1 }, out: 'Pas de dettes.' },
  ] },
  { id: 'fanzone', icon: '🎪', lite: true, title: 'Fan zone', text: () => 'L’organisateur propose un stand dans la fan zone.', ch: [
    { t: 'Grand stand avec simulateur (0,3 M€)', fx: { budget: -0.3, rep: 3 }, out: 'La file d’attente fait le tour du stand.' },
    { t: 'Un petit stand de produits dérivés', fx: { budget: 0.2 }, out: 'Les ventes sont correctes.' },
    { t: 'Pas de stand', fx: {}, out: 'Les fans iront voir les autres.' },
  ] },
  { id: 'aero', icon: '✈️', title: 'Ingénieur venu de l’aviation', text: () => 'Un ingénieur venu de l’aéronautique propose une nouvelle méthode de calcul.', ch: [
    { t: 'L’embaucher (0,5 M€)', fx: { budget: -0.5, dev: -2 }, out: 'Ses calculs accélèrent la soufflerie.' },
    { t: 'Une mission de conseil (0,2 M€)', fx: { budget: -0.2, dev: -1 }, out: 'Un coup de pouce ponctuel.' },
    { t: 'Non', fx: {}, out: 'Les avions, c’est autre chose.' },
  ] },
  { id: 'weather', icon: '🌦️', lite: true, title: 'Météorologue', text: () => 'Un météorologue indépendant propose ses prévisions ultra-précises.', ch: [
    { t: 'L’engager pour la saison (0,2 M€)', fx: { budget: -0.2, perf: { mu: 0.002 } }, out: 'Le muret anticipe mieux les averses.' },
    { t: 'Juste pour le prochain GP (0,05 M€)', fx: { budget: -0.1 }, out: 'Prévisions en poche.' },
    { t: 'Nos applis suffisent', fx: {}, out: 'Espérons qu’il ne pleuve pas.' },
  ] },
  { id: 'chairs', icon: '🪑', lite: true, title: 'Nouveau baquet', text: () => 'Un fabricant propose un baquet moulé sur mesure.', ch: [
    { t: 'L’essayer (0,1 M€)', fx: { budget: -0.1, perf: { steer: 0.004 } }, out: 'Tu sens mieux la voiture.' },
    { t: 'Garder l’ancien', fx: {}, out: 'On ne change rien.' },
    { t: 'Pour l’équipier seulement', fx: { mate: { tyre: 1 } }, out: 'Il ménage mieux ses pneus.' },
  ] },
  { id: 'rumor', icon: '🗞️', lite: true, title: 'Rumeur de départ', text: c => `Une rumeur annonce le départ de ${c.mate || 'ton équipier'} à la fin de la saison.`, ch: [
    { t: 'Démentir', fx: { moral: 1 }, out: 'La rumeur s’éteint.' },
    { t: 'Laisser planer le doute', fx: { mate: { spd: 1 }, moral: -1 }, out: 'Il pousse pour se montrer.' },
    { t: 'Ne rien dire', fx: {}, out: 'Les journalistes en parlent encore.' },
  ] },
  { id: 'pitwall', icon: '🖥️', lite: true, title: 'Écrans du muret', text: () => 'Les écrans du muret tombent en panne à répétition.', ch: [
    { t: 'Tout remplacer (0,3 M€)', fx: { budget: -0.3, conf: 1 }, out: 'Le muret est flambant neuf.' },
    { t: 'Réparer avec les moyens du bord', fx: { moral: -1 }, out: 'Ça tient avec du scotch.' },
    { t: 'Demander au sponsor technologique', fx: { rep: -1, budget: 0.1 }, out: 'Il fournit… en échange de pub.' },
  ] },
  { id: 'veteran', icon: '👴', title: 'Le doyen des mécaniciens', text: () => 'Le plus ancien mécanicien de l’écurie annonce sa retraite en fin de saison.', ch: [
    { t: 'Lui organiser une soirée d’adieu', fx: { moral: 3, budget: -0.1 }, out: 'Émotion garantie.' },
    { t: 'Lui proposer de former les jeunes', fx: { dev: -1, moral: 1 }, out: 'Il transmet son savoir.' },
    { t: 'Le remercier simplement', fx: { moral: -1 }, out: 'Il aurait aimé un peu plus.' },
  ] },
  { id: 'trackday', icon: '🎟️', title: 'Journée clients', text: () => 'De riches clients paieraient cher pour rouler dans une ancienne voiture de l’écurie.', ch: [
    { t: 'Organiser la journée (+0,8 M€)', fx: { budget: 0.8, dev: 1 }, out: 'Les clients repartent comblés, les mécaniciens épuisés.' },
    { t: 'Une version courte (+0,3 M€)', fx: { budget: 0.3 }, out: 'Une matinée bien rentable.' },
    { t: 'Refuser', fx: {}, out: 'Les vieilles voitures restent au musée.' },
  ] },
  { id: 'clash', icon: '🤬', lite: true, title: 'Altercation en piste', text: c => `${c.rival || 'Un rival'} t’accuse de l’avoir bloqué en essais.`, ch: [
    { t: 'Aller s’excuser', fx: { rep: 2 }, out: 'Il accepte tes excuses.' },
    { t: 'Répondre sèchement', fx: { rep: -1, moral: 1 }, out: 'La tension monte.' },
    { t: 'Laisser les commissaires juger', fx: {}, out: 'Pas de sanction.' },
  ] },
  { id: 'gloves', icon: '🧤', lite: true, title: 'Gants connectés', text: () => 'Une start-up propose des gants qui mesurent ton rythme cardiaque en course.', ch: [
    { t: 'Les tester (+0,2 M€)', fx: { budget: 0.2, rep: 1 }, out: 'Les téléspectateurs voient ton cœur battre en direct.' },
    { t: 'Seulement en essais', fx: { perf: { steer: 0.002 } }, out: 'Des données sur ta concentration.' },
    { t: 'Non merci', fx: {}, out: 'On garde les gants habituels.' },
  ] },
  { id: 'strikeair', icon: '🛫', title: 'Grève des contrôleurs aériens', text: () => 'Une grève bloque les avions : comment rejoindre le prochain Grand Prix ?', ch: [
    { t: 'Avion privé (0,8 M€)', fx: { budget: -0.8 }, out: 'Arrivée à l’heure.' },
    { t: 'Convoi de camions toute la nuit', fx: { moral: -2, perf: { mu: -0.002 } }, out: 'Arrivée épuisée, mais à temps.' },
    { t: 'Train de nuit avec toute l’équipe', fx: { moral: 2, budget: -0.2 }, out: 'Un voyage mémorable et soudé.' },
  ] },
];
