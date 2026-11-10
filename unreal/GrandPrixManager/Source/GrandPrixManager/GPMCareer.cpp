// Carrière : création de l'écurie (career.js), direction (events.js), sponsors (sponsors.js), mercato et négociations
// (market.js, nego.js). La presse, les flash info et les histoires sont dans GPMLife.cpp.
#include "GPMCareer.h"
#include "GrandPrixManager.h"
#include "GPMDataSubsystem.h"
#include "GPMSeason.h"
#include "GPMFactory.h"
#include "Dom/JsonValue.h"

namespace
{
	FString JStr(const TSharedPtr<FJsonObject>& O, const TCHAR* K) { FString Out; if (O.IsValid()) O->TryGetStringField(K, Out); return Out; }
	double JNum(const TSharedPtr<FJsonObject>& O, const TCHAR* K, double Def = 0.0) { double Out = Def; if (O.IsValid()) O->TryGetNumberField(K, Out); return Out; }
	float R1(float X) { return FMath::RoundToFloat(X * 10.f) / 10.f; }
	FLinearColor FromHex(int64 Hex) { return FLinearColor::FromSRGBColor(FColor(uint8((Hex >> 16) & 255), uint8((Hex >> 8) & 255), uint8(Hex & 255))); }
	// personnalités des pilotes (PERS de nego.js) : gourmandise, patience
	struct FPers { const TCHAR* Id; const TCHAR* Name; float Greed; int32 Pat; };
	const FPers PersList[] = { { TEXT("ambitieux"), TEXT("Ambitieux"), 1.f, 3 }, { TEXT("mercenaire"), TEXT("Mercenaire"), 1.15f, 2 },
		{ TEXT("fidele"), TEXT("Fidèle"), 0.92f, 4 }, { TEXT("jeune"), TEXT("Jeune loup"), 0.95f, 3 } };
	const FPers& PersOf(const FString& Id) { for (const FPers& P : PersList) if (Id == P.Id) return P; return PersList[0]; }
	const TCHAR* RoleName(const FString& R) { return R == TEXT("n1") ? TEXT("N°1") : R == TEXT("n2") ? TEXT("N°2") : TEXT("Égalité"); }
	const float MidMult = 2.5f;   // indemnités de transfert pendant la saison (MID_MULT de market.js)
	// primes proposées à l'équipier : par podium et par victoire, selon le salaire demandé
	float PodBonus(float Demand) { return R1(FMath::Max(0.2f, Demand * 0.12f)); }
	float WinBonus(float Demand) { return R1(FMath::Max(0.4f, Demand * 0.25f)); }
	// sponsors ponctuels des missions (makeMission d'events.js)
	const TCHAR* const MissionSponsors[] = { TEXT("Lumora"), TEXT("Kestrel Tech"), TEXT("Novacel"), TEXT("Brisa Mobile"), TEXT("Atlasbank"), TEXT("Orvane"),
		TEXT("Pixalto"), TEXT("Verdania") };
}

void UGPMCareerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UGPMDataSubsystem>();
	Collection.InitializeDependency<UGPMSeasonSubsystem>();
	Super::Initialize(Collection);
	if (UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>()) BaseTeams = Data->Teams;
	LoadOptions();
	ApplyIdentity();
}

UGPMSaveGame* UGPMCareerSubsystem::Save() const
{
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	return Season ? Season->State() : nullptr;
}

bool UGPMCareerSubsystem::IsCreated() const { const UGPMSaveGame* G = Save(); return G && G->bCreated; }

void UGPMCareerSubsystem::LoadOptions()
{
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	if (!Data) return;
	auto Arr = [Data](const TCHAR* File, const TCHAR* Key)
	{
		TArray<TSharedPtr<FJsonObject>> Out;
		if (const TSharedPtr<FJsonValue> V = Data->GetRaw(File, Key))
			for (const TSharedPtr<FJsonValue>& E : V->AsArray()) if (E->Type == EJson::Object) Out.Add(E->AsObject());
		return Out;
	};
	for (const TSharedPtr<FJsonObject>& O : Arr(TEXT("career"), TEXT("TIERS")))
		Tiers.Add({ JStr(O, TEXT("id")), JStr(O, TEXT("name")), JStr(O, TEXT("desc")), float(JNum(O, TEXT("budget"), 70)), float(JNum(O, TEXT("perf"), 0.96)),
			int32(JNum(O, TEXT("rep"), 50)), int32(JNum(O, TEXT("moral"), 60)) });
	for (const TSharedPtr<FJsonObject>& O : Arr(TEXT("config_motoristes"), TEXT("MOTORISTES")))
		Engines.Add({ JStr(O, TEXT("id")), JStr(O, TEXT("name")), int32(JNum(O, TEXT("perf"), 85)), int32(JNum(O, TEXT("fiab"), 80)), int32(JNum(O, TEXT("conso"), 100)), float(JNum(O, TEXT("price"), 20)) });
	for (const TSharedPtr<FJsonObject>& O : Arr(TEXT("config_couleurs"), TEXT("COULEURS")))
		Colors.Add({ JStr(O, TEXT("id")), JStr(O, TEXT("name")), FromHex(int64(JNum(O, TEXT("hex")))) });
	for (const TSharedPtr<FJsonObject>& O : Arr(TEXT("config_sponsors"), TEXT("SPONSORS")))
		if (JStr(O, TEXT("kind")) == TEXT("main"))
			Sponsors.Add({ JStr(O, TEXT("id")), JStr(O, TEXT("name")), JStr(O, TEXT("sector")), JStr(O, TEXT("goal")), float(JNum(O, TEXT("year"), 10)),
				float(JNum(O, TEXT("sign"), 2)), float(JNum(O, TEXT("bonus"), 0.5)), int32(JNum(O, TEXT("rep"))) });
	for (const TSharedPtr<FJsonObject>& O : Arr(TEXT("career"), TEXT("MARKET")))
	{
		FGPMMarketDriver D;
		D.Name = JStr(O, TEXT("name")); D.From = JStr(O, TEXT("from")); D.Age = int32(JNum(O, TEXT("age"), 25));
		D.Spd = JNum(O, TEXT("spd"), 75); D.Strat = JNum(O, TEXT("strat"), 70); D.Tyre = JNum(O, TEXT("tyre"), 70); D.Pot = JNum(O, TEXT("pot"), 60);
		D.Sponsor = JNum(O, TEXT("sponsor"));
		Market.Add(D);
	}
	// noms inventés proposés à la création (le jeu web laisse taper le nom ; ici on choisit dans une liste)
	TeamNames = { TEXT("Écurie Martel"), TEXT("Ardent GP"), TEXT("Nova Racing"), TEXT("Fauve Motorsport"), TEXT("Orage Racing"), TEXT("Brume GP"), TEXT("Comète F"), TEXT("Lys Bleu Racing") };
	PilotNames = { TEXT("Toi"), TEXT("P.-L. Martel"), TEXT("A. Delmas"), TEXT("R. Castel"), TEXT("J. Morvan"), TEXT("N. Ferrat") };
	if (const TSharedPtr<FJsonValue> V = Data->GetRaw(TEXT("life"), TEXT("OUTLETS")))
		for (const TSharedPtr<FJsonValue>& E : V->AsArray())
		{
			const TArray<TSharedPtr<FJsonValue>>& A = E->AsArray();
			if (A.Num() < 3) continue;
			Outlets.Add(A[0]->AsString() + TEXT(" · ") + A[1]->AsString());
			OutletColors.Add(FLinearColor(FColor::FromHex(A[2]->AsString())));
		}
	Life = MakeShared<FJsonObject>();
	for (const TCHAR* K : { TEXT("PRESS"), TEXT("EVENTS"), TEXT("STORIES") })
		if (const TSharedPtr<FJsonValue> V = Data->GetRaw(TEXT("life"), K)) Life->SetField(K, V);
	UE_LOG(LogGPM, Log, TEXT("Carrière : %d niveaux, %d motoristes, %d couleurs, %d sponsors, %d pilotes"), Tiers.Num(), Engines.Num(), Colors.Num(), Sponsors.Num(), Market.Num());
}

