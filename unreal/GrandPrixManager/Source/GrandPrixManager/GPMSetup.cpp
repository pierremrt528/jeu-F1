// Réglages détaillés de la voiture (ailerons, boîte, freins, différentiel, suspension, antiroulis, hauteur, carrossage,
// pression des pneus) : effet sur la physique de AGPMCar, tour simulé, conseil de l'ingénieur, réglages de base.
// Aussi les options des pilotes IA (agressivité, erreurs), gardées dans GameUserSettings.ini.
#include "GPMSetup.h"
#include "GPMGameMode.h"
#include "GPMCar.h"
#include "Misc/ConfigCacheIni.h"

// ---------- réglages → physique ----------

FGPMSetupFx FGPMCarSetup::Fx() const
{
	auto U = [this](int32 K) { return (FMath::Clamp(V[K], 0, 10) - 5) / 5.f; };   // −1 … 0 (neutre) … 1
	const float Uf = U(FrontWing), Ur = U(RearWing), Ug = U(Gears), Ud = U(Diff), Us = U(Susp), Ua = U(Arb), Uh = U(Ride), Uc = U(Camber), Up = U(Press);
	FGPMSetupFx F;
	// ailerons : appui sur leur essieu contre traînée (l'aileron arrière freine deux fois plus) ; suspension ferme et
	// voiture basse : l'aéro travaille mieux
	const float Platform = (1.f + 0.035f * Us) * (1.f - 0.03f * Uh);
	F.KdF = (1.f + 0.04f * Uf) * Platform;
	F.KdR = (1.f + 0.04f * Ur) * Platform;
	F.VTop = (1.f - 0.012f * Uf - 0.024f * Ur) * (1.f + 0.015f * Ug) * (1.f - 0.004f * Uh) * (1.f + 0.003f * Up);
	F.Acc = 1.f - 0.06f * Ug;
	// adhérence en virage : suspension souple, carrossage (meilleur vers les 3/4 du réglage), pression basse (meilleure
	// un peu sous la moyenne) ; les barres antiroulis déplacent l'adhérence entre l'avant et l'arrière
	const float Lat = (1.f - 0.006f * Us) * (1.f + 0.02f * Uc - 0.012f * Uc * Uc) * (1.f - 0.012f * Up - 0.01f * Up * Up);
	F.MuF = Lat * (1.f - 0.02f * Ua);
	F.MuR = Lat * (1.f + 0.02f * Ua);
	F.MuLong = (1.f - 0.025f * Uc) * (1.f - 0.01f * Up);
	F.Bias = 0.52f + 0.01f * FMath::Clamp(V[BrakeBias], 0, 10);
	F.BiasIdeal = 0.56f + 0.25f * (F.KdF - F.KdR) / (F.KdF + F.KdR);
	F.BrakeP = (0.8f + 0.02f * FMath::Clamp(V[BrakePress], 0, 10)) / 0.96f;
	F.Diff = 0.5f + 0.4f * Ud;
	F.Kerb = 1.f - 0.06f * Us - 0.03f * FMath::Max(0.f, -Uh);
	F.Bump = 1.f + 0.8f * Us;
	F.Bottom = FMath::Max(0.f, -Uh - 0.2f) * (1.25f - 0.5f * Us);
	F.Wear = (1.f + 0.15f * Uc) * (1.f - 0.18f * Up);
	return F;
}

const TCHAR* FGPMCarSetup::Name(int32 K)
{
	static const TCHAR* const Nm[Num] = { TEXT("Aileron avant"), TEXT("Aileron arrière"), TEXT("Rapports de boîte"), TEXT("Répartition de freinage"),
		TEXT("Pression de freinage"), TEXT("Différentiel"), TEXT("Suspension"), TEXT("Barres antiroulis"), TEXT("Hauteur de caisse"),
		TEXT("Carrossage"), TEXT("Pression des pneus") };
	return Nm[FMath::Clamp(K, 0, Num - 1)];
}

