# Grand Prix Manager — projet Unreal Engine 5

Plan complet de la migration : `../PLAN.md`.

## Ouvrir le projet sur Mac (étape 1)
1. Installe Epic Games Launcher, Unreal Engine 5.5+ (avec iOS et Android), Xcode et Git LFS (voir `../PLAN.md`).
2. `git clone https://github.com/pierremrt528/jeu-F1.git` puis `git checkout claude/project-thread-rdd8ot`.
3. Double-clique sur `unreal/GrandPrixManager/GrandPrixManager.uproject`.
   - Si Unreal demande quelle version utiliser, choisis la tienne (5.5 ou plus récente).
   - « Les modules sont manquants, recompiler ? » → **Oui** (Xcode compile le C++, 2 à 5 min la première fois).
4. Dans l'éditeur : *Fichier → Nouveau niveau → Basique*, enregistre-le sous `Content/Maps/Demarrage`.
5. Bouton ▶ Jouer : le journal de sortie affiche « Grand Prix Manager : démarrage ».

En cas d'erreur de compilation : clic droit sur le `.uproject` → *Generate Xcode project*, ouvre le `.xcworkspace` et lance la cible `GrandPrixManagerEditor`, puis copie l'erreur dans le fil.