float UGPMCareerSubsystem::Salary(const FGPMMarketDriver& D)
{
	if (D.Sponsor > 0.f) return -D.Sponsor;
	return R1(FMath::Pow(D.Rating() / 100.f, 6.f) * 40.f);
}

FString UGPMCareerSubsystem::Display(const FString& Driver) const
{
	const UGPMSaveGame* G = Save();
	return Driver == TEXT("Toi") && G && !G->PlayerName.IsEmpty() ? G->PlayerName : Driver;
}

void UGPMCareerSubsystem::ApplyIdentity()
{
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	const UGPMSaveGame* G = Save();
	if (!Data || BaseTeams.Num() == 0) return;
	Data->Teams = BaseTeams;
	if (!G || !G->bCreated) return;
	FGPMTeam& Me = Data->Teams[0];
	Me.Name = G->TeamName.IsEmpty() ? Me.Name : G->TeamName;
	if (Colors.IsValidIndex(G->Col1)) Me.Color = Colors[G->Col1].Color;
	if (Colors.IsValidIndex(G->Col2)) Me.Accent = Colors[G->Col2].Color;
	Me.Perf = G->BasePerf;
	if (Me.Drivers.Num() > 1 && !G->MateName.IsEmpty())
	{
		Me.Drivers[1].Name = G->MateName;
		Me.Drivers[1].Skill = MateSkill(G->MateSpd) * (G->MateRole == TEXT("n1") ? 1.003f : G->MateRole == TEXT("n2") ? 0.997f : 1.f);
	}
	for (int32 t = 1; t < Data->Teams.Num(); ++t)
		for (int32 d = 0; d < Data->Teams[t].Drivers.Num(); ++d)
			if (const FString* Who = G->Seats.Find(FString::Printf(TEXT("%s|%d"), *BaseTeams[t].Name, d)))
			{
				Data->Teams[t].Drivers[d].Name = *Who;
				if (const float* Sk = G->Skill.Find(*Who)) Data->Teams[t].Drivers[d].Skill = *Sk;
			}
}

// ---------- création de l'écurie ----------
float UGPMCareerSubsystem::PickBudget() const
{
	float B = Tiers.IsValidIndex(Pick.Tier) ? Tiers[Pick.Tier].Budget : 70.f;
	if (Engines.IsValidIndex(Pick.Engine)) B -= Engines[Pick.Engine].Price;
	if (Sponsors.IsValidIndex(Pick.Sponsor)) B += Sponsors[Pick.Sponsor].Sign;
	if (Market.IsValidIndex(Pick.Mate)) B -= Salary(Market[Pick.Mate]);
	return R1(B);
}

bool UGPMCareerSubsystem::PickValid(FString& Why) const
{
	if (Pick.Col1 == Pick.Col2) { Why = TEXT("Choisis deux couleurs différentes."); return false; }
	if (Sponsors.IsValidIndex(Pick.Sponsor) && Tiers.IsValidIndex(Pick.Tier) && Sponsors[Pick.Sponsor].Rep > Tiers[Pick.Tier].Rep)
	{ Why = TEXT("Ce sponsor veut une écurie plus réputée."); return false; }
	if (PickBudget() < 0.f) { Why = TEXT("Budget insuffisant : choisis un motoriste ou un équipier moins cher."); return false; }
	return true;
}

