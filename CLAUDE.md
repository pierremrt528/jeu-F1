# Grand Prix Manager — vision du jeu

Jeu mobile (PWA) qui mélange **course de monoplaces** vue du dessus et **gestion d'écurie**.
Jouable dans le navigateur du téléphone, installable sur l'écran d'accueil. Langue du jeu : français.

## Règles du projet
- **Univers 100 % fictif** : écuries, pilotes, circuits et logos inventés. Aucun nom, logo ou marque réels (F1, FIA, écuries, pilotes).
- **Mobile d'abord** : portrait, commandes tactiles, multi-touch. Tester à 390×844.
- **Simple** : HTML/CSS/JS vanilla + Canvas 2D, aucune étape de build, aucune dépendance. Pas de framework tant que ce n'est pas nécessaire.
- Petits changements ciblés ; ne pas réécrire ce qui marche.
- Après chaque modif des fichiers du jeu : incrémenter `VERSION` dans `sw.js` (sinon le téléphone garde l'ancienne version en cache).
- Déploiement : GitHub Pages depuis la branche `main`, racine du dépôt.

## Structure
- `index.html` : écrans (menu, résultats) + canvas.
- `game.js` : tout le jeu. Sections : données (équipes, circuit) → construction du circuit (spline Catmull-Rom ré-échantillonnée tous les 10 unités) → entrées (boutons tactiles dessinés sur le canvas + flèches clavier) → course (physique, IA, collisions, tours) → rendu (caméra qui suit le joueur, orientée vers le haut) → HUD + mini-carte → écrans.
- `style.css`, `manifest.json`, `sw.js` (cache hors-ligne, réseau d'abord), icônes.
- Tester en local : `python3 -m http.server` puis ouvrir `localhost:8000`.

## Feuille de route
1. ✅ **Prototype de course** : 1 circuit (Valmont), joueur + 5 IA, 3 tours, classement final.
2. **Stratégie pneus et arrêts au stand** : gommes tendre/médium/dure, usure qui fait baisser l'adhérence, voie des stands, choix de stratégie avant la course, ordres depuis le muret (pousser / économiser).
3. **Gestion d'écurie** : choix de son écurie, budget, salaires, sponsors.
4. **Développement de la voiture** : points de R&D sur moteur, aéro, châssis, fiabilité → modifient vmax, adhérence en virage, usure.
5. **Pilotes** : stats (vitesse, régularité, gestion des pneus), contrats, recrutement.
6. **Saison** : calendrier de plusieurs circuits fictifs, points, classements pilotes et constructeurs, qualifications.
7. **Sauvegarde locale** (localStorage) de la carrière.
8. Finitions : sons, météo/pluie, dégâts, voiture de sécurité, meilleurs graphismes.
