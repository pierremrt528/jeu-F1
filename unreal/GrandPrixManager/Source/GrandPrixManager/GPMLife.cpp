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
		G->DealName.Add(Fill(LStr(*Sub, TEXT("name"))));   // le nom peut venir d'une histoire ({v.firm})
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
		Out.Add(FString::Printf(TEXT("%s : %+.1f M€ par GP pendant %d GP"), *Fill(LStr(*Sub, TEXT("name"))), Gp, int32(Nn)));
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
// questions écrites à partir de ce qui vient de se passer en course : noms, places, écarts, incidents
TArray<FGPMDialog> UGPMCareerSubsystem::RaceQuestions(const FGPMRaceCtx& R) const
{
	const UGPMSaveGame* G = Save();
	TArray<TPair<int32, FGPMDialog>> Cands;   // (priorité, question)
	struct FAns { FString Text; float Moral, Rep, Conf, Budget; };   // réponse et effets : moral, réputation, confiance, budget
	auto Q = [&Cands](int32 Prio, const FString& Text, std::initializer_list<FAns> Answers)
	{
		FGPMDialog Dl;
		Dl.Text = TEXT("« ") + Text + TEXT(" »");
		for (const FAns& Ans : Answers)
		{
			FGPMChoice C;
			C.Text = Ans.Text;
			C.Fx = MakeShared<FJsonObject>();
			if (Ans.Moral != 0.f) C.Fx->SetNumberField(TEXT("moral"), Ans.Moral);
			if (Ans.Rep != 0.f) C.Fx->SetNumberField(TEXT("rep"), Ans.Rep);
			if (Ans.Conf != 0.f) C.Fx->SetNumberField(TEXT("conf"), Ans.Conf);
			if (Ans.Budget != 0.f) C.Fx->SetNumberField(TEXT("budget"), Ans.Budget);
			Dl.Choices.Add(C);
		}
		Cands.Add(TPair<int32, FGPMDialog>(Prio + FMath::RandRange(0, 2), Dl));
	};
	const FString Circ = R.Circuit.IsEmpty() ? FString(TEXT("ce circuit")) : R.Circuit;
	const FString Mate = G ? G->MateName : FString();
	const int32 Gain = R.Grid - R.Pos;
	auto S1 = [](float X) { return FString::Printf(TEXT("%.1f"), X).Replace(TEXT("."), TEXT(",")); };

	if (R.bDnf)
	{
		if (R.OutWhy == TEXT("moteur"))
			Q(20, FString::Printf(TEXT("Le moteur a lâché à %s alors que tu étais parti P%d. Les ingénieurs ont-ils une explication ?"), *Circ, R.Grid),
				{ { FString(TEXT("On va tout démonter cette nuit, ça ne doit plus arriver.")), 3, 1, 2, 0 }, { FString(TEXT("Le motoriste doit nous donner des réponses, et vite.")), -2, -1, -3, 0 }, { FString(TEXT("La fiabilité, c'est notre priorité à l'usine dès demain.")), 2, 2, 3, 0 } });
		else if (R.OutWhy == TEXT("drapeau noir"))
			Q(20, FString::Printf(TEXT("Drapeau noir à %s : trop de pénalités. Comment expliques-tu une telle course ?"), *Circ),
				{ { FString(TEXT("C'est de ma faute, je dois être plus discipliné.")), 2, 2, 1, 0 }, { FString(TEXT("Les commissaires ont été très sévères aujourd'hui.")), -1, -3, -2, 0 }, { FString(TEXT("Je préfère ne pas commenter.")), 0, -1, 0, 0 } });
		else
			Q(20, FString::Printf(TEXT("Ta course s'est terminée dans le mur à %s. Que s'est-il passé ?"), *Circ),
				{ { FString(TEXT("J'ai poussé trop fort, je m'excuse auprès des mécaniciens.")), 4, 2, 1, 0 }, { FString(TEXT("La voiture est partie sans prévenir, il faut comprendre.")), -1, 0, -1, 0 }, { FString(TEXT("C'est la course. Je ne changerai pas ma façon d'attaquer.")), 1, -2, -1, 0 } });
	}
	if (!R.bDnf && R.Grid == 1 && R.Pos > 1)
		Q(16, FString::Printf(TEXT("Parti en pole, arrivé P%d. Où cette course t'a-t-elle échappé ?"), R.Pos),
			{ { FString(TEXT("Le rythme de course n'était pas là, on doit travailler les longs relais.")), 2, 1, 1, 0 }, { FString(TEXT("La stratégie ne m'a pas aidé aujourd'hui.")), -3, 0, -1, 0 }, { FString(TEXT("La pole montre notre vitesse, la victoire viendra.")), 3, 1, 0, 0 } });
	if (!R.bDnf && Gain >= 3)
		Q(14, FString::Printf(TEXT("Parti P%d, arrivé P%d : %d places gagnées à %s. Où as-tu fait la différence ?"), R.Grid, R.Pos, Gain, *Circ),
			{ { FString(TEXT("Au départ et dans les premiers tours, j'ai pris tous les risques.")), 4, 3, 0, 0 }, { FString(TEXT("La stratégie du muret était parfaite, bravo à eux.")), 6, 1, 1, 0 }, { FString(TEXT("La voiture était meilleure en course qu'en qualifications.")), 3, 1, 1, 0 } });
	if (!R.bDnf && Gain <= -3)
		Q(14, FString::Printf(TEXT("Parti P%d, tu termines P%d. Qu'est-ce qui n'a pas marché ?"), R.Grid, R.Pos),
			{ { FString(TEXT("On a manqué de rythme, je ne cherche pas d'excuses.")), 1, 2, 0, 0 }, { FString(TEXT("Les pneus se sont dégradés beaucoup trop vite.")), 0, 0, 0, 0 }, { FString(TEXT("On a fait des erreurs au stand, il faudra les corriger.")), -3, 0, -1, 0 } });
	if (!R.bDnf && R.Pos > 1 && !R.Winner.IsEmpty() && R.Winner != TEXT("Toi") && R.Pos <= 6)
		Q(10, FString::Printf(TEXT("%s gagne à %s, tu termines à %s. Peut-on le battre cette saison ?"), *R.Winner, *Circ, *R.WinGap),
			{ { FString(FString::Printf(TEXT("Oui. %s a eu une course parfaite, ça n'arrivera pas à chaque fois."), *R.Winner)), 3, 1, 0, 0 }, { FString(TEXT("Pas encore, il nous faut des évolutions à l'usine.")), -1, 1, 1, 0 }, { FString(TEXT("Je ne pense qu'à ma course, pas aux autres.")), 1, 0, 0, 0 } });
	if (!R.bDnf && !R.Ahead.IsEmpty() && R.GapAhead > 0.f && R.GapAhead < 3.f)
		Q(12, FString::Printf(TEXT("Tu termines à seulement %s s de %s. Il t'a manqué quoi pour passer ?"), *S1(R.GapAhead), *R.Ahead),
			{ { FString(TEXT("Un tour de plus et je le passais !")), 3, 1, 0, 0 }, { FString(TEXT("De la vitesse en ligne droite, on va y travailler.")), 1, 1, 1, 0 }, { FString(FString::Printf(TEXT("%s a très bien défendu, chapeau."), *R.Ahead)), 1, 3, 0, 0 } });
	if (!R.bDnf && !R.Behind.IsEmpty() && R.GapBehind > 0.f && R.GapBehind < 1.5f)
		Q(12, FString::Printf(TEXT("%s a fini à %s s derrière toi. Comment as-tu tenu sous cette pression ?"), *R.Behind, *S1(R.GapBehind)),
			{ { FString(TEXT("J'ai gardé la tête froide et fermé toutes les portes.")), 4, 2, 0, 0 }, { FString(TEXT("Honnêtement, les derniers tours ont été très longs.")), 2, 1, 0, 0 }, { FString(TEXT("Je l'avais sous contrôle du début à la fin.")), 2, -1, 0, 0 } });
	if (!Mate.IsEmpty() && R.MatePos > 0 && !R.bDnf)
	{
		if (R.MatePos < R.Pos)
			Q(11, FString::Printf(TEXT("%s termine P%d, devant toi (P%d). Le rapport de force change-t-il dans l'équipe ?"), *Mate, R.MatePos, R.Pos),
				{ { FString(FString::Printf(TEXT("Bravo à %s, ça me motive pour la prochaine."), *Mate)), 3, 2, 0, 0 }, { FString(TEXT("Une course ne veut rien dire, je reste devant au championnat.")), -1, 0, 0, 0 }, { FString(TEXT("Deux voitures rapides, c'est bon pour l'écurie.")), 2, 1, 1, 0 } });
		else if (R.Pos <= 10)
			Q(7, FString::Printf(TEXT("Tu finis devant %s (P%d). Es-tu le leader de l'écurie ?"), *Mate, R.MatePos),
				{ { FString(TEXT("On se pousse mutuellement, c'est ce qui compte.")), 3, 1, 0, 0 }, { FString(TEXT("Les résultats parlent pour moi.")), -2, 1, 0, 0 }, { FString(TEXT("La direction décidera, moi je roule.")), 0, 0, 1, 0 } });
	}
	if (R.Pen > 0.f)
		Q(13, FString::Printf(TEXT("%.0f secondes de pénalité aujourd'hui. Les commissaires ont-ils eu raison ?"), R.Pen),
			{ { FString(TEXT("Oui, j'ai fait une erreur, ça ne se reproduira pas.")), 1, 2, 1, 0 }, { FString(TEXT("Non, la décision est injuste et on fera appel.")), -1, -2, -1, 0 }, { FString(TEXT("Ça fait partie de la course, on passe à la suite.")), 1, 0, 0, 0 } });
	if (R.bRed)
		Q(13, FString::Printf(TEXT("Drapeau rouge à %s, course arrêtée puis relancée. Comment as-tu vécu cette reprise ?"), *Circ),
			{ { FString(TEXT("Un nouveau départ, c'était une chance à saisir.")), 3, 1, 0, 0 }, { FString(TEXT("Le plus important, c'est que tout le monde aille bien.")), 2, 3, 0, 0 }, { FString(TEXT("Ça a cassé mon rythme, c'est frustrant.")), -2, 0, 0, 0 } });
	if (R.bSC && !R.bDnf)
		Q(9, Gain > 0 ? FString::Printf(TEXT("La voiture de sécurité t'a-t-elle aidé à remonter jusqu'à la P%d ?"), R.Pos) : FString::Printf(TEXT("La voiture de sécurité a resserré le peloton. Elle t'a coûté des places à %s ?"), *Circ),
			{ { FString(TEXT("On a bien réagi au muret, c'était le bon moment.")), 3, 1, 1, 0 }, { FString(TEXT("Elle a tout changé, c'est la loterie de la course.")), 0, 0, 0, 0 }, { FString(TEXT("Avec ou sans, notre rythme aurait parlé.")), 2, -1, 0, 0 } });
	if (R.bFlat)
		Q(13, TEXT("Une crevaison pendant la course : avais-tu trop poussé sur tes pneus ?"),
			{ { FString(TEXT("Oui, j'ai tiré trop longtemps sur ce train de pneus.")), 1, 2, 0, 0 }, { FString(TEXT("Un débris sur la piste, rien à faire.")), 0, 0, 0, 0 }, { FString(TEXT("Le muret aurait dû m'appeler plus tôt.")), -4, 0, -1, 0 } });
	if (R.bFastest && !R.bDnf)
		Q(8, FString::Printf(TEXT("Meilleur tour en course à %s. Cette voiture a encore de la marge ?"), *Circ),
			{ { FString(TEXT("Oui, et on en garde sous le pied.")), 3, 1, 0, 0 }, { FString(TEXT("C'est le fruit du travail de l'usine.")), 4, 1, 1, 0 }, { FString(TEXT("Un tour, c'est bien, une course entière, c'est mieux.")), 1, 1, 0, 0 } });
	if (R.bWet && !R.bDnf)
		Q(8, FString::Printf(TEXT("La pluie s'est invitée à %s. Le bon pneu au bon moment, c'était un pari ?"), *Circ),
			{ { FString(TEXT("Le muret a lu la météo parfaitement.")), 4, 1, 1, 0 }, { FString(TEXT("J'adore rouler sous la pluie, c'est là qu'on voit les pilotes.")), 2, 2, 0, 0 }, { FString(TEXT("On a perdu du temps à hésiter.")), -2, 0, 0, 0 } });
	if (R.Stops >= 2 && !R.bDnf)
		Q(6, FString::Printf(TEXT("%d arrêts au stand aujourd'hui. Pourquoi une stratégie aussi agressive ?"), R.Stops),
			{ { FString(TEXT("Des pneus frais pour attaquer à chaque relais.")), 2, 1, 0, 0 }, { FString(TEXT("L'usure était trop forte, on n'avait pas le choix.")), 0, 0, 0, 0 }, { FString(TEXT("On a pris un risque, il a payé.")), 3, 1, 0, 0 } });
	Cands.StableSort([](const TPair<int32, FGPMDialog>& X, const TPair<int32, FGPMDialog>& Y) { return X.Key > Y.Key; });
	TArray<FGPMDialog> Out;
	for (const TPair<int32, FGPMDialog>& C : Cands) Out.Add(C.Value);
	return Out;
}