void UGPMCareerSubsystem::CreateCareer()
{
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	if (!Season) return;
	Season->Reset();
	UGPMSaveGame* G = Save();
	if (!G || Tiers.Num() == 0 || Engines.Num() == 0 || Market.Num() == 0 || TeamNames.Num() == 0 || PilotNames.Num() == 0) return;
	const FGPMTierOpt& T = Tiers[FMath::Clamp(Pick.Tier, 0, Tiers.Num() - 1)];
	const FGPMEngineOpt& E = Engines[FMath::Clamp(Pick.Engine, 0, Engines.Num() - 1)];
	const FGPMMarketDriver& M = Market[FMath::Clamp(Pick.Mate, 0, Market.Num() - 1)];
	G->bCreated = true;
	G->TeamName = TeamNames[FMath::Clamp(Pick.Name, 0, TeamNames.Num() - 1)];
	G->PlayerName = PilotNames[FMath::Clamp(Pick.Pilot, 0, PilotNames.Num() - 1)];
	G->Tier = T.Id; G->Engine = E.Id; G->Col1 = Pick.Col1; G->Col2 = Pick.Col2;
	G->Sponsor = Sponsors.IsValidIndex(Pick.Sponsor) ? Sponsors[Pick.Sponsor].Id : FString();
	G->BasePerf = T.Perf * (1.f + (E.Perf - 85) * 0.0012f);   // enginePerf de career.js
	G->Fuel = E.Conso / 100.f;
	G->Budget = PickBudget(); G->Rep = T.Rep; G->Moral = T.Moral; G->Conf = 60;
	G->MateName = M.Name; G->MateSpd = M.Spd; G->MateStrat = M.Strat; G->MateTyre = M.Tyre; G->MatePot = M.Pot;
	G->MateSal = FMath::Max(0.f, Salary(M)); G->MateYears = 2; G->MateRole = TEXT("eq"); G->MateSince = G->Year;
	for (const FGPMMarketDriver& D : Market) if (D.Name != M.Name) G->Free.Add(D.Name);
	// bâtiments de départ selon le niveau (TIERS de career.js : milieu de peloton et top team ont déjà une usine équipée)
	if (UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>())
		if (const TSharedPtr<FJsonValue> TierList = Data->GetRaw(TEXT("career"), TEXT("TIERS")))
			for (const TSharedPtr<FJsonValue>& TierV : TierList->AsArray())
			{
				TSharedPtr<FJsonObject> TierO;
				if (TierV.IsValid() && TierV->Type == EJson::Object) TierO = TierV->AsObject();
				const TSharedPtr<FJsonObject>* Inf = nullptr;
				if (TierO.IsValid() && JStr(TierO, TEXT("id")) == T.Id && TierO->TryGetObjectField(TEXT("infra"), Inf))
					for (const TPair<FString, TSharedPtr<FJsonValue>>& Lv : (*Inf)->Values) G->InfraLv.Add(Lv.Key, FMath::Clamp(int32(Lv.Value->AsNumber()), 0, 3));
			}
	ApplyIdentity();
	Season->NewSeason(true);   // classements avec le nom de l'écurie (garde l'écurie qu'on vient de créer)
	if (UGPMSaveGame* G2 = Save()) G2->Year = 2027;
	Season->Save();
	if (UGPMFactorySubsystem* Factory = GetGameInstance()->GetSubsystem<UGPMFactorySubsystem>())
	{
		Factory->EnsureStaff();
		Factory->EnsurePerfStart();
	}
	UE_LOG(LogGPM, Log, TEXT("Nouvelle écurie : %s (%s, moteur %s, équipier %s)"), *G->TeamName, *T.Name, *E.Name, *M.Name);
}

// ---------- direction, sponsors, contrats ponctuels ----------
int32 UGPMCareerSubsystem::TeamPos() const
{
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	if (!Season) return 5;
	const TArray<FGPMStanding> Tm = Season->Teams();
	for (int32 k = 0; k < Tm.Num(); ++k) if (Tm[k].bPlayer) return k + 1;
	return 5;
}

TArray<TPair<FString, bool>> UGPMCareerSubsystem::Objectives() const
{
	TArray<TPair<FString, bool>> Out;
	const UGPMSaveGame* G = Save();
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	if (!G || !Season) return Out;
	int32 Pts = 0, Pod = 0, Win = 0;
	for (const FGPMStanding& Tm : Season->Teams()) if (Tm.bPlayer) Pts = Tm.Pts;
	for (const FGPMRaceResult& R : G->Results) { Pod += R.Pos >= 1 && R.Pos <= 3; Win += R.Pos == 1; }
	const int32 CPos = TeamPos();
	const bool bAny = G->Results.Num() > 0;
	// OBJ d'events.js : nouvelle écurie top 8 + 25 pts ; milieu top 5 + 1 podium ; top team top 2 + 3 victoires
	if (G->Tier == TEXT("new"))
	{
		Out.Emplace(FString::Printf(TEXT("Top 8 des écuries (P%d)"), CPos), bAny && CPos <= 8);
		Out.Emplace(FString::Printf(TEXT("25 points pour l'écurie (%d/25)"), Pts), Pts >= 25);
	}
	else if (G->Tier == TEXT("top"))
	{
		Out.Emplace(FString::Printf(TEXT("Top 2 des écuries (P%d)"), CPos), bAny && CPos <= 2);
		Out.Emplace(FString::Printf(TEXT("3 victoires (%d/3)"), Win), Win >= 3);
	}
	else
	{
		Out.Emplace(FString::Printf(TEXT("Top 5 des écuries (P%d)"), CPos), bAny && CPos <= 5);
		Out.Emplace(FString::Printf(TEXT("1 podium (%d/1)"), Pod), Pod >= 1);
	}
	return Out;
}

bool UGPMCareerSubsystem::IsFired() const { const UGPMSaveGame* G = Save(); return G && G->bCreated && (G->bFired || G->Conf <= 5); }

float UGPMCareerSubsystem::Boost(const TCHAR* Key) const { const UGPMSaveGame* G = Save(); return G ? G->Boost.FindRef(Key) : 0.f; }
void UGPMCareerSubsystem::ClearBoost() { if (UGPMSaveGame* G = Save()) G->Boost.Reset(); }

void UGPMCareerSubsystem::News(const FString& Msg) { if (UGPMSaveGame* G = Save()) G->News.Add(Msg); }

