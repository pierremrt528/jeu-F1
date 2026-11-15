// QG de l'écurie : installations (infra.js), chefs de pôle (staff.js) et courbes de performance (season.hist).
// La recherche des pièces est dans GPMFactory.cpp.
#include "GPMFactory.h"
#include "GrandPrixManager.h"
#include "GPMDataSubsystem.h"
#include "GPMSeason.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace GPMQGData
{
	float Round1(float X) { return FMath::RoundToFloat(X * 10.f) / 10.f; }
	const float BuildCostLv[3] = { 8.f, 16.f, 30.f };   // M€ par niveau (×1,2 pour l'espace partenaires et le centre médias)
	const int32 BuildDaysLv[3] = { 14, 28, 42 };        // jours de chantier
	const TCHAR* const ChiefNames[] = { TEXT("Abeyta"), TEXT("Barreau"), TEXT("Castell"), TEXT("Delorme"), TEXT("Engström"), TEXT("Ferreira"), TEXT("Gauthier"),
		TEXT("Hasegawa"), TEXT("Ilunga"), TEXT("Jansen"), TEXT("Kerroux"), TEXT("Lemaire"), TEXT("Moreau"), TEXT("Nyström"), TEXT("Oyelaran"), TEXT("Pagani"),
		TEXT("Quintal"), TEXT("Rossel"), TEXT("Solberg"), TEXT("Tavares"), TEXT("Urquiza"), TEXT("Vidal"), TEXT("Wolanski"), TEXT("Xhaferi"), TEXT("Yamada"),
		TEXT("Zeller"), TEXT("Arcand"), TEXT("Bianchi"), TEXT("Carvalho"), TEXT("Dumont") };
	const TCHAR* const ChiefOrigins[] = { TEXT("Ingénieur maison"), TEXT("Formule B"), TEXT("Jeune talent"), TEXT("Vétéran"), TEXT("Endurance"), TEXT("Université"),
		TEXT("Industrie automobile"), TEXT("Équipe de rallye"), TEXT("Ex-aéronautique"), TEXT("Banc d'essai") };
	float FeeOf(int32 Cost, int32 Time, int32 Risk) { return Round1(0.6f + (Cost + Time + Risk) * 0.075f); }
	FString TagOf(const FString& Id)
	{
		return Id == TEXT("rd") ? TEXT("R&D") : Id == TEXT("wind") ? TEXT("SOUF") : Id == TEXT("hosp") ? TEXT("VIP") : Id == TEXT("media") ? TEXT("TV")
			: Id == TEXT("acad") ? TEXT("ACAD") : Id == TEXT("shop") ? TEXT("ATEL") : TEXT("MÉCA");
	}
}

void UGPMFactorySubsystem::LoadQG()
{
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	if (!Data) return;
	if (const TSharedPtr<FJsonValue> Raw = Data->GetRaw(TEXT("infra"), TEXT("INFRA")))
		for (const TSharedPtr<FJsonValue>& V : Raw->AsArray())
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) continue;
			FGPMBuilding B;
			O->TryGetStringField(TEXT("id"), B.Id);
			O->TryGetStringField(TEXT("name"), B.Name);
			O->TryGetStringField(TEXT("desc"), B.Desc);
			B.Tag = GPMQGData::TagOf(B.Id);
			const TArray<TSharedPtr<FJsonValue>>* Lv = nullptr;
			if (O->TryGetArrayField(TEXT("lv"), Lv)) for (const TSharedPtr<FJsonValue>& L : *Lv) B.Lv.Add(L->AsString().Replace(TEXT("\u2212"), TEXT("-")));
			while (B.Lv.Num() < 3) B.Lv.Add(FString());
			BuildingList.Add(B);
		}
	if (const TSharedPtr<FJsonValue> Raw = Data->GetRaw(TEXT("staff"), TEXT("CHIEFS")))
		for (const TSharedPtr<FJsonValue>& V : Raw->AsArray())
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) continue;
			FGPMChief C;
			O->TryGetStringField(TEXT("id"), C.Id);
			O->TryGetStringField(TEXT("dept"), C.Dept);
			O->TryGetStringField(TEXT("name"), C.Name);
			O->TryGetStringField(TEXT("from"), C.From);
			C.Cost = int32(O->GetNumberField(TEXT("cost")));
			C.Time = int32(O->GetNumberField(TEXT("time")));
			C.Risk = int32(O->GetNumberField(TEXT("risk")));
			C.Fee = GPMQGData::FeeOf(C.Cost, C.Time, C.Risk);
			ChiefList.Add(C);
		}
}

