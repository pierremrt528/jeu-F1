# Grand Prix Manager — vision du jeu

Jeu mobile (PWA) de **course de monoplaces en 3D** doublé d'une **gestion d'écurie** : on pilote en vue cockpit (1re personne) ou arrière (3e personne) sur une saison de 10 circuits, et entre les courses on développe la voiture au garage et on répond à la presse.
Jouable dans le navigateur du téléphone, installable sur l'écran d'accueil. Langue du jeu : français.

## Règles du projet
- **Jeu original, univers 100 % fictif** : écuries, pilotes, circuits, logos et livrées inventés. Aucun nom, logo, tracé ou marque réels (F1, FIA, écuries, pilotes, circuits existants), et pas de reproduction d'un jeu existant.
- **Mobile d'abord** : course en paysage (tester à 844×390), menus utilisables aussi en portrait (390×844). Commandes tactiles multi-touch, inclinaison du téléphone en option, clavier sur ordinateur.
- **Performance** : viser 60 i/s sur un téléphone milieu de gamme. Low-poly, une géométrie par voiture, décor instancié, pas d'ombres temps réel, brouillard pour limiter la distance d'affichage, résolution adaptative.
- **Simple** : JS vanilla en modules ES + Three.js (copie locale dans `lib/`, pas de CDN, pas d'étape de build, pas d'autre dépendance).
- Code modulaire et léger ; petits changements ciblés ; ne pas réécrire ce qui marche.
- Après chaque modif des fichiers du jeu : incrémenter `VERSION` dans `sw.js` et ajouter les nouveaux fichiers à sa liste `FILES`.
- Déploiement : GitHub Pages depuis la branche `main`, racine du dépôt. Déployer à la fin de chaque étape.

