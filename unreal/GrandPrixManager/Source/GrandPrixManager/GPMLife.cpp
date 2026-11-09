// Vie de l'écurie : conférence de presse (press.js), flash info et brèves (events.js), histoires à épisodes (stories.js).
// Textes et conditions dans Content/Data/life.json (exporté par unreal/tools/export-life.mjs).
#include "GPMCareer.h"
#include "GrandPrixManager.h"
#include "GPMDataSubsystem.h"
#include "GPMSeason.h"
#include "Dom/JsonValue.h"

namespace
{
	FString LStr(const TSharedPtr<FJsonObject>& O, const TCHAR* K) { FString Out; if (O.IsValid()) O->TryGetStringField(K, Out); return Out; }
	float L1(float X) { return FMath::RoundToFloat(X * 10.f) / 10.f; }
	const TArray<TSharedPtr<FJsonValue>>* LArr(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return O.IsValid() && O->TryGetArrayField(K, A) ? A : nullptr;
	}
	template <typename T> void Shuffle(TArray<T>& A)
	{
		for (int32 i = A.Num() - 1; i > 0; --i) A.Swap(i, FMath::RandRange(0, i));
	}
	const TCHAR* FxName(const FString& K)
	{
		return K == TEXT("vtop") ? TEXT("vitesse de pointe") : K == TEXT("acc") ? TEXT("accélération") : K == TEXT("mu") ? TEXT("adhérence")
			: K == TEXT("brake") ? TEXT("freinage") : K == TEXT("kd") ? TEXT("appui") : TEXT("agilité");
	}
	FString Signed(int32 V) { return FString::Printf(TEXT("%s%d"), V > 0 ? TEXT("+") : TEXT(""), V); }

	/** Évaluateur des conditions when(c) du jeu web : c.x, !, &&, ||, === !== == != >= <= > <, nombres, 'texte', null, c.n - 3. */
	struct FCond
	{
		const TMap<FString, FString>& Vals;
		const FString& Src;
		int32 I = 0;
		FCond(const TMap<FString, FString>& InVals, const FString& InSrc) : Vals(InVals), Src(InSrc) {}
		void Ws() { while (I < Src.Len() && FChar::IsWhitespace(Src[I])) ++I; }
		bool Eat(const TCHAR* Tok)
		{
			Ws();
			const int32 L = FCString::Strlen(Tok);
			if (I + L <= Src.Len() && FCString::Strncmp(*Src + I, Tok, L) == 0) { I += L; return true; }
			return false;
		}
		static bool Truthy(const FString& V) { return !V.IsEmpty() && V != TEXT("0") && V != TEXT("false"); }
		FString Operand(bool& bNull)
		{
			Ws();
			bNull = false;
			if (I < Src.Len() && (Src[I] == '\'' || Src[I] == '"'))
			{
				const TCHAR Q = Src[I++];
				const int32 S0 = I;
				while (I < Src.Len() && Src[I] != Q) ++I;
				const FString Out = Src.Mid(S0, I - S0);
				++I;
				return Out;
			}
			if (Eat(TEXT("null"))) { bNull = true; return FString(); }
			if (Eat(TEXT("true"))) return TEXT("1");
			if (Eat(TEXT("false"))) return FString();
			const int32 S0 = I;
			if (Eat(TEXT("c.")))
			{
				const int32 K0 = I;
				while (I < Src.Len() && (FChar::IsAlnum(Src[I]) || Src[I] == '_' || Src[I] == '.')) ++I;
				FString V = Vals.FindRef(Src.Mid(K0, I - K0));
				// c.n - 3
				Ws();
				if (I + 1 < Src.Len() && (Src[I] == '-' || Src[I] == '+') && Src[I + 1] == ' ')
				{
					const TCHAR Op = Src[I]; ++I;
					bool bN = false;
					const FString B = Operand(bN);
					const double X = FCString::Atod(*V), Y = FCString::Atod(*B);
					V = FString::SanitizeFloat(Op == '-' ? X - Y : X + Y);
				}
				return V;
			}
			I = S0;
			while (I < Src.Len() && (FChar::IsDigit(Src[I]) || Src[I] == '.' || Src[I] == '-')) ++I;
			return Src.Mid(S0, I - S0);
		}
		bool Cmp()
		{
			bool bNa = false, bNb = false;
			const FString A = Operand(bNa);
			static const TCHAR* Ops[] = { TEXT("==="), TEXT("!=="), TEXT(">="), TEXT("<="), TEXT("=="), TEXT("!="), TEXT(">"), TEXT("<") };
			for (const TCHAR* Op : Ops)
				if (Eat(Op))
				{
					const FString B = Operand(bNb);
					const bool bEq = Op[0] == '=', bNe = Op[0] == '!';
					if (bNb) return bNe ? !A.IsEmpty() : A.IsEmpty();
					const bool bNum = A.IsNumeric() && B.IsNumeric();
					const double X = FCString::Atod(*A), Y = FCString::Atod(*B);
					if (bEq) return bNum ? X == Y : A == B;
					if (bNe) return bNum ? X != Y : A != B;
					if (!bNum) return false;
					return Op[0] == '>' ? (Op[1] == '=' ? X >= Y : X > Y) : (Op[1] == '=' ? X <= Y : X < Y);
				}
			return Truthy(A);
		}
		bool Not()
		{
			Ws();
			if (I < Src.Len() && Src[I] == '!' && (I + 1 >= Src.Len() || Src[I + 1] != '=')) { ++I; return !Not(); }
			if (Eat(TEXT("("))) { const bool R = Or(); Eat(TEXT(")")); return R; }
			return Cmp();
		}
		bool And() { bool R = Not(); while (Eat(TEXT("&&"))) { const bool B = Not(); R = R && B; } return R; }
		bool Or() { bool R = And(); while (Eat(TEXT("||"))) { const bool B = And(); R = R || B; } return R; }
	};
}