// ---------- installations ----------

int32 UGPMFactorySubsystem::Level(const FString& Id) const
{
	const UGPMSaveGame* G = Save();
	return G ? FMath::Clamp(G->InfraLv.FindRef(Id), 0, 3) : 0;
}

float UGPMFactorySubsystem::BuildCost(const FString& Id, int32 Lv)
{
	const float Base = GPMQGData::BuildCostLv[FMath::Clamp(Lv, 0, 2)];
	return GPMQGData::Round1(Base * (Id == TEXT("hosp") || Id == TEXT("media") ? 1.2f : 1.f));
}

int32 UGPMFactorySubsystem::BuildDays(int32 Lv) { return GPMQGData::BuildDaysLv[FMath::Clamp(Lv, 0, 2)]; }

bool UGPMFactorySubsystem::CanBuild(const FString& Id, FString& Why) const
{
	const UGPMSaveGame* G = Save();
	if (!G) return false;
	const int32 Lv = Level(Id);
	if (Lv >= 3) { Why = TEXT("Niveau maximum atteint."); return false; }
	if (!G->BuildId.IsEmpty())
	{
		FString Name = G->BuildId;
		for (const FGPMBuilding& B : BuildingList) if (B.Id == G->BuildId) Name = B.Name;
		Why = FString::Printf(TEXT("Chantier en cours : %s. Un seul à la fois."), *Name);
		return false;
	}
	const float Cost = BuildCost(Id, Lv);
	if (G->Budget < Cost) { Why = FString::Printf(TEXT("Budget insuffisant : il manque %.1f M€."), Cost - G->Budget); return false; }
	Why.Reset();
	return true;
}

bool UGPMFactorySubsystem::Build(const FString& Id)
{
	FString Why;
	UGPMSaveGame* G = Save();
	if (!G || !CanBuild(Id, Why)) return false;
	const int32 Lv = Level(Id);
	G->Budget = GPMQGData::Round1(G->Budget - BuildCost(Id, Lv));
	G->BuildId = Id;
	G->BuildLeft = G->BuildTot = BuildDays(Lv);
	Persist();
	return true;
}

FString UGPMFactorySubsystem::TickInfra(int32 Days)
{
	UGPMSaveGame* G = Save();
	if (!G || G->BuildId.IsEmpty() || Days <= 0) return FString();
	G->BuildLeft -= Days;
	if (G->BuildLeft > 0) { Persist(); return FString(); }
	const FString Id = G->BuildId;
	const int32 Lv = FMath::Min(3, Level(Id) + 1);
	G->InfraLv.Add(Id, Lv);
	G->BuildId.Reset();
	G->BuildLeft = G->BuildTot = 0;
	if (Id == TEXT("media") || Id == TEXT("hosp")) G->Rep = FMath::Min(100, G->Rep + 4);
	FString Name = Id;
	for (const FGPMBuilding& B : BuildingList) if (B.Id == Id) Name = B.Name;
	const FString Msg = FString::Printf(TEXT("Chantier terminé : %s niveau %d%s"), *Name, Lv, Id == TEXT("media") || Id == TEXT("hosp") ? TEXT(" (réputation +4)") : TEXT(""));
	G->News.Add(Msg);
	Persist();
	return Msg;
}

float UGPMFactorySubsystem::PartnerIncome() const
{
	static const float Hosp[4] = { 0.f, 1.f, 2.f, 3.5f };
	const UGPMSaveGame* G = Save();
	// base 1,4 M€ (sponsorIncome d'infra.js + la part fixe des petits partenaires) : ~2 M€ à 50 de réputation
	return GPMQGData::Round1(1.4f + (G ? G->Rep : 50) / 100.f + Hosp[FMath::Clamp(Level(TEXT("hosp")), 0, 3)]);
}

