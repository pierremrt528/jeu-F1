#include "GrandPrixManager.h"
#include "Modules/ModuleManager.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture.h"
#include "Misc/PackageName.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY(LogGPM);

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, GrandPrixManager, "GrandPrixManager");

namespace GPMArtCache
{
	// objet déjà cherché : trouvé (gardé par l'objet lui-même) ou absent (on ne cherche plus)
	TSet<FString> Missing;

	UObject* Find(const FString& Dir, const TCHAR* Name, UClass* Class)
	{
		const FString Package = Dir + Name;
		if (Missing.Contains(Package)) return nullptr;
		if (!FPackageName::DoesPackageExist(Package))
		{
			Missing.Add(Package);
			UE_LOG(LogGPM, Log, TEXT("Graphismes : %s absent (lance Tools/gpm_assets.py via POUR JOUER)"), *Package);
			return nullptr;
		}
		UObject* O = FSoftObjectPath(Package + TEXT(".") + Name).TryLoad();
		if (!O || !O->IsA(Class)) { Missing.Add(Package); return nullptr; }
		return O;
	}
}

UMaterialInterface* GPMArt::Material(const TCHAR* Name)
{
	return Cast<UMaterialInterface>(GPMArtCache::Find(TEXT("/Game/GPM/Materials/"), Name, UMaterialInterface::StaticClass()));
}

UTexture* GPMArt::Texture(const TCHAR* Name)
{
	return Cast<UTexture>(GPMArtCache::Find(TEXT("/Game/GPM/Textures/"), Name, UTexture::StaticClass()));
}

UMaterialInstanceDynamic* GPMArt::Make(const TCHAR* Name, UObject* Outer)
{
	UMaterialInterface* M = Material(Name);
	return M ? UMaterialInstanceDynamic::Create(M, Outer) : nullptr;
}

UMaterialInstanceDynamic* GPMArt::Surface(const FString& Kind, float Scale, UObject* Outer)
{
	UMaterialInstanceDynamic* M = Make(TEXT("M_GPM_Ground"), Outer);
	if (!M) return nullptr;
	// réglages par sol : photo, désaturation, teinte, relief, rugosité, reflets
	const TCHAR* Tex = TEXT("T_Grass");
	float Desat = 0.85f, Bump = 0.3f, Rough = 0.95f, Spec = 0.25f;
	FLinearColor Tint(0.62f, 1.5f, 0.42f);
	if (Kind == TEXT("asphalt")) { Tex = TEXT("T_Asphalt"); Desat = 0.35f; Bump = 1.f; Rough = 0.78f; Spec = 0.4f; Tint = FLinearColor(0.95f, 0.95f, 0.98f); }
	else if (Kind == TEXT("sand")) { Tex = TEXT("T_Sand"); Desat = 0.25f; Bump = 0.25f; Rough = 0.92f; Spec = 0.3f; Tint = FLinearColor(1.25f, 1.08f, 0.85f); }
	else if (Kind == TEXT("gravel")) { Tex = TEXT("T_Gravel"); Desat = 0.5f; Bump = 0.6f; Rough = 0.9f; Spec = 0.3f; Tint = FLinearColor(2.1f, 1.95f, 1.7f); }
	else if (Kind == TEXT("concrete")) { Tex = TEXT("T_Gravel"); Desat = 0.8f; Bump = 0.4f; Rough = 0.85f; Spec = 0.35f; Tint = FLinearColor(1.7f, 1.72f, 1.68f); }
	if (UTexture* T = Texture(Tex)) M->SetTextureParameterValue(TEXT("Tex"), T);
	if (UTexture* N = Texture(TEXT("T_Asphalt_N"))) M->SetTextureParameterValue(TEXT("NTex"), N);
	M->SetScalarParameterValue(TEXT("TexScale"), 1.f / FMath::Max(1.f, Scale * 100.f));
	M->SetScalarParameterValue(TEXT("Desat"), Desat);
	M->SetScalarParameterValue(TEXT("Bump"), Bump);
	M->SetScalarParameterValue(TEXT("Rough"), Rough);
	M->SetScalarParameterValue(TEXT("Spec"), Spec);
	M->SetVectorParameterValue(TEXT("Color"), Tint);
	return M;
}
