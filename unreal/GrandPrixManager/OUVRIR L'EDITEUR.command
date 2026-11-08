#!/bin/bash
# Double-clique : compile puis ouvre l'éditeur Unreal (pour modifier le jeu).
cd "$(dirname "$0")"
P="$PWD/GrandPrixManager.uproject"
mkdir -p Saved
UE=$(ls -d "/Users/Shared/Epic Games"/UE_5.* 2>/dev/null | sort | tail -1)
if [ -z "$UE" ]; then echo "Unreal Engine introuvable dans /Users/Shared/Epic Games. Installe-le depuis le Launcher Epic."; read -p "Entrée pour fermer"; exit 1; fi
echo "Unreal : $UE"
echo "Compilation du jeu (2 à 10 minutes la première fois, quelques secondes ensuite)…"
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" GrandPrixManagerEditor Mac Development -Project="$P" -WaitMutex 2>&1 | tee Saved/compilation.log
if [ ${PIPESTATUS[0]} -ne 0 ]; then
  echo; echo "❌ La compilation a échoué. Dis à Claude « erreur de compilation » : il lira Saved/compilation.log."
  read -p "Entrée pour fermer"; exit 1
fi
echo "✅ Ouverture de l'éditeur…"
open -n "$UE/Engine/Binaries/Mac/UnrealEditor.app" --args "$P"