const TCHAR* FGPMCarSetup::Help(int32 K)
{
	static const TCHAR* const Hl[Num] = {
		TEXT("Plus d'appui à l'avant : la voiture tourne mieux, mais l'arrière peut décrocher. Un peu moins de vitesse de pointe."),
		TEXT("Plus d'appui à l'arrière : voiture stable et bonne motricité, mais nettement plus lente en ligne droite."),
		TEXT("Rapports courts : meilleures accélérations. Rapports longs : plus de vitesse de pointe."),
		TEXT("Vers l'avant : freinage stable, mais la voiture tourne moins bien. Vers l'arrière : elle pivote mieux, mais l'arrière peut se dérober."),
		TEXT("Plus de pression : on freine plus fort, mais les roues bloquent plus vite (surtout sans ABS)."),
		TEXT("Bloqué : meilleure motricité en sortie des virages lents, mais la voiture pousse tout droit à la remise des gaz."),
		TEXT("Ferme : l'aéro travaille mieux dans les virages rapides. Souple : plus d'adhérence sur les bosses et les vibreurs."),
		TEXT("Avant plus ferme : la voiture sous-vire (l'avant glisse). Arrière plus ferme : elle survire (l'arrière glisse)."),
		TEXT("Basse : plus d'appui, mais la voiture talonne et décroche à haute vitesse, surtout avec une suspension souple."),
		TEXT("Plus de carrossage : plus d'adhérence en virage, un peu moins au freinage, et les pneus s'usent plus vite."),
		TEXT("Basse : plus d'adhérence, mais les pneus s'usent plus vite. Haute : moins d'usure et un peu plus de vitesse de pointe.") };
	return Hl[FMath::Clamp(K, 0, Num - 1)];
}

const TCHAR* FGPMCarSetup::LowLabel(int32 K)
{
	static const TCHAR* const L[Num] = { TEXT("Moins d'appui"), TEXT("Moins d'appui"), TEXT("Courts"), TEXT("Arrière"), TEXT("Douce"), TEXT("Ouvert"),
		TEXT("Souple"), TEXT("Arrière ferme"), TEXT("Basse"), TEXT("Faible"), TEXT("Basse") };
	return L[FMath::Clamp(K, 0, Num - 1)];
}

const TCHAR* FGPMCarSetup::HighLabel(int32 K)
{
	static const TCHAR* const H[Num] = { TEXT("Plus d'appui"), TEXT("Plus d'appui"), TEXT("Longs"), TEXT("Avant"), TEXT("Forte"), TEXT("Bloqué"),
		TEXT("Ferme"), TEXT("Avant ferme"), TEXT("Haute"), TEXT("Fort"), TEXT("Haute") };
	return H[FMath::Clamp(K, 0, Num - 1)];
}

FString FGPMCarSetup::Value(int32 K) const
{
	const int32 X = FMath::Clamp(V[FMath::Clamp(K, 0, Num - 1)], 0, 10);
	switch (K)
	{
	case FrontWing: case RearWing: return FString::Printf(TEXT("%d°"), 4 + 2 * X);
	case BrakeBias: return FString::Printf(TEXT("%d %% av."), 52 + X);
	case BrakePress: return FString::Printf(TEXT("%d %%"), 80 + 2 * X);
	case Diff: return FString::Printf(TEXT("%d %%"), 10 + 8 * X);
	case Ride: return FString::Printf(TEXT("%d mm"), 20 + 3 * X);
	case Camber: return FString::Printf(TEXT("-%.1f°"), 1.5f + 0.2f * X).Replace(TEXT("."), TEXT(","));
	case Press: return FString::Printf(TEXT("%.1f psi"), 20.5f + 0.4f * X).Replace(TEXT("."), TEXT(","));
	default: return FString::Printf(TEXT("%d / 10"), X);
	}
}

// ---------- réglages du week-end ----------

const AGPMGameMode::FSetup AGPMGameMode::Setups[5] = {
	{ TEXT("Appui maxi"), TEXT("Rapide dans les virages, lent en ligne droite."), 10, 10, 3 },
	{ TEXT("Appui élevé"), TEXT("Plus d'adhérence en courbe, un peu moins de vitesse de pointe."), 8, 8, 4 },
	{ TEXT("Équilibré"), TEXT("Le compromis de base."), 5, 5, 5 },
	{ TEXT("Vitesse élevée"), TEXT("Moins d'appui, plus rapide en ligne droite."), 2, 2, 6 },
	{ TEXT("Vitesse maxi"), TEXT("Très rapide en ligne droite, délicat dans les virages."), 0, 0, 7 },
};

