// Week-end de course comme dans le jeu web (main.js, tuning.js, strategy.js) : réglages de la voiture (5 préréglages,
// tour simulé, conseil de l'ingénieur, parc fermé), qualifications jouées ou simulées, stratégie de course
// (3 propositions, relais modifiables), départ ou course simulée, Grand Prix entièrement simulé.
#include "GPMGameMode.h"
#include "GPMCar.h"
#include "GPMSeason.h"
#include "GPMTrack.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

const AGPMGameMode::FSetup AGPMGameMode::Setups[5] = {
	{ TEXT("Appui maxi"), TEXT("Rapide dans les virages, lent en ligne droite."), 0.0375f, -0.04f },
	{ TEXT("Appui élevé"), TEXT("Plus d'adhérence en courbe, un peu moins de vitesse de pointe."), 0.025f, -0.02f },
	{ TEXT("Équilibré"), TEXT("Le compromis de base."), 0.f, 0.f },
	{ TEXT("Vitesse élevée"), TEXT("Moins d'appui, plus rapide en ligne droite."), -0.025f, 0.02f },
	{ TEXT("Vitesse maxi"), TEXT("Très rapide en ligne droite, délicat dans les virages."), -0.05f, 0.03f },
};

// nombre de tours selon le format (Sprint ~11 min, Long ~22 min), comme lapsFor de strategy.js
int32 AGPMGameMode::LapsFor() const
{
	const FGPMTrackData* T = CurTrack();
	if (!T || !Car) return 5;
	const FString ToursOpt = UGameplayStatics::ParseOption(OptionsString, TEXT("tours"));
	if (!ToursOpt.IsEmpty()) return FMath::Max(1, FCString::Atoi(*ToursOpt));
	if (!bCareer && SoloLaps > 0) return SoloLaps;
	float Est = 0.f;
	for (const float Spd : T->AISpeed) Est += 2.f / FMath::Max(5.f, Spd);
	Est *= 1.02f / FMath::Max(0.9f, float(Car->Perf));
	return FMath::Clamp(FMath::RoundToInt((bLongRace ? 22.f : 11.f) * 60.f / (Est * 1.12f)), 3, 60);
}

// tour de référence du joueur (pilote à 0,95, voiture et pièces de l'usine)
float AGPMGameMode::BaseLap() const
{
	const FGPMTrackData* T = CurTrack();
	if (!T || !Car) return 90.f;
	return LapTimeFor(T, 0.95f, Car->Perf * Car->UpVTop);
}

// tour simulé avec un réglage (lapSim de tuning.js) : vitesse limite en virage, freinages, accélérations
float AGPMGameMode::SetupLap(int32 K) const
{
	const FGPMTrackData* T = CurTrack();
	if (!T || !Car) return 90.f;
	const int32 N = T->Curvature.Num();
	if (N < 10) return 90.f;
	const FSetup& St = Setups[FMath::Clamp(K, 0, 4)];
	const float Grav = 9.81f, Mu = AGPMCar::Mu * 0.94f * Car->UpMu, Kd = AGPMCar::Kd * Car->UpKd * (1.f + St.Kd);
	const float Vt = AGPMCar::VTop * Car->Perf * Car->UpVTop * (1.f + St.VTop);
	TArray<float> V;
	V.SetNumUninitialized(N);
	for (int32 i = 0; i < N; ++i)
	{
		float Cv = 0.f;
		for (int32 j = -3; j <= 3; ++j) Cv = FMath::Max(Cv, FMath::Abs(T->Curvature[(i + j + N) % N]));
		const float Den = Cv - Mu * Kd;
		V[i] = Den > 0.f ? FMath::Min(Vt, FMath::Sqrt(Mu * Grav / Den)) : Vt;
	}
	for (int32 Pass = 0; Pass < 2; ++Pass)
		for (int32 i = N - 1; i >= 0; --i)
		{
			const float Vn = V[(i + 1) % N];
			V[i] = FMath::Min(V[i], FMath::Sqrt(Vn * Vn + 2.f * 0.8f * (AGPMCar::Brake0 + AGPMCar::BrakeK * Vn * Vn) * Car->UpBrake * 2.f));
		}
	float Vc = V[0], Tm = 0.f;
	for (int32 Pass = 0; Pass < 2; ++Pass)
		for (int32 i = 0; i < N; ++i)
		{
			const float A = AGPMCar::Acc * Car->Perf * Car->UpAcc * FMath::Max(0.f, 1.f - FMath::Pow(Vc / Vt, 2.2f));
			Vc = FMath::Min(V[(i + 1) % N], FMath::Sqrt(Vc * Vc + 2.f * A * 2.f));
			if (Pass == 1) Tm += 2.f / FMath::Max(Vc, 5.f);
		}
	return Tm;
}