// mission d'un sponsor ponctuel avant chaque GP (makeMission d'events.js), à accepter dans l'écran de saison
void UGPMCareerSubsystem::EnsureMission()
{
	UGPMSaveGame* G = Save();
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	if (!G || !G->bCreated || !Season || Season->IsOver() || G->MisRound == G->Round) return;
	const int32 Exp = G->Tier == TEXT("new") ? 14 : G->Tier == TEXT("top") ? 4 : 9;
	const TArray<FGPMStanding> Ds = Season->Drivers();
	int32 Me = 0;
	for (int32 k = 0; k < Ds.Num(); ++k) if (Ds[k].bPlayer) { Me = k; break; }
	const int32 RivalIdx = Me > 0 ? Me - 1 : 1;
	const FString Rival = Ds.IsValidIndex(RivalIdx) ? Ds[RivalIdx].Name : FString();
	G->MisRound = G->Round;
	G->bMisAcc = false;
	G->MisWho.Reset();
	G->MisV = 0;
	G->MisSponsor = MissionSponsors[FMath::RandRange(0, int32(UE_ARRAY_COUNT(MissionSponsors)) - 1)];
	const float Roll = FMath::FRand();
	if (Roll < 0.3f)
	{
		const int32 T = FMath::Clamp(Exp - 1 - FMath::RandRange(0, 2), 1, 16);
		G->MisKind = TEXT("top"); G->MisV = T; G->MisText = FString::Printf(TEXT("Termine dans le top %d"), T); G->MisGain = R1(1.2f + (Exp - T) * 0.35f);
	}
	else if (Roll < 0.48f)
	{
		const int32 Gn = 3 + FMath::RandRange(0, 2);
		G->MisKind = TEXT("gain"); G->MisV = Gn; G->MisText = FString::Printf(TEXT("Gagne au moins %d places pendant la course"), Gn); G->MisGain = R1(1.f + Gn * 0.3f);
	}
	else if (Roll < 0.6f) { G->MisKind = TEXT("fast"); G->MisText = TEXT("Signe le meilleur tour en course"); G->MisGain = 2.5f; }
	else if (Roll < 0.78f && !Rival.IsEmpty())
	{
		G->MisKind = TEXT("beat"); G->MisWho = Rival; G->MisText = FString::Printf(TEXT("Termine devant %s"), *Display(Rival)); G->MisGain = 1.8f;
	}
	else if (Roll < 0.9f)
	{
		const int32 T = FMath::Min(10, Exp + 1);
		G->MisKind = TEXT("both"); G->MisV = T; G->MisText = FString::Printf(TEXT("Les deux voitures dans le top %d"), T); G->MisGain = R1(2.f + (10 - T) * 0.2f);
	}
	else
	{
		const int32 T = FMath::Min(20, Exp + 3);
		G->MisKind = TEXT("finish"); G->MisV = T; G->MisText = FString::Printf(TEXT("Termine la course sans abandon, dans le top %d"), T); G->MisGain = 1.f;
	}
	Season->Save();
}

void UGPMCareerSubsystem::AcceptMission()
{
	UGPMSaveGame* G = Save();
	if (!G || G->MisRound != G->Round) return;
	G->bMisAcc = true;
	if (UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>()) Season->Save();
}

TArray<FString> UGPMCareerSubsystem::AfterRace(const FGPMRaceCtx& R)
{
	TArray<FString> Lines;
	UGPMSaveGame* G = Save();
	if (!G || !G->bCreated) return Lines;
	// direction : jugement du résultat selon le niveau de l'écurie (EXPECT)
	const int32 Exp = G->Tier == TEXT("new") ? 14 : G->Tier == TEXT("top") ? 4 : 9;
	const int32 D = R.bDnf ? -4 : FMath::Clamp(FMath::RoundToInt((Exp - R.Pos) * 0.6f), -5, 5);
	G->Conf = FMath::Clamp(G->Conf + D, 0, 100);
	Lines.Add(FString::Printf(TEXT("Direction : confiance %d %% (%s%d) · %s"), G->Conf, D >= 0 ? TEXT("+") : TEXT(""), D, *ConfText(G->Conf)));
	// sponsor principal : part annuelle + bonus si l'objectif de la course est rempli (GOALS de sponsors.js)
	for (const FGPMSponsorOpt& Sp : Sponsors)
		if (Sp.Id == G->Sponsor)
		{
			const bool bOk = Sp.Goal == TEXT("finish") ? !R.bDnf : Sp.Goal == TEXT("beatMate") ? (R.MatePos == 0 || R.Pos < R.MatePos)
				: Sp.Goal == TEXT("gain") ? R.Pos < R.Grid : Sp.Goal == TEXT("pts") ? R.Pos <= 10 : Sp.Goal == TEXT("podium") ? R.Pos <= 3
				: Sp.Goal == TEXT("fl") ? R.bFastest : false;
			const UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
			const float V = R1(Sp.Year / FMath::Max(1, Season ? Season->NumRounds() : 22) + (bOk ? Sp.Bonus : 0.f));
			G->Budget = R1(G->Budget + V);
			Lines.Add(FString::Printf(TEXT("%s : +%.1f M€%s"), *Sp.Name, V, bOk ? TEXT(" (objectif rempli)") : TEXT("")));
		}
	// contrats ponctuels (team.deals) : versés ou payés à chaque GP
	for (int32 k = G->DealName.Num() - 1; k >= 0; --k)
	{
		if (!G->DealGp.IsValidIndex(k) || !G->DealN.IsValidIndex(k)) continue;
		G->Budget = R1(G->Budget + G->DealGp[k]);
		Lines.Add(FString::Printf(TEXT("%s : %+.1f M€"), *G->DealName[k], G->DealGp[k]));
		if (--G->DealN[k] <= 0) { G->DealName.RemoveAt(k); G->DealGp.RemoveAt(k); G->DealN.RemoveAt(k); }
	}
	// mission du sponsor acceptée avant la course (checkMission d'events.js : un abandon la fait toujours échouer)
	if (G->bMisAcc && G->MisRound == G->Round - 1)
	{
		const int32 MateAt = R.MatePos > 0 ? R.MatePos : 99;
		bool bOk = !R.bDnf;
		if (bOk)
		{
			if (G->MisKind == TEXT("gain")) bOk = R.Grid - R.Pos >= G->MisV;
			else if (G->MisKind == TEXT("fast")) bOk = R.bFastest;
			else if (G->MisKind == TEXT("beat")) bOk = R.RivalPos > R.Pos;
			else if (G->MisKind == TEXT("both")) bOk = R.Pos <= G->MisV && MateAt <= G->MisV;
			else bOk = R.Pos <= G->MisV;
		}
		if (bOk) G->Budget = R1(G->Budget + G->MisGain);
		else G->Rep = FMath::Max(0, G->Rep - 3);
		Lines.Add(bOk ? FString::Printf(TEXT("Mission %s réussie : +%.1f M€"), *G->MisSponsor, G->MisGain)
			: FString::Printf(TEXT("Mission %s ratée (%s) : réputation -3"), *G->MisSponsor, *G->MisText));
	}
	G->bMisAcc = false;
	// primes du contrat de l'équipier : victoire ou, à défaut, podium
	if (R.MatePos >= 1 && R.MatePos <= 3 && (G->MateWin > 0.f || G->MatePod > 0.f))
	{
		const bool bMateWin = R.MatePos == 1 && G->MateWin > 0.f;
		const float Bonus = bMateWin ? G->MateWin : G->MatePod;
		if (Bonus > 0.f)
		{
			G->Budget = R1(G->Budget - Bonus);
			Lines.Add(FString::Printf(TEXT("Prime de %s (%s) : -%.1f M€"), *G->MateName, bMateWin ? TEXT("victoire") : TEXT("podium"), Bonus));
		}
	}
	G->Broken.Reset();   // nouvelle fenêtre de négociation au GP suivant
	BuildCtx(R);
	Queue.Reset();
	BuildPress(R);
	PickEvents();
	// mercato des IA à la mi-saison
	if (UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>())
		if (Season->Round() == Season->NumRounds() / 2) AIMercato(1 + FMath::RandRange(0, 1));
	return Lines;
}

