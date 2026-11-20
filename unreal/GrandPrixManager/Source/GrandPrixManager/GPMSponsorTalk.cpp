// Sponsors à la carte : on renégocie le contrat du sponsor principal (montant fixe, durée, prime par Grand Prix, objectif de course)
// et on signe jusqu'à 3 sponsors secondaires de la même façon. Le sponsor compare la valeur attendue de la demande (fixe + primes
// selon les chances de remplir l'objectif) à ce qu'il est prêt à payer (son contrat de base, la réputation de l'écurie, la durée).
#include "GPMCareer.h"
#include "GrandPrixManager.h"
#include "GPMSeason.h"

namespace GPMSpK
{
	float R1s(float X) { return FMath::RoundToFloat(X * 10.f) / 10.f; }
	const TCHAR* const Goals[7] = { TEXT("finish"), TEXT("pts"), TEXT("beatMate"), TEXT("gain"), TEXT("top5"), TEXT("podium"), TEXT("fl") };
	const float BonusMul[5] = { 0.f, 0.5f, 1.f, 1.5f, 2.f };
}

UGPMCareerSubsystem::FSpDeal UGPMCareerSubsystem::ParseDeal(const FString& S)
{
	FSpDeal D;
	TArray<FString> P;
	S.ParseIntoArray(P, TEXT("|"), false);
	if (P.Num() < 5) return D;
	D.Id = P[0]; D.Fix = FCString::Atof(*P[1]); D.Bonus = FCString::Atof(*P[2]); D.Goal = P[3]; D.Until = FCString::Atoi(*P[4]);
	return D;
}

UGPMCareerSubsystem::FSpDeal UGPMCareerSubsystem::MainDeal() const
{
	const UGPMSaveGame* G = Save();
	if (!G) return FSpDeal();
	if (!G->MainDeal.IsEmpty()) return ParseDeal(G->MainDeal);
	FSpDeal D;
	for (const FGPMSponsorOpt& Sp : Sponsors) if (Sp.Id == G->Sponsor) { D.Id = Sp.Id; D.Fix = Sp.Year; D.Bonus = Sp.Bonus; D.Goal = Sp.Goal; }
	return D;
}

TArray<UGPMCareerSubsystem::FSpDeal> UGPMCareerSubsystem::SecDeals() const
{
	TArray<FSpDeal> Out;
	if (const UGPMSaveGame* G = Save()) for (const FString& S : G->SecDeals) Out.Add(ParseDeal(S));
	return Out;
}

bool UGPMCareerSubsystem::CanTalkSponsor() const
{
	const UGPMSaveGame* G = Save();
	return G && G->bCreated && G->SpTalkAt != G->Round;
}

FString UGPMCareerSubsystem::GoalName(const FString& Goal)
{
	return Goal == TEXT("finish") ? TEXT("termines la course") : Goal == TEXT("pts") ? TEXT("marques des points") : Goal == TEXT("beatMate") ? TEXT("bats ton équipier")
		: Goal == TEXT("gain") ? TEXT("gagnes des places") : Goal == TEXT("top5") ? TEXT("finis dans le top 5") : Goal == TEXT("podium") ? TEXT("montes sur le podium")
		: Goal == TEXT("fl") ? TEXT("signes le meilleur tour") : TEXT("remplis l'objectif");
}

