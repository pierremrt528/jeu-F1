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
- `js/track.js` : spline Catmull-Rom ré-échantillonnée tous les 2 m, courbure, trajectoire, profil de vitesse IA.
- `js/scenery.js` : maillages du circuit et environnement (asphalte, vibreurs, herbe/gravier, murs publicitaires, grillages, tribunes, stands, feux, arbres, montagnes, ciel, soleil, carte d'environnement pour les reflets).
- `js/textures.js` : textures générées en canvas au lancement (aucune image à télécharger). Marques publicitaires inventées.
- `js/carmodel.js` : modèle 3D de monoplace (design original, 3 appels de rendu par voiture).
- `js/car.js` : physique arcade (adhérence + appui aéro, freinage, sous-virage, pente, murs).
- `js/ai.js` : pilotes IA (poursuite pure, profil de vitesse × niveau, dépassements).
- `js/race.js` : grille, feux, aspiration, collisions, tours, classement, résultats.
- `js/garage.js` : arbres de développement (`TREES`), points de R&D gagnés en course (`awardPoints`), multiplicateurs appliqués à la voiture du joueur (`carUpgrades` → `car.up`), écran du garage. Sauvegarde localStorage `gpm-garage`.
- `js/team.js` : état de l'écurie (moral, budget M€, réputation, bonus de la prochaine course), primes de course, modificateurs de course. Sauvegarde `gpm-team`.
- `js/press.js` : conférence de presse après la course (banque de questions selon le contexte, réponses avec effets), écran de presse.
- `js/input.js`, `js/hud.js` : commandes et HUD (position, tour, chronos, vitesse, rapport, mini-carte).
- `lib/three.module.min.js` (r170, licence MIT), `style.css`, `manifest.json`, `sw.js`, icônes.
- Tester en local : `python3 -m http.server` puis `localhost:8000`. Paramètres de test : `?circuit=sierra`, `?demo` (pilote auto), `?vitesse=5`, `?tours=1`, `?fps` (i/s, appels de rendu, triangles), `?grille=1`, `?rd=200` (points de R&D pour tester le garage).

## Feuille de route
1. ✅ **Moteur 3D de course** : 1 circuit (Valmont), physique arcade (adhérence, freinage, aspiration), 19 IA, caméras cockpit / arrière commutables, HUD, qualité adaptative. Passe graphique réaliste (textures, reflets, ciel, tribunes, décor).
2. ✅ **10 circuits générés à partir de données** (tracé + relief + décor) : urbains (immeubles), port, campagne, montagne avec fort dénivelé, forêt très sinueuse, désert rapide, plaine plate, côte avec mer, anneau rapide. Choix du circuit au menu avec aperçu.
3. ✅ **Garage** : 4 arbres de 6 pièces (moteur, aéro, châssis, fiabilité) avec prérequis ; points de R&D gagnés en course (place, places gagnées, meilleur tour) ; effets sur vitesse de pointe, accélération, appui, adhérence, freinage, agilité, aspiration, chocs, hors-piste. La note de fiabilité servira à l'étape 6 (casse).
4. ✅ **Presse** : 3 questions de journalistes fictifs après chaque course (selon la place, les places gagnées, le rival, le coéquipier, le circuit), réponses avec bonus/malus : moral (±1 % de performance), budget (achat de points R&D au garage), réputation (multiplie les points R&D), bonus/malus pour la prochaine course.
5. **Saison / carrière** : calendrier des 10 circuits, qualifications, points, classements pilotes et écuries, sauvegarde locale (localStorage).
6. **Météo, pneus, arrêts au stand, dégâts** : gommes et usure, pluie, voie des stands, ordres depuis le muret, casse.
7. Finitions : sons, voiture de sécurité, meilleurs graphismes.