TArray<FString> UGPMCareerSubsystem::SeasonReview()
{
	TArray<FString> Lines;
	UGPMSaveGame* G = Save();
	if (!G || !G->bCreated) return Lines;
	const TArray<TPair<FString, bool>> Obj = Objectives();
	int32 Met = 0;
	for (const TPair<FString, bool>& O : Obj) Met += O.Value;
	if (!G->bReviewed)
	{
		G->bReviewed = true;
		const int32 D = Met * 10 - (Obj.Num() - Met) * 15;
		G->Conf = FMath::Clamp(G->Conf + D, 0, 100);
		const float Bonus = G->Conf >= 80 ? R1(5 + Met * 3) : 0.f;
		G->Budget = R1(G->Budget + Bonus);
		G->bFired = G->Conf < 25;
		if (Bonus > 0) News(FString::Printf(TEXT("La direction est ravie : prime de %.1f M€"), Bonus));
		if (UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>()) Season->Save();
	}
	Lines.Add(FString::Printf(TEXT("Bilan de la direction : %d objectif%s sur %d · confiance %d %% (%s)"), Met, Met > 1 ? TEXT("s") : TEXT(""), Obj.Num(), G->Conf, *ConfText(G->Conf)));
	return Lines;
}

void UGPMCareerSubsystem::Intersaison()
{
	UGPMSaveGame* G = Save();
	if (!G || !G->bCreated) return;
	// motoriste et salaire de l'équipier payés pour la nouvelle saison ; fin de contrat → pilote de réserve
	for (const FGPMEngineOpt& E : Engines) if (E.Id == G->Engine) G->Budget = R1(G->Budget - E.Price);
	if (--G->MateYears <= 0)
	{
		News(FString::Printf(TEXT("Fin du contrat de %s : il quitte l'écurie."), *G->MateName));
		G->Free.Add(G->MateName);
		FGPMMarketDriver Best; Best.Spd = 0;
		for (const FString& N : G->Free) { const FGPMMarketDriver D = StatsOf(N); if (D.Name != G->MateName && Salary(D) <= 1.5f && D.Spd > Best.Spd) Best = D; }
		if (Best.Spd <= 0) { Best.Name = TEXT("R. Vallin"); Best.Spd = 70; Best.Strat = 65; Best.Tyre = 66; Best.Pot = 55; }
		G->Free.Remove(Best.Name);
		G->MateName = Best.Name; G->MateSpd = Best.Spd; G->MateStrat = Best.Strat; G->MateTyre = Best.Tyre; G->MatePot = Best.Pot;
		G->MateSal = FMath::Max(0.3f, Salary(Best)); G->MateYears = 1; G->MateRole = TEXT("eq"); G->MateSince = G->Year;
		News(FString::Printf(TEXT("%s, pilote de réserve, devient titulaire."), *Best.Name));
	}
	G->Budget = R1(G->Budget - G->MateSal);
	// les jeunes progressent un peu, l'équipier aussi (plus vite avec l'Académie de pilotes)
	UGPMFactorySubsystem* Factory = GetGameInstance()->GetSubsystem<UGPMFactorySubsystem>();
	const float Grow = Factory ? Factory->MateGrowth() : 1.f;
	G->MateSpd = FMath::Min(99.f, G->MateSpd + FMath::Max(0.f, (G->MatePot - G->MateSpd) * 0.15f * Grow));
	// chefs de pôle : salaires de la nouvelle saison, fins de contrat
	if (Factory) Factory->RenewStaff(G->Year + 1);
	AIMercato(3 + FMath::RandRange(0, 2));
	ApplyIdentity();
}

// ---------- mercato ----------
FGPMMarketDriver UGPMCareerSubsystem::StatsOf(const FString& Name) const
{
	for (const FGPMMarketDriver& D : Market) if (D.Name == Name) return D;
	FGPMMarketDriver D;
	D.Name = Name;
	const UGPMSaveGame* G = Save();
	float Sk = G ? G->Skill.FindRef(Name) : 0.f;
	if (Sk <= 0.f)
		for (const FGPMTeam& T : BaseTeams) for (const FGPMDriver& X : T.Drivers) if (X.Name == Name) Sk = X.Skill;
	if (Sk <= 0.f) Sk = 0.94f;
	// statsOf de career.js : le niveau d'un pilote rival donne ses notes
	D.Spd = FMath::Clamp((Sk - 0.87f) / 0.001f, 50.f, 99.f);
	D.Strat = FMath::Clamp(D.Spd - 4.f + float(GetTypeHash(Name) % 9), 50.f, 99.f);
	D.Tyre = FMath::Clamp(D.Spd - 6.f + float(GetTypeHash(Name + TEXT("t")) % 11), 50.f, 99.f);
	D.Pot = 45.f + float(GetTypeHash(Name + TEXT("p")) % 40);
	D.Age = 22 + int32(GetTypeHash(Name + TEXT("a")) % 14);
	return D;
}