int32 AGPMGameMode::BestSetup() const
{
	int32 Best = 2;
	float BestT = 1e9f;
	for (int32 k = 0; k < 5; ++k)
	{
		const float Tk = SetupLap(k);
		if (Tk < BestT) { BestT = Tk; Best = k; }
	}
	return Best;
}

// voiture sans réglage : pièces de l'usine et bonus en carrière, voiture de base en course solo
void AGPMGameMode::BaseUps()
{
	if (!Car) return;
	if (bCareer) { ApplyCareer(); return; }
	Car->UpVTop = Car->UpAcc = Car->UpMu = Car->UpBrake = Car->UpKd = Car->UpSteer = Car->UpFuel = 1.f;
}

// réglage du week-end (parc fermé) : appui et vitesse de pointe
void AGPMGameMode::ApplySetup()
{
	if (!Car) return;
	if (SetupK < 0) SetupK = BestSetup();
	SetupK = FMath::Clamp(SetupK, 0, 4);
	Car->UpKd *= 1.f + Setups[SetupK].Kd;
	Car->UpVTop *= 1.f + Setups[SetupK].VTop;
}

void AGPMGameMode::Restart()
{
	if (Phase == EPhase::Quali) StartQuali();
	else StartRace();
}

void AGPMGameMode::OpenTuning()
{
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	if (bCareer && Season && Track && Track->CircuitId != Season->CurrentCircuit()) { Pending = 4; LoadCircuit(Season->CurrentCircuit()); return; }
	BaseUps();
	Phase = EPhase::Tuning;
}

void AGPMGameMode::PlayQuali(int32 K)
{
	SetupK = FMath::Clamp(K, 0, 4);
	EndCine();
	StartQuali();
}

// qualifications simulées : temps du joueur selon le réglage choisi (simLap de main.js), mêmes temps des IA
void AGPMGameMode::SimQuali(int32 K)
{
	const FGPMTrackData* T = CurTrack();
	if (!T || !Car) return;
	SetupK = FMath::Clamp(K, 0, 4);
	BaseUps();
	const float Ratio = SetupLap(SetupK) / FMath::Max(1.f, SetupLap(2));
	ResetWeather();   // qualifications : météo du départ
	const float Mine = BaseLap() * (1.f + 0.006f * FMath::FRand()) * 0.985f * Ratio * WetLapK();
	for (AGPMCar* C : Cars)
		if (C != Car && !QTimes.Contains(C)) QTimes.Add(C, LapTimeFor(T, C->Skill, C->Perf) * (1.f + 0.006f * (FMath::FRand() - 0.3f)) * 0.985f * WetLapK());
	if (float* Prev = QTimes.Find(Car)) *Prev = FMath::Min(*Prev, Mine);
	else QTimes.Add(Car, Mine);
	QTimes.GenerateKeyArray(QOrder);
	QOrder.Sort([this](const AGPMCar& A, const AGPMCar& B) { return QTimes.FindRef(const_cast<AGPMCar*>(&A)) < QTimes.FindRef(const_cast<AGPMCar*>(&B)); });
	Order = QOrder;
	bGridReady = true;
	Phase = EPhase::QualiDone;
}

void AGPMGameMode::OpenPrerace()
{
	EndCine();
	StageGrid();
	BaseUps();
	ApplySetup();
	Plans = MakePlans();
	int32 Sum = 0;
	for (const TPair<TCHAR, int32>& S : Stints) Sum += S.Value;
	if ((Stints.Num() == 0 || Sum != LapsFor()) && Plans.Num()) Stints = Plans[0].Stints;
	Phase = EPhase::Prerace;
}

void AGPMGameMode::Go(bool bSim)
{
	if (Stints.Num() == 0)
	{
		Plans = MakePlans();
		if (Plans.Num()) Stints = Plans[0].Stints;
	}
	EndCine();
	if (bSim) SimRace();
	else StartRace();
}

// Grand Prix simulé d'un coup (bouton de l'écran de saison) : qualifications puis course avec la stratégie conseillée
void AGPMGameMode::SimGrandPrix()
{
	if (!bGridReady) { BaseUps(); SimQuali(SetupK >= 0 ? SetupK : BestSetup()); }
	BaseUps();
	ApplySetup();
	Plans = MakePlans();
	if (Plans.Num()) Stints = Plans[0].Stints;
	EndCine();
	SimRace();
}

