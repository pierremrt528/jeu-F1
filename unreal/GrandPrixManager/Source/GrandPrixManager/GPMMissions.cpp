// Missions des essais (hiver et essais libres) et usure selon le pilotage.
//  - Usure : en plus des glissades et des sorties de piste, la charge sur les pneus (gros freinages à haute vitesse,
//    freiner en tournant, coups de volant) compte. Un pilote qui anticipe ses freinages use moins qu'un pilote qui freine
//    tard et tourne sec. La référence (StressRef) est la moyenne du plateau, apprise en course sur les IA, pour que la
//    stratégie reste juste ; chaque IA a son propre style et varie d'un tour à l'autre.
//  - Essence : proportionnelle à l'accélérateur ; pied levé, le moteur ne consomme presque plus.
//  - Missions : portes à franchir sur la trajectoire idéale (au point de corde des virages), tour économe en pneus,
//    tour économe en essence, en plus du tour de référence, de la régularité et de l'attaque.
#include "GPMGameMode.h"
#include "GrandPrixManager.h"
#include "GPMCar.h"
#include "GPMSeason.h"
#include "GPMFactory.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

float AGPMCar::StressRef = 0.35f;
float AGPMCar::ErsDrainK = 1.f / 8.f, AGPMCar::ErsChargeK = 1.f, AGPMCar::ErsAccK = 0.12f, AGPMCar::ErsTopK = 0.03f;

namespace GPMMis
{
	constexpr float GateTol = 1.6f;    // m de part et d'autre de la trajectoire idéale
	constexpr float GateHalf = 2.4f;   // poteaux du portail
	float LineAt(const FGPMTrackData& T, int32 i)
	{
		const float Lim = FMath::Max(0.f, T.Width / 2.f - 1.2f);
		return FMath::Clamp(T.RL.IsValidIndex(i) ? T.RL[i] * 0.85f : 0.f, -Lim, Lim);
	}
}

// ---------- usure et essence ----------
void AGPMGameMode::WearAndFuel(AGPMCar* C, float Dt)
{
	const FGPMTrackData* T = C ? C->GetTrack() : nullptr;
	if (!T || C->PitPhase) return;
	const float Len = FMath::Max(1, T->WX.Num()) * 2.f, Dist = FMath::Abs(C->V) * Dt;
	if (Dist <= 0.f) return;
	// la moyenne du plateau, apprise sur les IA en course (pondérée par la distance)
	if (C != Car && Phase == EPhase::Race && !C->bFinished && !C->bOut && C->V > 15.f)
		AGPMCar::StressRef = FMath::Clamp(AGPMCar::StressRef + (C->Stress - AGPMCar::StressRef) * FMath::Min(1.f, Dist * 0.00004f), 0.05f, 3.f);
	if (C->StyleK <= 0.f) C->StyleK = C == Car ? 1.f : 0.9f + 0.2f * float(GetTypeHash(C->DriverName) % 1000) / 999.f;
	const float Style = FMath::Clamp(0.6f + 0.4f * C->Stress / AGPMCar::StressRef, 0.65f, 1.6f);
	// chaque IA a son style, qui varie un peu d'un tour à l'autre
	const float Pers = C == Car ? 1.f : C->StyleK * (1.f + 0.06f * FMath::PerlinNoise1D(C->RaceTime * 0.012f + C->StyleK * 40.f));
	const float Slide = FMath::Abs(C->Beta) > 0.05f ? FMath::Min(1.f, FMath::Abs(C->Beta) * 4.f) : 0.f;
	// vibreurs, roues bloquées (méplat), patinage et sous-virage frottent la gomme
	const float Scrub = (C->bKerb ? 0.6f : 0.f) + (C->bLock ? 2.f : 0.f) + 0.6f * FMath::Clamp(C->Slip - 0.2f, 0.f, 1.f);
	const float Heavy = FMath::Clamp(C->Mass / 1.05f, 0.92f, 1.12f);                 // plein d'essence : pneus plus chargés
	const float Rubber = Phase == EPhase::Race && TotalLaps > 0 ? 1.04f - 0.1f * FMath::Clamp(float(C->Lap) / TotalLaps, 0.f, 1.f) : 1.f;   // la piste se gomme
	const float Dirty = 1.f + 0.25f * FMath::Clamp(C->Wake, 0.f, 1.f);              // dans l'air sale, la voiture glisse
	float K = (1.f + 3.5f * Slide + Scrub + (C->bOffTrack ? 1.5f : 0.f)) * Style * Pers * Heavy * Rubber * Dirty * (C == Car ? 1.f : 0.92f) * AGPMCar::ModeWear(C->Mode) * C->UpTyre * C->SetupFx.Wear;
	K *= AGPMCar::WetWear(C->Tyre, TrackWet);
	C->Wear += Dist / Len * AGPMCar::TyreWpl(C->Tyre) * K;
	if (C == Car && Phase == EPhase::Race) { PStyleSum += Style * Dist; PStyleDist += Dist; }
	if (Phase == EPhase::Race) { FVector2f& Acc = RaceStyle.FindOrAdd(C); Acc.X += Style * Pers * Dist; Acc.Y += Dist; }
	// essence : pied levé, presque rien ; plein gaz, la consommation normale (un peu plus en mode attaque)
	if (!C->bFinished)
	{
		const float Thr = (0.05f + FMath::Clamp(C->Throttle, 0.f, 1.f)) * (1.f - 0.12f * FMath::Clamp(C->Slipstream, 0.f, 1.f));   // dans l'aspiration, moins d'effort
		C->Fuel = FMath::Max(0.f, C->Fuel - Dist / 1000.f * 1.7f * C->UpFuel * AGPMCar::ModeFuel(C->Mode) * Thr);
	}
}