bool UGPMCareerSubsystem::Eval(const FString& Expr) const
{
	if (Expr.IsEmpty()) return true;
	FCond P(Ctx, Expr);
	return P.Or();
}

FString UGPMCareerSubsystem::Fill(const FString& Tpl) const
{
	FString Out;
	int32 I = 0;
	while (I < Tpl.Len())
	{
		const int32 A = Tpl.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, I);
		const int32 B = A >= 0 ? Tpl.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, A) : -1;
		if (A < 0 || B < 0) { Out += Tpl.Mid(I); break; }
		Out += Tpl.Mid(I, A - I);
		const FString Key = Tpl.Mid(A + 1, B - A - 1);
		FString V;
		if (Key.StartsWith(TEXT("v.")) && StoryVars.IsValid())
		{
			// variables de l'histoire : {v.drv.name}
			TArray<FString> Path;
			Key.Mid(2).ParseIntoArray(Path, TEXT("."));
			TSharedPtr<FJsonObject> O = StoryVars;
			for (int32 k = 0; k < Path.Num() && O.IsValid(); ++k)
			{
				if (k == Path.Num() - 1) { if (!O->TryGetStringField(Path[k], V)) { double N; if (O->TryGetNumberField(Path[k], N)) V = FString::SanitizeFloat(N); } }
				else
				{
					const TSharedPtr<FJsonObject>* Sub = nullptr;
					if (O->TryGetObjectField(Path[k], Sub)) O = *Sub; else O.Reset();
				}
			}
		}
		else V = Ctx.FindRef(Key);
		if (V.IsEmpty()) V = Key == TEXT("mate") ? TEXT("ton équipier") : Key == TEXT("rival") ? TEXT("ton rival") : TEXT("…");
		Out += V;
		I = B + 1;
	}
	return Out;
}