// course simulée : temps de chaque pilote d'après son rythme, sa stratégie et sa place sur la grille
void AGPMGameMode::SimRace()
{
	const FGPMTrackData* T = CurTrack();
	if (!T || !Car) return;
	BaseUps();
	const int32 K = SetupK >= 0 ? SetupK : BestSetup();
	const float MyLap = BaseLap() * SetupLap(K) / FMath::Max(1.f, SetupLap(2)) * RaceWetK();
	const TArray<FPlan> AIPlans = MakePlans();
	bNoIntro = true;
	StartRace();
	bNoIntro = false;
	for (int32 k = 0; k < Order.Num(); ++k)
	{
		AGPMCar* C = Order[k];
		const float Lap = C == Car ? MyLap : LapTimeFor(T, C->Skill, C->Perf) * RaceWetK();
		TArray<TPair<TCHAR, int32>> St;
		if (C == Car && Stints.Num()) St = Stints;
		else if (AIPlans.Num()) St = AIPlans[FMath::RandRange(0, AIPlans.Num() - 1)].Stints;
		else St.Emplace(TCHAR('M'), TotalLaps);
		const float Tot = PlanTime(St, Lap) * (1.f + 0.004f * (FMath::FRand() * 2.f - 1.f)) + k * 0.35f + FMath::FRand() * 1.2f;
		C->bFinished = true;
		C->FinishTime = Tot;
		C->Lap = TotalLaps + 1;
		C->Stops = FMath::Max(0, St.Num() - 1);
		C->Tyre = St.Last().Key;
		C->BestLap = Lap * (1.002f + 0.01f * FMath::FRand());
	}
	Order.Sort([](const AGPMCar& A, const AGPMCar& B) { return A.FinishTime < B.FinishTime; });
	Clock = Order.Num() ? Order.Last()->FinishTime : 0.f;
	Phase = EPhase::Done;
	RecordResult();
}

// déroulé d'une stratégie (planLaps de strategy.js) : vie des pneus tour par tour, temps perdu, arrêts
float AGPMGameMode::PlanTime(const TArray<TPair<TCHAR, int32>>& InStints, float LapTime, float* OutMaxWear) const
{
	const FGPMTrackData* T = CurTrack();
	const float Loss = T ? T->PitLoss : 24.f;
	float Tm = 0.f, Worst = 0.f;
	for (int32 i = 0; i < InStints.Num(); ++i)
	{
		const TCHAR Ty = InStints[i].Key;
		const int32 L = InStints[i].Value;
		// pneus pluie : usure ×(1 + 2,4 × piste sèche) ; course sous la pluie : piste à 90 %
		const float PlanWet = bRainStart ? 0.9f : 0.f;
		const float W = AGPMCar::TyreWpl(Ty) * 1.05f * (Ty == TCHAR('W') ? 1.f + 2.4f * (1.f - PlanWet) : 1.f);
		if (i) Tm += Loss;
		for (int32 k = 0; k < L; ++k)
		{
			const float R0 = 1.f - k * W, R1 = 1.f - (k + 1) * W;
			float G = AGPMCar::TyreGrip(Ty) * AGPMCar::LifeGrip(FMath::Max(0.f, (R0 + R1) * 0.5f));
			if (R1 < 0.f) G *= 0.8f;
			Tm += LapTime + LapTime * 0.45f * (1.f / FMath::Sqrt(G) - 1.f);
		}
		Worst = FMath::Max(Worst, L * W);
	}
	if (OutMaxWear) *OutMaxWear = Worst;
	return Tm;
}

