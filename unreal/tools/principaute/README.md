# Circuit de Principauté — générateurs

Scripts Python (Pillow) qui ont produit le tracé et le décor du Circuit de Principauté, à lancer depuis ce dossier :

1. `design.py` / `fit2.py` / `render.py` : tracé en segments (droites et virages), refermé puis mis à l'échelle → `seg.json`.
2. `points.py` : points de contrôle `[x, z, alt, mur gauche, mur droit]`, ligne de départ et tunnel → `points.json` (copiés dans `js/data.js`).
3. Après `node unreal/tools/export-data.mjs` : `deco.py` place le décor (port, quais, yachts, immeubles, monuments, tribunes,
   arbres, lampadaires) → `Content/Data/tracks/principaute_deco.json` ; aperçu `deco.png`.
4. `models.py` : modèles low-poly (yacht, voilier, casino, palais, lampadaire) ajoutés à `Content/Data/decor.json` ; `view3d.py` pour les vérifier.

`common.py` et `geo.py` : chargement de la piste, outils de géométrie (polygones, grille d'occupation).