float UGPMCareerSubsystem::GoalChance(const FString& Goal) const
{
	const float Exp = float(Expected());   // place attendue par la direction : 4, 9 ou 14
	// la forme du moment compte : réussite de l'objectif sur les 8 derniers Grands Prix (la place attendue vaut 2 courses)
	const int32 Lim = Goal == TEXT("pts") ? 10 : Goal == TEXT("top5") ? 5 : Goal == TEXT("podium") ? 3 : 0;
	const UGPMSeasonSubsystem* Ss = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	if (Lim > 0 && Ss && Ss->Results().Num() >= 3)
	{
		const TArray<FGPMRaceResult>& Res = Ss->Results();
		int32 Hit = 0, Seen = 0;
		for (int32 k = Res.Num() - 1; k >= 0 && Seen < 8; --k, ++Seen) if (Res[k].Pos > 0 && Res[k].Pos <= Lim) ++Hit;
		const float Prior = FMath::Clamp((Lim + 4.f - Exp) / 10.f, 0.03f, 0.9f);
		return FMath::Clamp((Hit + 2.f * Prior) / (Seen + 2.f), 0.03f, 0.95f);
	}
	return Goal == TEXT("finish") ? 0.85f : Goal == TEXT("pts") ? FMath::Clamp((15.f - Exp) / 10.f, 0.1f, 0.9f) : Goal == TEXT("beatMate") ? 0.5f
		: Goal == TEXT("gain") ? 0.45f : Goal == TEXT("top5") ? FMath::Clamp((9.f - Exp) / 10.f, 0.05f, 0.75f) : Goal == TEXT("podium") ? FMath::Clamp((7.f - Exp) / 10.f, 0.03f, 0.6f)
		: Goal == TEXT("fl") ? FMath::Clamp((6.f - Exp) / 14.f, 0.02f, 0.35f) : 0.3f;
}

void UGPMCareerSubsystem::OpenSponsorTalk(int32 Sp, bool bSec)
{
	const UGPMSaveGame* G = Save();
	if (!G || !CanTalkSponsor()) return;
	if (!bSec) for (int32 k = 0; k < Sponsors.Num(); ++k) if (Sponsors[k].Id == G->Sponsor) Sp = k;
	const TArray<FGPMSponsorOpt>& L = bSec ? SecSponsors : Sponsors;
	if (!L.IsValidIndex(Sp)) return;
	SpTalk = FSpTalk();
	SpTalk.bSec = bSec; SpTalk.Sp = Sp;
	const FSpDeal Cur = bSec ? FSpDeal() : MainDeal();
	SpTalk.Fix = GPMSpK::R1s(bSec ? L[Sp].Year : Cur.Fix);
	const FString Goal = bSec ? L[Sp].Goal : Cur.Goal;
	for (int32 k = 0; k < 7; ++k) if (Goal == GPMSpK::Goals[k]) SpTalk.GoalI = k;
	SpTalk.Patience = FMath::Max(1, 3 + (NegoLevel == 0 ? 1 : NegoLevel == 2 ? -1 : 0));
	SponsorDialog(FString());
}

void UGPMCareerSubsystem::OpenSecondary()
{
	const UGPMSaveGame* G = Save();
	if (!G || !CanTalkSponsor()) return;
	SpTalk = FSpTalk();
	FGPMDialog Dl;
	Dl.Kind = FGPMDialog::EKind::Nego;
	Dl.Color = FLinearColor(0.31f, 0.82f, 0.48f);
	Dl.Kicker = FString::Printf(TEXT("SPONSORS SECONDAIRES · %d / 3"), G->SecDeals.Num());
	Dl.Title = TEXT("Petits partenaires : un montant fixe par saison et une prime à chaque objectif rempli");
	Dl.Text = TEXT("Choisis une marque pour ouvrir la discussion. Tout se négocie : montant, durée, prime et objectif.");
	for (const FSpDeal& D : SecDeals())
		for (const FGPMSponsorOpt& Sp : SecSponsors)
			if (Sp.Id == D.Id) Dl.Text += FString::Printf(TEXT("\n• %s : %.1f M€ par saison + %.1f M€ par GP si tu %s (jusqu'en %d)"), *Sp.Name, D.Fix, D.Bonus, *GoalName(D.Goal), D.Until);
	auto Add = [&Dl](const FString& Txt, int32 Act, bool bOff) { FGPMChoice C; C.Text = Txt; C.Action = Act; C.bOff = bOff; Dl.Choices.Add(C); };
	for (int32 k = 0; k < SecSponsors.Num(); ++k)
	{
		const FGPMSponsorOpt& Sp = SecSponsors[k];
		if (G->SecDeals.ContainsByPredicate([&Sp](const FString& S) { return S.StartsWith(Sp.Id + TEXT("|")); })) continue;
		const bool bRep = Sp.Rep <= G->Rep;
		Add(bRep ? FString::Printf(TEXT("%s (%s) · %.1f M€ / saison + %.1f M€ par GP si tu %s"), *Sp.Name, *Sp.Sector, Sp.Year, Sp.Bonus, *GoalName(Sp.Goal))
			: FString::Printf(TEXT("%s (%s) · réputation %d demandée"), *Sp.Name, *Sp.Sector, Sp.Rep), 100 + k, !bRep || G->SecDeals.Num() >= 3);
	}
	Add(TEXT("FERMER"), 46, false);
	if (Queue.Num()) Queue[0] = Dl; else Queue.Add(Dl);
}