// 3 stratégies proposées (makePlans) : toutes les suites de 1 à 3 gommes, meilleure répartition des tours,
// la plus rapide, puis la meilleure avec un autre nombre d'arrêts, puis une autre variante
TArray<AGPMGameMode::FPlan> AGPMGameMode::MakePlans() const
{
	const int32 Laps = LapsFor();
	const float LapT = BaseLap();
	// gommes possibles : sèches, ou pneus pluie seulement si la course part sous la pluie
	TArray<TCHAR> Set;
	if (bRainStart) Set.Add(TCHAR('W'));
	else { Set.Add(TCHAR('S')); Set.Add(TCHAR('M')); Set.Add(TCHAR('H')); }
	const int32 NSet = Set.Num();
	TMap<int32, float> Memo;
	auto Cost = [this, &Memo, &Set, LapT](int32 TyIdx, int32 Num) -> float
	{
		const int32 Key = TyIdx * 1000 + Num;
		if (const float* Found = Memo.Find(Key)) return *Found;
		TArray<TPair<TCHAR, int32>> One;
		One.Emplace(Set[TyIdx], Num);
		float Wear = 0.f;
		float Val = PlanTime(One, LapT, &Wear);
		if (Wear > 0.6f) Val = 1e9f;   // le muret fait rentrer avant la chute (40 %)
		Memo.Add(Key, Val);
		return Val;
	};
	TArray<TArray<int32>> Seqs;
	for (int32 a = 0; a < NSet; ++a)
	{
		Seqs.Add(TArray<int32>{ a });
		for (int32 b = a; b < NSet; ++b)
		{
			Seqs.Add(TArray<int32>{ a, b });
			for (int32 c = b; c < NSet; ++c) Seqs.Add(TArray<int32>{ a, b, c });
		}
	}
	TArray<FPlan> Cands;
	for (const TArray<int32>& Seq : Seqs)
	{
		const int32 M = Seq.Num();
		if (M > Laps) continue;
		TArray<float> Prev;
		Prev.Init(1e18f, Laps + 1);
		Prev[0] = 0.f;
		TArray<TArray<int32>> Choice;
		Choice.SetNum(M);
		for (int32 i = 0; i < M; ++i)
		{
			TArray<float> Nx;
			Nx.Init(1e18f, Laps + 1);
			Choice[i].Init(0, Laps + 1);
			for (int32 Done = 0; Done < Laps; ++Done)
			{
				if (Prev[Done] >= 1e17f) continue;
				for (int32 n = 1; Done + n <= Laps; ++n)
				{
					if (i == M - 1 && Done + n != Laps) continue;
					const float Cc = Prev[Done] + Cost(Seq[i], n);
					if (Cc < Nx[Done + n]) { Nx[Done + n] = Cc; Choice[i][Done + n] = n; }
				}
			}
			Prev = Nx;
		}
		if (Prev[Laps] >= 1e8f) continue;
		FPlan P;
		int32 Left = Laps;
		for (int32 i = M - 1; i >= 0; --i)
		{
			const int32 n = Choice[i][Left];
			P.Stints.Insert(TPair<TCHAR, int32>(Set[Seq[i]], n), 0);
			Left -= n;
		}
		P.Time = PlanTime(P.Stints, LapT, &P.MaxWear);
		Cands.Add(P);
	}
	Cands.Sort([](const FPlan& A, const FPlan& B) { return A.Time < B.Time; });
	TArray<FPlan> Out;
	if (Cands.Num() == 0)
	{
		FPlan P;
		P.Stints.Emplace(bRainStart ? TCHAR('W') : TCHAR('H'), Laps);
		P.Time = PlanTime(P.Stints, LapT, &P.MaxWear);
		Out.Add(P);
	}
	else
	{
		Out.Add(Cands[0]);
		for (const FPlan& Cd : Cands)
			if (Cd.Stints.Num() != Cands[0].Stints.Num()) { Out.Add(Cd); break; }
		for (const FPlan& Cd : Cands)
		{
			if (Out.Num() >= 3) break;
			bool bSame = false;
			for (const FPlan& O : Out) bSame |= O.Stints.Num() == Cd.Stints.Num() && O.Stints[0].Key == Cd.Stints[0].Key;
			if (!bSame) Out.Add(Cd);
		}
		for (const FPlan& Cd : Cands)
		{
			if (Out.Num() >= 3) break;
			bool bSame = false;
			for (const FPlan& O : Out) bSame |= O.Stints == Cd.Stints;
			if (!bSame) Out.Add(Cd);
		}
	}
	Out.Sort([](const FPlan& A, const FPlan& B) { return A.Time < B.Time; });
	for (int32 k = 0; k < Out.Num(); ++k)
	{
		FPlan& P = Out[k];
		P.Name = k == 0 ? FString(TEXT("Recommandée")) : FString::Printf(TEXT("Alternative %d"), k);
		bool bSoft = false;
		for (const TPair<TCHAR, int32>& S : P.Stints) bSoft |= S.Key == TCHAR('S');
		P.Desc = bRainStart ? (P.Stints.Num() == 1 ? TEXT("Pneus pluie jusqu'au bout. Le muret t'appelle si la piste sèche.") : TEXT("Pneus pluie neufs à mi-course. Le muret t'appelle si la piste sèche."))
			: P.Stints.Num() == 1 ? TEXT("Un seul relais : on gère les pneus jusqu'au bout.")
			: bSoft ? TEXT("Tendres rapides mais fragiles : plus d'attaque, plus d'arrêts.") : TEXT("Gommes durables : relais longs et peu de risques.");
	}
	return Out;
}
