# Grand Prix Manager sous Unreal Engine 5 — plan de migration

But : refaire tout le jeu (course 3D + gestion d'écurie) en Unreal Engine 5 pour viser une qualité « jeu mobile du commerce » (iOS et Android), en gardant l'univers 100 % fictif et toutes les règles de `CLAUDE.md` (écuries, pilotes, circuits, marques inventés ; français ; paysage en course).

La version web (Three.js, `js/`) reste en ligne et sert de **référence** : chaque système Unreal reprend les règles, chiffres et équilibrages du fichier JS cité. Le projet Unreal vit dans `unreal/GrandPrixManager/` du même dépôt.

## Comment utiliser ce plan
Chaque étape ci-dessous est un **prompt autonome** : copie le bloc « Prompt » dans un nouveau fil avec Claude. Claude écrit le code C++ / les fichiers de config ici, puis tu ouvres le projet sur ton Mac pour compiler et tester (ou Claude le fait via une session Remote Control sur le Mac).
Règles communes à toutes les étapes :
- C++ pour la logique (dossier `Source/GrandPrixManager/`), Blueprints seulement comme coquilles fines (widgets, matériaux, niveaux).
- Données en tableaux (`DataTable` / JSON dans `Content/Data/`), jamais en dur dans les Blueprints.
- Mobile d'abord : renderer mobile (forward), 60 i/s sur un téléphone milieu de gamme, pas d'ombres dynamiques sauf le soleil en cascade unique, LOD partout, instanciation (HISM) du décor.
- Une étape = une branche + un commit poussé ; ne pas casser ce qui marche.

## Ce que tu dois installer sur ton Mac (une seule fois)
1. **Epic Games Launcher** (epicgames.com) → onglet Unreal Engine → installer **UE 5.5 ou plus récent** (cocher « iOS » et « Android » dans les options de la version, ~60 Go).
2. **Xcode** (App Store) puis `xcode-select --install` : obligatoire pour compiler le C++ sur Mac et pour iOS.
3. **Git LFS** : `brew install git-lfs && git lfs install` (les fichiers `.uasset` sont lourds).
4. Pour Android : **Android Studio** + SDK/NDK aux versions demandées par Unreal (menu Unreal *Platforms → Android → Install SDK* / Turnkey le guide).
5. Pour tester sur iPhone : un compte Apple Developer (gratuit pour ton propre téléphone, 99 $/an pour publier).

---

## Étape 1 — Projet et configuration mobile ✅ (préparée)
**Prompt :** « Crée le projet Unreal C++ `GrandPrixManager` dans `unreal/GrandPrixManager/` : `.uproject` (plugins Chaos Vehicles, Enhanced Input, CommonUI), module C++ et cibles Game/Editor, `.gitignore` et `.gitattributes` (LFS pour uasset/umap/wav/png/fbx), config mobile : renderer mobile forward, MSAA 4×, pas de Lumen/Nanite/ombres virtuelles, Vulkan Android + Metal iOS, orientation paysage, profils de scalabilité Bas/Moyen/Haut, résolution adaptative (`r.MobileContentScaleFactor`, `r.ScreenPercentage`). Ajoute un `GameMode` C++ vide et une carte de démarrage à créer à la main. Écris un README qui dit comment ouvrir et compiler sur Mac. »
**Test :** double-clic sur `GrandPrixManager.uproject` → « Oui » pour compiler → l'éditeur s'ouvre.

## Étape 2 — Données du jeu ✅ (préparée)
**Prompt :** « Écris un script Node `unreal/tools/export-data.mjs` qui importe les modules de `js/` (data.js, config/*.js, garage TREES, infra INFRA, staff CHIEFS, strategy TYRES/FORMATS/MODES, season CALENDAR/GAPS, events EVENTS, stories STORIES/EXTRA, press) et les écrit en JSON dans `unreal/GrandPrixManager/Content/Data/`. Côté C++, crée les structs `FTeamRow`, `FCircuitRow`, `FThemeRow`, `FPartRow`, `FInfraRow`, `FChiefRow`, `FTyreRow`, `FEventRow`… (`USTRUCT(BlueprintType)` héritant de `FTableRowBase`) et un `UGPMDataSubsystem` (GameInstanceSubsystem) qui charge ces JSON au démarrage. »

## Étape 3 — Circuits procéduraux
**Prompt :** « Porte `js/track.js` et `js/scenery.js` : un acteur `AGPMTrack` lit un `FCircuitRow` (points de contrôle ou `layout` S/L/R + `elev`), construit une spline Catmull-Rom ré-échantillonnée tous les 2 m (mêmes formules que track.js : courbure, trajectoire idéale, profil de vitesse IA), puis génère avec `UDynamicMeshComponent`/`ProceduralMeshComponent` : asphalte, vibreurs (zones de courbure > 1/220), herbe/gravier, murs variables (`WL`/`WR`), ponts aux croisements, voie des stands (`pitLane`). Décor (arbres, tribunes, panneaux, immeubles, montagnes selon `THEMES`) en HISM avec LOD. Collision simple (murs = boîtes, sol = mesh). Commande console `GPM.Circuit <id>` pour charger chacun des 22 circuits. »

## Étape 4 — Monoplace et physique (Chaos Vehicle)
**Prompt :** « Crée `AGPMCar` (hérite de `AWheeledVehiclePawn`) avec un `UChaosWheeledVehicleMovementComponent` réglé comme une monoplace fictive : ~800 kg, 4 roues, moteur ~750 ch avec courbe de couple, boîte 8 rapports, appui aéro proportionnel à v² (force vers le bas appliquée en C++ dans `Tick`, réduite par l'air sale et les ailerons abîmés comme dans `js/car.js`), freins puissants, différentiel arrière. Reprends les aides d'`js/car.js` (ABS, antipatinage, stabilité, aide à la trajectoire). Modèle 3D : exporte `js/carmodel.js` en glTF depuis la version web (script Playwright) puis importe-le, ou crée un modèle original low-poly (< 15 k triangles, 1 matériau à masque de livrée). Caméras arrière et cockpit commutables, secousses. Le joueur doit pouvoir faire un tour de Valmont. »

## Étape 5 — Commandes et HUD
**Prompt :** « Enhanced Input : clavier/manette sur ordinateur ; sur mobile, widget UMG multi-touch (gauche/droite, accélérateur, frein, DRS/stand), inclinaison du téléphone en option (gyroscope via `UInputComponent` motion). HUD UMG (position, tour, chronos, vitesse, rapport, mini-carte dessinée depuis la spline) comme `js/hud.js`, adapté à 844×390 avec marges de sécurité (encoche). »

## Étape 6 — IA des pilotes
**Prompt :** « Porte `js/ai.js` : `UGPMDriverAI` (composant) qui pilote un `AGPMCar` par poursuite pure sur la trajectoire de la spline, profil de vitesse × niveau du pilote × difficulté (`DIFFS`), dépassements, défense, aspiration, `yieldTo` (statut N°2). 19 IA + le joueur, ABS et antipatinage pour les IA. Optimisation : au-delà de 150 m de la caméra, IA en mode « cinématique » (déplacement sur la spline sans physique complète). »

## Étape 7 — Week-end de course
**Prompt :** « Porte `js/race.js`, `js/qsession.js` et `js/safety.js` : grille, feux de départ, tours et secteurs, classement en direct, collisions et pénalités, qualifications jouées (tour lancé, tour des temps des 20 pilotes, secteurs violet/vert/jaune), voiture de sécurité, résultats. Course simulée sans rendu (`simulate()`) pour « Simuler la course ». Paramètres de test en console : `GPM.Tours`, `GPM.Meteo`, `GPM.SC`. »

## Étape 8 — Pneus, carburant, stands, météo, dégâts
**Prompt :** « Porte `js/strategy.js`, `js/prerace.js`, `js/debris.js` et `js/tuning.js` : gommes S/M/H/W (`lifeGrip`, crevaison), carburant (masse, panne sèche), modes de pilotage, stratégies `makePlans`/`riskyPlan` des IA, voie des stands (limiteur, arrêt au box), radio à 3 boutons, météo (pluie, averses, piste mouillée), ailerons qui se tordent puis se détachent (Chaos destruction légère ou mesh séparé), casse moteur selon la fiabilité, 5 réglages de voiture et parc fermé. Écran d'avant-course avec graphique d'usure. »

## Étape 9 — Graphismes mobile « haut de gamme »
**Prompt :** « Matériaux : asphalte avec relief et humidité (paramètre global `Wetness`), herbe, gravier, vibreurs, livrée par masque de couleurs (`TEAMS.color/accent`), carrosserie à reflets (capture de réflexion par circuit). Ciel : `SkyAtmosphere` + nuages en dôme texturé (pas de nuages volumétriques sur mobile), brouillard exponentiel, soleil directionnel avec une cascade d'ombres. Niagara : gerbes d'eau, fumée de pneus, étincelles, confettis. Post-traitement mobile léger (bloom, tonemapper). Profils Bas/Moyen/Haut testés sur iPhone et Android milieu de gamme, objectif 60 i/s. Textures CC0 uniquement (crédits dans `Content/CREDITS.md`). »

## Étape 10 — Sons
**Prompt :** « MetaSounds : moteur à 4 boucles par motoriste (idle/low/mid/high, fichiers de `sons/`) fondues selon le régime et la charge, voiture la plus proche, crissements selon le glissement, vent, pluie, chocs, feux, radio filtrée. Atténuation et concurrence limitées pour le mobile. Réglage Son dans les options. »

## Étape 11 — Cœur de la gestion (sauvegarde, écurie, saison)
**Prompt :** « Porte en C++ (UObjects tenus par un `UGPMCareerSubsystem`) `js/team.js`, `js/career.js`, `js/season.js`, `js/sponsors.js` : écurie (moral, budget, réputation, confiance), calendrier daté de 22 GP (12/16/22), pré-saison en 3 étapes, points, classements pilotes/écuries, courbes de performance, sponsors et missions. Sauvegarde unique `UGPMSaveGame` (USaveGame, slot `gpm`) qui remplace les clés localStorage `gpm-*`. Tests d'automatisation (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`) qui simulent une saison complète et comparent les points/budget aux résultats de la version web pour une même graine aléatoire. »

## Étape 12 — Usine : recherche, infrastructures, chefs
**Prompt :** « Porte `js/garage.js`, `js/infra.js`, `js/staff.js` : 5 arbres de pièces (prix, délais en jours, risques), ateliers, règlement de mi-saison, 7 bâtiments × 3 niveaux, chefs de pôle et leurs contrats, rythme de développement des rivaux, `carUpgrades` → multiplicateurs appliqués à `AGPMCar`. »

## Étape 13 — Mercato et négociations
**Prompt :** « Porte `js/market.js` et `js/nego.js` : marché des transferts (hiver, mi-saison, intersaison), mercato des IA, jeunes et vétérans, négociations avec agents (personnalités, patience, contre-offres, clauses, primes, statut N°1/N°2), contrats pluriannuels. »

## Étape 14 — Vie de l'écurie : événements, histoires, presse, direction
**Prompt :** « Porte `js/events.js`, `js/stories.js`, `js/press.js` : flash info à choix et brèves, 10 histoires à 5 épisodes, ~70 questions de presse avec effets, objectifs de la direction, bilan de fin de saison et licenciement. Textes dans des DataTables (localisables). »

## Étape 15 — Menus et écrans (UMG + CommonUI)
**Prompt :** « Recrée tous les écrans d'`index.html` en UMG/CommonUI, portrait et paysage : accueil (Carrière / Course solo), création de l'écurie en 6 écrans (couleurs, logo 2 couleurs parmi 20 emblèmes, pilote, motoriste, équipier, sponsors), QG (Usine, Installations, Chefs), saison et calendrier par mois, classements, réglages, presse, négociations, résultats. Style commun (couleurs `--acc`/`--acc2` → thème CommonUI). Atelier 3D derrière les menus (`js/showroom.js`) : niveau séparé avec plateau tournant et sol réfléchissant, pilote 3D (`js/pilot3d.js`). »

## Étape 16 — Cinématiques
**Prompt :** « Avec Sequencer + caméras Cine : survol du circuit avant les feux (drone, travelling sur la grille, grue), tour d'honneur et confettis après un podium, sacre de fin de saison, scènes au QG (bienvenue, rencontre avec un pilote et son agent, signature et photo). Touche l'écran pour passer. »

## Étape 17 — Builds, performances, publication
**Prompt :** « Configure le packaging iOS (signature automatique Xcode, icônes, écran de lancement) et Android (AAB, Vulkan + repli OpenGL ES, textures ASTC), profils de périphériques (`DeviceProfiles.ini`) par gamme de téléphone, Unreal Insights pour viser 60 i/s, taille de l'app < 1 Go. Script `unreal/tools/build-ios.sh` / `build-android.sh` (RunUAT BuildCookRun). Checklist App Store / Play Store. »

---
Ordre conseillé : 1 → 2 → 3 → 4 → 5 (première course jouable) → 6 → 7 → 8 → 9 → 10 (course complète) → 11 à 15 (gestion) → 16 → 17.