void UGPMCareerSubsystem::BuildCtx(const FGPMRaceCtx& R)
{
	const UGPMSaveGame* G = Save();
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	Ctx.Reset();
	if (!G || !Season || !Data) return;
	auto B = [](bool b) { return b ? FString(TEXT("1")) : FString(); };
	Ctx.Add(TEXT("pos"), FString::FromInt(R.Pos));
	Ctx.Add(TEXT("grid"), FString::FromInt(R.Grid));
	Ctx.Add(TEXT("gained"), FString::FromInt(R.Grid - R.Pos));
	Ctx.Add(TEXT("dnf"), B(R.bDnf));
	Ctx.Add(TEXT("fastest"), B(R.bFastest));
	Ctx.Add(TEXT("wet"), B(R.bWet));
	Ctx.Add(TEXT("sc"), B(R.bSC));
	Ctx.Add(TEXT("flat"), B(R.bFlat));
	Ctx.Add(TEXT("pen"), FString::FromInt(FMath::RoundToInt(R.Pen)));
	Ctx.Add(TEXT("stops"), FString::FromInt(R.Stops));
	Ctx.Add(TEXT("circuit"), R.Circuit);
	Ctx.Add(TEXT("winner"), R.Winner);
	Ctx.Add(TEXT("rival"), R.Rival);
	Ctx.Add(TEXT("mate"), G->MateName);
	Ctx.Add(TEXT("mateAhead"), B(R.MatePos > 0 && R.MatePos < R.Pos));
	const TArray<FGPMStanding> Drv = Season->Drivers();
	for (int32 k = 0; k < Drv.Num(); ++k) if (Drv[k].bPlayer) Ctx.Add(TEXT("champ"), FString::FromInt(k + 1));
	const int32 Rd = Season->Round(), N = Season->NumRounds();
	Ctx.Add(TEXT("round"), FString::FromInt(Rd));
	Ctx.Add(TEXT("n"), FString::FromInt(N));
	Ctx.Add(TEXT("last"), B(Rd >= N));
	Ctx.Add(TEXT("mid"), B(Rd == N / 2));
	Ctx.Add(TEXT("role"), G->MateRole);
	Ctx.Add(TEXT("conf"), FString::FromInt(G->Conf));
	Ctx.Add(TEXT("tier"), G->Tier);
	Ctx.Add(TEXT("dev"), B(G->DevPart.Num() > 0));
	for (const FString& F : G->Flags) Ctx.Add(TEXT("flags.") + F, TEXT("1"));
	static const TCHAR* Stars[] = { TEXT("Lily Vroom"), TEXT("TurboTom"), TEXT("Max Drift"), TEXT("Zoé Paddock"), TEXT("Kenji Apex"), TEXT("Nina Slipstream") };
	static const TCHAR* Brands[] = { TEXT("Lumora"), TEXT("Kestrel Tech"), TEXT("Novacel"), TEXT("Brisa Mobile"), TEXT("Atlasbank"), TEXT("Orvane"), TEXT("Pixalto"), TEXT("Verdania") };
	Ctx.Add(TEXT("star"), Stars[FMath::RandRange(0, 5)]);
	Ctx.Add(TEXT("brand"), Brands[FMath::RandRange(0, 7)]);
	if (Data->Teams.Num() > 1) Ctx.Add(TEXT("rivalTeam"), Data->Teams[FMath::RandRange(1, Data->Teams.Num() - 1)].Name);
	const FGPMTrackData* Nx = Season->IsOver() ? nullptr : Data->GetTrack(Season->CurrentCircuit());
	Ctx.Add(TEXT("next"), Nx ? Nx->Name : FString(TEXT("la prochaine manche")));
}

