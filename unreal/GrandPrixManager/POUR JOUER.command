#!/bin/bash
# Double-clique sur ce fichier : il compile le jeu puis le lance (sans l'éditeur).
cd "$(dirname "$0")"
P="$PWD/GrandPrixManager.uproject"
mkdir -p Saved
# Cherche une installation complète d'Unreal (le Launcher peut créer des dossiers imbriqués, ex. UE_5.5/UE_5.5/UE_5.5)
UE=""
for B in $(find "/Users/Shared/Epic Games" -maxdepth 9 -path "*/Engine/Build/BatchFiles/Mac/Build.sh" 2>/dev/null | sort -r | tr ' ' '|'); do
  B="${B//|/ }"; R="${B%/Engine/Build/BatchFiles/Mac/Build.sh}"
  if [ -e "$R/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" ]; then UE="$R"; break; fi
done
if [ -z "$UE" ]; then echo "Unreal Engine introuvable dans /Users/Shared/Epic Games. Installe-le depuis le Launcher Epic."; read -p "Entrée pour fermer"; exit 1; fi
echo "Unreal : $UE"
echo "Compilation du jeu (2 à 10 minutes la première fois, quelques secondes ensuite)…"
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" GrandPrixManagerEditor Mac Development -Project="$P" -WaitMutex 2>&1 | tee Saved/compilation.log
if [ ${PIPESTATUS[0]} -ne 0 ]; then
  echo; echo "❌ La compilation a échoué. Dis à Claude « erreur de compilation » : il lira Saved/compilation.log."
  read -p "Entrée pour fermer"; exit 1
fi
# Textures et matériaux du jeu (Tools/gpm_assets.py), créés une fois puis à chaque nouvelle version du script
V=$(grep -m1 "^VERSION = " Tools/gpm_assets.py | tr -dc '0-9')
if [ -n "$V" ] && [ ! -f "Content/GPM/assets-v$V.txt" ]; then
  echo "🎨 Création des textures et matériaux (une seule fois, 1 à 3 minutes)…"
  "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$P" -run=pythonscript -script="$PWD/Tools/gpm_assets.py" -unattended -nopause -nosplash -stdout -FullStdOutLogOutput > Saved/assets.log 2>&1
  grep "GPMAssets" Saved/assets.log | tail -20
  [ -f "Content/GPM/assets-v$V.txt" ] || echo "⚠️ Matériaux non créés : le jeu garde ses couleurs simples (dis-le à Claude, il lira Saved/assets.log)."
fi
echo "✅ Lancement du jeu… (flèches ou ZQSD pour conduire, C : caméra, R : replacer, Échap puis Cmd+Q pour quitter)"
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$P" -game -windowed -ResX=1266 -ResY=585 -log -abslog="$PWD/Saved/ue.log" 2>&1 | tee Saved/jeu.log | grep --line-buffered "LogGPM"
