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
	// personnalité d'un pilote selon son âge (jeune loup ou ambitieux avant 23 ans, mercenaire ou fidèle après 31)
	FString PersFor(const FString& Name, int32 Age)
	{
		const uint32 H = GetTypeHash(Name);
		const float U = float(H % 1000) / 1000.f;
		if (Age <= 22) return U < 0.7f ? TEXT("jeune") : TEXT("ambitieux");
		if (Age >= 32) return U < 0.5f ? TEXT("mercenaire") : TEXT("fidele");
		return PersList[H % 3].Id;
	}
	// clause libératoire proposée dans un contrat : quatre saisons de salaire, 2 M€ au moins
	float ClauseOf(float Demand) { return FMath::RoundToFloat(FMath::Max(2.f, Demand * 4.f) * 10.f) / 10.f; }
	// jeunes pilotes de Formule B qui arrivent sur le marché (noms inventés)
	const TCHAR* const RookieFirst[] = { TEXT("N."), TEXT("L."), TEXT("T."), TEXT("É."), TEXT("A."), TEXT("M."), TEXT("J."), TEXT("R."), TEXT("S."), TEXT("K."), TEXT("C."), TEXT("V."),
		TEXT("O."), TEXT("D."), TEXT("I."), TEXT("Y.") };
	const TCHAR* const RookieLast[] = { TEXT("Vasseur"), TEXT("Lindahl"), TEXT("Marchal"), TEXT("Ferraz"), TEXT("Ivanova"), TEXT("Delcourt"), TEXT("Sato"), TEXT("Brunel"), TEXT("Kovač"),
		TEXT("Aldana"), TEXT("Quenneville"), TEXT("Arslan"), TEXT("Mwangi"), TEXT("Rautio"), TEXT("Esposti"), TEXT("Calloway"), TEXT("Nordin"), TEXT("Bastide") };
	const TCHAR* RoleName(const FString& R) { return R == TEXT("n1") ? TEXT("N°1") : R == TEXT("n2") ? TEXT("N°2") : TEXT("Égalité"); }
	const float MidMult = 2.5f;   // indemnités de transfert pendant la saison (MID_MULT de market.js)
	// primes proposées à l'équipier : par podium et par victoire, selon le salaire demandé
	float PodBonus(float Demand) { return R1(FMath::Max(0.2f, Demand * 0.12f)); }
	float WinBonus(float Demand) { return R1(FMath::Max(0.4f, Demand * 0.25f)); }
	// sponsors ponctuels des missions (makeMission d'events.js)
	const TCHAR* const MissionSponsors[] = { TEXT("Lumora"), TEXT("Kestrel Tech"), TEXT("Novacel"), TEXT("Brisa Mobile"), TEXT("Atlasbank"), TEXT("Orvane"),
		TEXT("Pixalto"), TEXT("Verdania") };
}

int32 UGPMCareerSubsystem::NegoLevel = 1;
int32 UGPMCareerSubsystem::CareerLen = 0;

void UGPMCareerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UGPMDataSubsystem>();
	Collection.InitializeDependency<UGPMSeasonSubsystem>();
	Super::Initialize(Collection);
	if (UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>()) OrigTeams = Data->Teams;
	BaseTeams = OrigTeams;
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
		(JStr(O, TEXT("kind")) == TEXT("main") ? Sponsors : SecSponsors).Add({ JStr(O, TEXT("id")), JStr(O, TEXT("name")), JStr(O, TEXT("sector")), JStr(O, TEXT("goal")), float(JNum(O, TEXT("year"), 10)),
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
	if (!Data || OrigTeams.Num() < 2) return;
	// disposition du plateau : l'écurie du joueur est toujours la première ; l'écurie d'origine du jeu (Martel) devient une rivale
	// quand le joueur rejoint une autre écurie (elle prend sa place) ou crée la 11e
	BaseTeams = OrigTeams;
	if (G && G->bCreated && G->Layout >= 1)
	{
		auto MartelRival = [](FGPMTeam& M)
		{
			if (M.Drivers.Num() < 2) return;
			M.Drivers[0].Name = TEXT("R. Valcourt"); M.Drivers[0].bPlayer = false; M.Drivers[0].Skill = 0.95f;
			M.Drivers[1].Name = TEXT("E. Montrand"); M.Drivers[1].Skill = 0.935f;
		};
		if (G->Layout == 1)
		{
			const int32 X = BaseTeams.IndexOfByPredicate([G](const FGPMTeam& T) { return T.Name == G->JoinTeam; });
			if (X > 0)
			{
				BaseTeams.Swap(0, X);
				MartelRival(BaseTeams[X]);
				if (BaseTeams[0].Drivers.Num()) { BaseTeams[0].Drivers[0].Name = TEXT("Toi"); BaseTeams[0].Drivers[0].bPlayer = true; BaseTeams[0].Drivers[0].Skill = 0.95f; }
			}
		}
		else
		{
			FGPMTeam Mine = BaseTeams[0];
			MartelRival(BaseTeams[0]);
			BaseTeams.Insert(Mine, 0);
		}
	}
	Data->Teams = BaseTeams;
	if (!G || !G->bCreated) return;
	FGPMTeam& Me = Data->Teams[0];
	if (G->Layout != 1)   // une écurie rejointe garde son nom et ses couleurs
	{
		Me.Name = G->TeamName.IsEmpty() ? Me.Name : G->TeamName;
		if (Colors.IsValidIndex(G->Col1)) Me.Color = Colors[G->Col1].Color;
		if (Colors.IsValidIndex(G->Col2)) Me.Accent = Colors[G->Col2].Color;
	}
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
void UGPMCareerSubsystem::JoinPlan(int32 Idx, int32& OutTier, int32& OutEngine, int32& OutSponsor, FGPMMarketDriver& OutMate) const
{
	const FGPMTeam& X = OrigTeams[Idx];
	auto IndexOf = [](auto& Arr, const FString& Id, int32 Def) { for (int32 k = 0; k < Arr.Num(); ++k) if (Arr[k].Id == Id) return k; return Def; };
	OutTier = IndexOf(Tiers, X.Perf >= 0.985f ? TEXT("top") : X.Perf >= 0.965f ? TEXT("mid") : TEXT("new"), 1);
	OutEngine = IndexOf(Engines, X.Perf >= 0.985f ? TEXT("valdrin") : X.Perf >= 0.975f ? TEXT("orsetti") : X.Perf >= 0.965f ? TEXT("brennhall") : X.Perf >= 0.955f ? TEXT("hexadyne") : TEXT("kiyora"), 1);
	OutSponsor = 0;
	float BestYear = -1.f;
	const int32 TierRep = Tiers.IsValidIndex(OutTier) ? Tiers[OutTier].Rep : 50;
	for (int32 k = 0; k < Sponsors.Num(); ++k) if (Sponsors[k].Rep <= TierRep && Sponsors[k].Year > BestYear) { BestYear = Sponsors[k].Year; OutSponsor = k; }
	// l'équipier : le deuxième pilote de l'écurie
	const FGPMDriver& Mate = X.Drivers.Num() > 1 ? X.Drivers[1] : X.Drivers[0];
	OutMate = FGPMMarketDriver();
	OutMate.Name = Mate.Name; OutMate.From = X.Name; OutMate.Age = 28;
	OutMate.Spd = FMath::Clamp((Mate.Skill - 0.87f) * 1000.f, 55.f, 99.f); OutMate.Strat = 75.f; OutMate.Tyre = 74.f; OutMate.Pot = 60.f;
}

float UGPMCareerSubsystem::JoinBudget(int32 Idx) const
{
	if (!OrigTeams.IsValidIndex(Idx) || Tiers.Num() == 0 || Engines.Num() == 0) return 0.f;
	int32 T = 1, E = 1, S = 0;
	FGPMMarketDriver M;
	JoinPlan(Idx, T, E, S, M);
	float B = Tiers[T].Budget - Engines[E].Price - FMath::Max(0.f, Salary(M));
	if (Sponsors.IsValidIndex(S)) B += Sponsors[S].Sign;
	return R1(B);
}

float UGPMCareerSubsystem::PickBudget() const
{
	if (Pick.Join >= 0 && OrigTeams.IsValidIndex(Pick.Join)) return JoinBudget(Pick.Join);
	float B = Tiers.IsValidIndex(Pick.Tier) ? Tiers[Pick.Tier].Budget : 70.f;
	if (Engines.IsValidIndex(Pick.Engine)) B -= Engines[Pick.Engine].Price;
	if (Sponsors.IsValidIndex(Pick.Sponsor)) B += Sponsors[Pick.Sponsor].Sign;
	if (Market.IsValidIndex(Pick.Mate)) B -= Salary(Market[Pick.Mate]);
	return R1(B);
}

bool UGPMCareerSubsystem::PickValid(FString& Why) const
{
	if (Pick.Join >= 0) return true;
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
	const bool bJoin = Pick.Join >= 0 && OrigTeams.IsValidIndex(Pick.Join);
	int32 TierI = FMath::Clamp(Pick.Tier, 0, Tiers.Num() - 1), EngI = FMath::Clamp(Pick.Engine, 0, Engines.Num() - 1), SpI = Pick.Sponsor;
	FGPMMarketDriver M = Market[FMath::Clamp(Pick.Mate, 0, Market.Num() - 1)];
	if (bJoin) JoinPlan(Pick.Join, TierI, EngI, SpI, M);
	const FGPMTierOpt& T = Tiers[TierI];
	const FGPMEngineOpt& E = Engines[EngI];
	G->bCreated = true;
	G->Layout = bJoin ? 1 : 2;
	G->JoinTeam = bJoin ? OrigTeams[Pick.Join].Name : FString();
	G->TeamName = bJoin ? OrigTeams[Pick.Join].Name : TeamNames[FMath::Clamp(Pick.Name, 0, TeamNames.Num() - 1)];
	G->PlayerName = PilotNames[FMath::Clamp(Pick.Pilot, 0, PilotNames.Num() - 1)];
	G->Tier = T.Id; G->Engine = E.Id; G->Col1 = Pick.Col1; G->Col2 = Pick.Col2;
	G->Sponsor = Sponsors.IsValidIndex(SpI) ? Sponsors[SpI].Id : FString();
	G->BasePerf = bJoin ? OrigTeams[Pick.Join].Perf : T.Perf * (1.f + (E.Perf - 85) * 0.0012f);   // enginePerf de career.js
	G->Fuel = E.Conso / 100.f;
	G->Budget = PickBudget(); G->Rep = T.Rep; G->Moral = T.Moral; G->Conf = 60;
	G->MateName = M.Name; G->MateSpd = M.Spd; G->MateStrat = M.Strat; G->MateTyre = M.Tyre; G->MatePot = M.Pot;
	G->MateSal = FMath::Max(0.f, Salary(M)); G->MateYears = 2; G->MateRole = TEXT("eq"); G->MateSince = G->Year;
	for (const FGPMMarketDriver& D : Market) if (D.Name != M.Name) G->Free.Add(D.Name);
	// écurie rejointe : le pilote dont tu prends le volant se retrouve libre
	if (bJoin && Pick.Join > 0 && OrigTeams[Pick.Join].Drivers.Num())
	{
		const FGPMDriver& Out = OrigTeams[Pick.Join].Drivers[0];
		G->Free.AddUnique(Out.Name); G->Skill.Add(Out.Name, Out.Skill); G->DrvPot.Add(Out.Name, 62.f);
	}
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
	{
		FString Obj;
		for (const TPair<FString, bool>& O : Objectives()) Obj += TEXT("\n[ ] ") + O.Key;
		SendMail(TEXT("direction"), TEXT("Président de l'écurie"), FString::Printf(TEXT("Bienvenue chez %s"), *G->TeamName),
			FString::Printf(TEXT("Bienvenue au QG, %s. Lance tes premières recherches à l'usine et recrute tes chefs de pôle.\n\nNos objectifs pour la saison%s\n\nTous les messages de l'écurie arriveront dans cette boîte : presse, mercato, événements, sponsors et usine."), *G->PlayerName, *Obj));
		Season->Save();
	}
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
	// (les ambitions suivent les résultats : le niveau de départ la première saison, puis le classement de la saison précédente)
	if (Ambition() == 0)
	{
		Out.Emplace(FString::Printf(TEXT("Top 8 des écuries (P%d)"), CPos), bAny && CPos <= 8);
		Out.Emplace(FString::Printf(TEXT("25 points pour l'écurie (%d/25)"), Pts), Pts >= 25);
	}
	else if (Ambition() == 2)
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

int32 UGPMCareerSubsystem::Ambition() const
{
	const UGPMSaveGame* G = Save();
	if (!G) return 1;
	if (G->Ambition >= 0) return FMath::Clamp(G->Ambition, 0, 2);
	return G->Tier == TEXT("new") ? 0 : G->Tier == TEXT("top") ? 2 : 1;
}

void UGPMCareerSubsystem::SecondChance()
{
	UGPMSaveGame* G = Save();
	if (!G || !G->bCreated) return;
	const float Before = G->Budget;
	G->bFired = false; G->Conf = 50; G->Ambition = 0; G->Moral = 50;
	G->Budget = R1(FMath::Max(25.f, G->Budget * 0.5f));
	G->Rep = FMath::Max(0, G->Rep - 15);
	++G->Rescues;
	SendMail(TEXT("direction"), TEXT("Nouveau propriétaire"), TEXT("Je rachète l'écurie, et je te garde"),
		FString::Printf(TEXT("L'ancienne direction est partie. Je crois en ce projet et je te laisse les commandes de %s.\n\nLes comptes sont remis à plat : budget de %.1f M€ (au lieu de %.1f), réputation en baisse après cette crise. Mes ambitions sont modestes pour commencer : le top 8 des écuries et 25 points. Montre-moi que j'ai eu raison."),
			*G->TeamName, G->Budget, Before));
	if (UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>()) Season->Save();
}

bool UGPMCareerSubsystem::IsFired() const { const UGPMSaveGame* G = Save(); return G && G->bCreated && (G->bFired || G->Conf <= 5); }

float UGPMCareerSubsystem::Boost(const TCHAR* Key) const { const UGPMSaveGame* G = Save(); return G ? G->Boost.FindRef(Key) : 0.f; }
void UGPMCareerSubsystem::ShiftDev(float Days)
{
	if (UGPMSaveGame* G = Save()) for (TPair<FString, float>& E : G->DevLeft) E.Value = FMath::Max(1.f, E.Value - Days);
}

void UGPMCareerSubsystem::ClearBoost() { if (UGPMSaveGame* G = Save()) G->Boost.Reset(); }

void UGPMCareerSubsystem::News(const FString& Msg) { if (UGPMSaveGame* G = Save()) G->News.Add(Msg); }

// ---------- boîte mail ----------
void UGPMCareerSubsystem::SendMail(const FString& InTag, const FString& InFrom, const FString& InSubj, const FString& InBody)
{
	UGPMSaveGame* G = Save();
	if (!G || !G->bCreated || InSubj.IsEmpty()) return;
	FGPMMail M;
	M.Tag = InTag; M.From = InFrom; M.Subject = InSubj; M.Body = InBody;
	M.Year = G->Year; M.Round = G->Round;
	G->Mail.Insert(M, 0);
	if (G->Mail.Num() > 150) G->Mail.SetNum(150);
}

int32 UGPMCareerSubsystem::Unread() const
{
	const UGPMSaveGame* G = Save();
	int32 N = 0;
	if (G) for (const FGPMMail& M : G->Mail) N += M.bRead ? 0 : 1;
	return N;
}

void UGPMCareerSubsystem::RaceMails(const FGPMRaceCtx& R, const TArray<FString>& Lines)
{
	UGPMSaveGame* G = Save();
	if (!G || !G->bCreated) return;
	const FString Gp = FString::Printf(TEXT("GP %s"), *R.Circuit);
	const FString Res = R.bDnf ? FString(TEXT("abandon")) : FString::Printf(TEXT("P%d, parti P%d"), R.Pos, R.Grid);
	const TCHAR* Word = R.bDnf ? TEXT("Un abandon coûte cher, nous attendons une réaction.") : R.Pos <= 3 ? TEXT("Superbe résultat, toute l'usine est fière.")
		: R.Pos < R.Grid ? TEXT("Bonne remontée, continuez ainsi.") : TEXT("Résultat noté. Nous comptons sur toi pour progresser.");
	// bilan : direction, sponsors, primes (lignes d'AfterRace), puis objectifs de la saison
	FString Body = FString(Word) + TEXT("\n");
	for (const FString& L : Lines) Body += TEXT("\n• ") + L;
	Body += FString::Printf(TEXT("\n\nBudget : %.1f M€\n\nObjectifs de la saison"), G->Budget);
	for (const TPair<FString, bool>& O : Objectives()) Body += FString::Printf(TEXT("\n%s %s"), O.Value ? TEXT("[x]") : TEXT("[ ]"), *O.Key);
	SendMail(TEXT("direction"), TEXT("Direction de l'écurie"), Gp + TEXT(" : ") + Res, Body);
	// nouvelles de l'usine (livraisons, retards, échecs, chefs) et du mercato des rivaux
	FString Fab, Mkt;
	int32 NMkt = 0;
	for (const FString& N : G->News)
	{
		if (N.StartsWith(TEXT("Mercato"))) { Mkt += (Mkt.IsEmpty() ? TEXT("• ") : TEXT("\n• ")) + N; ++NMkt; }
		else Fab += (Fab.IsEmpty() ? TEXT("• ") : TEXT("\n• ")) + N;
	}
	if (!Fab.IsEmpty()) SendMail(TEXT("usine"), TEXT("Bureau technique"), Fab.Contains(TEXT("Livrée")) ? TEXT("Livraison à l'usine") : Fab.Contains(TEXT("Échec")) ? TEXT("Recherche ratée au banc d'essai") : TEXT("Nouvelles de l'usine"), Fab);
	if (NMkt) SendMail(TEXT("mercato"), TEXT("Le Paddock (presse)"), FString::Printf(TEXT("Mercato : %d transfert%s chez les rivaux"), NMkt, NMkt > 1 ? TEXT("s") : TEXT("")), Mkt);
}

// mission d'un sponsor ponctuel avant chaque GP (makeMission d'events.js), à accepter dans l'écran de saison
void UGPMCareerSubsystem::EnsureMission()
{
	UGPMSaveGame* G = Save();
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	if (!G || !G->bCreated || !Season || Season->IsOver() || G->MisRound == G->Round) return;
	const int32 Exp = Expected();
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
	SendMail(TEXT("sponsor"), G->MisSponsor, TEXT("Proposition de mission pour le prochain GP"),
		FString::Printf(TEXT("%s.\n\nPrime si c'est réussi : +%.1f M€. En cas d'échec : réputation -3.\nAccepte la mission dans l'écran de la saison avant les qualifications."), *G->MisText, G->MisGain));
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
	TickAcademy();
	TArray<FString> Lines;
	UGPMSaveGame* G = Save();
	if (!G || !G->bCreated) return Lines;
	// direction : jugement du résultat selon le niveau de l'écurie (EXPECT)
	const int32 Exp = Expected();
	const int32 D = R.bDnf ? -4 : FMath::Clamp(FMath::RoundToInt((Exp - R.Pos) * 0.6f), -5, 5);
	G->Conf = FMath::Clamp(G->Conf + D, 0, 100);
	Lines.Add(FString::Printf(TEXT("Direction : confiance %d %% (%s%d) · %s"), G->Conf, D >= 0 ? TEXT("+") : TEXT(""), D, *ConfText(G->Conf)));
	// sponsor principal : part annuelle + bonus si l'objectif de la course est rempli (GOALS de sponsors.js)
	// (contrat renégocié : montant fixe, prime et objectif de la négociation) ; sponsors secondaires de la même façon
	{
		auto GoalOk = [&R](const FString& Goal)
		{
			return Goal == TEXT("finish") ? !R.bDnf : Goal == TEXT("beatMate") ? (R.MatePos == 0 || R.Pos < R.MatePos)
				: Goal == TEXT("gain") ? R.Pos < R.Grid : Goal == TEXT("pts") ? R.Pos <= 10 : Goal == TEXT("top5") ? R.Pos <= 5 : Goal == TEXT("podium") ? R.Pos <= 3
				: Goal == TEXT("fl") ? R.bFastest : false;
		};
		const UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
		const int32 NR = FMath::Max(1, Season ? Season->NumRounds() : 22);
		const FSpDeal Main = MainDeal();
		for (const FGPMSponsorOpt& Sp : Sponsors)
			if (Sp.Id == Main.Id)
			{
				const bool bOk = GoalOk(Main.Goal);
				const float V = R1(Main.Fix / NR + (bOk ? Main.Bonus : 0.f));
				G->Budget = R1(G->Budget + V);
				Lines.Add(FString::Printf(TEXT("%s : +%.1f M€%s"), *Sp.Name, V, bOk ? TEXT(" (objectif rempli)") : TEXT("")));
			}
		float SecV = 0.f;
		int32 SecOk = 0;
		for (const FSpDeal& Dd : SecDeals())
		{
			const bool bOk = !R.bDnf && GoalOk(Dd.Goal);
			SecV += Dd.Fix / NR + (bOk ? Dd.Bonus : 0.f);
			SecOk += bOk;
		}
		if (SecV > 0.f)
		{
			G->Budget = R1(G->Budget + SecV);
			Lines.Add(FString::Printf(TEXT("Sponsors secondaires : +%.1f M€ (%d objectif%s rempli%s)"), R1(SecV), SecOk, SecOk > 1 ? TEXT("s") : TEXT(""), SecOk > 1 ? TEXT("s") : TEXT("")));
		}
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
	if (UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>())
	{
		// mercato des IA à la mi-saison
		if (Season->Round() == Season->NumRounds() / 2) AIMercato(1 + FMath::RandRange(0, 1));
		// rappel : le contrat de l'équipier se termine en fin de saison
		if (G->MateYears <= 1 && Season->Round() == FMath::Max(1, Season->NumRounds() - 4))
			SendMail(TEXT("mercato"), TEXT("Cellule recrutement"), FString::Printf(TEXT("Contrat de %s : fin de saison en vue"), *G->MateName),
				FString::Printf(TEXT("Le contrat de %s se termine après le dernier Grand Prix. Ouvre le mercato pour le prolonger (bouton PROLONGER), ou cherche son remplaçant : sans accord, il partira à l'intersaison et le pilote de réserve prendra son volant."), *G->MateName));
	}
	// une écurie mieux classée peut faire une offre pour l'équipier (après la presse et le flash info)
	RivalOfferCheck(R);
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
		const int32 D = Met * 10 - (Obj.Num() - Met) * 12;
		G->Conf = FMath::Clamp(G->Conf + D, 0, 100);
		const float Bonus = G->Conf >= 80 ? R1(5 + Met * 3) : 0.f;
		G->Budget = R1(G->Budget + Bonus);
		G->bFired = G->Conf < 25;
		if (Bonus > 0) News(FString::Printf(TEXT("La direction est ravie : prime de %.1f M€"), Bonus));
		SendMail(TEXT("direction"), TEXT("Président de l'écurie"), G->bFired ? FString(TEXT("Fin de saison : nous devons parler")) : FString::Printf(TEXT("Bilan de la saison %d"), G->Year),
			FString::Printf(TEXT("Objectifs atteints : %d sur %d.\nConfiance de la direction : %d %% (%s).%s%s"), Met, Obj.Num(), G->Conf, *ConfText(G->Conf),
				Bonus > 0 ? *FString::Printf(TEXT("\nPrime de la direction : +%.1f M€."), Bonus) : TEXT(""), G->bFired ? TEXT("\n\nLa confiance est rompue : la direction cherche ton remplaçant.") : TEXT("")));
		if (UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>()) Season->Save();
	}
	Lines.Add(FString::Printf(TEXT("Bilan de la direction : %d objectif%s sur %d · confiance %d %% (%s)"), Met, Met > 1 ? TEXT("s") : TEXT(""), Obj.Num(), G->Conf, *ConfText(G->Conf)));
	return Lines;
}

void UGPMCareerSubsystem::Intersaison()
{
	UGPMSaveGame* G = Save();
	if (!G || !G->bCreated) return;
	// ambitions de la direction pour la saison qui vient : elles suivent le classement des écuries de la saison écoulée
	{
		const int32 Was = Ambition(), CPos = TeamPos(), Now = CPos <= 2 ? 2 : CPos <= 6 ? 1 : 0;
		G->Ambition = Now;
		if (Now != Was)
		{
			static const TCHAR* Goal[3] = { TEXT("le top 8 des écuries et 25 points"), TEXT("le top 5 des écuries et un podium"), TEXT("le top 2 des écuries et 3 victoires") };
			SendMail(TEXT("direction"), TEXT("Président de l'écurie"), Now > Was ? TEXT("Nos ambitions montent d'un cran") : TEXT("Des objectifs revus à la baisse"),
				FString::Printf(TEXT("Après notre %s place au championnat des écuries, la direction %s ses ambitions. Objectifs de la nouvelle saison : %s. Chaque course sera jugée sur une %de place attendue."),
					CPos == 1 ? TEXT("1re") : *FString::Printf(TEXT("%de"), CPos), Now > Was ? TEXT("relève") : TEXT("abaisse"), Goal[Now], Now == 0 ? 14 : Now == 2 ? 4 : 9));
		}
	}
	// bilan du mercato d'intersaison, envoyé par la presse (les nouvelles sont effacées par la nouvelle saison)
	TArray<FString> Mk;
	// prime d'objectif de l'équipier (négociée au contrat) : place au championnat des pilotes de la saison écoulée
	if (G->MateObj > 0 && G->MateObjPrize > 0.f)
		if (UGPMSeasonSubsystem* Ssn = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>())
		{
			const TArray<FGPMStanding> Drv = Ssn->Drivers();
			const int32 Pos = Drv.IndexOfByPredicate([G](const FGPMStanding& St) { return St.Name == G->MateName; }) + 1;
			const int32 Need = G->MateObj == 1 ? 10 : G->MateObj == 2 ? 6 : 3;
			const bool bMet = Pos > 0 && Pos <= Need;
			if (bMet) G->Budget = R1(G->Budget - G->MateObjPrize);
			SendMail(TEXT("mercato"), TEXT("Cellule recrutement"), FString::Printf(TEXT("Prime d'objectif de %s"), *G->MateName),
				bMet ? FString::Printf(TEXT("%s termine P%d du championnat (objectif : top %d). Prime versée : %.1f M€."), *G->MateName, Pos, Need, G->MateObjPrize)
					: FString::Printf(TEXT("%s termine P%d du championnat (objectif : top %d). Pas de prime à verser."), *G->MateName, FMath::Max(Pos, 1), Need));
		}
	// sponsor principal : versement de début de saison, chaque année (le reste arrive par parts après chaque GP)
	for (const FGPMSponsorOpt& Sp : Sponsors)
		if (Sp.Id == G->Sponsor)
		{
			G->Budget = R1(G->Budget + Sp.Sign);
			SendMail(TEXT("sponsor"), Sp.Name, FString::Printf(TEXT("Saison %d : notre contrat continue"), G->Year + 1),
				FString::Printf(TEXT("Nous restons à vos côtés pour la saison %d.\nVersement de début de saison : %.1f M€ (déjà sur votre compte).\nPuis %.1f M€ par saison, versés par parts après chaque Grand Prix, plus %.1f M€ de bonus à chaque objectif de course rempli."),
					G->Year + 1, Sp.Sign, Sp.Year, Sp.Bonus));
			News(FString::Printf(TEXT("%s verse %.1f M€ pour la nouvelle saison."), *Sp.Name, Sp.Sign));
		}
	// contrats négociés arrivés à échéance : le sponsor principal revient à ses conditions de base, les secondaires s'en vont
	if (!G->MainDeal.IsEmpty() && ParseDeal(G->MainDeal).Until <= G->Year)
	{
		G->MainDeal.Reset();
		SendMail(TEXT("sponsor"), TEXT("Service partenariats"), TEXT("Contrat du sponsor principal à renégocier"),
			TEXT("Le contrat négocié avec ton sponsor principal est arrivé à son terme : il continue aux conditions de base. Tu peux le renégocier depuis l'écran de saison."));
	}
	for (int32 k = G->SecDeals.Num() - 1; k >= 0; --k)
		if (ParseDeal(G->SecDeals[k]).Until <= G->Year)
		{
			const FString Id = ParseDeal(G->SecDeals[k]).Id;
			G->SecDeals.RemoveAt(k);
			for (const FGPMSponsorOpt& Sp : SecSponsors)
				if (Sp.Id == Id) News(FString::Printf(TEXT("Fin du contrat avec %s : tu peux le resigner depuis l'écran de saison."), *Sp.Name));
		}
	// motoriste et salaire de l'équipier payés pour la nouvelle saison ; fin de contrat → pilote de réserve
	for (const FGPMEngineOpt& E : Engines) if (E.Id == G->Engine) G->Budget = R1(G->Budget - E.Price);
	if (--G->MateYears <= 0)
	{
		const FString Old = G->MateName;
		G->Skill.Add(Old, MateSkill(G->MateSpd)); G->DrvPot.Add(Old, G->MatePot);
		G->Free.AddUnique(Old);
		const FString In = PromoteReserve(false);
		Mk.Add(FString::Printf(TEXT("%s arrive en fin de contrat et quitte %s. %s, pilote de réserve, devient titulaire pour une saison."), *Old, *G->TeamName, *In));
	}
	G->Budget = R1(G->Budget - G->MateSal);
	AcademyYear();
	// les jeunes progressent un peu, l'équipier aussi (plus vite avec l'Académie de pilotes) ; après 33 ans, il décline
	UGPMFactorySubsystem* Factory = GetGameInstance()->GetSubsystem<UGPMFactorySubsystem>();
	const float Grow = Factory ? Factory->MateGrowth() : 1.f;
	G->MateSpd = FMath::Min(99.f, G->MateSpd + FMath::Max(0.f, (G->MatePot - G->MateSpd) * 0.15f * Grow));
	if (StatsOf(G->MateName).Age + 1 >= 33) G->MateSpd = FMath::Max(55.f, G->MateSpd - FMath::FRandRange(1.f, 3.f));
	// chefs de pôle : salaires de la nouvelle saison, fins de contrat
	if (Factory) Factory->RenewStaff(G->Year + 1);
	// transferts entre écuries rivales, puis tous les pilotes vieillissent : progression des jeunes, déclin des anciens, retraites ;
	// nouveaux venus de Formule B ; enfin les contrats qui se terminent
	const int32 N0 = G->News.Num();
	AIMercato(2 + FMath::RandRange(0, 1));
	for (int32 k = N0; k < G->News.Num(); ++k) Mk.Add(G->News[k].Replace(TEXT("Mercato : "), TEXT("")));
	Mk.Append(AgeDrivers());
	const TArray<FString> New = Rookies(2 + FMath::RandRange(0, 1));
	if (New.Num()) Mk.Add(FString::Printf(TEXT("Formule B : %s arrive%s sur le marché des pilotes."), *FString::Join(New, TEXT(", ")), New.Num() > 1 ? TEXT("nt") : TEXT("")));
	// contrats des pilotes rivaux qui se terminent : prolongés, ou le pilote part et l'écurie en engage un autre
	if (UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>())
		for (int32 t = 1; t < Data->Teams.Num() && t < BaseTeams.Num(); ++t)
			for (int32 d = 0; d < Data->Teams[t].Drivers.Num(); ++d)
			{
				const FString N = G->Seats.FindRef(FString::Printf(TEXT("%s|%d"), *BaseTeams[t].Name, d));
				if (N.IsEmpty() || UntilOf(N) > G->Year) continue;
				const FGPMMarketDriver D = StatsOf(N);
				const float Keep = D.Age >= 33 ? 0.35f : D.Rating() >= 80.f ? 0.75f : 0.55f;
				if (FMath::FRand() < Keep)
				{
					const int32 Until = G->Year + FMath::RandRange(1, 3);
					G->DrvUntil.Add(N, Until);
					Mk.Add(FString::Printf(TEXT("%s prolonge chez %s jusqu'en %d."), *N, *Data->Teams[t].Name, Until));
				}
				else
				{
					G->DrvUntil.Remove(N);
					const FString In = FillSeat(t, d, FMath::FRand() < 0.3f);
					G->Free.AddUnique(N);
					Mk.Add(FString::Printf(TEXT("Fin de contrat : %s quitte %s, qui engage %s."), *N, *Data->Teams[t].Name, *In));
				}
			}
	// trop de pilotes libres : les plus faibles et les plus âgés quittent la discipline
	while (G->Free.Num() > 16)
	{
		FString Worst; float Low = 1e9f;
		for (const FString& N : G->Free)
		{
			const FGPMMarketDriver D = StatsOf(N);
			const float Sc = D.Rating() - (D.Age >= 32 ? 10.f : 0.f) + (D.Sponsor > 0.f ? 20.f : 0.f);
			if (Sc < Low) { Low = Sc; Worst = N; }
		}
		G->Free.Remove(Worst);
		Mk.Add(FString::Printf(TEXT("Sans volant, %s part courir en endurance."), *Worst));
	}
	ApplyIdentity();
	if (Mk.Num())
		SendMail(TEXT("mercato"), TEXT("Le Paddock (presse)"), FString::Printf(TEXT("Mercato d'intersaison : %d mouvement%s"), Mk.Num(), Mk.Num() > 1 ? TEXT("s") : TEXT("")),
			TEXT("• ") + FString::Join(Mk, TEXT("\n• ")));
}

// ---------- mercato ----------
float UGPMCareerSubsystem::SkillOf(const FString& Name) const
{
	const UGPMSaveGame* G = Save();
	if (G) if (const float* Sk = G->Skill.Find(Name)) return *Sk;
	for (const FGPMMarketDriver& M : Market) if (M.Name == Name) return MateSkill(M.Spd);
	for (const FGPMTeam& T : BaseTeams) for (const FGPMDriver& X : T.Drivers) if (X.Name == Name && X.Skill > 0.f) return X.Skill;
	return 0.94f;
}

FGPMMarketDriver UGPMCareerSubsystem::StatsOf(const FString& Name) const
{
	const UGPMSaveGame* G = Save();
	FGPMMarketDriver D;
	bool bJson = false;
	for (const FGPMMarketDriver& M : Market) if (M.Name == Name) { D = M; bJson = true; break; }
	if (!bJson)
	{
		D.Name = Name;
		// statsOf de career.js : le niveau d'un pilote rival donne ses notes
		D.Spd = FMath::Clamp((SkillOf(Name) - 0.87f) / 0.001f, 50.f, 99.f);
		D.Strat = FMath::Clamp(D.Spd - 4.f + float(GetTypeHash(Name) % 9), 50.f, 99.f);
		D.Tyre = FMath::Clamp(D.Spd - 6.f + float(GetTypeHash(Name + TEXT("t")) % 11), 50.f, 99.f);
		D.Pot = 45.f + float(GetTypeHash(Name + TEXT("p")) % 40);
		D.Age = 22 + int32(GetTypeHash(Name + TEXT("a")) % 14);
	}
	else if (G && G->Skill.Contains(Name)) D.Spd = FMath::Clamp((G->Skill[Name] - 0.87f) / 0.001f, 50.f, 99.f);
	// carrière : âge de la saison en cours, potentiel et provenance des pilotes arrivés depuis
	if (G)
	{
		if (const int32* B = G->DrvBorn.Find(Name)) D.Age = G->Year - *B; else D.Age += G->Year - 2027;
		if (const float* P = G->DrvPot.Find(Name)) D.Pot = *P;
		if (const FString* F = G->DrvFrom.Find(Name)) D.From = *F;
	}
	return D;
}

FGPMMarketDriver UGPMCareerSubsystem::MateAsDriver() const
{
	const UGPMSaveGame* G = Save();
	FGPMMarketDriver D = StatsOf(G ? G->MateName : FString());
	if (!G) return D;
	D.Spd = G->MateSpd; D.Strat = G->MateStrat; D.Tyre = G->MateTyre; D.Pot = G->MatePot;
	D.Sponsor = 0.f; D.Team.Reset(); D.Seat = -1; D.From = G->TeamName;
	return D;
}

int32 UGPMCareerSubsystem::UntilOf(const FString& Name) const
{
	const UGPMSaveGame* G = Save();
	if (!G) return 2027;
	if (const int32* U = G->DrvUntil.Find(Name)) return *U;
	return G->Year + int32(GetTypeHash(Name + TEXT("c")) % 3);
}

float UGPMCareerSubsystem::FeeOf(const FGPMMarketDriver& D) const
{
	if (D.Seat < 0) return 0.f;
	const UGPMSaveGame* G = Save();
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	const bool bMid = Season && Season->Round() > 0 && !Season->IsOver();
	// plus il reste d'années de contrat, plus l'écurie demande cher (un pilote en fin de contrat part presque pour rien)
	const int32 Left = UntilOf(D.Name) - (G ? G->Year : 2027);
	const float K = Left <= 0 ? 0.4f : Left == 1 ? 1.f : 1.4f;
	return FMath::CeilToFloat(FMath::Max(1.f, Salary(D)) * 0.8f * K * (bMid ? MidMult : 1.f) * 2.f) / 2.f;
}

FString UGPMCareerSubsystem::BestFree(float MaxSal) const
{
	const UGPMSaveGame* G = Save();
	FString Best;
	float BestR = -1.f;
	if (!G) return Best;
	for (const FString& N : G->Free)
	{
		if (N == G->MateName || G->Retired.Contains(N)) continue;
		const FGPMMarketDriver D = StatsOf(N);
		if (Salary(D) > MaxSal || D.Rating() <= BestR) continue;
		BestR = D.Rating(); Best = N;
	}
	return Best;
}

TArray<FString> UGPMCareerSubsystem::Rookies(int32 N)
{
	TArray<FString> Out;
	UGPMSaveGame* G = Save();
	if (!G) return Out;
	for (int32 Try = 0; Out.Num() < N && Try < 80; ++Try)
	{
		const FString Name = FString(RookieFirst[FMath::RandRange(0, UE_ARRAY_COUNT(RookieFirst) - 1)]) + TEXT(" ") + RookieLast[FMath::RandRange(0, UE_ARRAY_COUNT(RookieLast) - 1)];
		bool bTaken = G->Free.Contains(Name) || G->Seats.FindKey(Name) || G->Retired.Contains(Name) || G->Skill.Contains(Name) || Name == G->MateName;
		for (const FGPMMarketDriver& M : Market) bTaken |= M.Name == Name;
		for (const FGPMTeam& T : BaseTeams) for (const FGPMDriver& X : T.Drivers) bTaken |= X.Name == Name;
		if (bTaken) continue;
		// classement en Formule B : le champion est plus rapide et a plus de potentiel
		const int32 Rank = FMath::RandRange(1, 8);
		G->Skill.Add(Name, MateSkill(FMath::FRandRange(70.f, 82.f) - Rank * 0.6f));
		G->DrvPot.Add(Name, FMath::Clamp(FMath::FRandRange(72.f, 96.f) - Rank, 62.f, 95.f));
		G->DrvBorn.Add(Name, G->Year - FMath::RandRange(18, 21));
		G->DrvFrom.Add(Name, Rank == 1 ? FString(TEXT("Formule B · champion")) : FString::Printf(TEXT("Formule B · %de"), Rank));
		G->Free.Add(Name);
		Out.Add(Name);
	}
	return Out;
}

FString UGPMCareerSubsystem::FillSeat(int32 T, int32 Seat, bool bRookie)
{
	UGPMSaveGame* G = Save();
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	if (!G || !Data || !BaseTeams.IsValidIndex(T) || !Data->Teams.IsValidIndex(T)) return FString();
	// grande écurie : le meilleur pilote libre ; petite écurie : un pilote abordable, ou un jeune de Formule B
	const bool bTop = Data->Teams[T].Perf >= 0.975f;
	FString In = bRookie && !bTop ? FString() : BestFree(bTop ? 99.f : 3.f);
	if (In.IsEmpty()) { const TArray<FString> R = Rookies(1); if (R.Num()) In = R[0]; }
	if (In.IsEmpty()) return FString();
	G->Free.Remove(In);
	if (!G->Skill.Contains(In)) G->Skill.Add(In, SkillOf(In));
	G->Seats.Add(FString::Printf(TEXT("%s|%d"), *BaseTeams[T].Name, Seat), In);
	G->DrvUntil.Add(In, G->Year + 1 + FMath::RandRange(0, 1));
	G->DriverTeam.Add(In, Data->Teams[T].Name);
	G->DriverPts.FindOrAdd(In);
	return In;
}

TArray<FString> UGPMCareerSubsystem::AgeDrivers()
{
	TArray<FString> Lines;
	UGPMSaveGame* G = Save();
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	if (!G || !Data) return Lines;
	WriteSeats();
	TArray<FString> All;
	for (const TPair<FString, FString>& S : G->Seats) All.AddUnique(S.Value);
	for (const FString& N : G->Free) All.AddUnique(N);
	All.Remove(G->MateName);
	for (const FString& N : All)
	{
		const FGPMMarketDriver D = StatsOf(N);
		const int32 Age = D.Age + 1;   // âge de la saison qui commence
		// retraite : de plus en plus probable après 34 ans
		const float PRet = Age >= 38 ? 1.f : Age >= 36 ? 0.45f : Age >= 34 ? 0.12f : 0.f;
		if (PRet > 0.f && FMath::FRand() < PRet)
		{
			G->Retired.AddUnique(N);
			G->Free.Remove(N);
			FString Line = FString::Printf(TEXT("%s prend sa retraite à %d ans."), *N, Age);
			if (const FString* Key = G->Seats.FindKey(N))
			{
				FString Club, SeatS;
				if (Key->Split(TEXT("|"), &Club, &SeatS, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
					for (int32 t = 1; t < BaseTeams.Num() && t < Data->Teams.Num(); ++t)
						if (BaseTeams[t].Name == Club)
						{
							const FString In = FillSeat(t, FCString::Atoi(*SeatS), FMath::FRand() < 0.4f);
							if (!In.IsEmpty()) Line += FString::Printf(TEXT(" %s prend son volant chez %s."), *In, *Data->Teams[t].Name);
							break;
						}
			}
			Lines.Add(Line);
			continue;
		}
		// progression vers le potentiel chez les jeunes, déclin après 32 ans
		const float Room = FMath::Max(0.f, D.Pot - D.Spd);
		const float Dv = Age <= 24 ? Room * 0.18f + FMath::FRandRange(-1.f, 2.f)
			: Age <= 29 ? Room * 0.08f + FMath::FRandRange(-1.5f, 1.5f)
			: Age <= 32 ? FMath::FRandRange(-2.f, 0.5f) : -FMath::FRandRange(1.5f, 3.5f);
		G->Skill.Add(N, FMath::Clamp(SkillOf(N) + Dv * 0.001f, 0.88f, 0.995f));
		const bool bSeated = G->Seats.FindKey(N) != nullptr;
		if (Dv >= 3.5f) Lines.Add(FString::Printf(TEXT("%s (%d ans) a beaucoup progressé cet hiver."), *N, Age));
		else if (Dv <= -3.f && bSeated) Lines.Add(FString::Printf(TEXT("%s (%d ans) commence à perdre de sa vitesse."), *N, Age));
	}
	return Lines;
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
	if (D.Sponsor > 0.f) return FString::Printf(TEXT("apporte %.1f M€"), D.Sponsor);
	FString S = FString::Printf(TEXT("≈ %.1f M€ / saison"), Salary(D));
	if (D.Seat >= 0) S += FString::Printf(TEXT("\nindemnité ≈ %.1f M€"), FeeOf(D));
	return S;
}

bool UGPMCareerSubsystem::CanExtend() const
{
	const UGPMSaveGame* G = Save();
	return G && G->bCreated && !G->MateName.IsEmpty() && G->MateYears <= 2;
}

void UGPMCareerSubsystem::OpenExtend()
{
	if (CanExtend()) OpenNego(MateAsDriver(), true);
}

void UGPMCareerSubsystem::OpenNego(const FGPMMarketDriver& D, bool bExtend)
{
	const UGPMSaveGame* G = Save();
	if (!G) return;
	Nego = FGPMNego();
	Nego.D = D;
	Nego.bExtend = bExtend;
	const uint32 H = GetTypeHash(D.Name);
	const bool bVet = D.Age >= 32;
	Nego.Pers = PersFor(D.Name, D.Age);
	const FPers& P = PersOf(Nego.Pers);
	const float W = Nego.Pers == TEXT("ambitieux") ? 1.6f : Nego.Pers == TEXT("mercenaire") ? 0.5f : 1.f;
	const float Rt = D.Rating();
	float Int = 45.f + (G->Rep - 50) * 0.7f * W + (6 - TeamPos()) * 2.5f * W - FMath::Max(0.f, Rt - 78.f) * 1.2f + (float(H % 97) / 97.f - 0.5f) * 12.f;
	if (Nego.Pers == TEXT("jeune")) Int += 10.f;
	if (D.Seat >= 0 && UntilOf(D.Name) <= G->Year) Int += 8.f;   // en fin de contrat, il cherche à assurer son avenir
	if (D.Seat < 0 && !bExtend) Int += 5.f;                        // un pilote libre veut retrouver un volant
	Nego.bWantN1 = Rt >= 84.f || (Nego.Pers == TEXT("ambitieux") && Rt >= 80.f);
	if (bExtend)
	{
		Int += 10.f;                                                // il connaît déjà l'écurie
		if (G->MateRole == TEXT("n2") && Nego.bWantN1) Int -= 15.f; // mais il en a assez d'être N°2
	}
	Nego.Interest = FMath::Clamp(FMath::RoundToInt(Int), 5, 95);
	Nego.Patience = FMath::Max(1, P.Pat + (NegoLevel == 0 ? 1 : NegoLevel == 2 ? -1 : 0));   // difficulté des négociations
	Nego.Years = Nego.Pers == TEXT("fidele") ? (bVet ? 2 : 3) : ((Nego.Pers == TEXT("mercenaire") || bVet) ? 1 : 2);
	Nego.Role = Nego.bWantN1 || (bExtend && G->MateRole == TEXT("n1")) ? TEXT("n1") : TEXT("eq");
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	const bool bMid = Season && Season->Round() > 0 && !Season->IsOver();
	const UGPMFactorySubsystem* Factory = GetGameInstance()->GetSubsystem<UGPMFactorySubsystem>();
	Nego.Ask = FMath::Max(0.3f, Salary(D)) * P.Greed * (1.f + (55 - Nego.Interest) / 160.f) * (bMid && !bExtend ? 1.2f : 1.f) * (Factory ? Factory->SalaryCut() : 1.f) * (NegoLevel == 0 ? 0.92f : NegoLevel == 2 ? 1.1f : 1.f);
	if (bExtend && G->MateSal > 0.f) Nego.Ask = FMath::Max(Nego.Ask, G->MateSal * 1.05f);   // jamais moins que son salaire actuel
	Nego.Demand = FMath::CeilToFloat(Nego.Ask * 1.1f * 10.f) / 10.f;
	Nego.Fee = bExtend ? 0.f : FeeOf(D);
	// l'écurie du pilote ne récupère l'équipier actuel que s'il vaut à peu près son pilote, sinon elle engage quelqu'un d'autre
	Nego.bSwap = D.Seat >= 0 && MateAsDriver().Rating() >= Rt - 8.f;
	Nego.bOpen = true;
	Queue.Reset();
	if (G->Broken.Contains(D.Name)) { NegoDialog(TEXT("L'agent ne répond plus à tes appels. Réessaie après le prochain Grand Prix.")); Queue[0].Chosen = 0; Queue[0].Result = TEXT("Discussions rompues."); return; }
	if (D.Sponsor > 0.f && !bExtend)
	{
		Nego.Demand = 0.f; Nego.Years = 1; Nego.Role = TEXT("eq");
		NegoDialog(FString::Printf(TEXT("%s arrive avec son sponsor : %.1f M€ pour l'écurie s'il obtient le volant."), *D.Name, D.Sponsor));
		return;
	}
	if (Nego.Interest < 20)
	{
		NegoDialog(bExtend ? FString(TEXT("Mon client préfère étudier d'autres propositions avant de s'engager.")) : FString::Printf(TEXT("Mon client ne souhaite pas rejoindre %s pour le moment."), *G->TeamName));
		Queue[0].Chosen = 0;
		Queue[0].Result = bExtend ? FString::Printf(TEXT("%s ne veut pas prolonger pour l'instant (intérêt %d)."), *D.Name, Nego.Interest)
			: FString::Printf(TEXT("%s refuse de discuter (intérêt %d)."), *D.Name, Nego.Interest);
		return;
	}
	const TCHAR* Ys = Nego.Years > 1 ? TEXT("s") : TEXT("");
	if (bExtend)
	{
		OfferDialog(FString::Printf(TEXT("Mon client est prêt à prolonger de %d saison%s pour %.1f M€ par saison%s."), Nego.Years, Ys, Nego.Demand,
			Nego.bWantN1 ? TEXT(", avec le statut de pilote N°1") : TEXT("")));
		return;
	}
	FString Line = FString::Printf(TEXT("Mon client demande %.1f M€ par saison sur %d saison%s%s."), Nego.Demand, Nego.Years, Ys,
		Nego.bWantN1 ? TEXT(", avec le statut de pilote N°1") : TEXT(""));
	if (Nego.Fee > 0.f)
	{
		const int32 Until = UntilOf(D.Name);
		Line += Until <= G->Year ? FString(TEXT(" Son contrat se termine en fin de saison.")) : FString::Printf(TEXT(" Il est sous contrat jusqu'en %d."), Until);
		Line += Nego.bSwap ? FString::Printf(TEXT(" %s demande %.1f M€ d'indemnité pour le libérer, et récupère %s."), *D.Team, Nego.Fee, *G->MateName)
			: FString::Printf(TEXT(" %s demande %.1f M€ d'indemnité pour le libérer ; %s ne l'intéresse pas et redeviendra pilote libre."), *D.Team, Nego.Fee, *G->MateName);
	}
	OfferDialog(Line);
}

void UGPMCareerSubsystem::NegoDialog(const FString& Line)
{
	const UGPMSaveGame* G = Save();
	FGPMDialog Dl;
	Dl.Kind = FGPMDialog::EKind::Nego;
	Dl.Color = FLinearColor(0.2f, 0.6f, 1.f);
	const FPers& P = PersOf(Nego.Pers);
	Dl.Kicker = FString::Printf(TEXT("%s · %s · %s · intérêt %d · patience %d"), Nego.bExtend ? TEXT("PROLONGATION") : TEXT("NÉGOCIATION"), *Nego.D.Name, P.Name, Nego.Interest, Nego.Patience);
	Dl.Title = FString::Printf(TEXT("%s, %d ans · note %.0f (vitesse %.0f, stratégie %.0f, pneus %.0f, potentiel %.0f)"), *Nego.D.From, Nego.D.Age, Nego.D.Rating(),
		Nego.D.Spd, Nego.D.Strat, Nego.D.Tyre, Nego.D.Pot);
	Dl.Text = TEXT("« ") + Line + TEXT(" »");
	const float Budget = G ? G->Budget : 0.f;
	// une prolongation ou un contrat signé après la dernière course ne coûte rien tout de suite : le salaire est payé à l'intersaison
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	const bool bPayNow = !Nego.bExtend && !(Season && Season->IsOver());
	auto Add = [&Dl, Budget, bPayNow, this](const FString& Txt, int32 Act, float Sal)
	{
		FGPMChoice C; C.Text = Txt; C.Action = Act;
		C.bOff = Act != 4 && (bPayNow ? Sal : 0.f) + Nego.Fee > Budget + 1e-3f;
		if (C.bOff) C.Text += TEXT(" (budget insuffisant)");
		Dl.Choices.Add(C);
	};
	const TCHAR* Ys = Nego.Years > 1 ? TEXT("s") : TEXT("");
	if (Nego.D.Sponsor > 0.f && !Nego.bExtend) Add(FString::Printf(TEXT("Signer %s (1 saison, apporte %.1f M€)"), *Nego.D.Name, Nego.D.Sponsor), 0, 0.f);
	else
	{
		Add(FString::Printf(TEXT("%s : %.1f M€ × %d saison%s%s, statut %s"), Nego.bExtend ? TEXT("Prolonger") : TEXT("Accepter"), Nego.Demand, Nego.Years, Ys,
			Nego.bExtend ? TEXT(" de plus") : TEXT(""), RoleName(Nego.Role)), 0, Nego.Demand);
		Add(FString::Printf(TEXT("Proposer %.1f M€ par saison"), R1(Nego.Demand * 0.9f)), 1, R1(Nego.Demand * 0.9f));
		Add(FString::Printf(TEXT("Proposer %.1f M€ par saison"), R1(Nego.Demand * 0.78f)), 2, R1(Nego.Demand * 0.78f));
		if (Nego.Role != TEXT("n2")) Add(FString::Printf(TEXT("Proposer %.1f M€ avec le statut N°2"), R1(Nego.Demand * 0.85f)), 3, R1(Nego.Demand * 0.85f));
		Add(FString::Printf(TEXT("Proposer %.1f M€ + primes (%.1f M€ par podium, %.1f M€ par victoire)"), R1(Nego.Demand * 0.82f), PodBonus(Nego.Demand), WinBonus(Nego.Demand)), 5,
			R1(Nego.Demand * 0.82f));
		Add(FString::Printf(TEXT("Proposer %.1f M€ avec une clause libératoire de %.1f M€ (un rival pourra le racheter à ce prix)"), R1(Nego.Demand * 0.9f), ClauseOf(Nego.Demand)), 6,
			R1(Nego.Demand * 0.9f));
	}
	Add(TEXT("Quitter la négociation"), 4, 0.f);
	if (Queue.Num()) Queue[0] = Dl; else Queue.Add(Dl);
}

// ---------- offre à la carte (façon FIFA) ----------
namespace GPMOfferK
{
	const float SignK[4] = { 0.f, 0.15f, 0.3f, 0.6f };   // prime à la signature, part de ce qu'il demande par saison
	const TCHAR* RoleCycle[3] = { TEXT("eq"), TEXT("n1"), TEXT("n2") };
	float R1o(float V) { return FMath::RoundToFloat(V * 10.f) / 10.f; }
}

// chance que l'équipier atteigne l'objectif (top 10 / top 6 / podium du championnat) selon la place de l'écurie
float UGPMCareerSubsystem::ObjChance(int32 Lvl) const
{
	const int32 P = TeamPos();
	if (Lvl == 1) return P <= 6 ? 0.85f : P <= 8 ? 0.5f : 0.2f;
	if (Lvl == 2) return P <= 3 ? 0.75f : P <= 6 ? 0.4f : 0.1f;
	if (Lvl == 3) return P <= 2 ? 0.5f : P <= 4 ? 0.2f : 0.04f;
	return 0.f;
}

float UGPMCareerSubsystem::OfferValue(float& OutCost) const
{
	using namespace GPMOfferK;
	const UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	const bool bPayNow = !Nego.bExtend && !(Season && Season->IsOver());
	const float Ask = FMath::Max(0.3f, Nego.Ask);
	const float SignAmt = R1o(Ask * SignK[FMath::Clamp(Nego.OSignL, 0, 3)]);
	const float Pod = PodBonus(Ask) * Nego.OBonusL, Win = WinBonus(Ask) * Nego.OBonusL;
	OutCost = R1o((bPayNow ? Nego.OSal : 0.f) + SignAmt + Nego.Fee);
	// podiums et victoires espérés par saison selon la place de l'écurie ; un ambitieux y croit plus qu'un mercenaire
	const int32 P = TeamPos();
	const float Pods = P <= 2 ? 6.f : P <= 4 ? 3.f : P <= 6 ? 1.f : 0.3f, Wins = P <= 2 ? 2.f : P <= 4 ? 0.7f : P <= 6 ? 0.2f : 0.05f;
	const FString& Ps = Nego.Pers;
	const float BonusW = Ps == TEXT("ambitieux") ? 0.9f : Ps == TEXT("jeune") ? 0.8f : Ps == TEXT("fidele") ? 0.6f : 0.4f;
	const float V = Nego.OSal + SignAmt / FMath::Max(1, Nego.OYears) + (Pod * Pods + Win * Wins) * BonusW + ObjPrizeOf(Nego.OObj, Ask) * ObjChance(Nego.OObj) * 0.9f;
	const FString& Ro = Nego.ORole;
	const float RoleT = Nego.bWantN1 ? (Ro == TEXT("n1") ? 0.12f : Ro == TEXT("n2") ? -0.3f : -0.12f) : (Ro == TEXT("n1") ? 0.05f : Ro == TEXT("n2") ? -0.08f : 0.f);
	// durée : un fidèle ou un vétéran veut de la sécurité, un mercenaire garde sa liberté
	const float Yx = float(Nego.OYears - 1);
	const float YearsT = Nego.D.Age >= 32 ? 0.04f * Yx : Ps == TEXT("fidele") ? 0.03f * Yx : Ps == TEXT("mercenaire") ? -0.04f * Yx : Ps == TEXT("jeune") ? 0.02f * Yx : 0.f;
	// clause libératoire basse : une porte de sortie, appréciée des mercenaires et des ambitieux
	const float ClauseW = Ps == TEXT("mercenaire") ? 0.12f : Ps == TEXT("ambitieux") ? 0.1f : Ps == TEXT("jeune") ? 0.06f : 0.03f;
	const float ClauseT = Nego.OClauseL == 2 ? ClauseW : Nego.OClauseL == 1 ? ClauseW * 0.4f : 0.f;
	return V / Ask + RoleT + YearsT + ClauseT;
}

void UGPMCareerSubsystem::OfferDialog(const FString& Line)
{
	using namespace GPMOfferK;
	const UGPMSaveGame* G = Save();
	if (!Nego.bBuilder)
	{
		Nego.bBuilder = true;
		Nego.OSal = R1o(FMath::Max(0.3f, Nego.Ask)); Nego.OYears = Nego.Years; Nego.ORole = Nego.Role;
		Nego.OSignL = Nego.OBonusL = Nego.OObj = Nego.OClauseL = 0;
	}
	Nego.OLine = Line;
	FGPMDialog Dl;
	Dl.Kind = FGPMDialog::EKind::Nego;
	Dl.Color = FLinearColor(0.2f, 0.6f, 1.f);
	const FPers& P = PersOf(Nego.Pers);
	Dl.Kicker = FString::Printf(TEXT("%s · %s · %s · intérêt %d · patience %d"), Nego.bExtend ? TEXT("PROLONGATION") : TEXT("NÉGOCIATION"), *Nego.D.Name, P.Name, Nego.Interest, Nego.Patience);
	Dl.Title = FString::Printf(TEXT("%s, %d ans · note %.0f (vitesse %.0f, stratégie %.0f, pneus %.0f, potentiel %.0f)"), *Nego.D.From, Nego.D.Age, Nego.D.Rating(),
		Nego.D.Spd, Nego.D.Strat, Nego.D.Tyre, Nego.D.Pot);
	float Cost = 0.f;
	const float Score = OfferValue(Cost);
	const float Thr = 1.f - (Nego.Interest - 50) / 500.f + (NegoLevel - 1) * 0.05f;
	const TCHAR* Mood = Score >= Thr + 0.02f ? TEXT("il devrait accepter") : Score >= Thr - 0.14f ? TEXT("il hésite, encore un effort") : TEXT("offre trop basse, il risque de se fâcher");
	const float Budget = G ? G->Budget : 0.f;
	Dl.Text = FString::Printf(TEXT("« %s »\n\nCoût immédiat de ton offre : %.1f M€ (budget %.1f M€)%s · Avis de l'agent : %s."), *Line, Cost, Budget,
		Nego.Fee > 0.f ? *FString::Printf(TEXT(", dont %.1f M€ d'indemnité"), Nego.Fee) : TEXT(""), Mood);
	const float Ask = FMath::Max(0.3f, Nego.Ask);
	auto Add = [&Dl](const FString& Txt, int32 Act, bool bOff = false) { FGPMChoice C; C.Text = Txt; C.Action = Act; C.bOff = bOff; Dl.Choices.Add(C); };
	const FString& Ro = Nego.ORole;
	Add(TEXT("−"), 20, Nego.OSal <= 0.31f);
	Add(FString::Printf(TEXT("Salaire|%.1f M€ par saison"), Nego.OSal), 21);
	Add(FString::Printf(TEXT("Durée|%d saison%s"), Nego.OYears, Nego.OYears > 1 ? TEXT("s") : TEXT("")), 22);
	Add(FString::Printf(TEXT("Statut|%s"), Ro == TEXT("n1") ? TEXT("Pilote N°1") : Ro == TEXT("n2") ? TEXT("Pilote N°2 (il te laisse passer)") : TEXT("Égalité")), 24);
	const float SignAmt = R1o(Ask * SignK[FMath::Clamp(Nego.OSignL, 0, 3)]);
	Add(FString::Printf(TEXT("Prime à la signature|%s"), SignAmt > 0.f ? *FString::Printf(TEXT("%.1f M€"), SignAmt) : TEXT("Aucune")), 25);
	Add(FString::Printf(TEXT("Primes de résultats|%s"), Nego.OBonusL ? *FString::Printf(TEXT("%.1f M€ par podium · %.1f M€ par victoire"), PodBonus(Ask) * Nego.OBonusL, WinBonus(Ask) * Nego.OBonusL) : TEXT("Aucune")), 26);
	Add(FString::Printf(TEXT("Prime d'objectif|%s"), Nego.OObj ? *FString::Printf(TEXT("%.1f M€ s'il finit %s du championnat"), ObjPrizeOf(Nego.OObj, Ask),
		Nego.OObj == 1 ? TEXT("dans le top 10") : Nego.OObj == 2 ? TEXT("dans le top 6") : TEXT("sur le podium")) : TEXT("Aucune")), 27);
	Add(FString::Printf(TEXT("Clause libératoire|%s"), Nego.OClauseL == 1 ? *FString::Printf(TEXT("Élevée : %.1f M€"), R1o(Ask * 6.f)) : Nego.OClauseL == 2 ? *FString::Printf(TEXT("Basse : %.1f M€ (un rival pourra le racheter)"), R1o(Ask * 3.f)) : TEXT("Aucune")), 28);
	Add(TEXT("ENVOYER L'OFFRE"), 30, Cost > Budget + 1e-3f);
	const UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	const bool bPayNow = !Nego.bExtend && !(Season && Season->IsOver());
	const float DemCost = R1o((bPayNow ? Nego.Demand : 0.f) + Nego.Fee);
	Add(FString::Printf(TEXT("ACCEPTER SA DEMANDE (%.1f M€, %d saison%s)"), Nego.Demand, Nego.Years, Nego.Years > 1 ? TEXT("s") : TEXT("")), 31, DemCost > Budget + 1e-3f);
	Add(TEXT("QUITTER"), 4);
	if (Queue.Num()) Queue[0] = Dl; else Queue.Add(Dl);
}

void UGPMCareerSubsystem::BuildAct(int32 Action)
{
	using namespace GPMOfferK;
	UGPMSaveGame* G = Save();
	FGPMDialog* Dl = Current();
	if (!G || !Dl) return;
	const float Ask = FMath::Max(0.3f, Nego.Ask), Step = R1o(FMath::Max(0.1f, Ask * 0.05f));
	switch (Action)
	{
	case 20: Nego.OSal = R1o(FMath::Max(0.3f, Nego.OSal - Step)); break;
	case 21: Nego.OSal = R1o(FMath::Min(Ask * 2.f, Nego.OSal + Step)); break;
	case 22: Nego.OYears = Nego.OYears % 3 + 1; break;
	case 24:
	{
		int32 k = 0;
		while (k < 3 && Nego.ORole != RoleCycle[k]) ++k;
		Nego.ORole = RoleCycle[(k + 1) % 3];
		break;
	}
	case 25: Nego.OSignL = (Nego.OSignL + 1) % 4; break;
	case 26: Nego.OBonusL = (Nego.OBonusL + 1) % 3; break;
	case 27: Nego.OObj = (Nego.OObj + 1) % 4; break;
	case 28: Nego.OClauseL = (Nego.OClauseL + 1) % 3; break;
	default: break;
	}
	auto Close = [&]()
	{
		Nego.Demand = Nego.OSal; Nego.Role = Nego.ORole; Nego.Years = Nego.OYears;
		Nego.Pod = PodBonus(Ask) * Nego.OBonusL; Nego.Win = WinBonus(Ask) * Nego.OBonusL;
		Nego.Clause = Nego.OClauseL == 1 ? R1o(Ask * 6.f) : Nego.OClauseL == 2 ? R1o(Ask * 3.f) : 0.f;
		Nego.SignB = R1o(Ask * SignK[FMath::Clamp(Nego.OSignL, 0, 3)]);
		Nego.Obj = Nego.OObj; Nego.ObjPrize = ObjPrizeOf(Nego.OObj, Ask);
		Sign();
	};
	if (Action == 31) { Nego.OSal = FMath::Max(Nego.OSal, Nego.Demand); Nego.OYears = Nego.Years; if (Nego.bWantN1) Nego.ORole = TEXT("n1"); Close(); return; }
	if (Action != 30) { OfferDialog(Nego.OLine); return; }
	float Cost = 0.f;
	const float Score = OfferValue(Cost);
	const float Thr = 1.f - (Nego.Interest - 50) / 500.f + (NegoLevel - 1) * 0.05f + (FMath::FRand() - 0.5f) * 0.04f;
	if (Score >= Thr) { Close(); return; }
	FString Line;
	if (Score >= Thr - 0.14f)
	{
		Nego.Patience -= 1;
		const float Missing = (Thr - Score) * Ask;
		Nego.Demand = FMath::CeilToFloat((Nego.OSal + Missing * 1.05f + 0.05f) * 10.f) / 10.f;
		Line = Nego.bWantN1 && Nego.ORole != TEXT("n1") ? FString::Printf(TEXT("On s'en approche, mais mon client veut le statut de N°1. Sinon, il faudra %.1f M€ par saison."), Nego.Demand)
			: Nego.OYears == 1 && Nego.D.Age >= 32 ? FString::Printf(TEXT("Il veut de la sécurité : une saison de plus, ou %.1f M€ par saison."), Nego.Demand)
			: FString::Printf(TEXT("On s'en approche. Il manque %.1f M€ par saison : en salaire, en primes ou à la signature, à vous de voir."), R1o(Missing + 0.05f));
	}
	else
	{
		Nego.Patience -= 2;
		Nego.Interest = FMath::Max(5, Nego.Interest - 8);
		Line = TEXT("C'est une plaisanterie ? Revenez avec une offre sérieuse.");
	}
	if (Nego.Patience > 0) { OfferDialog(Line); return; }
	G->Broken.AddUnique(Nego.D.Name);
	SendMail(TEXT("mercato"), FString::Printf(TEXT("Agent de %s"), *Nego.D.Name), FString::Printf(TEXT("Discussions rompues : %s"), *Nego.D.Name),
		TEXT("Mon client ne donne pas suite. Nous pourrons reparler après le prochain Grand Prix, avec une offre sérieuse."));
	Dl->Chosen = 0;
	Dl->bBad = true;
	Dl->Result = FString::Printf(TEXT("L'agent de %s claque la porte : discussions rompues jusqu'au prochain Grand Prix."), *Nego.D.Name);
}

void UGPMCareerSubsystem::NegoAct(int32 Action)
{
	UGPMSaveGame* G = Save();
	FGPMDialog* Dl = Current();
	if (!G || !Dl) return;
	if ((Action >= 40 && Action < 60) || Action >= 100) { SponsorAct(Action); return; }
	if (Action >= 20 && Action < 40) { BuildAct(Action); return; }
	if (Action >= 10) { OfferAct(Action); return; }
	if (Action == 4) { Dl->Chosen = 0; Dl->Result = TEXT("Tu quittes la table. L'agent garde ton numéro."); return; }
	if (Action == 0) { Sign(); return; }
	const float Bid = R1(Nego.Demand * (Action == 1 || Action == 6 ? 0.9f : Action == 2 ? 0.78f : Action == 5 ? 0.82f : 0.85f));
	const FString Role = Action == 3 ? FString(TEXT("n2")) : Nego.Role;
	const float RoleT = Nego.bWantN1 ? (Role == TEXT("n1") ? 0.12f : Role == TEXT("n2") ? -0.3f : 0.f) : (Role == TEXT("n1") ? 0.05f : Role == TEXT("n2") ? -0.08f : 0.f);
	// primes : un ambitieux ou un jeune loup y croit plus qu'un mercenaire
	const float Pod = Action == 5 ? PodBonus(Nego.Demand) : 0.f, Win = Action == 5 ? WinBonus(Nego.Demand) : 0.f;
	const float BonusW = Nego.Pers == TEXT("ambitieux") ? 0.9f : Nego.Pers == TEXT("jeune") ? 0.8f : Nego.Pers == TEXT("fidele") ? 0.6f : 0.4f;
	// clause libératoire : un mercenaire ou un ambitieux aime garder une porte de sortie, un fidèle s'en moque
	const float Clause = Action == 6 ? ClauseOf(Nego.Demand) : 0.f;
	const float ClauseT = Action != 6 ? 0.f : Nego.Pers == TEXT("mercenaire") ? 0.12f : Nego.Pers == TEXT("ambitieux") ? 0.1f : Nego.Pers == TEXT("jeune") ? 0.06f : 0.03f;
	const float Score = (Bid + (Pod + Win) * BonusW) / Nego.Ask + RoleT + ClauseT - (Nego.bWantN1 && Role != TEXT("n1") ? 0.12f : 0.f);
	const float Thr = 1.f - (Nego.Interest - 50) / 500.f + (NegoLevel - 1) * 0.05f + (FMath::FRand() - 0.5f) * 0.04f;
	if (Score >= Thr)
	{
		Nego.Demand = Bid; Nego.Role = Role; Nego.Pod = Pod; Nego.Win = Win; Nego.Clause = Clause;
		Sign();
		return;
	}
	if (Score >= Thr - 0.14f)
	{
		Nego.Patience -= 1;
		Nego.Role = Role;
		Nego.Demand = FMath::CeilToFloat(FMath::Max(Bid * 1.04f, Nego.Ask * (Thr - RoleT) * 1.02f) * 10.f) / 10.f;
		if (Nego.Patience > 0) { NegoDialog(FString::Printf(TEXT("On s'en approche. Mon client signerait pour %.1f M€ par saison, statut %s."), Nego.Demand, RoleName(Nego.Role))); return; }
	}
	else
	{
		Nego.Patience -= 2;
		Nego.Interest = FMath::Max(5, Nego.Interest - 8);
		if (Nego.Patience > 0) { NegoDialog(TEXT("C'est une plaisanterie ? Revenez avec une offre sérieuse.")); return; }
	}
	G->Broken.AddUnique(Nego.D.Name);
	SendMail(TEXT("mercato"), FString::Printf(TEXT("Agent de %s"), *Nego.D.Name), FString::Printf(TEXT("Discussions rompues : %s"), *Nego.D.Name),
		TEXT("Mon client ne donne pas suite. Nous pourrons reparler après le prochain Grand Prix, avec une offre sérieuse."));
	Dl->Chosen = 0;
	Dl->bBad = true;
	Dl->Result = FString::Printf(TEXT("L'agent de %s claque la porte : discussions rompues jusqu'au prochain Grand Prix."), *Nego.D.Name);
}

void UGPMCareerSubsystem::Sign()
{
	UGPMSaveGame* G = Save();
	FGPMDialog* Dl = Current();
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	if (!G || !Dl || !Data) return;
	// après la dernière course, la saison est finie : le contrat commence à la saison suivante et le salaire est payé à l'intersaison
	const bool bOver = Season && Season->IsOver();
	const bool bMid = Season && Season->Round() > 0 && !bOver;
	const float Sal = Nego.D.Sponsor > 0.f ? 0.f : Nego.Demand;
	const float Cost = R1((Nego.bExtend || bOver ? 0.f : Sal) + Nego.Fee + Nego.SignB - Nego.D.Sponsor);
	if (Cost > G->Budget + 1e-3f) { Dl->Chosen = 0; Dl->bBad = true; Dl->Result = TEXT("Budget insuffisant pour signer ce contrat."); return; }
	G->Budget = R1(G->Budget - Cost);
	const TCHAR* Ys = Nego.Years > 1 ? TEXT("s") : TEXT("");
	FString Extra;
	if (Nego.Pod > 0.f || Nego.Win > 0.f) Extra += FString::Printf(TEXT(" · primes %.1f M€ par podium, %.1f M€ par victoire"), Nego.Pod, Nego.Win);
	if (Nego.Clause > 0.f) Extra += FString::Printf(TEXT(" · clause libératoire %.1f M€"), Nego.Clause);
	if (Nego.SignB > 0.f) Extra += FString::Printf(TEXT(" · prime à la signature %.1f M€"), Nego.SignB);
	if (Nego.Obj > 0) Extra += FString::Printf(TEXT(" · prime d'objectif %.1f M€ s'il finit %s du championnat"), Nego.ObjPrize, Nego.Obj == 1 ? TEXT("dans le top 10") : Nego.Obj == 2 ? TEXT("dans le top 6") : TEXT("sur le podium"));
	Dl->Chosen = 0;
	Dl->bBad = false;
	if (Nego.bExtend)
	{
		// prolongation : les saisons s'ajoutent au contrat en cours, le nouveau salaire compte dès la saison prochaine
		G->MateYears += Nego.Years;
		G->MateSal = Sal; G->MateRole = Nego.Role; G->MatePod = Nego.Pod; G->MateWin = Nego.Win; G->MateClause = Nego.Clause; G->MateObj = Nego.Obj; G->MateObjPrize = Nego.ObjPrize;
		const int32 Until = G->Year + G->MateYears - 1;
		ApplyIdentity();
		News(FString::Printf(TEXT("%s prolonge chez %s jusqu'en %d."), *Nego.D.Name, *G->TeamName, Until));
		SendMail(TEXT("mercato"), TEXT("Cellule recrutement"), FString::Printf(TEXT("Prolongation : %s"), *Nego.D.Name),
			FString::Printf(TEXT("%s prolonge son contrat jusqu'en %d.\nSalaire : %.1f M€ par saison dès la saison prochaine · statut %s%s"), *Nego.D.Name, Until, Sal, RoleName(Nego.Role), *Extra));
		Dl->Result = FString::Printf(TEXT("%s prolonge jusqu'en %d (%.1f M€ par saison dès la saison prochaine, statut %s)."), *Nego.D.Name, Until, Sal, RoleName(Nego.Role));
	}
	else
	{
		const FString Old = G->MateName;
		G->Skill.Add(Old, MateSkill(G->MateSpd)); G->DrvPot.Add(Old, G->MatePot);
		// pilote d'une écurie rivale : elle récupère l'équipier s'il lui plaît, sinon elle engage un autre pilote et l'équipier devient libre
		FString OldGoes = FString::Printf(TEXT("%s redevient pilote libre."), *Old);
		if (Nego.D.Seat >= 0)
		{
			WriteSeats();
			for (int32 t = 1; t < Data->Teams.Num(); ++t)
				if (Data->Teams[t].Name == Nego.D.Team && BaseTeams.IsValidIndex(t))
				{
					if (Nego.bSwap)
					{
						G->Seats.Add(FString::Printf(TEXT("%s|%d"), *BaseTeams[t].Name, Nego.D.Seat), Old);
						G->DriverTeam.Add(Old, Nego.D.Team);
						G->DrvUntil.Add(Old, G->Year + 1 + FMath::RandRange(0, 1));
						OldGoes = FString::Printf(TEXT("%s part chez %s dans l'échange."), *Old, *Nego.D.Team);
					}
					else
					{
						const FString In = FillSeat(t, Nego.D.Seat, false);
						G->Free.AddUnique(Old);
						OldGoes = FString::Printf(TEXT("%s redevient pilote libre ; %s engage %s pour remplacer %s."), *Old, *Nego.D.Team, *In, *Nego.D.Name);
					}
				}
			G->DrvUntil.Remove(Nego.D.Name);
		}
		else { G->Free.Remove(Nego.D.Name); G->Free.AddUnique(Old); }
		G->MateName = Nego.D.Name; G->MateSpd = Nego.D.Spd; G->MateStrat = Nego.D.Strat; G->MateTyre = Nego.D.Tyre; G->MatePot = Nego.D.Pot;
		// signé en cours de saison : le contrat couvre la fin de la saison en plus des saisons négociées
		G->MateSal = Sal; G->MateYears = Nego.Years + (bOver || bMid ? 1 : 0); G->MateRole = Nego.Role; G->MateSince = G->Year;
		G->MatePod = Nego.Pod; G->MateWin = Nego.Win; G->MateClause = Nego.Clause; G->MateObj = Nego.Obj; G->MateObjPrize = Nego.ObjPrize;
		G->DriverTeam.Add(Nego.D.Name, G->TeamName);
		G->DriverPts.FindOrAdd(Nego.D.Name);
		G->DriverPts.FindOrAdd(Old);
		ApplyIdentity();
		const int32 Until = G->Year + G->MateYears - 1;
		News(FString::Printf(TEXT("%s signe chez %s jusqu'en %d ! %s"), *Nego.D.Name, *G->TeamName, Until, *OldGoes));
		SendMail(TEXT("mercato"), TEXT("Cellule recrutement"), FString::Printf(TEXT("Contrat signé : %s"), *Nego.D.Name),
			FString::Printf(TEXT("%s rejoint %s pour %d saison%s (jusqu'en %d).\nSalaire : %.1f M€ par saison · statut %s%s\nCoût immédiat : %.1f M€.\n%s"), *Nego.D.Name, *G->TeamName,
				Nego.Years, Ys, Until, Sal, RoleName(Nego.Role), *Extra, Cost, *OldGoes));
		Dl->Result = FString::Printf(TEXT("%s signe jusqu'en %d (%.1f M€ par saison, statut %s). Coût immédiat : %.1f M€. %s"), *Nego.D.Name, Until, Sal, RoleName(Nego.Role), Cost, *OldGoes);
	}
	if (Nego.Pod > 0.f || Nego.Win > 0.f) Dl->Result += FString::Printf(TEXT(" Primes : %.1f M€ par podium, %.1f M€ par victoire."), Nego.Pod, Nego.Win);
	if (Nego.Clause > 0.f) Dl->Result += FString::Printf(TEXT(" Clause libératoire : %.1f M€."), Nego.Clause);
	Nego.bOpen = false;
	bJustSigned = true;
	if (Season) Season->Save();
}

// ---------- offres des rivaux pour l'équipier ----------
void UGPMCareerSubsystem::RivalOfferCheck(const FGPMRaceCtx& R)
{
	UGPMSaveGame* G = Save();
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	if (!G || !Season || G->MateName.IsEmpty()) return;
	const int32 Rd = Season->Round();
	if (Season->IsOver() || Rd >= Season->NumRounds() - 1 || Rd - G->MateOfferAt < 4) return;
	// un équipier rapide, surtout après un podium, attire les écuries mieux classées
	const float Rt = MateAsDriver().Rating();
	float P = Rt >= 82.f ? 0.14f : Rt >= 76.f ? 0.08f : 0.03f;
	if (R.MatePos >= 1 && R.MatePos <= 3) P += 0.12f;
	else if (R.MatePos >= 1 && R.MatePos <= 6) P += 0.04f;
	if (FMath::FRand() >= P) return;
	const TArray<FGPMStanding> Tm = Season->Teams();
	const int32 Me = TeamPos();
	TArray<FString> Better;
	for (int32 k = 0; k < Tm.Num(); ++k) if (!Tm[k].bPlayer && (k < Me - 1 || (Me <= 2 && k < 4))) Better.Add(Tm[k].Name);
	if (Better.Num()) MakeOffer(Better[FMath::RandRange(0, Better.Num() - 1)]);
}

void UGPMCareerSubsystem::DebugRivalOffer()
{
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	if (!Season || !Data || Data->Teams.Num() < 2) return;
	Queue.Reset();
	MakeOffer(Data->Teams[1].Name);
}

void UGPMCareerSubsystem::MakeOffer(const FString& Team)
{
	UGPMSaveGame* G = Save();
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	if (!G) return;
	const FGPMMarketDriver M = MateAsDriver();
	Offer.Team = Team;
	Offer.Fee = R1(FMath::Max(1.5f, Salary(M)) * (0.8f + 0.6f * FMath::Max(1, G->MateYears)) * 1.5f);
	G->MateOfferAt = Season ? Season->Round() : 0;
	FGPMDialog Dl;
	Dl.Kind = FGPMDialog::EKind::Nego;
	Dl.Color = FLinearColor(1.f, 0.55f, 0.12f);
	auto Add = [&Dl](const FString& T, int32 A) { FGPMChoice C; C.Text = T; C.Action = A; Dl.Choices.Add(C); };
	// clause libératoire : si elle est assez basse, l'écurie la paie et l'équipier part sans qu'on puisse s'y opposer
	if (G->MateClause > 0.f && G->MateClause <= Offer.Fee * 1.6f)
	{
		Offer.Fee = G->MateClause;
		Dl.Kicker = TEXT("MERCATO · CLAUSE LIBÉRATOIRE");
		Dl.Title = FString::Printf(TEXT("%s lève la clause de %s"), *Team, *G->MateName);
		Dl.Text = FString::Printf(TEXT("« %s verse les %.1f M€ de la clause libératoire. Le contrat de %s ne te laisse pas le choix : il part dès le prochain Grand Prix. »"), *Team, Offer.Fee,
			*G->MateName);
		Add(TEXT("Prendre acte et faire monter le pilote de réserve"), 13);
	}
	else
	{
		Dl.Kicker = TEXT("MERCATO · OFFRE DE TRANSFERT");
		Dl.Title = FString::Printf(TEXT("%s veut recruter %s"), *Team, *G->MateName);
		Dl.Text = FString::Printf(TEXT("« Nous proposons %.1f M€ d'indemnité pour libérer %s, sous contrat chez vous jusqu'en %d. Il rejoindrait notre écurie dès le prochain Grand Prix. »"),
			Offer.Fee, *G->MateName, G->Year + G->MateYears - 1);
		Add(FString::Printf(TEXT("Accepter : +%.1f M€, le pilote de réserve prend son volant"), Offer.Fee), 10);
		Add(TEXT("Refuser : il est sous contrat et il reste"), 11);
		Add(FString::Printf(TEXT("Refuser et le prolonger d'une saison (salaire porté à %.1f M€)"), R1(G->MateSal * 1.2f + 0.1f)), 12);
	}
	Queue.Add(Dl);
}

void UGPMCareerSubsystem::OfferAct(int32 Action)
{
	UGPMSaveGame* G = Save();
	FGPMDialog* Dl = Current();
	if (!G || !Dl) return;
	const FString Mate = G->MateName;
	Dl->Chosen = 0;
	Dl->bBad = false;
	FString Subj;
	if (Action == 10 || Action == 13)
	{
		MateLeaves(Offer.Team, Offer.Fee);
		Subj = FString::Printf(TEXT("Transfert : %s part chez %s"), *Mate, *Offer.Team);
		Dl->Result = FString::Printf(TEXT("%s rejoint %s (+%.1f M€). %s, pilote de réserve, prend son volant jusqu'à la fin de la saison. Le mercato reste ouvert pour trouver mieux."),
			*Mate, *Offer.Team, Offer.Fee, *G->MateName);
	}
	else if (Action == 11)
	{
		// un ambitieux ou un mercenaire qu'on empêche de partir peut faire la tête
		const bool bUpset = PersFor(Mate, StatsOf(Mate).Age) != TEXT("fidele") && PersFor(Mate, StatsOf(Mate).Age) != TEXT("jeune") && FMath::FRand() < 0.6f;
		if (bUpset) { G->Moral = FMath::Max(0, G->Moral - 4); Dl->bBad = true; }
		Subj = FString::Printf(TEXT("Offre refusée : %s reste"), *Mate);
		Dl->Result = bUpset ? FString::Printf(TEXT("%s voulait partir : il fait la tête et l'ambiance s'en ressent (moral de l'équipe -4)."), *Mate)
			: FString::Printf(TEXT("%s reste concentré sur la saison. %s devra revenir avec une meilleure offre."), *Mate, *Offer.Team);
	}
	else
	{
		G->MateYears += 1;
		G->MateSal = R1(G->MateSal * 1.2f + 0.1f);
		Subj = FString::Printf(TEXT("Prolongation : %s"), *Mate);
		Dl->Result = FString::Printf(TEXT("%s prolonge d'une saison, jusqu'en %d. Son salaire passe à %.1f M€ dès la saison prochaine."), *Mate, G->Year + G->MateYears - 1, G->MateSal);
	}
	SendMail(TEXT("mercato"), TEXT("Cellule recrutement"), Subj, Dl->Text + TEXT("\n\n") + Dl->Result);
	if (UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>()) Season->Save();
}

void UGPMCareerSubsystem::MateLeaves(const FString& Team, float Gain)
{
	UGPMSaveGame* G = Save();
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	if (!G || !Data) return;
	const FString Old = G->MateName;
	WriteSeats();
	G->Skill.Add(Old, MateSkill(G->MateSpd)); G->DrvPot.Add(Old, G->MatePot);
	for (int32 t = 1; t < Data->Teams.Num() && t < BaseTeams.Num(); ++t)
	{
		if (Data->Teams[t].Name != Team) continue;
		// il prend le volant du pilote le moins bien noté de l'écurie, qui devient pilote libre
		const FString K0 = FString::Printf(TEXT("%s|0"), *BaseTeams[t].Name), K1 = FString::Printf(TEXT("%s|1"), *BaseTeams[t].Name);
		const FString Key = SkillOf(G->Seats.FindRef(K0)) <= SkillOf(G->Seats.FindRef(K1)) ? K0 : K1;
		const FString Out = G->Seats.FindRef(Key);
		if (!Out.IsEmpty()) { G->Free.AddUnique(Out); G->DrvUntil.Remove(Out); }
		G->Seats.Add(Key, Old);
		G->DrvUntil.Add(Old, G->Year + 2);
		G->DriverTeam.Add(Old, Team);
		break;
	}
	G->Budget = R1(G->Budget + Gain);
	PromoteReserve(true);
	ApplyIdentity();
}

FString UGPMCareerSubsystem::PromoteReserve(bool bPayNow)
{
	UGPMSaveGame* G = Save();
	if (!G) return FString();
	// le pilote de réserve de l'académie s'il y en a un, sinon le meilleur pilote libre abordable, devient titulaire pour une saison
	const FString Name = !G->Reserve.IsEmpty() && G->Academy.Contains(G->Reserve) ? G->Reserve : BestFree(FMath::Max(1.5f, G->Budget * 0.06f));
	G->Academy.Remove(Name); G->AcadUntil.Remove(Name);
	if (G->Reserve == Name) G->Reserve.Reset();
	FGPMMarketDriver B;
	if (Name.IsEmpty()) { B.Name = TEXT("R. Vallin"); B.Spd = 70; B.Strat = 65; B.Tyre = 66; B.Pot = 55; }
	else B = StatsOf(Name);
	G->Free.Remove(B.Name);
	G->MateName = B.Name; G->MateSpd = B.Spd; G->MateStrat = B.Strat; G->MateTyre = B.Tyre; G->MatePot = B.Pot;
	G->MateSal = FMath::Max(0.3f, Salary(B)); G->MateYears = 1; G->MateRole = TEXT("eq"); G->MateSince = G->Year;
	G->MatePod = 0.f; G->MateWin = 0.f; G->MateClause = 0.f; G->MateObj = 0; G->MateObjPrize = 0.f;
	if (bPayNow) G->Budget = R1(G->Budget - G->MateSal);
	if (B.Sponsor > 0.f) G->Budget = R1(G->Budget + B.Sponsor);   // pilote payant : son sponsor verse sa part
	G->DriverTeam.Add(B.Name, G->TeamName);
	G->DriverPts.FindOrAdd(B.Name);
	return B.Name;
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
	// aiMercato de market.js, en plus logique : les bons pilotes des petites écuries montent chez les grandes,
	// les titulaires décevants sont remplacés par un meilleur pilote libre ou, dans les petites écuries, par un jeune de Formule B
	UGPMSaveGame* G = Save();
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	if (!G || !Data || Data->Teams.Num() < 3) return;
	WriteSeats();
	const int32 NT = FMath::Min(Data->Teams.Num(), BaseTeams.Num());
	auto Key = [this](int32 T, int32 D) { return FString::Printf(TEXT("%s|%d"), *BaseTeams[T].Name, D); };
	auto Weak = [this, G, &Key](int32 T) { return SkillOf(G->Seats.FindRef(Key(T, 0))) <= SkillOf(G->Seats.FindRef(Key(T, 1))) ? 0 : 1; };
	for (int32 Done = 0, Try = 0; Done < Moves && Try < Moves * 5; ++Try)
	{
		const int32 T1 = FMath::RandRange(1, NT - 1);
		if (FMath::FRand() < 0.45f)
		{
			const int32 T2 = FMath::RandRange(1, NT - 1);
			if (T2 == T1) continue;
			const int32 Hi = Data->Teams[T1].Perf >= Data->Teams[T2].Perf ? T1 : T2, Lo = Hi == T1 ? T2 : T1;
			const int32 DHi = Weak(Hi), DLo = 1 - Weak(Lo);
			const FString Up = G->Seats.FindRef(Key(Lo, DLo)), Down = G->Seats.FindRef(Key(Hi, DHi));
			if (Up.IsEmpty() || Down.IsEmpty() || SkillOf(Up) < SkillOf(Down) + 0.002f) continue;
			G->Seats.Add(Key(Hi, DHi), Up); G->Seats.Add(Key(Lo, DLo), Down);
			G->DriverTeam.Add(Up, Data->Teams[Hi].Name); G->DriverTeam.Add(Down, Data->Teams[Lo].Name);
			G->DrvUntil.Add(Up, G->Year + FMath::RandRange(2, 3)); G->DrvUntil.Add(Down, G->Year + 1);
			News(FString::Printf(TEXT("Mercato : %s quitte %s pour %s ; %s fait le chemin inverse."), *Up, *Data->Teams[Lo].Name, *Data->Teams[Hi].Name, *Down));
		}
		else
		{
			const int32 D1 = Weak(T1);
			const FString Out = G->Seats.FindRef(Key(T1, D1));
			if (Out.IsEmpty() || UntilOf(Out) > G->Year + 1) continue;   // on ne casse pas un contrat long
			const bool bTop = Data->Teams[T1].Perf >= 0.975f;
			const FString Cand = BestFree(bTop ? 99.f : 3.f);
			const bool bBetter = !Cand.IsEmpty() && SkillOf(Cand) >= SkillOf(Out) + 0.002f;
			if (!bBetter && (bTop || FMath::FRand() < 0.5f)) continue;
			const FString In = FillSeat(T1, D1, !bBetter);
			if (In.IsEmpty()) continue;
			G->Free.AddUnique(Out);
			G->DrvUntil.Remove(Out);
			News(bBetter ? FString::Printf(TEXT("Mercato : %s remplace %s chez %s."), *In, *Out, *Data->Teams[T1].Name)
				: FString::Printf(TEXT("Mercato : %s mise sur %s, jeune de Formule B, à la place de %s."), *Data->Teams[T1].Name, *In, *Out));
		}
		++Done;
	}
	ApplyIdentity();
}