// ---------- effets ----------
void UGPMCareerSubsystem::ApplyFx(const TSharedPtr<FJsonObject>& Fx, float Jit)
{
	UGPMSaveGame* G = Save();
	if (!G || !Fx.IsValid()) return;
	// ampleur variable (±25 %) sur le moral, la réputation, la confiance et la performance
	auto Jitter = [Jit](double V) { return V == 0.0 ? 0 : int32(FMath::Sign(V) * FMath::Max(1.0, FMath::RoundToDouble(FMath::Abs(V) * Jit))); };
	double V = 0.0;
	if (Fx->TryGetNumberField(TEXT("moral"), V)) { V = Jitter(V); Fx->SetNumberField(TEXT("moral"), V); G->Moral = FMath::Clamp(G->Moral + int32(V), 0, 100); }
	if (Fx->TryGetNumberField(TEXT("rep"), V)) { V = Jitter(V); Fx->SetNumberField(TEXT("rep"), V); G->Rep = FMath::Clamp(G->Rep + int32(V), 0, 100); }
	if (Fx->TryGetNumberField(TEXT("conf"), V)) { V = Jitter(V); Fx->SetNumberField(TEXT("conf"), V); G->Conf = FMath::Clamp(G->Conf + int32(V), 0, 100); }
	if (Fx->TryGetNumberField(TEXT("budget"), V)) G->Budget = L1(G->Budget + float(V));
	const TSharedPtr<FJsonObject>* Sub = nullptr;
	if (Fx->TryGetObjectField(TEXT("perf"), Sub))
		for (TPair<FString, TSharedPtr<FJsonValue>>& E : (*Sub)->Values)
		{
			const double P = FMath::RoundToDouble(E.Value->AsNumber() * Jit * 1e4) / 1e4;
			E.Value = MakeShared<FJsonValueNumber>(P);
			G->Boost.FindOrAdd(E.Key) += float(P);
		}
	if (Fx->TryGetNumberField(TEXT("dev"), V))
		for (TPair<FString, float>& E : G->DevLeft) E.Value = FMath::Max(1.f, E.Value + float(V));
	if (Fx->TryGetObjectField(TEXT("mate"), Sub))
		for (const TPair<FString, TSharedPtr<FJsonValue>>& E : (*Sub)->Values)
		{
			float* St = E.Key == TEXT("spd") ? &G->MateSpd : E.Key == TEXT("strat") ? &G->MateStrat : E.Key == TEXT("tyre") ? &G->MateTyre : nullptr;
			if (St) *St = FMath::Clamp(*St + float(E.Value->AsNumber()), 40.f, 99.f);
		}
	if (Fx->TryGetNumberField(TEXT("wage"), V)) G->MateSal = L1(G->MateSal + float(V));
	FString S;
	if (Fx->TryGetStringField(TEXT("role"), S)) G->MateRole = S;
	if (Fx->TryGetStringField(TEXT("flag"), S)) G->Flags.AddUnique(S);
	if (Fx->TryGetNumberField(TEXT("grid"), V)) G->GridPen += int32(V);
	if (Fx->TryGetObjectField(TEXT("deal"), Sub))
	{
		G->DealName.Add(LStr(*Sub, TEXT("name")));
		double Gp = 0.0, Nn = 1.0;
		(*Sub)->TryGetNumberField(TEXT("gp"), Gp); (*Sub)->TryGetNumberField(TEXT("n"), Nn);
		G->DealGp.Add(float(Gp)); G->DealN.Add(int32(Nn));
	}
	// histoire : le pilote de l'histoire devient l'équipier, l'ancien rejoint les pilotes libres
	if (Fx->TryGetObjectField(TEXT("sign"), Sub) && StoryVars.IsValid())
	{
		const TSharedPtr<FJsonObject>* Drv = nullptr;
		if (StoryVars->TryGetObjectField(LStr(*Sub, TEXT("drv")), Drv))
		{
			G->Free.AddUnique(G->MateName);
			G->MateName = LStr(*Drv, TEXT("name"));
			double X = 75; (*Drv)->TryGetNumberField(TEXT("spd"), X); G->MateSpd = X;
			X = 70; (*Drv)->TryGetNumberField(TEXT("strat"), X); G->MateStrat = X;
			X = 70; (*Drv)->TryGetNumberField(TEXT("tyre"), X); G->MateTyre = X;
			X = 70; (*Drv)->TryGetNumberField(TEXT("pot"), X); G->MatePot = X;
			X = 1; (*Sub)->TryGetNumberField(TEXT("sal"), X); G->MateSal = X;
			X = 1; (*Sub)->TryGetNumberField(TEXT("years"), X); G->MateYears = int32(X);
			G->MateRole = LStr(*Sub, TEXT("role")).IsEmpty() ? FString(TEXT("eq")) : LStr(*Sub, TEXT("role"));
			G->DriverTeam.Add(G->MateName, G->TeamName);
			G->DriverPts.FindOrAdd(G->MateName);
			Fx->SetStringField(TEXT("signed"), G->MateName);
		}
	}
	ApplyIdentity();
	if (UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>()) Season->Save();
}

