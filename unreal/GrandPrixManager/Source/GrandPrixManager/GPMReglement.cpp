// Grandes refontes du règlement : portage de js/reglement.js et de la partie « ère » de garage.js / season.js / main.js.
// Tous les 3 à 5 ans (20 % / 40 % / 40 %), la forme de la voiture et les arbres de développement changent et toutes les
// pièces repartent de zéro. Chaque ère a un pôle roi (arbre de 12 pièces dont 3 propres à l'ère, effets ×1,25). Le règlement
// est publié au 1er janvier de l'année d'avant : la voiture de la nouvelle ère se développe un an à l'avance, avec les mêmes
// ateliers et le même budget. Entre deux refontes, un règlement annuel remet à zéro 1 ou 2 pôles à l'intersaison.
#include "GPMFactory.h"
#include "GrandPrixManager.h"
#include "GPMDataSubsystem.h"
#include "GPMSeason.h"

#include "GPMReglementData.inl"

namespace
{
	// générateur pseudo-aléatoire de reglement.js (mêmes arbres que la version web pour une même ère)
	struct FRng
	{
		uint32 A;
		explicit FRng(uint32 Seed) : A(uint32(uint64(Seed) * 2654435761ull)) {}
		double operator()()
		{
			A += 0x6D2B79F5u;
			uint32 T = A;
			T = (T ^ (T >> 15)) * (T | 1u);
			T ^= T + (T ^ (T >> 7)) * (T | 61u);
			return double(T ^ (T >> 14)) / 4294967296.0;
		}
	};

	template <typename T>
	void Shuffle(TArray<T>& A, FRng& R)
	{
		for (int32 k = A.Num() - 1; k > 0; --k)
		{
			const int32 j = FMath::Min(k, int32(R() * (k + 1)));
			A.Swap(k, j);
		}
	}

	// années entre deux refontes : 3 (20 %), 4 (40 %) ou 5 (40 %)
	int32 ReformGap() { const float R = FMath::FRand(); return R < 0.2f ? 3 : R < 0.6f ? 4 : 5; }
	// ère suivante : jamais la Fondation, jamais la même
	int32 NextEra(int32 Cur)
	{
		TArray<int32> C;
		for (int32 i = 1; i < int32(UE_ARRAY_COUNT(GEras)); ++i) if (i != Cur) C.Add(i);
		return C[FMath::RandRange(0, C.Num() - 1)];
	}

	float Score(const TArray<FGPMTree>& T)
	{
		float S = 0.f;
		for (const FGPMTree& Tr : T) for (const FGPMPart& P : Tr.Parts) for (const TPair<FString, float>& F : P.Fx) S += PerfMap().FindRef(F.Key) * F.Value;
		return S;
	}
}

const UGPMFactorySubsystem::FEra& UGPMFactorySubsystem::EraDef(int32 I) { return GEras[FMath::Clamp(I, 0, int32(UE_ARRAY_COUNT(GEras)) - 1)]; }
int32 UGPMFactorySubsystem::NumEras() { return int32(UE_ARRAY_COUNT(GEras)); }