float UGPMFactorySubsystem::PrizeMul() const { return 1.f + 0.1f * Level(TEXT("media")); }
float UGPMFactorySubsystem::MateGrowth() const { return 1.f + 0.5f * Level(TEXT("acad")); }
float UGPMFactorySubsystem::SalaryCut() const { return 1.f - 0.05f * Level(TEXT("acad")); }

float UGPMFactorySubsystem::PitBonus() const
{
	static const float Pit[4] = { 0.f, 0.3f, 0.6f, 1.f };
	return -Pit[FMath::Clamp(Level(TEXT("pitc")), 0, 3)];
}

// ---------- chefs de pôle ----------

const TArray<TPair<FString, FString>>& UGPMFactorySubsystem::Depts()
{
	static const TArray<TPair<FString, FString>> List = []()
	{
		TArray<TPair<FString, FString>> L;
		L.Emplace(TEXT("moteur"), TEXT("Moteur"));
		L.Emplace(TEXT("aero"), TEXT("Aéro"));
		L.Emplace(TEXT("chassis"), TEXT("Châssis"));
		L.Emplace(TEXT("fiab"), TEXT("Fiabilité"));
		L.Emplace(TEXT("stand"), TEXT("Stands"));
		return L;
	}();
	return List;
}

FString UGPMFactorySubsystem::DeptName(const FString& Dept)
{
	for (const TPair<FString, FString>& D : Depts()) if (D.Key == Dept) return D.Value;
	return Dept;
}

FGPMChief UGPMFactorySubsystem::ChiefById(const FString& Id) const
{
	FGPMChief Out;
	if (Id.IsEmpty()) return Out;
	for (const FGPMChief& C : ChiefList) if (C.Id == Id) return C;
	const UGPMSaveGame* G = Save();
	if (!G) return Out;
	const FString Prefix = Id + TEXT("|");
	for (const FString& Line : G->ChiefGen)
	{
		if (!Line.StartsWith(Prefix)) continue;
		TArray<FString> Parts;
		Line.ParseIntoArray(Parts, TEXT("|"), false);
		if (Parts.Num() < 7) continue;
		Out.Id = Parts[0]; Out.Dept = Parts[1]; Out.Name = Parts[2]; Out.From = Parts[3];
		Out.Cost = FCString::Atoi(*Parts[4]); Out.Time = FCString::Atoi(*Parts[5]); Out.Risk = FCString::Atoi(*Parts[6]);
		Out.Fee = GPMQGData::FeeOf(Out.Cost, Out.Time, Out.Risk);
		return Out;
	}
	return Out;
}

FGPMChief UGPMFactorySubsystem::ChiefOf(const FString& Dept) const
{
	const UGPMSaveGame* G = Save();
	return G ? ChiefById(G->ChiefOf.FindRef(Dept)) : FGPMChief();
}

FGPMChief UGPMFactorySubsystem::NewChief(const FString& Dept, int32 Q, const FString& From)
{
	// gen de staff.js : la qualité Q est répartie au hasard entre prix, délai et risque
	FGPMChief C;
	UGPMSaveGame* G = Save();
	if (!G) return C;
	const float W0 = FMath::FRand() + 0.3f, W1 = FMath::FRand() + 0.3f, W2 = FMath::FRand() + 0.3f, Sum = W0 + W1 + W2;
	C.Cost = FMath::Clamp(FMath::RoundToInt(Q * W0 / Sum), 3, 35);
	C.Time = FMath::Clamp(FMath::RoundToInt(Q * W1 / Sum), 3, 35);
	C.Risk = FMath::Clamp(FMath::RoundToInt(Q * W2 / Sum), 3, 35);
	C.Fee = GPMQGData::FeeOf(C.Cost, C.Time, C.Risk);
	C.Dept = Dept;
	C.From = From;
	static const TCHAR Initials[] = TEXT("ABCDEFGHJKLMNOPRSTVWY");
	const int32 NInit = int32(UE_ARRAY_COUNT(Initials)) - 1, NNames = int32(UE_ARRAY_COUNT(GPMQGData::ChiefNames));
	for (int32 Try = 0; Try < 40; ++Try)
	{
		C.Name = FString::Chr(Initials[FMath::RandRange(0, NInit - 1)]) + TEXT(". ") + GPMQGData::ChiefNames[FMath::RandRange(0, NNames - 1)];
		bool bUsed = false;
		for (const FGPMChief& B : ChiefList) bUsed |= B.Name == C.Name;
		for (const FString& Line : G->ChiefGen) bUsed |= Line.Contains(TEXT("|") + C.Name + TEXT("|"));
		if (!bUsed) break;
	}
	C.Id = FString::Printf(TEXT("g%d"), ++G->ChiefSeq);
	G->ChiefGen.Add(FString::Printf(TEXT("%s|%s|%s|%s|%d|%d|%d"), *C.Id, *C.Dept, *C.Name, *C.From, C.Cost, C.Time, C.Risk));
	return C;
}