## Structure
- `index.html` : écrans (menu, HUD, commandes tactiles, pause, résultats) + canvas WebGL.
- `js/main.js` : rendu Three.js, caméras (arrière / cockpit), boucle, écrans, réglages (localStorage `gpm-settings`).
- `js/data.js` : écuries (10 × 2 pilotes), thèmes de décor (`THEMES` : ciel, sol, arbres, montagnes, ville, mer) et 10 circuits. Un circuit = points de contrôle `[x, z, altitude]` ou `layout` (S longueur / L-R rayon angle, fermé automatiquement) + `elev` (profil d'altitude), largeur, `runoff` (distance au mur).
- `js/track.js` : spline Catmull-Rom ré-échantillonnée tous les 2 m, courbure, trajectoire, profil de vitesse IA. `pitLane(track)` : géométrie de la voie des stands (entrée, boxes, sortie, côté `lat` positif), utilisée par la physique et le décor.
- `js/scenery.js` : maillages du circuit et environnement (asphalte, vibreurs, herbe/gravier, murs publicitaires, grillages, tribunes, stands, feux, arbres, montagnes, ciel, soleil, carte d'environnement pour les reflets).
- `js/textures.js` : textures générées en canvas au lancement (aucune image à télécharger). Marques publicitaires inventées.
- `js/carmodel.js` : modèle 3D de monoplace (design original, 3 appels de rendu par voiture).
- `js/car.js` : physique arcade (adhérence + appui aéro, freinage, sous-virage, pente, murs).
- `js/ai.js` : pilotes IA (poursuite pure, profil de vitesse × niveau, dépassements).
- `js/race.js` : grille, feux, aspiration, collisions, tours, classement, résultats.
- `js/garage.js` : arbres de développement (`TREES`, 4 pôles × 6 pièces). Une pièce se paie en M€ (`price`) et est livrée après 1 à 3 GP (`delay`, `tickDevelopment` après chaque course) ; une seule pièce en cours par pôle (`garage.dev`). Multiplicateurs de la voiture du joueur (`carUpgrades` → `car.up`, inclut la fiabilité du motoriste). Sauvegarde `gpm-garage`.
- `js/team.js` : état de l'écurie (moral, budget M€, réputation, bonus de la prochaine course), primes de course, modificateurs de course. Sauvegarde `gpm-team`.
- `js/press.js` : conférence de presse après la course (banque de questions selon le contexte, réponses avec effets), écran de presse.
- `js/career.js` : montage de l'écurie en début de saison : niveau (`TIERS` : nouvelle écurie / milieu de peloton / top team → budget, perf, réputation), 2e pilote (`MARKET` : Formule B et vétérans, stats vitesse/stratégie/pneus/potentiel, salaire), motoriste (`ENGINES` : 4 marques inventées, performance/fiabilité, prix). Appliqué à `TEAMS[0]`. Sauvegarde `gpm-career`.
- `js/strategy.js` : formats de course (`FORMATS` Sprint ~11 min / Long ~22 min, `lapsFor` calcule le nombre de tours), pneus (`TYRES` S/M/H/W, durée en km, usure accrue par les dérapages `c.slip` et le hors-piste), carburant (`FPK` kg/km, `TANK`, la masse alourdit la voiture, panne sèche = abandon), modes de pilotage (`MODES` attaque/normal/économie), 3 stratégies proposées avant le départ (`makePlans` : Prudente / Équilibrée / Agressive, la plus rapide marquée), météo (`forecast`, `R.wet`), arrêts (`wantBox`, `nextStop`, `pitStep` dans la voie des stands, limiteur 80 km/h, arrêt devant le box de l'écurie), radio du muret (`radioAsk` infos pneus/carburant/météo/rivaux/stratégie, `radioOrder` consignes), dégâts d'aileron et casse selon la fiabilité. Messages radio (`R.radioMsg`).
- `js/safety.js` : voiture de sécurité (après un abandon ou un gros choc, file indienne, pénalité de 5 s si le joueur dépasse).
- `js/audio.js` : sons générés en Web Audio (moteur, voiture la plus proche, crissements, vent, pluie, chocs, feux, radio). Réglage Son au menu.
- `js/fx.js` : particules partagées (gerbes d'eau sous la pluie, fumée des pneus).
- `js/season.js` : saison (calendrier `CALENDAR`, qualifications estimées pour les IA, points 25-18-15… + meilleur tour, classements pilotes/écuries, progression des rivaux `dev`). Sauvegarde `gpm-season`. Les écrans saison/qualifs/classements sont gérés dans `main.js`.
- `js/input.js`, `js/hud.js` : commandes et HUD (position, tour, chronos, vitesse, rapport, mini-carte).
- `lib/three.module.min.js` (r170, licence MIT), `style.css`, `manifest.json`, `sw.js`, icônes.
- Tester en local : `python3 -m http.server` puis `localhost:8000`. Paramètres de test : `?circuit=sierra`, `?demo` (pilote auto), `?vitesse=5`, `?tours=1`, `?fps` (i/s, appels de rendu, triangles), `?grille=1`, `?meteo=pluie|averse|eclaircie`, `?sc` (voiture de sécurité forcée). Clavier : B = stand, R = radio.

## Feuille de route
1. ✅ **Moteur 3D de course** : 1 circuit (Valmont), physique arcade (adhérence, freinage, aspiration), 19 IA, caméras cockpit / arrière commutables, HUD, qualité adaptative. Passe graphique réaliste (textures, reflets, ciel, tribunes, décor).
2. ✅ **10 circuits générés à partir de données** (tracé + relief + décor) : urbains (immeubles), port, campagne, montagne avec fort dénivelé, forêt très sinueuse, désert rapide, plaine plate, côte avec mer, anneau rapide. Choix du circuit au menu avec aperçu.
3. ✅ **Garage** : 4 arbres de 6 pièces (moteur, aéro, châssis, fiabilité) avec prérequis ; points de R&D gagnés en course (place, places gagnées, meilleur tour) ; effets sur vitesse de pointe, accélération, appui, adhérence, freinage, agilité, aspiration, chocs, hors-piste. La note de fiabilité servira à l'étape 6 (casse).
4. ✅ **Presse** : 3 questions de journalistes fictifs après chaque course (selon la place, les places gagnées, le rival, le coéquipier, le circuit), réponses avec bonus/malus : moral (±1 % de performance), budget (achat de points R&D au garage), réputation (multiplie les points R&D), bonus/malus pour la prochaine course.
5. ✅ **Saison / carrière** : calendrier des 10 circuits, qualifications, points, classements pilotes et écuries, sauvegarde locale (localStorage). Début de saison : choix du niveau d'écurie (budget), du 2e pilote et du motoriste ; développement payé en M€ avec délai de livraison.
6. ✅ **Météo, pneus, arrêts au stand, dégâts** : 3 gommes sèches + pluie avec usure (gestion des pneus du pilote), pluie et averses (ciel gris, gouttes, piste mouillée), choix des pneus avant la course, bouton STAND et voie des stands, stratégie des IA, messages radio du muret, dégâts d'aileron réparés au stand, casse moteur selon la fiabilité (abandon ou perte de puissance).
7. ✅ **Finitions** : sons générés, voiture de sécurité, gerbes d'eau et fumée, secousses de caméra, bouton « Recommencer la carrière ».
8. ✅ **Stratégie** : formats Sprint / Long, 3 stratégies au choix avant le départ (pneus, carburant), radio du muret (infos et consignes), carburant (poids, ravitaillement), usure des pneus selon les dérapages, voie des stands intégrée aux circuits.