FString UGPMCareerSubsystem::FxText(const TSharedPtr<FJsonObject>& Fx) const
{
	TArray<FString> Out;
	if (!Fx.IsValid()) return FString();
	double V = 0.0;
	if (Fx->TryGetNumberField(TEXT("moral"), V) && V != 0.0) Out.Add(TEXT("Moral ") + Signed(int32(V)));
	if (Fx->TryGetNumberField(TEXT("budget"), V) && V != 0.0) Out.Add(FString::Printf(TEXT("Budget %+.1f M€"), V));
	if (Fx->TryGetNumberField(TEXT("rep"), V) && V != 0.0) Out.Add(TEXT("Réputation ") + Signed(int32(V)));
	if (Fx->TryGetNumberField(TEXT("conf"), V) && V != 0.0) Out.Add(TEXT("Confiance de la direction ") + Signed(int32(V)));
	const TSharedPtr<FJsonObject>* Sub = nullptr;
	if (Fx->TryGetObjectField(TEXT("perf"), Sub))
		for (const TPair<FString, TSharedPtr<FJsonValue>>& E : (*Sub)->Values)
			Out.Add(FString::Printf(TEXT("%+.1f %% %s (prochaine course)"), E.Value->AsNumber() * 100.0, FxName(E.Key)));
	if (Fx->TryGetNumberField(TEXT("dev"), V) && V != 0.0)
		Out.Add(V > 0 ? FString::Printf(TEXT("Recherches : %d jours de retard"), int32(V)) : FString::Printf(TEXT("Recherches en avance de %d jours"), int32(-V)));
	if (Fx->TryGetObjectField(TEXT("mate"), Sub))
		for (const TPair<FString, TSharedPtr<FJsonValue>>& E : (*Sub)->Values)
			Out.Add(FString::Printf(TEXT("Équipier : %s %s"), E.Key == TEXT("spd") ? TEXT("vitesse") : E.Key == TEXT("strat") ? TEXT("stratégie") : TEXT("pneus"), *Signed(int32(E.Value->AsNumber()))));
	if (Fx->TryGetNumberField(TEXT("wage"), V)) Out.Add(FString::Printf(TEXT("Salaire de l'équipier %+.1f M€ par saison"), V));
	FString S;
	if (Fx->TryGetStringField(TEXT("role"), S)) Out.Add(TEXT("Statut de l'équipier : ") + FString(S == TEXT("n1") ? TEXT("N°1") : S == TEXT("n2") ? TEXT("N°2") : TEXT("Égalité")));
	if (Fx->TryGetObjectField(TEXT("deal"), Sub))
	{
		double Gp = 0, Nn = 1;
		(*Sub)->TryGetNumberField(TEXT("gp"), Gp); (*Sub)->TryGetNumberField(TEXT("n"), Nn);
		Out.Add(FString::Printf(TEXT("%s : %+.1f M€ par GP pendant %d GP"), *LStr(*Sub, TEXT("name")), Gp, int32(Nn)));
	}
	if (Fx->TryGetStringField(TEXT("signed"), S)) Out.Add(TEXT("Nouvel équipier : ") + S);
	if (Fx->TryGetNumberField(TEXT("grid"), V)) Out.Add(FString::Printf(TEXT("%d places de pénalité sur la grille"), int32(V)));
	return Out.Num() ? FString::Join(Out, TEXT(" · ")) : FString(TEXT("Aucun effet"));
}

FGPMChoice UGPMCareerSubsystem::ReadChoice(const TSharedPtr<FJsonObject>& O) const
{
	FGPMChoice C;
	C.Text = Fill(LStr(O, TEXT("t")));
	C.Out = Fill(LStr(O, TEXT("out")));
	C.Next = LStr(O, TEXT("next"));
	double Gp = -1; if (O->TryGetNumberField(TEXT("gap"), Gp)) C.Gap = int32(Gp);
	const TSharedPtr<FJsonObject>* Sub = nullptr;
	// copie des effets : l'ampleur est tirée au moment du choix
	C.Fx = MakeShared<FJsonObject>();
	if (O->TryGetObjectField(TEXT("fx"), Sub)) FJsonObject::Duplicate(*Sub, C.Fx);
	if (O->TryGetObjectField(TEXT("risk"), Sub))
	{
		(*Sub)->TryGetNumberField(TEXT("p"), C.RiskP);
		C.RiskOut = Fill(LStr(*Sub, TEXT("out")));
		C.RiskFx = MakeShared<FJsonObject>();
		const TSharedPtr<FJsonObject>* RFx = nullptr;
		if ((*Sub)->TryGetObjectField(TEXT("fx"), RFx)) FJsonObject::Duplicate(*RFx, C.RiskFx);
	}
	return C;
}