TArray<FGPMMarketDriver> UGPMCareerSubsystem::MarketList() const
{
	TArray<FGPMMarketDriver> Out;
	const UGPMSaveGame* G = Save();
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	if (!G || !Data) return Out;
	for (const FString& N : G->Free) { FGPMMarketDriver D = StatsOf(N); D.Team.Reset(); D.Seat = -1; if (D.From.IsEmpty()) D.From = TEXT("Pilote libre"); Out.Add(D); }
	for (int32 t = 1; t < Data->Teams.Num(); ++t)
		for (int32 d = 0; d < Data->Teams[t].Drivers.Num(); ++d)
		{
			FGPMMarketDriver D = StatsOf(Data->Teams[t].Drivers[d].Name);
			D.Team = Data->Teams[t].Name; D.Seat = d; D.From = D.Team;
			Out.Add(D);
		}
	Out.Sort([](const FGPMMarketDriver& A, const FGPMMarketDriver& B) { return A.Rating() > B.Rating(); });
	return Out;
}

FString UGPMCareerSubsystem::Status(const FGPMMarketDriver& D) const
{
	const UGPMSaveGame* G = Save();
	if (G && G->Broken.Contains(D.Name)) return TEXT("discussions rompues");
	return D.Sponsor > 0.f ? FString::Printf(TEXT("apporte %.1f M€"), D.Sponsor) : FString::Printf(TEXT("≈ %.1f M€ / saison"), Salary(D));
}

void UGPMCareerSubsystem::OpenNego(const FGPMMarketDriver& D)
{
	const UGPMSaveGame* G = Save();
	if (!G) return;
	Nego = FGPMNego();
	Nego.D = D;
	const uint32 H = GetTypeHash(D.Name);
	const bool bYoung = D.Age <= 22, bVet = D.Age >= 32;
	const float U = float(H % 1000) / 1000.f;
	if (bYoung) Nego.Pers = U < 0.7f ? TEXT("jeune") : TEXT("ambitieux");
	else if (bVet) Nego.Pers = U < 0.5f ? TEXT("mercenaire") : TEXT("fidele");
	else Nego.Pers = PersList[H % 3].Id;
	const FPers& P = PersOf(Nego.Pers);
	const float W = Nego.Pers == TEXT("ambitieux") ? 1.6f : Nego.Pers == TEXT("mercenaire") ? 0.5f : 1.f;
	const float Rt = D.Rating();
	float Int = 45.f + (G->Rep - 50) * 0.7f * W + (6 - TeamPos()) * 2.5f * W - FMath::Max(0.f, Rt - 78.f) * 1.2f + (float(H % 97) / 97.f - 0.5f) * 12.f;
	if (Nego.Pers == TEXT("jeune")) Int += 10.f;
	Nego.Interest = FMath::Clamp(FMath::RoundToInt(Int), 5, 95);
	Nego.Patience = P.Pat;
	Nego.Years = Nego.Pers == TEXT("fidele") ? (bVet ? 2 : 3) : ((Nego.Pers == TEXT("mercenaire") || bVet) ? 1 : 2);
	Nego.bWantN1 = Rt >= 84.f || (Nego.Pers == TEXT("ambitieux") && Rt >= 80.f);
	Nego.Role = Nego.bWantN1 ? TEXT("n1") : TEXT("eq");
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	const bool bMid = Season && Season->Round() > 0 && !Season->IsOver();
	const UGPMFactorySubsystem* Factory = GetGameInstance()->GetSubsystem<UGPMFactorySubsystem>();
	Nego.Ask = FMath::Max(0.3f, Salary(D)) * P.Greed * (1.f + (55 - Nego.Interest) / 160.f) * (bMid ? 1.2f : 1.f) * (Factory ? Factory->SalaryCut() : 1.f);
	Nego.Demand = FMath::CeilToFloat(Nego.Ask * 1.1f * 10.f) / 10.f;
	Nego.Fee = D.Seat >= 0 ? FMath::CeilToFloat(FMath::Max(1.f, Salary(D)) * 0.8f * (bMid ? MidMult : 1.f) * 2.f) / 2.f : 0.f;
	Nego.bOpen = true;
	Queue.Reset();
	if (G->Broken.Contains(D.Name)) { NegoDialog(TEXT("L'agent ne répond plus à tes appels. Réessaie après le prochain Grand Prix.")); Queue[0].Chosen = 0; Queue[0].Result = TEXT("Discussions rompues."); return; }
	if (D.Sponsor > 0.f)
	{
		Nego.Demand = 0.f; Nego.Years = 1; Nego.Role = TEXT("eq");
		NegoDialog(FString::Printf(TEXT("%s arrive avec son sponsor : %.1f M€ pour l'écurie s'il obtient le volant."), *D.Name, D.Sponsor));
		return;
	}
	if (Nego.Interest < 20)
	{
		NegoDialog(FString::Printf(TEXT("Mon client ne souhaite pas rejoindre %s pour le moment."), *G->TeamName));
		Queue[0].Chosen = 0; Queue[0].Result = FString::Printf(TEXT("%s refuse de discuter (intérêt %d)."), *D.Name, Nego.Interest);
		return;
	}
	FString Line = FString::Printf(TEXT("Mon client demande %.1f M€ par saison sur %d saison%s%s."), Nego.Demand, Nego.Years, Nego.Years > 1 ? TEXT("s") : TEXT(""),
		Nego.bWantN1 ? TEXT(", avec le statut de pilote N°1") : TEXT(""));
	if (Nego.Fee > 0.f) Line += FString::Printf(TEXT(" %s demande %.1f M€ d'indemnité pour le libérer, et récupère %s."), *D.Team, Nego.Fee, *G->MateName);
	NegoDialog(Line);
}

