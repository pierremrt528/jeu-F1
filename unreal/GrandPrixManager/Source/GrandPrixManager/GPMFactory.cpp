#include "GPMFactory.h"
#include "GrandPrixManager.h"
#include "GPMDataSubsystem.h"
#include "GPMSeason.h"
#include "Dom/JsonObject.h"

void UGPMFactorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UGPMDataSubsystem>();
	Collection.InitializeDependency<UGPMSeasonSubsystem>();
	Super::Initialize(Collection);
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	const TSharedPtr<FJsonValue> Raw = Data ? Data->GetRaw(TEXT("garage"), TEXT("TREES")) : nullptr;
	if (!Raw.IsValid()) return;
	for (const TSharedPtr<FJsonValue>& TV : Raw->AsArray())
	{
		const TSharedPtr<FJsonObject> T = TV->AsObject();
		FGPMTree Tr;
		Tr.Id = T->GetStringField(TEXT("id"));
		Tr.Name = T->GetStringField(TEXT("name"));
		Tr.Color = FLinearColor(FColor::FromHex(T->GetStringField(TEXT("color"))));
		for (const TSharedPtr<FJsonValue>& NV : T->GetArrayField(TEXT("nodes")))
		{
			const TSharedPtr<FJsonObject> N = NV->AsObject();
			FGPMPart P;
			P.Id = N->GetStringField(TEXT("id"));
			P.Name = N->GetStringField(TEXT("name"));
			N->TryGetStringField(TEXT("desc"), P.Desc);
			P.Tree = Tr.Id;
			P.Cost = N->GetNumberField(TEXT("cost"));
			const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
			if (N->TryGetArrayField(TEXT("at"), A) && A->Num() == 2) { P.Row = int32((*A)[0]->AsNumber()); P.Col = int32((*A)[1]->AsNumber()); }
			if (N->TryGetArrayField(TEXT("req"), A)) for (const TSharedPtr<FJsonValue>& R : *A) P.Req.Add(R->AsString());
			if (N->TryGetArrayField(TEXT("reqAny"), A)) for (const TSharedPtr<FJsonValue>& R : *A) P.ReqAny.Add(R->AsString());
			const TSharedPtr<FJsonObject>* Fx = nullptr;
			if (N->TryGetObjectField(TEXT("fx"), Fx))
				for (const TPair<FString, TSharedPtr<FJsonValue>>& E : (*Fx)->Values) P.Fx.Add(E.Key, float(E.Value->AsNumber()));
			Tr.Parts.Add(P);
		}
		TreeList.Add(Tr);
	}
	UE_LOG(LogGPM, Log, TEXT("Usine : %d pôles"), TreeList.Num());
}

UGPMSaveGame* UGPMFactorySubsystem::Save() const
{
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	return Season ? Season->State() : nullptr;
}

void UGPMFactorySubsystem::Persist() const
{
	if (UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>()) Season->Save();
}

const FGPMPart* UGPMFactorySubsystem::Part(const FString& Id) const
{
	for (const FGPMTree& T : TreeList) for (const FGPMPart& P : T.Parts) if (P.Id == Id) return &P;
	return nullptr;
}

int32 UGPMFactorySubsystem::Running() const
{
	const UGPMSaveGame* G = Save();
	return G ? G->DevPart.Num() : 0;
}

float UGPMFactorySubsystem::Budget() const { const UGPMSaveGame* G = Save(); return G ? G->Budget : 0.f; }

void UGPMFactorySubsystem::AddMoney(float M)
{
	if (UGPMSaveGame* G = Save()) { G->Budget = FMath::RoundToFloat((G->Budget + M) * 10.f) / 10.f; Persist(); }
}

UGPMFactorySubsystem::EState UGPMFactorySubsystem::StateOf(const FGPMPart& P) const
{
	const UGPMSaveGame* G = Save();
	if (!G) return EState::Locked;
	if (G->Owned.Contains(P.Id)) return EState::Owned;
	if (const FString* Cur = G->DevPart.Find(P.Tree)) { if (*Cur == P.Id) return EState::Running; }
	for (const FString& R : P.Req) if (!G->Owned.Contains(R)) return EState::Locked;
	if (P.ReqAny.Num())
	{
		bool bAny = false;
		for (const FString& R : P.ReqAny) bAny |= G->Owned.Contains(R);
		if (!bAny) return EState::Locked;
	}
	if (G->DevPart.Contains(P.Tree) || Running() >= Workshops()) return EState::Busy;
	if (G->Budget < Price(P)) return EState::TooExpensive;
	return EState::Available;
}

bool UGPMFactorySubsystem::Start(const FString& Id)
{
	const FGPMPart* P = Part(Id);
	UGPMSaveGame* G = Save();
	if (!P || !G || StateOf(*P) != EState::Available) return false;
	G->Budget = FMath::RoundToFloat((G->Budget - Price(*P)) * 10.f) / 10.f;
	G->DevPart.Add(P->Tree, P->Id);
	G->DevLeft.Add(P->Tree, float(Delay(*P)));
	G->DevTot.Add(P->Tree, float(Delay(*P)));
	Persist();
	return true;
}

void UGPMFactorySubsystem::Tick(int32 Days)
{
	UGPMSaveGame* G = Save();
	if (!G || Days <= 0) return;
	TArray<FString> Done;
	for (TPair<FString, FString>& D : G->DevPart)
	{
		float& Left = G->DevLeft.FindOrAdd(D.Key);
		Left -= Days;
		if (Left > 0.f) continue;
		const FGPMPart* P = Part(D.Value);
		if (!P) { Done.Add(D.Key); continue; }
		// retard possible une fois (22 %), puis échec possible (argent perdu)
		if (!G->Delayed.Contains(P->Id) && FMath::RandRange(0, 99) < 22)
		{
			G->Delayed.Add(P->Id);
			const int32 More = FMath::RandRange(7, 14);
			Left += More;
			G->News.Add(FString::Printf(TEXT("Retard : %s, %d jours de plus."), *P->Name, More));
			continue;
		}
		if (FMath::RandRange(0, 99) < FailPct(*P)) G->News.Add(FString::Printf(TEXT("Échec : %s a cassé au banc d essai, la pièce est perdue."), *P->Name));
		else { G->Owned.Add(P->Id); G->News.Add(FString::Printf(TEXT("Livrée : %s est montée sur la voiture."), *P->Name)); }
		G->Delayed.Remove(P->Id);
		Done.Add(D.Key);
	}
	for (const FString& K : Done) { G->DevPart.Remove(K); G->DevLeft.Remove(K); G->DevTot.Remove(K); }
	Persist();
}

FGPMUpgrades UGPMFactorySubsystem::Upgrades() const
{
	FGPMUpgrades U;
	const UGPMSaveGame* G = Save();
	if (!G) return U;
	for (const FString& Id : G->Owned)
		if (const FGPMPart* P = Part(Id))
			for (const TPair<FString, float>& F : P->Fx)
			{
				if (F.Key == TEXT("vtop")) U.VTop += F.Value;
				else if (F.Key == TEXT("acc")) U.Acc += F.Value;
				else if (F.Key == TEXT("mu")) U.Mu += F.Value;
				else if (F.Key == TEXT("brake")) U.Brake += F.Value;
				else if (F.Key == TEXT("kd")) U.Kd += F.Value;
				else if (F.Key == TEXT("steer")) U.Steer += F.Value;
				else if (F.Key == TEXT("fuel")) U.Fuel += F.Value;
			}
	return U;
}
