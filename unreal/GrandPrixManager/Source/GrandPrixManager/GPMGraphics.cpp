// Qualité graphique (réglages) : Fluide pour les petites machines, Normale (mesurée à 60 i/s sur un MacBook Air M3),
// Ultra pour une image plus réaliste : ombres longues et fines sur 4 cascades, ombres de contact sous les voitures,
// occlusion ambiante, lissage MSAA net (pas de flou), filtrage 16x de l'asphalte au loin, rayons du soleil, léger grain.
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
	const int32 Q = FMath::Clamp(GfxLevel, 0, 2);
	static const TCHAR* const Cascades[3] = { TEXT("1"), TEXT("2"), TEXT("4") };
	static const TCHAR* const ShadowDist[3] = { TEXT("0.6"), TEXT("0.8"), TEXT("1.6") };
	static const TCHAR* const ShadowRes[3] = { TEXT("1024"), TEXT("2048"), TEXT("4096") };
	static const TCHAR* const Aniso[3] = { TEXT("4"), TEXT("8"), TEXT("16") };
	static const TCHAR* const View[3] = { TEXT("0.8"), TEXT("1"), TEXT("1.5") };
	SetCV(TEXT("r.Shadow.CSM.MaxCascades"), Cascades[Q]);
	SetCV(TEXT("r.Shadow.DistanceScale"), ShadowDist[Q]);
	SetCV(TEXT("r.Shadow.MaxCSMResolution"), ShadowRes[Q]);
	SetCV(TEXT("r.MaxAnisotropy"), Aniso[Q]);
	SetCV(TEXT("r.ViewDistanceScale"), View[Q]);
	SetCV(TEXT("r.AmbientOcclusionLevels"), Q == 2 ? TEXT("2") : TEXT("0"));
	SetCV(TEXT("r.DetailMode"), Q == 2 ? TEXT("2") : TEXT("1"));
	SetCV(TEXT("r.LensFlareQuality"), Q == 2 ? TEXT("2") : TEXT("0"));
	SetCV(TEXT("r.SceneColorFringeQuality"), Q == 2 ? TEXT("1") : TEXT("0"));
	// lissage : MSAA 4x en ultra (bords nets, rendu « forward »), FXAA sinon
	SetCV(TEXT("r.AntiAliasingMethod"), Q == 2 ? TEXT("3") : TEXT("1"));
	SetCV(TEXT("r.MSAACount"), Q == 2 ? TEXT("4") : TEXT("1"));
	SetCV(TEXT("r.Tonemapper.Sharpen"), Q == 2 ? TEXT("0.3") : TEXT("0.5"));

	if (Sun)
		if (UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			// ombres de contact (les voitures « posées » sur l'asphalte) et rayons du soleil dans la brume
			SunLight->ContactShadowLength = Q == 2 ? 0.03f : 0.f;
			SunLight->SetEnableLightShaftBloom(Q == 2);
			SunLight->SetBloomScale(0.12f);
			SunLight->SetBloomThreshold(4.f);
			SunLight->MarkRenderStateDirty();
		}

	if (PPVol)
	{
		FPostProcessSettings& S = PPVol->Settings;
		const bool bUltra = Q == 2;
		S.bOverride_AmbientOcclusionIntensity = true; S.AmbientOcclusionIntensity = bUltra ? 0.55f : 0.f;
		S.bOverride_AmbientOcclusionRadius = true; S.AmbientOcclusionRadius = 120.f;
		S.bOverride_FilmGrainIntensity = true; S.FilmGrainIntensity = bUltra ? 0.04f : 0.f;
		S.bOverride_SceneFringeIntensity = true; S.SceneFringeIntensity = bUltra ? 0.25f : 0.f;
		S.bOverride_BloomIntensity = true; S.BloomIntensity = bUltra ? 0.45f : 0.35f;
		S.bOverride_LensFlareIntensity = true; S.LensFlareIntensity = bUltra ? 0.35f : 0.25f;
		// image un peu plus « caméra de télé » : blancs moins brûlés, noirs plus profonds
		S.bOverride_FilmToe = true; S.FilmToe = bUltra ? 0.58f : 0.55f;
		S.bOverride_FilmShoulder = true; S.FilmShoulder = bUltra ? 0.24f : 0.26f;
	}
	UE_LOG(LogGPM, Log, TEXT("Qualité graphique : %s"), Q == 0 ? TEXT("fluide") : Q == 1 ? TEXT("normale") : TEXT("ultra"));
}