FString UGPMFactorySubsystem::EraName(int32 I, int32 Gen)
{
	static const TCHAR* Roman[] = { TEXT(""), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V") };
	FString N = FString(TEXT("Ère ")) + EraDef(I).Name;
	if (Gen >= NumEras()) N += FString(TEXT(" ")) + Roman[FMath::Min(4, Gen / (NumEras() - 1))];
	return N;
}

// arbres d'une ère : chaque pôle reprend la forme (prérequis, rangées) d'un autre pôle de base, parfois en miroir, avec de
// nouvelles pièces tirées de son catalogue ; ids uniques par génération (« g3m4 »)
TArray<FGPMTree> UGPMFactorySubsystem::EraTrees(int32 I, int32 Gen, const TArray<FGPMTree>& Base)
{
	if (Gen <= 0 || Base.Num() == 0) return Base;
	const FEra& E = EraDef(I);
	const FEraData& ED = EraData()[FMath::Clamp(I, 0, EraData().Num() - 1)];
	const FString Key = E.Key;
	FRng R(uint32(Gen * 97 + I));
	// pôle roi : le plus grand arbre (aéro de base) agrandi de deux pièces ; les autres pôles se partagent au hasard les quatre autres formes
	const FGPMTree* Big = Base.FindByPredicate([](const FGPMTree& T) { return T.Id == TEXT("aero"); });
	if (!Big) Big = &Base[0];
	FGPMTree BigT = *Big;
	{
		FGPMPart X1; X1.Id = TEXT("x1"); X1.Row = 5; X1.Col = 1; X1.Cost = 85.f; X1.Req = { TEXT("a8") };
		FGPMPart X2; X2.Id = TEXT("x2"); X2.Row = 5; X2.Col = 3; X2.Cost = 75.f; X2.Req = { TEXT("a7") };
		BigT.Parts.Add(X1); BigT.Parts.Add(X2);
	}
	TArray<const FGPMTree*> Rest;
	for (const FGPMTree& T : Base) if (&T != Big) Rest.Add(&T);
	Shuffle(Rest, R);
	int32 j = 0;
	TArray<const FGPMTree*> Tpl;
	for (const FGPMTree& B : Base) Tpl.Add(B.Id == Key ? &BigT : Rest.IsValidIndex(j) ? Rest[j++] : Big);

	TArray<FGPMTree> Out;
	for (int32 k = 0; k < Base.Num(); ++k)
	{
		const FGPMTree& B = Base[k];
		const FGPMTree& T = *Tpl[k];
		const bool bKey = !Key.IsEmpty() && B.Id == Key;
		const bool bMirror = R() < 0.5;
		const TArray<FPoolPart>* PoolB = Pool().Find(B.Id);
		FGPMTree NT = B;
		NT.Parts.Reset();
		if (!PoolB || PoolB->Num() == 0) { Out.Add(B); continue; }
		const FPoolPart& Top = PoolB->Last();
		TArray<FPoolPart> Cat(PoolB->GetData(), PoolB->Num() - 1);
		Shuffle(Cat, R);
		const int32 NExtra = bKey ? ED.Extra.Num() : 0;
		Cat.SetNum(FMath::Clamp(T.Parts.Num() - 1 - NExtra, 0, Cat.Num()));
		TArray<FPoolPart> Pick;
		if (bKey) Pick.Append(ED.Extra);
		Pick.Append(Cat);
		Shuffle(Pick, R);
		Pick.Add(Top);   // la dernière pièce du catalogue au sommet

		TArray<FGPMPart> Ord = T.Parts;
		Ord.StableSort([](const FGPMPart& X, const FGPMPart& Y) { return X.Row != Y.Row ? X.Row < Y.Row : X.Cost < Y.Cost; });
		TMap<FString, FString> Ids;
		for (int32 n = 0; n < Ord.Num(); ++n) Ids.Add(Ord[n].Id, FString::Printf(TEXT("g%d%c%d"), Gen, B.Id.Len() ? B.Id[0] : TEXT('x'), n + 1));
		const float LvB = LevelMap().FindRef(B.Id) > 0.f ? LevelMap().FindRef(B.Id) : 1.f, LvT = LevelMap().FindRef(T.Id) > 0.f ? LevelMap().FindRef(T.Id) : 1.f;
		for (int32 n = 0; n < Ord.Num(); ++n)
		{
			const FGPMPart& Src = Ord[n];
			const FPoolPart& PP = Pick[FMath::Min(n == Ord.Num() - 1 ? Pick.Num() - 1 : n, Pick.Num() - 1)];
			FGPMPart P;
			P.Id = Ids[Src.Id];
			P.Name = PP.Name; P.Desc = PP.Desc; P.Tree = B.Id;
			P.Cost = float(FMath::Max(6, FMath::RoundToInt(Src.Cost / LvT * LvB * (0.9 + R() * 0.2))));
			P.Row = Src.Row;
			P.Col = bMirror ? 4 - Src.Col : Src.Col;
			for (const FString& Q : Src.Req) if (const FString* Id = Ids.Find(Q)) P.Req.Add(*Id);
			for (const FString& Q : Src.ReqAny) if (const FString* Id = Ids.Find(Q)) P.ReqAny.Add(*Id);
			for (const FW& W : PP.W)
			{
				const float* Mul = ED.Mul.Find(W.K);
				const float V = UnitMap().FindRef(W.K) * W.V * FMath::Sqrt(P.Cost / LvB / 40.f) * (Mul ? *Mul : 1.f) * (bKey ? 1.25f : 1.f);   // pôle roi : effets renforcés
				const FString K(W.K);
				P.Fx.Add(K, K == TEXT("rel") ? float(FMath::RoundToInt(V)) : K == TEXT("pit") ? FMath::RoundToFloat(V * 10.f) / 10.f : FMath::RoundToFloat(V * 1000.f) / 1000.f);
			}
			NT.Parts.Add(P);
		}
		Out.Add(NT);
	}
	// potentiel total de performance ramené à celui des arbres de base (±5 %) : l'ère change où se trouve la performance, pas combien
	const float SOut = Score(Out);
	const float K = SOut > 0.f ? Score(Base) / SOut * float(0.95 + R() * 0.1) : 1.f;
	for (FGPMTree& T : Out) for (FGPMPart& P : T.Parts) for (TPair<FString, float>& F : P.Fx)
		if (PerfMap().Contains(F.Key)) F.Value = FMath::RoundToFloat(F.Value * K * 1000.f) / 1000.f;
	return Out;
}

void UGPMFactorySubsystem::EnsureEra() const
{
	UGPMSaveGame* G = Save();
	if (!G || G->EraAt > 0) return;
	G->EraI = 0; G->EraGen = 0;
	G->EraAt = G->Year + ReformGap();
	G->EraNi = NextEra(0);
	G->FutOwned.Reset();
	G->RivalFut.Reset();
}

void UGPMFactorySubsystem::SyncEra() const
{
	EnsureEra();
	const UGPMSaveGame* G = Save();
	const int32 I = G ? G->EraI : 0, Gen = G ? G->EraGen : 0, Ni = G ? G->EraNi : 1;
	const int32 K = I * 1000000 + Gen * 100 + Ni;
	if (K == EraKey || BaseList.Num() == 0) return;
	EraKey = K;
	TreeList = EraTrees(I, Gen, BaseList);
	FutList = EraTrees(Ni, Gen + 1, BaseList);
	for (FGPMTree& T : FutList) for (FGPMPart& P : T.Parts) P.bFut = true;
	// modèle 3D de la monoplace de l'ère
	if (UGPMDataSubsystem* Data = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGPMDataSubsystem>() : nullptr) Data->CarEra = EraDef(I).Id;
	UE_LOG(LogGPM, Log, TEXT("Règlement : %s, prochaine refonte %d (%s)"), *EraName(I, Gen), G ? G->EraAt : 0, *EraName(Ni, Gen + 1));
}

int32 UGPMFactorySubsystem::EraIndex() const { EnsureEra(); const UGPMSaveGame* G = Save(); return G ? G->EraI : 0; }
int32 UGPMFactorySubsystem::NextEraIndex() const { EnsureEra(); const UGPMSaveGame* G = Save(); return G ? G->EraNi : 1; }
int32 UGPMFactorySubsystem::EraGen() const { EnsureEra(); const UGPMSaveGame* G = Save(); return G ? G->EraGen : 0; }
int32 UGPMFactorySubsystem::Year() const { const UGPMSaveGame* G = Save(); return G ? G->Year : 2027; }
int32 UGPMFactorySubsystem::ReformYear() const { EnsureEra(); const UGPMSaveGame* G = Save(); return G ? G->EraAt : 2031; }
FString UGPMFactorySubsystem::CurrentEraName() const { return EraName(EraIndex(), EraGen()); }
FString UGPMFactorySubsystem::NextEraName() const { return EraName(NextEraIndex(), EraGen() + 1); }
FString UGPMFactorySubsystem::KingDept(bool bFut) const { return EraDef(bFut ? NextEraIndex() : EraIndex()).Key; }
FString UGPMFactorySubsystem::CarEraId(bool bFut) const { return EraDef(bFut ? NextEraIndex() : EraIndex()).Id; }
int32 UGPMFactorySubsystem::FutCount() const { const UGPMSaveGame* G = Save(); return G ? G->FutOwned.Num() : 0; }

bool UGPMFactorySubsystem::FutureOpen() const
{
	EnsureEra();
	const UGPMSaveGame* G = Save();
	return G && G->bCreated && G->Year >= G->EraAt - 1;
}

void UGPMFactorySubsystem::RivalGain(const FString& Team, float Gain)
{
	UGPMSaveGame* G = Save();
	if (!G) return;
	if (!FutureOpen()) { G->RivalDev.FindOrAdd(Team) += Gain; return; }
	// l'année qui précède une refonte, une partie des moyens va à la voiture suivante
	const float Share = 0.3f + 0.3f * FMath::FRand();
	G->RivalDev.FindOrAdd(Team) += Gain * (1.f - Share);
	G->RivalFut.FindOrAdd(Team) += Gain * Share * 1.6f;
}

void UGPMFactorySubsystem::EraAfterRace(int32 Round, int32 NumRounds, bool bOver, TArray<TPair<FString, FString>>& OutMails, TArray<FString>& OutLines)
{
	UGPMSaveGame* G = Save();
	if (!G || !G->bCreated) return;
	EnsureEra();
	const int32 Mid = NumRounds / 2;
	// rumeurs à la mi-saison, deux ans avant la refonte
	if (Round == Mid && G->Year == G->EraAt - 2)
	{
		OutMails.Emplace(FString::Printf(TEXT("Rumeurs : un tout nouveau règlement pour %d ?"), G->EraAt),
			FString::Printf(TEXT("Selon nos informations, l'instance dirigeante prépare une refonte complète du règlement technique pour la saison %d.\n\nLe texte officiel devrait être publié au 1er janvier %d : à partir de là, chaque écurie pourra commencer à développer sa voiture %d."), G->EraAt, G->EraAt - 1, G->EraAt));
		OutLines.Add(FString::Printf(TEXT("Rumeurs : un tout nouveau règlement se prépare pour %d. Le texte sera publié au 1er janvier %d."), G->EraAt, G->EraAt - 1));
	}
	// règlement annuel (pas l'année où la voiture de la prochaine ère se développe) : 1 ou 2 pôles, hors stands
	if ((Round == Mid || bOver) && G->Reg.Num() == 0 && !FutureOpen())
	{
		TArray<FString> Pool;
		for (const FGPMTree& T : Trees()) if (T.Id != TEXT("stand")) Pool.Add(T.Id);
		for (int32 k = Pool.Num() - 1; k > 0; --k) Pool.Swap(k, FMath::RandRange(0, k));
		Pool.SetNum(FMath::Min(Pool.Num(), FMath::FRand() < 0.35f ? 2 : 1));
		G->Reg = Pool;
		FString Names;
		for (int32 k = 0; k < Pool.Num(); ++k)
		{
			const FGPMTree* T = Trees().FindByPredicate([&](const FGPMTree& X) { return X.Id == Pool[k]; });
			Names += (k ? TEXT(" et ") : TEXT("")) + (T ? T->Name : Pool[k]);
		}
		const bool bTwo = Pool.Num() > 1;
		OutMails.Emplace(FString::Printf(TEXT("Nouveau règlement %d : %s"), G->Year + 1, *Names),
			FString::Printf(TEXT("De nouvelles règles s'appliqueront en %d pour %s.\n\nToutes les écuries repartiront de zéro %s : tes pièces %s seront retirées à l'intersaison. Inutile d'y investir gros d'ici là !"),
				G->Year + 1, *Names, bTwo ? TEXT("dans ces domaines") : TEXT("dans ce domaine"), bTwo ? TEXT("de ces pôles") : TEXT("de ce pôle")));
		OutLines.Add(FString::Printf(TEXT("Règlement %d : nouvelles règles pour %s (pièces retirées à l'intersaison)."), G->Year + 1, *Names));
	}
	Persist();
}

bool UGPMFactorySubsystem::Intersaison(int32 NextYear, FString& OutSubj, FString& OutBody)
{
	UGPMSaveGame* G = Save();
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	if (!G || !Data) return false;
	EnsureEra();
	SyncEra();
	TArray<FString> Rivals;
	for (int32 t = 1; t < Data->Teams.Num(); ++t) Rivals.Add(Data->Teams[t].Name);
	if (NextYear >= G->EraAt)
	{
		// refonte : la voiture de la nouvelle ère remplace l'ancienne (pièces et recherches en cours), tout le reste repart de zéro
		const int32 Parts = G->FutOwned.Num(), Lost = G->Owned.Num();
		const int32 NewI = G->EraNi, NewGen = G->EraGen + 1;
		G->Owned = G->FutOwned;
		G->FutOwned.Reset();
		TMap<FString, FString> DP; TMap<FString, float> DL, DT;
		for (const TPair<FString, FString>& D : G->DevPart)
			if (D.Key.StartsWith(TEXT("F:")))
			{
				const FString K = D.Key.Mid(2);
				DP.Add(K, D.Value); DL.Add(K, G->DevLeft.FindRef(D.Key)); DT.Add(K, G->DevTot.FindRef(D.Key));
			}
		G->DevPart = DP; G->DevLeft = DL; G->DevTot = DT;
		G->EraI = NewI; G->EraGen = NewGen;
		G->EraAt = NextYear + ReformGap();
		G->EraNi = NextEra(NewI);
		G->Reg.Reset();
		// les rivaux repartent de ce qu'ils ont préparé, et la hiérarchie est rebattue
		for (const FString& T : Rivals) G->RivalDev.Add(T, G->RivalFut.FindRef(T) - 0.006f + (FMath::FRand() - 0.5f) * 0.026f);
		G->RivalFut.Reset();
		const FString Name = EraName(NewI, NewGen);
		const FEra& E = EraDef(NewI);
		G->ReformYear = NextYear;
		G->ReformText = FString::Printf(TEXT("%s. %s %s Tout le plateau est reparti de zéro : %s. Prochaine refonte : pas avant %d."), *Name, E.Focus, E.Desc,
			Parts ? *FString::Printf(TEXT("%d pièce%s préparée%s l'an dernier équipe%s ta voiture"), Parts, Parts > 1 ? TEXT("s") : TEXT(""), Parts > 1 ? TEXT("s") : TEXT(""), Parts > 1 ? TEXT("nt") : TEXT(""))
				: TEXT("aucune pièce préparée l'an dernier, tout est à construire"), G->EraAt - 1);
		OutSubj = FString::Printf(TEXT("%s : le nouveau règlement entre en vigueur"), *Name);
		OutBody = FString::Printf(TEXT("%s\n%s\n\nToutes les écuries repartent de zéro : %d pièce%s de l'ancienne voiture %s, %d pièce%s de la nouvelle voiture %s déjà prête%s.\n\nLa hiérarchie est rebattue : les écuries qui ont le mieux préparé cette saison partent devant."),
			E.Focus, E.Desc, Lost, Lost > 1 ? TEXT("s") : TEXT(""), Lost > 1 ? TEXT("sont retirées") : TEXT("est retirée"), Parts, Parts > 1 ? TEXT("s") : TEXT(""), Parts > 1 ? TEXT("sont") : TEXT("est"), Parts > 1 ? TEXT("s") : TEXT(""));
		EraKey = -1;
		SyncEra();
		Persist();
		UE_LOG(LogGPM, Log, TEXT("Refonte du règlement %d : %s (%d pièces prêtes, %d retirées)"), NextYear, *Name, Parts, Lost);
		return true;
	}
	// règlement annuel : les pièces des pôles concernés sont retirées, les rivaux reculent un peu dans ces domaines
	const int32 NReg = G->Reg.Num();
	if (NReg)
	{
		FString Names;
		for (int32 k = 0; k < NReg; ++k)
		{
			const FString Id = G->Reg[k];
			if (const FGPMTree* T = TreeList.FindByPredicate([&](const FGPMTree& X) { return X.Id == Id; }))
			{
				Names += (k ? TEXT(" et ") : TEXT("")) + T->Name;
				G->Owned.RemoveAll([T](const FString& O) { return T->Parts.ContainsByPredicate([&O](const FGPMPart& P) { return P.Id == O; }); });
			}
			for (const FString& K : { Id, Id + TEXT("+") }) { G->DevPart.Remove(K); G->DevLeft.Remove(K); G->DevTot.Remove(K); }
		}
		OutSubj = FString::Printf(TEXT("Règlement %d en vigueur"), NextYear);
		OutBody = FString::Printf(TEXT("Les nouvelles règles s'appliquent : tes pièces %s ont été retirées. Tout le plateau repart de zéro dans %s."), *Names, NReg > 1 ? TEXT("ces domaines") : TEXT("ce domaine"));
	}
	// les rivaux gardent la moitié de leurs progrès ; un nouveau règlement les fait reculer et rebat un peu les cartes
	for (const FString& T : Rivals) G->RivalDev.Add(T, G->RivalDev.FindRef(T) * 0.5f - NReg * 0.004f + (FMath::FRand() - 0.5f) * 0.008f * NReg);
	G->Reg.Reset();
	Persist();
	return false;
}

bool UGPMFactorySubsystem::EraPublished(FString& OutSubj, FString& OutBody) const
{
	const UGPMSaveGame* G = Save();
	if (!G || !G->bCreated || G->Year != G->EraAt - 1) return false;
	const FEra& E = EraDef(G->EraNi);
	OutSubj = FString::Printf(TEXT("Officiel : refonte totale du règlement en %d"), G->EraAt);
	OutBody = FString::Printf(TEXT("Le règlement %d est publié : %s.\n%s\n%s\n\nAu 1er janvier %d, toutes les pièces de toutes les écuries repartent de zéro et les arbres de développement changent.\nTu peux développer la voiture %d dès aujourd'hui (QG, Recherche, onglet « Voiture %d »). Attention : les ateliers et le budget sont partagés avec la voiture %d."),
		G->EraAt, *NextEraName(), E.Focus, E.Desc, G->EraAt, G->EraAt, G->EraAt, G->Year);
	return true;
}

void UGPMFactorySubsystem::ForceReformSoon()
{
	EnsureEra();
	if (UGPMSaveGame* G = Save()) { G->EraAt = G->Year + 1; Persist(); }
}