void UGPMCareerSubsystem::NegoDialog(const FString& Line)
{
	const UGPMSaveGame* G = Save();
	FGPMDialog Dl;
	Dl.Kind = FGPMDialog::EKind::Nego;
	Dl.Color = FLinearColor(0.2f, 0.6f, 1.f);
	const FPers& P = PersOf(Nego.Pers);
	Dl.Kicker = FString::Printf(TEXT("NÉGOCIATION · %s · %s · intérêt %d · patience %d"), *Nego.D.Name, P.Name, Nego.Interest, Nego.Patience);
	Dl.Title = FString::Printf(TEXT("%s, %d ans · note %.0f (vitesse %.0f, stratégie %.0f, pneus %.0f, potentiel %.0f)"), *Nego.D.From, Nego.D.Age, Nego.D.Rating(),
		Nego.D.Spd, Nego.D.Strat, Nego.D.Tyre, Nego.D.Pot);
	Dl.Text = TEXT("« ") + Line + TEXT(" »");
	const float Budget = G ? G->Budget : 0.f;
	auto Add = [&Dl, Budget, this](const FString& Txt, int32 Act, float Sal)
	{
		FGPMChoice C; C.Text = Txt; C.Action = Act;
		C.bOff = Act != 4 && Sal + Nego.Fee > Budget + 1e-3f;
		if (C.bOff) C.Text += TEXT(" (budget insuffisant)");
		Dl.Choices.Add(C);
	};
	const TCHAR* Ys = Nego.Years > 1 ? TEXT("s") : TEXT("");
	if (Nego.D.Sponsor > 0.f) Add(FString::Printf(TEXT("Signer %s (1 saison, apporte %.1f M€)"), *Nego.D.Name, Nego.D.Sponsor), 0, 0.f);
	else
	{
		Add(FString::Printf(TEXT("Accepter : %.1f M€ × %d saison%s, statut %s"), Nego.Demand, Nego.Years, Ys, RoleName(Nego.Role)), 0, Nego.Demand);
		Add(FString::Printf(TEXT("Proposer %.1f M€ par saison"), R1(Nego.Demand * 0.9f)), 1, R1(Nego.Demand * 0.9f));
		Add(FString::Printf(TEXT("Proposer %.1f M€ par saison"), R1(Nego.Demand * 0.78f)), 2, R1(Nego.Demand * 0.78f));
		if (Nego.Role != TEXT("n2")) Add(FString::Printf(TEXT("Proposer %.1f M€ avec le statut N°2"), R1(Nego.Demand * 0.85f)), 3, R1(Nego.Demand * 0.85f));
		Add(FString::Printf(TEXT("Proposer %.1f M€ + primes (%.1f M€ par podium, %.1f M€ par victoire)"), R1(Nego.Demand * 0.82f), PodBonus(Nego.Demand), WinBonus(Nego.Demand)), 5,
			R1(Nego.Demand * 0.82f));
	}
	Add(TEXT("Quitter la négociation"), 4, 0.f);
	if (Queue.Num()) Queue[0] = Dl; else Queue.Add(Dl);
}

void UGPMCareerSubsystem::NegoAct(int32 Action)
{
	UGPMSaveGame* G = Save();
	FGPMDialog* Dl = Current();
	if (!G || !Dl) return;
	if (Action == 4) { Dl->Chosen = 0; Dl->Result = TEXT("Tu quittes la table. L'agent garde ton numéro."); return; }
	if (Action == 0) { Sign(); return; }
	const float Offer = R1(Nego.Demand * (Action == 1 ? 0.9f : Action == 2 ? 0.78f : Action == 5 ? 0.82f : 0.85f));
	const FString Role = Action == 3 ? FString(TEXT("n2")) : Nego.Role;
	const float RoleT = Nego.bWantN1 ? (Role == TEXT("n1") ? 0.12f : Role == TEXT("n2") ? -0.3f : 0.f) : (Role == TEXT("n1") ? 0.05f : Role == TEXT("n2") ? -0.08f : 0.f);
	// primes : un ambitieux ou un jeune loup y croit plus qu'un mercenaire
	const float Pod = Action == 5 ? PodBonus(Nego.Demand) : 0.f, Win = Action == 5 ? WinBonus(Nego.Demand) : 0.f;
	const float BonusW = Nego.Pers == TEXT("ambitieux") ? 0.9f : Nego.Pers == TEXT("jeune") ? 0.8f : Nego.Pers == TEXT("fidele") ? 0.6f : 0.4f;
	const float Score = (Offer + (Pod + Win) * BonusW) / Nego.Ask + RoleT - (Nego.bWantN1 && Role != TEXT("n1") ? 0.12f : 0.f);
	const float Thr = 1.f - (Nego.Interest - 50) / 500.f + (FMath::FRand() - 0.5f) * 0.04f;
	if (Score >= Thr)
	{
		Nego.Demand = Offer; Nego.Role = Role; Nego.Pod = Pod; Nego.Win = Win;
		Sign();
		return;
	}
	if (Score >= Thr - 0.14f)
	{
		Nego.Patience -= 1;
		Nego.Role = Role;
		Nego.Demand = FMath::CeilToFloat(FMath::Max(Offer * 1.04f, Nego.Ask * (Thr - RoleT) * 1.02f) * 10.f) / 10.f;
		if (Nego.Patience > 0) { NegoDialog(FString::Printf(TEXT("On s'en approche. Mon client signerait pour %.1f M€ par saison, statut %s."), Nego.Demand, RoleName(Nego.Role))); return; }
	}
	else
	{
		Nego.Patience -= 2;
		Nego.Interest = FMath::Max(5, Nego.Interest - 8);
		if (Nego.Patience > 0) { NegoDialog(TEXT("C'est une plaisanterie ? Revenez avec une offre sérieuse.")); return; }
	}
	G->Broken.AddUnique(Nego.D.Name);
	Dl->Chosen = 0;
	Dl->bBad = true;
	Dl->Result = FString::Printf(TEXT("L'agent de %s claque la porte : discussions rompues jusqu'au prochain Grand Prix."), *Nego.D.Name);
}

