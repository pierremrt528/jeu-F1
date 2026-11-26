// Qualité graphique (réglages) : Fluide pour les petites machines, Normale (mesurée à 60 i/s sur un MacBook Air M3),
// Ultra pour une image plus réaliste : ombres longues et fines sur 4 cascades, ombres de contact sous les voitures,
// occlusion ambiante, lissage MSAA net (pas de flou), filtrage 16x de l'asphalte au loin, rayons du soleil, léger grain.
// Maximale (Mac puissant) : image calculée jusqu'à 140 % de la définition de l'écran puis réduite (UpdatePerf), ombres
// jusqu'à l'horizon, occlusion plus fine, vue deux fois plus lointaine, ombres de tous les immeubles (SetBigShadows dès Ultra).
#include "GPMScenery.h"
#include "GPMGameMode.h"
#include "GrandPrixManager.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/PostProcessVolume.h"
#include "HAL/IConsoleManager.h"

void AGPMGameMode::ApplyGraphics()
{
	auto SetCV = [](const TCHAR* Name, const TCHAR* Value)
	{
		if (IConsoleVariable* CV = IConsoleManager::Get().FindConsoleVariable(Name)) CV->Set(Value, ECVF_SetByCode);
	};
	const int32 Q = FMath::Clamp(GfxLevel, 0, 3);
	const bool bHigh = Q >= 2, bMax = Q == 3;
	static const TCHAR* const Cascades[4] = { TEXT("1"), TEXT("2"), TEXT("4"), TEXT("4") };
	static const TCHAR* const ShadowDist[4] = { TEXT("0.6"), TEXT("0.8"), TEXT("1.6"), TEXT("3") };
	static const TCHAR* const ShadowRes[4] = { TEXT("1024"), TEXT("2048"), TEXT("4096"), TEXT("4096") };
	static const TCHAR* const Aniso[4] = { TEXT("4"), TEXT("8"), TEXT("16"), TEXT("16") };
	static const TCHAR* const View[4] = { TEXT("0.8"), TEXT("1"), TEXT("1.5"), TEXT("2") };
	SetCV(TEXT("r.Shadow.CSM.MaxCascades"), Cascades[Q]);
	SetCV(TEXT("r.Shadow.DistanceScale"), ShadowDist[Q]);
	SetCV(TEXT("r.Shadow.MaxCSMResolution"), ShadowRes[Q]);
	SetCV(TEXT("r.MaxAnisotropy"), Aniso[Q]);
	SetCV(TEXT("r.ViewDistanceScale"), View[Q]);
	SetCV(TEXT("r.AmbientOcclusionLevels"), bMax ? TEXT("3") : bHigh ? TEXT("2") : TEXT("0"));
	SetCV(TEXT("r.DetailMode"), bHigh ? TEXT("2") : TEXT("1"));
	SetCV(TEXT("r.LensFlareQuality"), bHigh ? TEXT("2") : TEXT("0"));
	SetCV(TEXT("r.SceneColorFringeQuality"), bHigh ? TEXT("1") : TEXT("0"));
	// petits objets (lampadaires, bateaux au loin) : gardent leur ombre plus loin en Maximale
	SetCV(TEXT("r.Shadow.RadiusThreshold"), bMax ? TEXT("0.01") : TEXT("0.03"));
	// lissage : MSAA 4x en ultra (bords nets, rendu « forward »), FXAA sinon
	SetCV(TEXT("r.AntiAliasingMethod"), bHigh ? TEXT("3") : TEXT("1"));
	SetCV(TEXT("r.MSAACount"), bHigh ? TEXT("4") : TEXT("1"));
	SetCV(TEXT("r.Tonemapper.Sharpen"), bMax ? TEXT("0.2") : bHigh ? TEXT("0.3") : TEXT("0.5"));
	// suréchantillonnage : la résolution adaptative peut monter au-dessus de 100 % (UpdatePerf) ; en revenant d'un niveau
	// inférieur, on repart de 100 %
	if (!bMax && ScreenPct > 100)
	{
		ScreenPct = 100;
		SetCV(TEXT("r.ScreenPercentage"), TEXT("100"));
	}
	if (Scenery) Scenery->SetBigShadows(bHigh);

	if (Sun)
		if (UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			// ombres de contact (les voitures « posées » sur l'asphalte) et rayons du soleil dans la brume
			SunLight->ContactShadowLength = bMax ? 0.045f : bHigh ? 0.03f : 0.f;
			SunLight->SetEnableLightShaftBloom(bHigh);
			SunLight->SetBloomScale(0.12f);
			SunLight->SetBloomThreshold(4.f);
			SunLight->MarkRenderStateDirty();
		}

	if (PPVol)
	{
		FPostProcessSettings& S = PPVol->Settings;
		const bool bUltra = bHigh;
		S.bOverride_AmbientOcclusionIntensity = true; S.AmbientOcclusionIntensity = bUltra ? 0.55f : 0.f;
		S.bOverride_AmbientOcclusionRadius = true; S.AmbientOcclusionRadius = bMax ? 90.f : 120.f;
		S.bOverride_AmbientOcclusionQuality = true; S.AmbientOcclusionQuality = bMax ? 100.f : 50.f;
		S.bOverride_FilmGrainIntensity = true; S.FilmGrainIntensity = bUltra ? 0.04f : 0.f;
		S.bOverride_SceneFringeIntensity = true; S.SceneFringeIntensity = bUltra ? 0.25f : 0.f;
		S.bOverride_BloomIntensity = true; S.BloomIntensity = bUltra ? 0.45f : 0.35f;
		S.bOverride_LensFlareIntensity = true; S.LensFlareIntensity = bUltra ? 0.35f : 0.25f;
		// image un peu plus « caméra de télé » : blancs moins brûlés, noirs plus profonds
		S.bOverride_FilmToe = true; S.FilmToe = bUltra ? 0.58f : 0.55f;
		S.bOverride_FilmShoulder = true; S.FilmShoulder = bUltra ? 0.24f : 0.26f;
	}
	UE_LOG(LogGPM, Log, TEXT("Qualité graphique : %s"), Q == 0 ? TEXT("fluide") : Q == 1 ? TEXT("normale") : Q == 2 ? TEXT("ultra") : TEXT("maximale"));
}