// tour simulé (lapSim de tuning.js) : vitesse limite en virage de l'essieu le plus faible, freinages limités par la
// force des freins et l'adhérence de l'avant, accélérations limitées par la motricité, talonnage et vibreurs
float AGPMGameMode::SetupLap(const FGPMCarSetup& S) const
{
	const FGPMTrackData* T = CurTrack();
	if (!T || !Car) return 90.f;
	const int32 N = T->Curvature.Num();
	if (N < 10) return 90.f;
	const FGPMSetupFx F = S.Fx();
	const float G = 9.81f, Mu = AGPMCar::Mu * 0.94f * Car->UpMu, Kd = AGPMCar::Kd * Car->UpKd;
	const float Vt = AGPMCar::VTop * Car->Perf * Car->UpVTop * F.VTop;
	const float MuF = Mu * F.MuF * (1.f - 0.035f * (F.Diff - 0.5f)), MuR = Mu * F.MuR;
	const float BiasD = F.Bias - F.BiasIdeal, BiasK = 1.f - (BiasD > 0.f ? 1.6f * BiasD : -1.f * BiasD);
	auto Bottom = [&F](float Sp) { return 1.f - 0.05f * F.Bottom * FMath::Clamp((Sp - 55.f) / 25.f, 0.f, 1.f); };
	auto Corner = [G](float Cv, float MuA, float KdA) { const float Den = Cv - MuA * KdA; return Den > 0.f ? FMath::Sqrt(MuA * G / Den) : 1e9f; };
	TArray<float> V;
	V.SetNumUninitialized(N);
	for (int32 i = 0; i < N; ++i)
	{
		float Cv = 0.f;
		for (int32 j = -3; j <= 3; ++j) Cv = FMath::Max(Cv, FMath::Abs(T->Curvature[(i + j + N) % N]));
		const float KerbK = Cv > 1.f / 220.f ? 1.f - 0.1f * (1.f - F.Kerb) : 1.f;
		float Sp = FMath::Min(Vt, FMath::Min(Corner(Cv, MuF * KerbK, Kd * F.KdF), Corner(Cv, MuR * KerbK, Kd * F.KdR)));
		const float Bt = Bottom(Sp);
		if (Bt < 1.f) Sp = FMath::Min(Vt, FMath::Min(Corner(Cv, MuF * KerbK * Bt, Kd * F.KdF), Corner(Cv, MuR * KerbK * Bt, Kd * F.KdR)));
		V[i] = Sp;
	}
	for (int32 Pass = 0; Pass < 2; ++Pass)
		for (int32 i = N - 1; i >= 0; --i)
		{
			const float Vn = V[(i + 1) % N];
			const float Force = 0.8f * (AGPMCar::Brake0 + AGPMCar::BrakeK * Vn * Vn) * Car->UpBrake * F.BrakeP;
			const float Cap = 0.8f * 1.1f * MuF * F.MuLong * BiasK * (G + Kd * F.KdF * Vn * Vn) * Bottom(Vn);
			const float Dec = Force <= Cap ? Force : Cap * (1.f - 0.15f * FMath::Min(1.f, (Force - Cap) / Cap));
			V[i] = FMath::Min(V[i], FMath::Sqrt(Vn * Vn + 2.f * Dec * 2.f));
		}
	float Vc = V[0], Tm = 0.f;
	for (int32 Pass = 0; Pass < 2; ++Pass)
		for (int32 i = 0; i < N; ++i)
		{
			float A = AGPMCar::Acc * Car->Perf * Car->UpAcc * F.Acc * FMath::Max(0.f, 1.f - FMath::Pow(Vc / Vt, 2.2f));
			A = FMath::Min(A, MuR * (G + Kd * F.KdR * Vc * Vc) * 0.65f * F.MuLong * (0.92f + 0.16f * F.Diff));
			Vc = FMath::Min(V[(i + 1) % N], FMath::Sqrt(Vc * Vc + 2.f * A * 2.f));
			if (Pass == 1) Tm += 2.f / FMath::Max(Vc, 5.f);
		}
	// répartition de freinage mal réglée : entrées de virage moins bonnes (blocages à l'avant ou arrière qui se dérobe)
	return Tm * (1.f + 0.05f * FMath::Abs(BiasD));
}

float AGPMGameMode::SetupRatio() const
{
	return SetupLap(Setup) / FMath::Max(1.f, SetupLap(FGPMCarSetup()));
}