// ---------- presse ----------
void UGPMCareerSubsystem::BuildPress(const FGPMRaceCtx& R)
{
	UGPMSaveGame* G = Save();
	const TArray<TSharedPtr<FJsonValue>>* Qs = LArr(Life, TEXT("PRESS"));
	if (!G || !Qs) return;
	TArray<int32> Special, General;
	for (int32 i = 0; i < Qs->Num(); ++i)
	{
		const FString W = LStr((*Qs)[i]->AsObject(), TEXT("when"));
		if (W.IsEmpty()) General.Add(i); else if (Eval(W)) Special.Add(i);
	}
	Shuffle(Special); Shuffle(General);
	// les questions jamais posées récemment d'abord
	auto Fresh = [G](TArray<int32>& A) { A.StableSort([G](int32 X, int32 Y) { return !G->PressSeen.Contains(X) && G->PressSeen.Contains(Y); }); };
	Fresh(Special); Fresh(General);
	TArray<int32> List;
	for (int32 i : Special) { bool bRes = false; (*Qs)[i]->AsObject()->TryGetBoolField(TEXT("res"), bRes); if (bRes) { List.Add(i); break; } }
	for (int32 i : Special) { bool bRes = false; (*Qs)[i]->AsObject()->TryGetBoolField(TEXT("res"), bRes); if (List.Num() < 2 && !bRes && !List.Contains(i)) List.Add(i); }
	for (int32 i : General) if (List.Num() < 3) List.Add(i);
	TArray<int32> People;
	for (int32 k = 0; k < Outlets.Num(); ++k) People.Add(k);
	Shuffle(People);
	for (int32 k = 0; k < List.Num(); ++k)
	{
		const TSharedPtr<FJsonObject> Q = (*Qs)[List[k]]->AsObject();
		FGPMDialog Dl;
		Dl.Kind = FGPMDialog::EKind::Press;
		const int32 Who = People.IsValidIndex(k) ? People[k] : 0;
		Dl.Kicker = FString::Printf(TEXT("CONFÉRENCE DE PRESSE · %s — question %d/%d"), Outlets.IsValidIndex(Who) ? *Outlets[Who] : TEXT("Journaliste"), k + 1, List.Num());
		if (OutletColors.IsValidIndex(Who)) Dl.Color = OutletColors[Who];
		Dl.Text = TEXT("« ") + Fill(LStr(Q, TEXT("q"))) + TEXT(" »");
		if (const TArray<TSharedPtr<FJsonValue>>* As = LArr(Q, TEXT("a")))
			for (const TSharedPtr<FJsonValue>& AV : *As)
			{
				const TArray<TSharedPtr<FJsonValue>>& P = AV->AsArray();
				if (P.Num() < 2) continue;
				FGPMChoice C;
				C.Text = Fill(P[0]->AsString());
				C.Fx = MakeShared<FJsonObject>();
				if (P[1]->Type == EJson::Object) FJsonObject::Duplicate(P[1]->AsObject(), C.Fx);
				Dl.Choices.Add(C);
			}
		Shuffle(Dl.Choices);
		Queue.Add(Dl);
		G->PressSeen.Insert(List[k], 0);
	}
	if (G->PressSeen.Num() > 36) G->PressSeen.SetNum(36);
}