void UGPMFactorySubsystem::EnsureStaff()
{
	UGPMSaveGame* G = Save();
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	if (!G || !Data || G->bStaffInit) return;
	G->ChiefFree.Reset();
	for (const FGPMChief& C : ChiefList)
		if (G->ChiefOf.FindRef(C.Dept) != C.Id) G->ChiefFree.Add(C.Id);
	// chefs des écuries rivales : meilleurs dans les écuries les plus performantes (rivalQ de staff.js)
	for (int32 t = 1; t < Data->Teams.Num(); ++t)
		for (const TPair<FString, FString>& D : Depts())
		{
			const FString Key = Data->Teams[t].Name + TEXT("|") + D.Key;
			if (G->RivalChief.Contains(Key)) continue;
			const int32 Q = FMath::RoundToInt(26.f + FMath::Clamp((Data->Teams[t].Perf - 0.95f) / 0.04f, 0.f, 1.f) * 26.f) + FMath::RandRange(-6, 6);
			G->RivalChief.Add(Key, NewChief(D.Key, Q, GPMQGData::ChiefOrigins[FMath::RandRange(0, 5)]).Id);
		}
	G->bStaffInit = true;
	Persist();
}

TArray<FGPMChiefOffer> UGPMFactorySubsystem::Candidates(const FString& Dept) const
{
	TArray<FGPMChiefOffer> Free, Rivals;
	const UGPMSaveGame* G = Save();
	if (!G) return Free;
	for (const FString& Id : G->ChiefFree)
	{
		FGPMChiefOffer O;
		O.C = ChiefById(Id);
		if (!O.C.IsValid() || O.C.Dept != Dept) continue;
		O.bFresh = G->ChiefFresh.Contains(Id);
		Free.Add(O);
	}
	const FString Suffix = TEXT("|") + Dept;
	for (const TPair<FString, FString>& R : G->RivalChief)
	{
		if (!R.Key.EndsWith(Suffix)) continue;
		FGPMChiefOffer O;
		O.C = ChiefById(R.Value);
		O.Club = R.Key.LeftChop(Suffix.Len());
		if (O.C.IsValid()) Rivals.Add(O);
	}
	Free.Sort([](const FGPMChiefOffer& A, const FGPMChiefOffer& B) { return A.C.Quality() > B.C.Quality(); });
	Rivals.Sort([](const FGPMChiefOffer& A, const FGPMChiefOffer& B) { return A.C.Quality() > B.C.Quality(); });
	Free.Append(Rivals);
	return Free;
}

float UGPMFactorySubsystem::ChiefMul(const FString& Dept, int32 Which) const
{
	const FGPMChief C = ChiefOf(Dept);
	if (!C.IsValid()) return 1.f;
	const int32 Pct = Which == 0 ? C.Cost : Which == 1 ? C.Time : C.Risk;
	return 1.f - Pct / 100.f;
}

float UGPMFactorySubsystem::RivalDevMul(const FString& Team) const
{
	const UGPMSaveGame* G = Save();
	if (!G) return 1.f;
	float Sum = 0.f;
	bool bAny = false;
	for (const TPair<FString, FString>& D : Depts())
	{
		const FGPMChief C = ChiefById(G->RivalChief.FindRef(Team + TEXT("|") + D.Key));
		if (!C.IsValid()) continue;
		Sum += C.Quality() / 45.f;
		bAny = true;
	}
	return bAny ? 0.7f + Sum * 0.06f : 1.f;
}