float AGPMGameMode::NomWear() const
{
	return Car ? AGPMCar::TyreWpl(Car->Tyre) * Car->UpTyre * Car->SetupFx.Wear : 0.03f;
}

float AGPMGameMode::NomFuel() const
{
	const FGPMTrackData* T = Car ? Car->GetTrack() : nullptr;
	return T ? T->WX.Num() * 2.f / 1000.f * 1.7f * Car->UpFuel : 8.f;
}

// ---------- missions ----------
int32 AGPMGameMode::TestType(int32 Day) const
{
	const UGPMSeasonSubsystem* S = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	const UGPMSaveGame* G = S ? S->State() : nullptr;
	const int32 Year = G ? G->Year : 2030;
	// trois missions différentes chaque hiver, dans un ordre qui change ; les nouvelles (portes, pneus, essence) 3 fois sur 4
	const int32 Slot = (Day + Year) % 3;
	FRandomStream R(Year * 31 + Day * 7 + 3);
	return R.FRand() < 0.75f ? 3 + Slot : Slot;
}

FString AGPMGameMode::MissionName(int32 Type)
{
	static const TCHAR* const N[6] = { TEXT("Tour de référence"), TEXT("Régularité"), TEXT("Attaque"), TEXT("Portes de trajectoire"), TEXT("Gestion des pneus"), TEXT("Économie d'essence") };
	return N[FMath::Clamp(Type, 0, 5)];
}

FString AGPMGameMode::MissionGoal(int32 Type)
{
	static const TCHAR* const N[6] = {
		TEXT("un tour propre à moins de 3 % de la référence"),
		TEXT("trois tours propres, écart maximal 1,5 %"),
		TEXT("un tour propre aussi vite que la référence"),
		TEXT("80 % des portes sur un tour propre, à moins de 4 % de la référence"),
		TEXT("2 tours propres à +3,5 % maxi, usure des pneus 25 % sous la normale"),
		TEXT("2 tours propres à +4 % maxi, 20 % d'essence en moins (lève le pied)") };
	return N[FMath::Clamp(Type, 0, 5)];
}

void AGPMGameMode::MisReset()
{
	LapWear.Reset(); LapFuel.Reset(); LapGate.Reset();
	MisLapSeen = Car ? Car->Lap : 0;
	MisW0 = Car ? Car->Wear : 0.f; MisF0 = Car ? Car->Fuel : 0.f;
	GateNext = GateHit = 0;
	for (uint8& St : GateSt) St = 0;
}

void AGPMGameMode::MisLap()
{
	if (!Car) return;
	if (Car->Lap >= 2 && MisLapSeen >= 1)
	{
		LapWear.Add(Car->Wear - MisW0);
		LapFuel.Add(MisF0 - Car->Fuel);
		LapGate.Add(GateIdx.Num() ? float(GateHit) / GateIdx.Num() : 0.f);
	}
	MisLapSeen = Car->Lap;
	MisW0 = Car->Wear; MisF0 = Car->Fuel;
	if (GateIdx.Num())
	{
		GateNext = GateHit = 0;
		for (uint8& St : GateSt) St = 0;
		RefreshGates();
	}
}

// ---------- portes ----------
void AGPMGameMode::ClearGates()
{
	if (GateActor) GateActor->Destroy();
	GateActor = nullptr;
	GateMesh.Reset();
	GateIdx.Reset(); GateSt.Reset();
	GateNext = GateHit = 0;
}