// ---------- flash info et histoires ----------
void UGPMCareerSubsystem::EventDialog(const TSharedPtr<FJsonObject>& E)
{
	FGPMDialog Dl;
	Dl.Kind = FGPMDialog::EKind::Event;
	bool bLite = false;
	E->TryGetBoolField(TEXT("lite"), bLite);
	Dl.Kicker = bLite ? TEXT("EN BREF") : TEXT("FLASH INFO");
	Dl.Color = bLite ? FLinearColor(1.f, 0.6f, 0.2f) : FLinearColor(1.f, 0.24f, 0.18f);
	Dl.Title = Fill(LStr(E, TEXT("title")));
	Dl.Text = Fill(LStr(E, TEXT("text")));
	Dl.Jit = FMath::RoundToFloat((0.75f + FMath::FRand() * 0.5f) * 10.f) / 10.f;
	if (const TArray<TSharedPtr<FJsonValue>>* Ch = LArr(E, TEXT("ch")))
		for (const TSharedPtr<FJsonValue>& CV : *Ch) Dl.Choices.Add(ReadChoice(CV->AsObject()));
	if (Dl.Choices.Num()) Queue.Add(Dl);
}

bool UGPMCareerSubsystem::StoryDialog(const FString& Id, const FString& Step, int32 Ep)
{
	const UGPMSaveGame* G = Save();
	const TArray<TSharedPtr<FJsonValue>>* Sts = LArr(Life, TEXT("STORIES"));
	if (!G || !Sts) return false;
	for (const TSharedPtr<FJsonValue>& SV : *Sts)
	{
		const TSharedPtr<FJsonObject> St = SV->AsObject();
		if (LStr(St, TEXT("id")) != Id) continue;
		if (const TArray<TSharedPtr<FJsonValue>>* Vars = LArr(St, TEXT("vars")))
			if (Vars->Num()) StoryVars = (*Vars)[FMath::Clamp(G->StoryVar, 0, Vars->Num() - 1)]->AsObject();
		const TSharedPtr<FJsonObject>* Steps = nullptr;
		const TSharedPtr<FJsonObject>* Ep0 = nullptr;
		if (!St->TryGetObjectField(TEXT("steps"), Steps) || !(*Steps)->TryGetObjectField(Step, Ep0)) return false;
		FGPMDialog Dl;
		Dl.Kind = FGPMDialog::EKind::Story;
		Dl.Kicker = FString::Printf(TEXT("HISTOIRE · ÉPISODE %d · %s"), Ep, *LStr(St, TEXT("title")));
		Dl.Color = FLinearColor(0.69f, 0.42f, 1.f);
		Dl.Title = Fill(LStr(*Ep0, TEXT("title")));
		Dl.Text = Fill(LStr(*Ep0, TEXT("text")));
		Dl.StoryId = Id; Dl.Step = Step;
		Dl.Jit = FMath::RoundToFloat((0.75f + FMath::FRand() * 0.5f) * 10.f) / 10.f;
		if (const TArray<TSharedPtr<FJsonValue>>* Ch = LArr(*Ep0, TEXT("ch")))
			for (const TSharedPtr<FJsonValue>& CV : *Ch) Dl.Choices.Add(ReadChoice(CV->AsObject()));
		Queue.Add(Dl);
		return true;
	}
	return false;
}

