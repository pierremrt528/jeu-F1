#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGPM, Log, All);

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture;

/**
 * Matériaux et textures créés par Tools/gpm_assets.py dans Content/GPM (lancé par « POUR JOUER.command »).
 * S'ils n'existent pas encore, les fonctions renvoient nullptr et le jeu garde ses couleurs simples.
 */
namespace GPMArt
{
	/** /Game/GPM/Materials/<Name> */
	GRANDPRIXMANAGER_API UMaterialInterface* Material(const TCHAR* Name);
	/** /Game/GPM/Textures/<Name> */
	GRANDPRIXMANAGER_API UTexture* Texture(const TCHAR* Name);
	/** Instance modifiable d'un matériau (couleur, rugosité…), ou nullptr. */
	GRANDPRIXMANAGER_API UMaterialInstanceDynamic* Make(const TCHAR* Name, UObject* Outer);
	/**
	 * Sol photo projeté à plat (M_GPM_Ground) : « asphalt », « grass », « sand », « gravel », « concrete ».
	 * Scale = taille d'une répétition de la photo, en mètres.
	 */
	GRANDPRIXMANAGER_API UMaterialInstanceDynamic* Surface(const FString& Kind, float Scale, UObject* Outer);
}