void UGPMCareerSubsystem::SponsorDialog(const FString& Line)
{
	using namespace GPMSpK;
	const UGPMSaveGame* G = Save();
	const TArray<FGPMSponsorOpt>& L = SpTalk.bSec ? SecSponsors : Sponsors;
	if (!G || !L.IsValidIndex(SpTalk.Sp)) return;
	const FGPMSponsorOpt& Sp = L[SpTalk.Sp];
	const UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	const int32 NR = FMath::Max(1, Season ? Season->NumRounds() : 22);
	const FString Goal = Goals[SpTalk.GoalI];
	const float BonusGp = R1s(Sp.Bonus * BonusMul[SpTalk.BonusL]);
	const float Chance = GoalChance(Goal);
	FGPMDialog Dl;
	Dl.Kind = FGPMDialog::EKind::Nego;
	Dl.Color = FLinearColor(0.31f, 0.82f, 0.48f);
	Dl.Kicker = FString::Printf(TEXT("%s · %s · patience %d"), SpTalk.bSec ? TEXT("SPONSOR SECONDAIRE") : TEXT("SPONSOR PRINCIPAL"), *Sp.Name, SpTalk.Patience);
	Dl.Title = FString::Printf(TEXT("%s · contrat de base : %.1f M€ par saison, %.1f M€ par GP si tu %s"), *Sp.Sector, Sp.Year, Sp.Bonus, *GoalName(Sp.Goal));
	Dl.Text = FString::Printf(TEXT("Ta demande : %.1f M€ fixes par saison, %.1f M€ par Grand Prix si tu %s, sur %d saison%s.\nValeur estimée : environ %.1f M€ par saison (tes chances de remplir l'objectif : %d %%).%s"),
		SpTalk.Fix, BonusGp, *GoalName(Goal), SpTalk.Years, SpTalk.Years > 1 ? TEXT("s") : TEXT(""), R1s(SpTalk.Fix + NR * BonusGp * Chance), FMath::RoundToInt(Chance * 100.f),
		SpTalk.bSec ? *FString::Printf(TEXT("\nÀ la signature : %.1f M€."), Sp.Sign) : TEXT(""));
	if (!Line.IsEmpty()) Dl.Text += TEXT("\n\n") + Line;
	auto Add = [&Dl](const FString& Txt, int32 Act) { FGPMChoice C; C.Text = Txt; C.Action = Act; Dl.Choices.Add(C); };
	Add(TEXT("Montant fixe −0,5 M€"), 40);
	Add(TEXT("Montant fixe +0,5 M€"), 41);
	Add(FString::Printf(TEXT("Durée : %d saison%s ›"), SpTalk.Years, SpTalk.Years > 1 ? TEXT("s") : TEXT("")), 42);
	Add(FString::Printf(TEXT("Prime par Grand Prix : %.1f M€ ›"), BonusGp), 43);
	Add(FString::Printf(TEXT("Objectif : si tu %s ›"), *GoalName(Goal)), 44);
	Add(TEXT("PROPOSER CE CONTRAT"), 45);
	Add(TEXT("QUITTER LA TABLE"), 46);
	if (Queue.Num()) Queue[0] = Dl; else Queue.Add(Dl);
}