float UGPMFactorySubsystem::ChiefAsk(const FGPMChief& C, int32 Years)
{
	// contrat court : un peu plus cher par saison ; contrat long : un peu moins
	return GPMQGData::Round1(C.Fee * (Years <= 1 ? 1.1f : Years == 2 ? 1.f : 0.94f));
}

float UGPMFactorySubsystem::ReleaseCost(const FString& Dept) const
{
	const UGPMSaveGame* G = Save();
	if (!G || !G->ChiefOf.Contains(Dept)) return 0.f;
	const int32 Left = G->ChiefUntil.FindRef(Dept) - G->Year;
	return Left > 0 ? GPMQGData::Round1(0.5f * G->ChiefSal.FindRef(Dept) * Left) : 0.f;
}

float UGPMFactorySubsystem::HireCost(const FGPMChiefOffer& O, int32 Years) const
{
	return GPMQGData::Round1(ChiefAsk(O.C, Years) + (O.Club.IsEmpty() ? 0.f : ChiefFee(O.C)) + ReleaseCost(O.C.Dept));
}

bool UGPMFactorySubsystem::Hire(const FGPMChiefOffer& O, int32 Years)
{
	UGPMSaveGame* G = Save();
	if (!G || !O.C.IsValid()) return false;
	const int32 Y = FMath::Clamp(Years, 1, 3);
	const float Cost = HireCost(O, Y);
	if (Cost > G->Budget + 1e-3f) return false;
	G->Budget = GPMQGData::Round1(G->Budget - Cost);
	const FString Dept = O.C.Dept, Old = G->ChiefOf.FindRef(Dept);
	G->ChiefFree.Remove(O.C.Id);
	G->ChiefFresh.Remove(O.C.Id);
	if (!O.Club.IsEmpty())
	{
		// l'écurie rivale récupère ton ancien chef, ou prend un intérimaire
		const FString Key = O.Club + TEXT("|") + Dept;
		G->RivalChief.Add(Key, !Old.IsEmpty() ? Old : NewChief(Dept, FMath::RandRange(14, 24), TEXT("Intérim")).Id);
	}
	else if (!Old.IsEmpty()) G->ChiefFree.AddUnique(Old);
	G->ChiefOf.Add(Dept, O.C.Id);
	G->ChiefSal.Add(Dept, ChiefAsk(O.C, Y));
	G->ChiefUntil.Add(Dept, G->Year + Y - 1);
	G->News.Add(FString::Printf(TEXT("%s prend la tête du pôle %s (%d saison%s, %.1f M€ par saison)."), *O.C.Name, *DeptName(Dept), Y, Y > 1 ? TEXT("s") : TEXT(""), ChiefAsk(O.C, Y)));
	Persist();
	return true;
}

bool UGPMFactorySubsystem::Extend(const FString& Dept)
{
	UGPMSaveGame* G = Save();
	if (!G || !G->ChiefOf.Contains(Dept)) return false;
	int32& Until = G->ChiefUntil.FindOrAdd(Dept);
	if (Until - G->Year >= 3) return false;
	Until = FMath::Max(Until, G->Year) + 1;
	Persist();
	return true;
}