void AGPMGameMode::BuildGates()
{
	ClearGates();
	const FGPMTrackData* T = CurTrack();
	if (!T || !GetWorld()) return;
	const int32 N = T->WX.Num();
	if (N < 100 || T->RL.Num() != N || T->Curvature.Num() != N) return;
	// une porte au point de corde de chaque virage marqué (courbure maximale), 70 m au moins entre deux portes, 14 au plus
	TArray<FVector2f> Cand;   // (courbure, index)
	for (int32 i = 25; i < N - 25; ++i)
	{
		const float C0 = FMath::Abs(T->Curvature[i]);
		if (C0 < 1.f / 260.f) continue;
		bool bMax = true;
		for (int32 d = -20; d <= 20 && bMax; ++d) if (d && FMath::Abs(T->Curvature[(i + d + N) % N]) > C0) bMax = false;
		if (bMax) Cand.Add(FVector2f(C0, float(i)));
	}
	Cand.Sort([](const FVector2f& A, const FVector2f& B) { return A.X > B.X; });
	for (const FVector2f& P : Cand)
	{
		if (GateIdx.Num() >= 14) break;
		const int32 I = int32(P.Y);
		bool bFar = true;
		for (int32 Gi : GateIdx) { const int32 D = FMath::Abs(Gi - I); if (FMath::Min(D, N - D) < 35) bFar = false; }
		if (bFar) GateIdx.Add(I);
	}
	GateIdx.Sort();
	GateSt.Init(0, GateIdx.Num());
	if (GateIdx.Num() == 0) return;
	GateActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
	if (!GateActor) return;
	USceneComponent* Root = NewObject<USceneComponent>(GateActor, TEXT("Racine"));
	GateActor->SetRootComponent(Root);
	Root->RegisterComponent();
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	// à franchir (jaune), franchie (vert), manquée (rouge)
	const FLinearColor Cols[3] = { FLinearColor(1.f, 0.72f, 0.05f), FLinearColor(0.15f, 0.9f, 0.3f), FLinearColor(0.95f, 0.15f, 0.1f) };
	for (int32 k = 0; k < 3; ++k)
	{
		UInstancedStaticMeshComponent* M = NewObject<UInstancedStaticMeshComponent>(GateActor);
		M->SetStaticMesh(Cube);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->CastShadow = false;
		M->SetupAttachment(Root);
		if (UMaterialInstanceDynamic* Mi = GPMArt::Make(TEXT("M_GPM_Inst"), GateActor))
		{
			Mi->SetVectorParameterValue(TEXT("Color"), Cols[k]);
			Mi->SetVectorParameterValue(TEXT("Color2"), Cols[k]);
			Mi->SetScalarParameterValue(TEXT("Rough"), 0.5f);
			Mi->SetScalarParameterValue(TEXT("Windows"), 0.f);
			Mi->SetVectorParameterValue(TEXT("Glow"), Cols[k] * 0.8f);   // les portes se voient de loin
			M->SetMaterial(0, Mi);
			M->SetNumCustomDataFloats(2);
		}
		else if (Base)
		{
			UMaterialInstanceDynamic* Mi2 = UMaterialInstanceDynamic::Create(Base, GateActor);
			Mi2->SetVectorParameterValue(TEXT("Color"), Cols[k]);
			M->SetMaterial(0, Mi2);
		}
		M->RegisterComponent();
		GateMesh.Add(M);
	}
	RefreshGates();
}

void AGPMGameMode::RefreshGates()
{
	const FGPMTrackData* T = CurTrack();
	if (!T || GateMesh.Num() != 3) return;
	for (UInstancedStaticMeshComponent* M : GateMesh) if (M) M->ClearInstances();
	for (int32 g = 0; g < GateIdx.Num(); ++g)
	{
		UInstancedStaticMeshComponent* M = GateMesh[FMath::Clamp(int32(GateSt.IsValidIndex(g) ? GateSt[g] : 0), 0, 2)];
		if (!M) continue;
		const int32 i = GateIdx[g];
		const float Rx = -T->TZ[i], Rz = T->TX[i], L = GPMMis::LineAt(*T, i);
		const FVector Side(Rx, Rz, 0.f);
		const FRotator Rot = FRotationMatrix::MakeFromX(Side).Rotator();
		auto At = [T, i, Rx, Rz](float Lat, float H) { return FGPMTrackData::ToWorld(T->WX[i] + Rx * Lat, T->WY[i] + H, T->WZ[i] + Rz * Lat); };
		// deux poteaux et une barre en haut : la voiture doit passer dessous, au milieu
		M->AddInstance(FTransform(Rot, At(L - GPMMis::GateHalf, 0.9f), FVector(0.14f, 0.14f, 1.8f)), true);
		M->AddInstance(FTransform(Rot, At(L + GPMMis::GateHalf, 0.9f), FVector(0.14f, 0.14f, 1.8f)), true);
		M->AddInstance(FTransform(Rot, At(L, 1.85f), FVector(GPMMis::GateHalf * 2.f + 0.14f, 0.14f, 0.22f)), true);
	}
}