void UGPMCareerSubsystem::SponsorAct(int32 Action)
{
	using namespace GPMSpK;
	UGPMSaveGame* G = Save();
	FGPMDialog* Dl = Current();
	if (!G || !Dl) return;
	if (Action >= 100) { OpenSponsorTalk(Action - 100, true); return; }
	if (Action == 46)
	{
		Dl->Chosen = 0;
		Dl->Result = SpTalk.Sp >= 0 ? TEXT("Tu quittes la table : le contrat actuel continue.") : TEXT("À bientôt.");
		return;
	}
	const TArray<FGPMSponsorOpt>& L = SpTalk.bSec ? SecSponsors : Sponsors;
	if (!L.IsValidIndex(SpTalk.Sp)) return;
	const FGPMSponsorOpt& Sp = L[SpTalk.Sp];
	if (Action == 40) SpTalk.Fix = FMath::Max(0.5f, SpTalk.Fix - 0.5f);
	else if (Action == 41) SpTalk.Fix += 0.5f;
	else if (Action == 42) SpTalk.Years = SpTalk.Years % 3 + 1;
	else if (Action == 43) SpTalk.BonusL = (SpTalk.BonusL + 1) % 5;
	else if (Action == 44) SpTalk.GoalI = (SpTalk.GoalI + 1) % 7;
	if (Action != 45) { SponsorDialog(FString()); return; }

	const UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	const int32 NR = FMath::Max(1, Season ? Season->NumRounds() : 22);
	const FString Goal = Goals[SpTalk.GoalI];
	const float BonusGp = R1s(Sp.Bonus * BonusMul[SpTalk.BonusL]);
	// ce que le sponsor est prêt à payer : son contrat de base, plus si l'écurie a bonne réputation et s'engage longtemps
	const float RepK = FMath::Clamp(1.f + (G->Rep - Sp.Rep) / 200.f, 0.85f, 1.25f);
	const float Base = Sp.Year + NR * Sp.Bonus * GoalChance(Sp.Goal);
	const float Cap = Base * RepK * (1.f + 0.04f * (SpTalk.Years - 1)) * (1.f - (NegoLevel - 1) * 0.05f) * (1.f + (FMath::FRand() - 0.5f) * 0.04f);
	const float Bonuses = NR * BonusGp * GoalChance(Goal);
	const float Cost = SpTalk.Fix + Bonuses;
	if (Cost <= Cap)
	{
		const FString Deal = FString::Printf(TEXT("%s|%.2f|%.2f|%s|%d"), *Sp.Id, SpTalk.Fix, BonusGp, *Goal, G->Year + SpTalk.Years - 1);
		G->SpTalkAt = G->Round;
		if (SpTalk.bSec) { G->SecDeals.Add(Deal); G->Budget = R1s(G->Budget + Sp.Sign); }
		else G->MainDeal = Deal;
		Dl->Chosen = 0;
		Dl->Result = FString::Printf(TEXT("Marché conclu avec %s : %.1f M€ par saison, %.1f M€ par Grand Prix si tu %s, jusqu'en %d.%s"), *Sp.Name, SpTalk.Fix, BonusGp, *GoalName(Goal),
			G->Year + SpTalk.Years - 1, SpTalk.bSec ? *FString::Printf(TEXT(" Prime de signature : %.1f M€."), Sp.Sign) : TEXT(""));
		News(FString::Printf(TEXT("Nouveau contrat avec %s : %.1f M€ par saison."), *Sp.Name, SpTalk.Fix));
		if (UGPMSeasonSubsystem* Ss = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>()) Ss->Save();
		return;
	}
	FString Line;
	if (Cost <= Cap * 1.12f)
	{
		SpTalk.Patience -= 1;
		Line = FString::Printf(TEXT("« C'est un peu trop pour nous. Avec ces primes, nous pouvons aller jusqu'à %.1f M€ fixes. »"), FMath::Max(0.f, R1s(Cap - Bonuses - 0.05f)));
	}
	else
	{
		SpTalk.Patience -= 2;
		Line = FString::Printf(TEXT("« Vous plaisantez ? Votre demande vaut %.1f M€ par saison, nous pensions plutôt à %.1f. »"), R1s(Cost), R1s(Cap));
	}
	if (SpTalk.Patience <= 0)
	{
		G->SpTalkAt = G->Round;
		Dl->Chosen = 0;
		Dl->Result = FString::Printf(TEXT("%s met fin à la discussion jusqu'au prochain Grand Prix. Le contrat actuel continue."), *Sp.Name);
		if (UGPMSeasonSubsystem* Ss = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>()) Ss->Save();
		return;
	}
	SponsorDialog(Line);
}