void UGPMCareerSubsystem::PickEvents()
{
	UGPMSaveGame* G = Save();
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	if (!G || !Season) return;
	const int32 Rd = Season->Round(), N = Season->NumRounds();
	bool bStory = false;
	// histoire en cours : l'épisode suivant passe avant le reste ; sinon une histoire démarre une fois sur deux
	if (!G->StoryId.IsEmpty())
	{
		if (G->StoryDue <= Rd) bStory = StoryDialog(G->StoryId, G->StoryStep, G->StoryEp);
		if (!bStory && G->StoryDue <= Rd) G->StoryId.Reset();
	}
	else if (Rd >= 1 && Rd <= N - 7 && FMath::FRand() < 0.5f)
		if (const TArray<TSharedPtr<FJsonValue>>* Sts = LArr(Life, TEXT("STORIES")))
		{
			TArray<TSharedPtr<FJsonObject>> Pool;
			for (const TSharedPtr<FJsonValue>& SV : *Sts)
			{
				const TSharedPtr<FJsonObject> St = SV->AsObject();
				if (!G->SeenSt.Contains(LStr(St, TEXT("id"))) && Eval(LStr(St, TEXT("when")))) Pool.Add(St);
			}
			if (Pool.Num())
			{
				const TSharedPtr<FJsonObject> St = Pool[FMath::RandRange(0, Pool.Num() - 1)];
				G->StoryId = LStr(St, TEXT("id")); G->StoryStep = TEXT("a"); G->StoryDue = Rd; G->StoryEp = 1; G->StoryVar = FMath::RandRange(0, 3);
				G->SeenSt.Add(G->StoryId);
				bStory = StoryDialog(G->StoryId, G->StoryStep, 1);
				if (!bStory) G->StoryId.Reset();
			}
		}
	// flash info : les suites débloquées par un choix précédent (flags) d'abord
	const TArray<TSharedPtr<FJsonValue>>* Evs = LArr(Life, TEXT("EVENTS"));
	if (!Evs) return;
	TArray<TSharedPtr<FJsonObject>> Pool, Follow, Lite;
	for (const TSharedPtr<FJsonValue>& EV : *Evs)
	{
		const TSharedPtr<FJsonObject> E = EV->AsObject();
		bool bPre = false, bLite = false;
		E->TryGetBoolField(TEXT("pre"), bPre); E->TryGetBoolField(TEXT("lite"), bLite);
		const FString W = LStr(E, TEXT("when"));
		if (bPre || G->SeenEv.Contains(LStr(E, TEXT("id"))) || !Eval(W)) continue;
		Pool.Add(E);
		if (W.Contains(TEXT("flags."))) Follow.Add(E);
		if (bLite) Lite.Add(E);
	}
	TSharedPtr<FJsonObject> First;
	if (!bStory && Pool.Num()) First = Follow.Num() && FMath::FRand() < 0.6f ? Follow[FMath::RandRange(0, Follow.Num() - 1)] : Pool[FMath::RandRange(0, Pool.Num() - 1)];
	if (First.IsValid()) { G->SeenEv.Add(LStr(First, TEXT("id"))); EventDialog(First); }
	if (FMath::FRand() < 0.4f)
	{
		Lite.Remove(First);
		if (Lite.Num()) { const TSharedPtr<FJsonObject> E = Lite[FMath::RandRange(0, Lite.Num() - 1)]; G->SeenEv.Add(LStr(E, TEXT("id"))); EventDialog(E); }
	}
	if (G->SeenEv.Num() > Evs->Num() - 6) G->SeenEv.Reset();   // tout a été vu : on recommence
}

// ---------- choix ----------
void UGPMCareerSubsystem::Choose(int32 I)
{
	FGPMDialog* Dl = Current();
	if (!Dl || Dl->Chosen >= 0 || !Dl->Choices.IsValidIndex(I)) return;
	const FGPMChoice& C = Dl->Choices[I];
	if (Dl->Kind == FGPMDialog::EKind::Nego) { if (!C.bOff) NegoAct(C.Action); return; }
	const bool bBadLuck = C.RiskP > 0.0 && FMath::FRand() < C.RiskP;
	const TSharedPtr<FJsonObject> Fx = bBadLuck && C.RiskFx.IsValid() ? C.RiskFx : C.Fx;
	ApplyFx(Fx, Dl->Kind == FGPMDialog::EKind::Press ? 1.f : Dl->Jit);
	Dl->Chosen = I;
	Dl->bBad = bBadLuck;
	Dl->Result = bBadLuck ? C.RiskOut : C.Out;
	Dl->FxText = FxText(Fx);
	if (Dl->Kind == FGPMDialog::EKind::Story)
		if (UGPMSaveGame* G = Save())
		{
			UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
			if (!C.Next.IsEmpty())
			{
				G->StoryStep = C.Next;
				G->StoryDue = (Season ? Season->Round() : 0) + (C.Gap >= 0 ? C.Gap : 1 + FMath::RandRange(0, 1));
				++G->StoryEp;
				Dl->Result += TEXT("   À suivre…");
			}
			else
			{
				G->StoryId.Reset();
				Dl->Result += TEXT("   Fin de l'histoire.");
			}
			if (Season) Season->Save();
		}
}

void UGPMCareerSubsystem::Next()
{
	if (Queue.Num()) Queue.RemoveAt(0);
}