void AGPMGameMode::GateTick()
{
	const FGPMTrackData* T = Car ? Car->GetTrack() : nullptr;
	if (!T || !GateIdx.IsValidIndex(GateNext) || Car->Lap < 1) return;
	const int32 g = GateIdx[GateNext];
	if (Car->Idx < g || Car->Idx - g > 80) return;
	const float Off = FMath::Abs(Car->Lat - GPMMis::LineAt(*T, g));
	const bool bIn = Off <= GPMMis::GateTol;
	if (GateSt.IsValidIndex(GateNext)) GateSt[GateNext] = bIn ? 1 : 2;
	if (bIn) ++GateHit;
	else Say(FString::Printf(TEXT("Porte manquée : %.1f m de la trajectoire."), Off));
	++GateNext;
	RefreshGates();
}

FString AGPMGameMode::MisStatus(int32 Type) const
{
	if (!Car) return FString();
	if (Type == 3) return FString::Printf(TEXT("portes %d / %d"), GateHit, GateIdx.Num());
	if (Type == 4) return FString::Printf(TEXT("usure du tour %.1f %% (normale %.1f %%)"), (Car->Wear - MisW0) * 100.f, NomWear() * 100.f);
	if (Type == 5) return FString::Printf(TEXT("essence du tour %.1f kg (normale %.1f)"), MisF0 - Car->Fuel, NomFuel());
	return FString();
}

// ---------- course : le dernier tour du joueur ----------
void AGPMGameMode::PlayerLapLog()
{
	if (!Car || Car->Lap == RaceLapSeen) return;
	if (RaceLapSeen >= 1 && Car->Lap == RaceLapSeen + 1)
	{
		const float Dw = Car->Wear - RaceW0, Df = RaceF0 - Car->Fuel;   // négatifs après un arrêt (pneus neufs, ravitaillement)
		if (Dw > 0.f) LapWearR = Dw;
		if (Df > 0.f) LapFuelR = Df;
		// début de relais : l'usure à viser par tour pour finir le relais avec des pneus encore bons
		if (Stints.IsValidIndex(Car->Stops) && StintMsgStop != Car->Stops && RadioT <= 0.f)
		{
			StintMsgStop = Car->Stops;
			const int32 Len = FMath::Max(1, Stints[Car->Stops].Value);
			Say(FString::Printf(TEXT("Muret : relais de %d tours en %s, vise %.1f %% d'usure par tour au maximum."), Len, *TyreName(Car->Tyre), 70.f / Len));
		}
		// usure plus rapide ou plus lente que prévu (un message tous les 4 tours au plus)
		const float Nom = AGPMCar::TyreWpl(Car->Tyre) * Car->UpTyre * Car->SetupFx.Wear * AGPMCar::ModeWear(Car->Mode) * AGPMCar::WetWear(Car->Tyre, TrackWet);
		if (Dw > 0.f && Nom > 0.f && Car->Lap >= 3 && Car->Lap - WearMsgLap >= 4 && RadioT <= 0.f)
		{
			const float Rt = Dw / Nom;
			if (Rt > 1.2f) { WearMsgLap = Car->Lap; Say(FString::Printf(TEXT("Muret : tu uses les pneus %.0f %% plus vite que prévu. Freine plus tôt, en ligne droite, et tourne en douceur."), (Rt - 1.f) * 100.f)); }
			else if (Rt < 0.85f) { WearMsgLap = Car->Lap; Say(FString::Printf(TEXT("Muret : belle gestion, les pneus s'usent %.0f %% moins vite que prévu."), (1.f - Rt) * 100.f)); }
		}
	}
	RaceLapSeen = Car->Lap;
	RaceW0 = Car->Wear; RaceF0 = Car->Fuel;
}

// ---------- batterie selon le règlement ----------
void AGPMGameMode::SetErsRules()
{
	const UGPMFactorySubsystem* F = GetGameInstance()->GetSubsystem<UGPMFactorySubsystem>();
	const UGPMSeasonSubsystem* S = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	ErsEra = bCareer && F && S && S->State() && S->State()->bCreated ? F->EraIndex() : 0;
	const UGPMFactorySubsystem::FErs E = UGPMFactorySubsystem::ErsRules(ErsEra);
	// les fractions de la batterie : la recharge de référence (4 MJ) est celle de l'ère Fondation
	AGPMCar::ErsDrainK = E.Drain / FMath::Max(0.5f, E.Cap);
	AGPMCar::ErsChargeK = E.Charge * 4.f / FMath::Max(0.5f, E.Cap);
	AGPMCar::ErsAccK = E.Acc;
	AGPMCar::ErsTopK = E.Top;
}
