// Académie des jeunes pilotes : on repère des jeunes de 16 à 18 ans (karting, formules régionales), on les signe en contrat junior
// (3 saisons, petit salaire), ils progressent après chaque Grand Prix (plus vite avec le bâtiment Académie), l'un d'eux peut être
// pilote de réserve (il remplace l'équipier qui part) ou être titularisé tout de suite (l'équipier actuel est libéré).
#include "GPMCareer.h"
#include "GrandPrixManager.h"
#include "GPMSeason.h"
#include "GPMFactory.h"

namespace GPMAcadK
{
	float R1a(float X) { return FMath::RoundToFloat(X * 10.f) / 10.f; }
}

int32 UGPMCareerSubsystem::AcadSlots() const
{
	const UGPMFactorySubsystem* F = GetGameInstance()->GetSubsystem<UGPMFactorySubsystem>();
	return 2 + (F ? F->Level(TEXT("acad")) : 0);
}

float UGPMCareerSubsystem::JuniorSal(const FString& Name) const
{
	return FMath::Max(0.1f, GPMAcadK::R1a(0.1f + (StatsOf(Name).Pot - 62.f) / 100.f));
}

float UGPMCareerSubsystem::PromoteCost() const
{
	const UGPMSaveGame* G = Save();
	return G ? GPMAcadK::R1a(0.5f * G->MateSal * FMath::Max(0, G->MateYears)) : 0.f;
}

void UGPMCareerSubsystem::EnsureScouts()
{
	UGPMSaveGame* G = Save();
	if (!G || !G->bCreated || G->ScoutList.Num() >= 3) return;
	for (const FString& Name : Rookies(3 - G->ScoutList.Num()))
	{
		// plus jeunes et moins rapides qu'en Formule B, mais avec du potentiel
		G->Free.Remove(Name);
		G->DrvBorn.Add(Name, G->Year - FMath::RandRange(16, 18));
		G->Skill.Add(Name, MateSkill(FMath::FRandRange(60.f, 72.f)));
		G->DrvPot.Add(Name, FMath::Clamp(G->DrvPot.FindRef(Name) + FMath::FRandRange(-2.f, 4.f), 64.f, 97.f));
		const int32 Rank = FMath::RandRange(1, 6);
		G->DrvFrom.Add(Name, FMath::RandBool() ? FString::Printf(TEXT("Karting · %de du championnat"), Rank) : FString::Printf(TEXT("Formule régionale · %de"), Rank));
		G->ScoutList.Add(Name);
	}
}

bool UGPMCareerSubsystem::SignJunior(const FString& Name)
{
	UGPMSaveGame* G = Save();
	if (!G || !G->ScoutList.Contains(Name) || G->Academy.Num() >= AcadSlots()) return false;
	const float Sal = JuniorSal(Name);
	if (Sal > G->Budget) return false;
	G->Budget = GPMAcadK::R1a(G->Budget - Sal);
	G->ScoutList.Remove(Name);
	G->Academy.Add(Name);
	G->AcadUntil.Add(Name, G->Year + 2);
	if (G->Reserve.IsEmpty()) G->Reserve = Name;
	News(FString::Printf(TEXT("%s rejoint l'académie (contrat junior jusqu'en %d, %.1f M€ par saison)."), *Name, G->Year + 2, Sal));
	if (UGPMSeasonSubsystem* Ss = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>()) Ss->Save();
	return true;
}

void UGPMCareerSubsystem::ReleaseJunior(const FString& Name)
{
	UGPMSaveGame* G = Save();
	if (!G || !G->Academy.Contains(Name)) return;
	G->Academy.Remove(Name); G->AcadUntil.Remove(Name);
	if (G->Reserve == Name) G->Reserve.Reset();
	G->Free.AddUnique(Name);   // il tente sa chance ailleurs
	if (UGPMSeasonSubsystem* Ss = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>()) Ss->Save();
}