void UGPMFactorySubsystem::RotateStaff()
{
	UGPMSaveGame* G = Save();
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	if (!G || !Data) return;
	EnsureStaff();
	G->ChiefFresh.Reset();
	for (const TPair<FString, FString>& D : Depts())
	{
		TArray<FString> Here;
		for (const FString& Id : G->ChiefFree) if (ChiefById(Id).Dept == D.Key) Here.Add(Id);
		// un chef libre part (65 %) ; parfois engagé par une écurie rivale, dont l'ancien chef devient libre
		if (Here.Num() && FMath::FRand() < 0.65f)
		{
			const FString Gone = Here[FMath::RandRange(0, Here.Num() - 1)];
			Here.Remove(Gone);
			G->ChiefFree.Remove(Gone);
			if (FMath::FRand() < 0.4f && Data->Teams.Num() > 1)
			{
				const FString Key = Data->Teams[FMath::RandRange(1, Data->Teams.Num() - 1)].Name + TEXT("|") + D.Key;
				const FString Prev = G->RivalChief.FindRef(Key);
				if (!Prev.IsEmpty()) { G->ChiefFree.Add(Prev); Here.Add(Prev); }
				G->RivalChief.Add(Key, Gone);
			}
		}
		const int32 Want = FMath::FRand() < 0.3f ? 5 : 4;
		while (Here.Num() < Want)
		{
			const FString From = GPMQGData::ChiefOrigins[FMath::RandRange(0, int32(UE_ARRAY_COUNT(GPMQGData::ChiefOrigins)) - 1)];
			const int32 Q = From == TEXT("Vétéran") ? FMath::RandRange(38, 55) : From == TEXT("Jeune talent") ? FMath::RandRange(18, 30) : FMath::RandRange(22, 46);
			const FGPMChief C = NewChief(D.Key, Q, From);
			G->ChiefFree.Add(C.Id);
			G->ChiefFresh.Add(C.Id);
			Here.Add(C.Id);
		}
	}
	// on oublie les chefs générés qui ne sont plus nulle part
	TSet<FString> Keep;
	for (const TPair<FString, FString>& H : G->ChiefOf) Keep.Add(H.Value);
	for (const FString& Id : G->ChiefFree) Keep.Add(Id);
	for (const TPair<FString, FString>& R : G->RivalChief) Keep.Add(R.Value);
	G->ChiefGen.RemoveAll([&Keep](const FString& Line)
	{
		FString Id, Rest;
		return Line.Split(TEXT("|"), &Id, &Rest) && !Keep.Contains(Id);
	});
	Persist();
}

float UGPMFactorySubsystem::RenewStaff(int32 NewYear)
{
	UGPMSaveGame* G = Save();
	if (!G) return 0.f;
	float Total = 0.f;
	TArray<FString> Hired;
	G->ChiefOf.GetKeys(Hired);
	for (const FString& Dept : Hired)
	{
		const FGPMChief C = ChiefOf(Dept);
		if (G->ChiefUntil.FindRef(Dept) < NewYear)
		{
			G->News.Add(FString::Printf(TEXT("Fin du contrat de %s : le pôle %s n'a plus de chef."), C.IsValid() ? *C.Name : TEXT("ton chef"), *DeptName(Dept)));
			if (C.IsValid()) G->ChiefFree.AddUnique(C.Id);
			G->ChiefOf.Remove(Dept); G->ChiefSal.Remove(Dept); G->ChiefUntil.Remove(Dept);
			continue;
		}
		Total += G->ChiefSal.FindRef(Dept);
	}
	Total = GPMQGData::Round1(Total);
	G->Budget = GPMQGData::Round1(G->Budget - Total);
	if (Total > 0.f) G->News.Add(FString::Printf(TEXT("Salaires des chefs de pôle pour %d : %.1f M€."), NewYear, Total));
	Persist();
	return Total;
}

// ---------- courbes de performance ----------

float UGPMFactorySubsystem::PlayerPerf(bool bFut) const
{
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	const FGPMUpgrades U = Upgrades(bFut);
	const float Base = Data && Data->Teams.Num() ? Data->Teams[0].Perf : 0.96f;
	return Base * (1.f + 0.6f * (U.VTop - 1.f) + 0.15f * (U.Acc - 1.f) + 0.08f * (U.Kd - 1.f) + 0.25f * (U.Mu - 1.f) + 0.04f * (U.Brake - 1.f) + 0.04f * (U.Steer - 1.f));
}

void UGPMFactorySubsystem::RecordPerf()
{
	UGPMSaveGame* G = Save();
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	if (!G || !Data || Data->Teams.Num() == 0) return;
	const int32 N = Data->Teams.Num();
	if (G->PerfN != N) { G->PerfHist.Reset(); G->PerfN = N; }
	for (int32 t = 0; t < N; ++t)
		G->PerfHist.Add(t == 0 ? PlayerPerf() : Data->Teams[t].Perf + G->RivalDev.FindRef(Data->Teams[t].Name));
	Persist();
}

void UGPMFactorySubsystem::EnsurePerfStart()
{
	const UGPMSaveGame* G = Save();
	if (G && G->PerfHist.Num() == 0 && G->Round == 0) RecordPerf();
}