void UGPMCareerSubsystem::BuildPress(const FGPMRaceCtx& R)
{
	UGPMSaveGame* G = Save();
	const TArray<TSharedPtr<FJsonValue>>* Qs = LArr(Life, TEXT("PRESS"));
	if (!G || !Qs) return;
	TArray<int32> Special, General;
	for (int32 i = 0; i < Qs->Num(); ++i)
	{
		const FString W = LStr((*Qs)[i]->AsObject(), TEXT("when"));
		// « !!c.rival » / « !!c.mate » sont vrais à chaque course : questions générales
		if (W.IsEmpty() || W == TEXT("!!c.rival") || W == TEXT("!!c.mate")) General.Add(i); else if (Eval(W)) Special.Add(i);
	}
	Shuffle(Special); Shuffle(General);
	// les questions jamais posées récemment d'abord
	auto Fresh = [G](TArray<int32>& A) { A.StableSort([G](int32 X, int32 Y) { return !G->PressSeen.Contains(X) && G->PressSeen.Contains(Y); }); };
	Fresh(Special); Fresh(General);
	// d'abord la question sur le résultat, puis jusqu'à deux questions sur les faits de la course (noms, écarts, incidents),
	// puis les questions de contexte ; les questions générales ne servent qu'à compléter
	TArray<FGPMDialog> Facts = RaceQuestions(R);
	if (Facts.Num() > 2) Facts.SetNum(2);
	TArray<int32> List;
	for (int32 i : Special) { bool bRes = false; (*Qs)[i]->AsObject()->TryGetBoolField(TEXT("res"), bRes); if (bRes) { List.Add(i); break; } }
	for (int32 i : Special) { bool bRes = false; (*Qs)[i]->AsObject()->TryGetBoolField(TEXT("res"), bRes); if (List.Num() + Facts.Num() < 3 && !bRes && !List.Contains(i)) List.Add(i); }
	for (int32 i : General) if (List.Num() + Facts.Num() < 3) List.Add(i);
	const int32 Total = List.Num() + Facts.Num();
	TArray<int32> People;
	for (int32 k = 0; k < Outlets.Num(); ++k) People.Add(k);
	Shuffle(People);
	int32 Asked = 0;
	auto Head = [&](FGPMDialog& Dl)
	{
		Dl.Kind = FGPMDialog::EKind::Press;
		const int32 Who = People.IsValidIndex(Asked) ? People[Asked] : 0;
		++Asked;
		Dl.Kicker = FString::Printf(TEXT("CONFÉRENCE DE PRESSE · %s — question %d/%d"), Outlets.IsValidIndex(Who) ? *Outlets[Who] : TEXT("Journaliste"), Asked, Total);
		if (OutletColors.IsValidIndex(Who)) Dl.Color = OutletColors[Who];
	};
	for (int32 k = 0; k < List.Num(); ++k)
	{
		if (k == 1)
			for (FGPMDialog& F : Facts) { Head(F); Shuffle(F.Choices); Queue.Add(F); }
		const TSharedPtr<FJsonObject> Q = (*Qs)[List[k]]->AsObject();
		FGPMDialog Dl;
		Head(Dl);
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
	if (List.Num() <= 1) for (FGPMDialog& F : Facts) { Head(F); Shuffle(F.Choices); Queue.Add(F); }
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

void UGPMCareerSubsystem::PreEvents(bool bLaunch)
{
	UGPMSaveGame* G = Save();
	if (!G || !G->bCreated) return;
	Queue.Reset();
	FGPMRaceCtx R;   // pas encore de course : contexte neutre
	BuildCtx(R);
	if (!bLaunch) { PickEvents(); return; }
	if (const TArray<TSharedPtr<FJsonValue>>* Evs = LArr(Life, TEXT("EVENTS")))
		for (const TSharedPtr<FJsonValue>& EV : *Evs)
			if (LStr(EV->AsObject(), TEXT("id")) == TEXT("launch")) { EventDialog(EV->AsObject()); return; }
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
	if (Dl->Kind == FGPMDialog::EKind::Press)
	{
		// « CONFÉRENCE DE PRESSE · Média — question k/n » → le nom du média
		FString Who = Dl->Kicker;
		Who.Split(TEXT(" · "), nullptr, &Who);
		Who.Split(TEXT(" — "), &Who, nullptr);
		PressLog.Add(FString::Printf(TEXT("%s : %s\n> %s%s"), *Who, *Dl->Text, *C.Text, Dl->FxText.IsEmpty() ? TEXT("") : *(TEXT("\n  ") + Dl->FxText)));
	}
	else
	{
		const bool bStory = Dl->Kind == FGPMDialog::EKind::Story;
		FString From = bStory ? Dl->Kicker : FString(Dl->Kicker == TEXT("EN BREF") ? TEXT("En bref") : TEXT("Flash info"));
		if (bStory) From.Split(TEXT(" · "), nullptr, &From, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		SendMail(TEXT("evenement"), From, Dl->Title, FString::Printf(TEXT("%s\n\nTa décision : %s\n%s%s%s"), *Dl->Text, *C.Text, Dl->bBad ? TEXT("(pas de chance) ") : TEXT(""), *Dl->Result,
			Dl->FxText.IsEmpty() ? TEXT("") : *(TEXT("\n\n") + Dl->FxText)));
	}
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
	// fin de la conférence de presse : un seul message qui reprend les questions et les réponses
	if (PressLog.Num() && !(Queue.Num() && Queue[0].Kind == FGPMDialog::EKind::Press))
	{
		SendMail(TEXT("presse"), TEXT("Service de presse"), FString::Printf(TEXT("Revue de presse : %d question%s après la course"), PressLog.Num(), PressLog.Num() > 1 ? TEXT("s") : TEXT("")),
			FString::Join(PressLog, TEXT("\n\n")));
		PressLog.Reset();
		if (UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>()) Season->Save();
	}
}