float AGPMGameMode::SetupBalance(const FGPMCarSetup& S) const
{
	const FGPMSetupFx F = S.Fx();
	const float Sp = 60.f, Kd = AGPMCar::Kd * (Car ? Car->UpKd : 1.f);
	const float Fr = F.MuF * (1.f - 0.035f * (F.Diff - 0.5f)) * (9.81f + Kd * F.KdF * Sp * Sp);
	const float Rr = F.MuR * (9.81f + Kd * F.KdR * Sp * Sp);
	return (Fr - Rr) / (0.5f * (Fr + Rr));
}

// conseil de l'ingénieur : on part de la voiture de base et on règle chaque cran tour à tour (trois passes) pour le
// meilleur tour simulé, en comptant un peu l'usure des pneus pour la course
const FGPMCarSetup& AGPMGameMode::Advice() const
{
	const FGPMTrackData* T = CurTrack();
	uint32 Key = T ? GetTypeHash(T->Id) : 0u;
	if (Car)
		for (const float X : { Car->Perf, Car->UpMu, Car->UpKd, Car->UpVTop, Car->UpAcc, Car->UpBrake })
			Key = HashCombine(Key, GetTypeHash(FMath::RoundToInt(X * 10000.f)));
	Key |= 1u;
	if (Key == AdviceKey) return AdviceCache;
	auto Score = [this](const FGPMCarSetup& S) { return SetupLap(S) * (1.f + 0.02f * (S.Fx().Wear - 1.f)); };
	FGPMCarSetup Best;
	float BestS = Score(Best);
	for (int32 Pass = 0; Pass < 3; ++Pass)
		for (int32 k = 0; k < FGPMCarSetup::Num; ++k)
		{
			const int32 Was = Best.V[k];
			for (int32 x = 0; x <= 10; ++x)
			{
				if (x == Was) continue;
				FGPMCarSetup Try = Best;
				Try.V[k] = x;
				const float Sc = Score(Try);
				if (Sc < BestS - 1e-4f) { BestS = Sc; Best = Try; }
			}
		}
	AdviceCache = Best;
	AdviceKey = Key;
	return AdviceCache;
}

void AGPMGameMode::EnsureSetup()
{
	if (SetupK >= 0) return;
	Setup = Advice();
	SetupK = 0;
	SetupRow = FMath::Clamp(SetupRow, 0, FGPMCarSetup::Num - 1);
}

int32 AGPMGameMode::PresetOf(const FGPMCarSetup& S) const
{
	for (int32 k = 0; k < 5; ++k)
		if (S.V[FGPMCarSetup::FrontWing] == Setups[k].FW && S.V[FGPMCarSetup::RearWing] == Setups[k].RW && S.V[FGPMCarSetup::Gears] == Setups[k].Gears) return k;
	return -1;
}

void AGPMGameMode::ApplyPreset(int32 K)
{
	const FSetup& P = Setups[FMath::Clamp(K, 0, 4)];
	Setup.V[FGPMCarSetup::FrontWing] = P.FW;
	Setup.V[FGPMCarSetup::RearWing] = P.RW;
	Setup.V[FGPMCarSetup::Gears] = P.Gears;
	SetupK = 0;
}

// réglages du week-end (parc fermé) appliqués à la physique de la voiture du joueur
void AGPMGameMode::ApplySetup()
{
	if (!Car) return;
	EnsureSetup();
	Car->SetupFx = Setup.Fx();
}

// ---------- options des pilotes IA ----------

int32 AGPMCar::AIAggro = 1;
bool AGPMCar::bAIErrors = true;

void AGPMCar::LoadAIOptions()
{
	if (!GConfig) return;
	GConfig->GetInt(TEXT("GPM"), TEXT("AIAggro"), AIAggro, GGameUserSettingsIni);
	GConfig->GetBool(TEXT("GPM"), TEXT("AIErrors"), bAIErrors, GGameUserSettingsIni);
	AIAggro = FMath::Clamp(AIAggro, 0, 2);
}

void AGPMCar::SetAIOptions(int32 InAggro, bool bInErrors)
{
	AIAggro = FMath::Clamp(InAggro, 0, 2);
	bAIErrors = bInErrors;
	if (!GConfig) return;
	GConfig->SetInt(TEXT("GPM"), TEXT("AIAggro"), AIAggro, GGameUserSettingsIni);
	GConfig->SetBool(TEXT("GPM"), TEXT("AIErrors"), bAIErrors, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}