void UGPMCareerSubsystem::SetReserve(const FString& Name)
{
	UGPMSaveGame* G = Save();
	if (!G || !G->Academy.Contains(Name)) return;
	G->Reserve = Name;
	if (UGPMSeasonSubsystem* Ss = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>()) Ss->Save();
}

bool UGPMCareerSubsystem::PromoteJunior(const FString& Name)
{
	UGPMSaveGame* G = Save();
	if (!G || !G->Academy.Contains(Name)) return false;
	const float Cost = PromoteCost();
	if (Cost > G->Budget) return false;
	// l'équipier actuel est libéré (la moitié des saisons restantes), le junior prend son volant pour deux saisons
	const FString Old = G->MateName;
	G->Budget = GPMAcadK::R1a(G->Budget - Cost);
	G->Skill.Add(Old, MateSkill(G->MateSpd)); G->DrvPot.Add(Old, G->MatePot);
	G->Free.AddUnique(Old);
	G->Reserve = Name;
	const FString In = PromoteReserve(true);
	G->MateYears = 2;
	G->MateSal = FMath::Max(0.3f, GPMAcadK::R1a(G->MateSal * 0.7f));   // il doit tout à l'académie : salaire modeste
	News(FString::Printf(TEXT("%s, formé à l'académie, devient titulaire à la place de %s."), *In, *Old));
	SendMail(TEXT("equipe"), TEXT("Académie"), FString::Printf(TEXT("%s titularisé"), *In),
		FString::Printf(TEXT("%s quitte l'écurie (indemnité %.1f M€). %s, notre junior, prend le volant pour deux saisons à %.1f M€ par saison."), *Old, Cost, *In, G->MateSal));
	ApplyIdentity();
	if (UGPMSeasonSubsystem* Ss = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>()) Ss->Save();
	return true;
}

void UGPMCareerSubsystem::TickAcademy()
{
	UGPMSaveGame* G = Save();
	if (!G || !G->bCreated) return;
	const UGPMFactorySubsystem* F = GetGameInstance()->GetSubsystem<UGPMFactorySubsystem>();
	const float Grow = F ? F->MateGrowth() : 1.f;
	for (const FString& Name : G->Academy)
	{
		// progression vers le potentiel, plus rapide avec le bâtiment Académie ; un peu de hasard d'une course à l'autre
		float& Sk = G->Skill.FindOrAdd(Name, MateSkill(65.f));
		const float Target = MateSkill(G->DrvPot.FindRef(Name));
		Sk = FMath::Min(Target, Sk + FMath::Max(0.f, Target - Sk) * 0.03f * Grow * FMath::FRandRange(0.5f, 1.5f));
	}
	EnsureScouts();
}

void UGPMCareerSubsystem::AcademyYear()
{
	UGPMSaveGame* G = Save();
	if (!G) return;
	// salaires juniors de la nouvelle saison ; contrats terminés : le jeune part tenter sa chance ailleurs
	float Paid = 0.f;
	for (int32 k = G->Academy.Num() - 1; k >= 0; --k)
	{
		const FString Name = G->Academy[k];
		if (G->AcadUntil.FindRef(Name) <= G->Year)
		{
			G->Academy.RemoveAt(k); G->AcadUntil.Remove(Name);
			if (G->Reserve == Name) G->Reserve.Reset();
			G->Free.AddUnique(Name);
			SendMail(TEXT("equipe"), TEXT("Académie"), FString::Printf(TEXT("%s quitte l'académie"), *Name),
				FString::Printf(TEXT("Son contrat junior est terminé. %s rejoint le marché des pilotes : tu peux encore le recruter au mercato."), *Name));
			continue;
		}
		Paid += JuniorSal(Name);
	}
	G->Budget = GPMAcadK::R1a(G->Budget - Paid);
	// nouvelle détection : de nouveaux jeunes à suivre
	for (const FString& Name : G->ScoutList) G->Free.AddUnique(Name);   // les jeunes non signés tentent leur chance ailleurs
	G->ScoutList.Reset();
	EnsureScouts();
}