void UGPMCareerSubsystem::Sign()
{
	UGPMSaveGame* G = Save();
	FGPMDialog* Dl = Current();
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	if (!G || !Dl || !Data) return;
	const float Sal = Nego.D.Sponsor > 0.f ? 0.f : Nego.Demand;
	const float Cost = R1(Sal + Nego.Fee - Nego.D.Sponsor);
	if (Cost > G->Budget + 1e-3f) { Dl->Chosen = 0; Dl->bBad = true; Dl->Result = TEXT("Budget insuffisant pour signer ce contrat."); return; }
	G->Budget = R1(G->Budget - Cost);
	const FString Old = G->MateName;
	// pilote d'une écurie rivale : échange avec l'équipier ; sinon l'ancien équipier rejoint les pilotes libres
	if (Nego.D.Seat >= 0)
	{
		for (int32 t = 1; t < Data->Teams.Num(); ++t)
			if (Data->Teams[t].Name == Nego.D.Team && BaseTeams.IsValidIndex(t))
			{
				G->Seats.Add(FString::Printf(TEXT("%s|%d"), *BaseTeams[t].Name, Nego.D.Seat), Old);
				G->Skill.Add(Old, MateSkill(G->MateSpd));
				G->DriverTeam.Add(Old, Nego.D.Team);
			}
		WriteSeats();
	}
	else { G->Free.Remove(Nego.D.Name); G->Free.AddUnique(Old); }
	G->MateName = Nego.D.Name; G->MateSpd = Nego.D.Spd; G->MateStrat = Nego.D.Strat; G->MateTyre = Nego.D.Tyre; G->MatePot = Nego.D.Pot;
	G->MateSal = Sal; G->MateYears = Nego.Years; G->MateRole = Nego.Role; G->MateSince = G->Year;
	G->MatePod = Nego.Pod; G->MateWin = Nego.Win;
	G->DriverTeam.Add(Nego.D.Name, G->TeamName);
	G->DriverPts.FindOrAdd(Nego.D.Name);
	G->DriverPts.FindOrAdd(Old);
	ApplyIdentity();
	News(FString::Printf(TEXT("%s signe chez %s ! %s s'en va."), *Nego.D.Name, *G->TeamName, *Old));
	Dl->Chosen = 0;
	Dl->bBad = false;
	Dl->Result = FString::Printf(TEXT("%s signe pour %d saison%s (%.1f M€ par saison, statut %s). Coût immédiat : %.1f M€."), *Nego.D.Name, Nego.Years,
		Nego.Years > 1 ? TEXT("s") : TEXT(""), Sal, RoleName(Nego.Role), Cost);
	if (Nego.Pod > 0.f || Nego.Win > 0.f)
		Dl->Result += FString::Printf(TEXT(" Primes : %.1f M€ par podium, %.1f M€ par victoire."), Nego.Pod, Nego.Win);
	Nego.bOpen = false;
	bJustSigned = true;
	if (UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>()) Season->Save();
}

void UGPMCareerSubsystem::WriteSeats()
{
	// garde les places des écuries rivales (après un échange ou le mercato des IA)
	UGPMSaveGame* G = Save();
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	if (!G || !Data) return;
	for (int32 t = 1; t < Data->Teams.Num() && t < BaseTeams.Num(); ++t)
		for (int32 d = 0; d < Data->Teams[t].Drivers.Num(); ++d)
		{
			const FString Key = FString::Printf(TEXT("%s|%d"), *BaseTeams[t].Name, d);
			if (!G->Seats.Contains(Key)) { G->Seats.Add(Key, Data->Teams[t].Drivers[d].Name); G->Skill.Add(Data->Teams[t].Drivers[d].Name, Data->Teams[t].Drivers[d].Skill); }
		}
}

void UGPMCareerSubsystem::AIMercato(int32 Moves)
{
	// aiMercato de market.js : échanges entre écuries rivales, jeunes de Formule B et vétérans qui prennent un volant
	UGPMSaveGame* G = Save();
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	if (!G || !Data || Data->Teams.Num() < 3) return;
	WriteSeats();
	static const TCHAR* First[] = { TEXT("N."), TEXT("L."), TEXT("T."), TEXT("É."), TEXT("A."), TEXT("M."), TEXT("J."), TEXT("R."), TEXT("S."), TEXT("K.") };
	static const TCHAR* Last[] = { TEXT("Vasseur"), TEXT("Lindahl"), TEXT("Marchal"), TEXT("Ferraz"), TEXT("Ivanova"), TEXT("Delcourt"), TEXT("Sato"), TEXT("Brunel"), TEXT("Kovač"), TEXT("Aldana") };
	for (int32 m = 0; m < Moves; ++m)
	{
		const int32 T1 = FMath::RandRange(1, Data->Teams.Num() - 1), D1 = FMath::RandRange(0, 1);
		const FString K1 = FString::Printf(TEXT("%s|%d"), *BaseTeams[T1].Name, D1);
		const FString Who1 = G->Seats.FindRef(K1);
		if (FMath::FRand() < 0.5f)
		{
			int32 T2 = FMath::RandRange(1, Data->Teams.Num() - 1);
			if (T2 == T1) T2 = T1 % (Data->Teams.Num() - 1) + 1;
			const int32 D2 = FMath::RandRange(0, 1);
			const FString K2 = FString::Printf(TEXT("%s|%d"), *BaseTeams[T2].Name, D2);
			const FString Who2 = G->Seats.FindRef(K2);
			G->Seats.Add(K1, Who2); G->Seats.Add(K2, Who1);
			G->DriverTeam.Add(Who2, Data->Teams[T1].Name); G->DriverTeam.Add(Who1, Data->Teams[T2].Name);
			News(FString::Printf(TEXT("Mercato : %s (%s) et %s (%s) échangent leurs volants."), *Who1, *Data->Teams[T1].Name, *Who2, *Data->Teams[T2].Name));
		}
		else
		{
			// un jeune ou un pilote libre remplace un titulaire, qui devient pilote libre
			FString In;
			float Sk = 0.9f + FMath::FRand() * 0.05f;
			if (G->Free.Num() && FMath::FRand() < 0.5f) { In = G->Free[FMath::RandRange(0, G->Free.Num() - 1)]; Sk = MateSkill(StatsOf(In).Spd); G->Free.Remove(In); }
			else In = FString(First[FMath::RandRange(0, 9)]) + TEXT(" ") + Last[FMath::RandRange(0, 9)];
			if (G->Seats.FindKey(In) || In == G->MateName) continue;
			G->Seats.Add(K1, In); G->Skill.Add(In, Sk);
			G->Free.AddUnique(Who1);
			G->DriverTeam.Add(In, Data->Teams[T1].Name);
			G->DriverPts.FindOrAdd(In);
			News(FString::Printf(TEXT("Mercato : %s remplace %s chez %s."), *In, *Who1, *Data->Teams[T1].Name));
		}
	}
	ApplyIdentity();
}
