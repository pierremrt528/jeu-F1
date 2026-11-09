#include "GPMSeason.h"
#include "GrandPrixManager.h"
#include "GPMDataSubsystem.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const TCHAR* Slot = TEXT("gpm-season");
}

void UGPMSeasonSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UGPMDataSubsystem>();
	Super::Initialize(Collection);
	S = Cast<UGPMSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (!S || S->Calendar.Num() == 0) NewSeason();
	UE_LOG(LogGPM, Log, TEXT("Saison %d : manche %d / %d"), S->Year, S->Round + 1, S->Calendar.Num());
}

void UGPMSeasonSubsystem::NewSeason(bool bNextYear)
{
	const int32 Y = S && bNextYear ? S->Year + 1 : 2027;
	S = Cast<UGPMSaveGame>(UGameplayStatics::CreateSaveGameObject(UGPMSaveGame::StaticClass()));
	S->Year = Y;
	if (UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>())
	{
		S->Calendar = Data->Calendar.Num() ? Data->Calendar : Data->GetCircuitIds();
		for (const FGPMTeam& T : Data->Teams)
		{
			S->TeamPts.Add(T.Name, 0);
			for (const FGPMDriver& D : T.Drivers) { S->DriverPts.Add(D.Name, 0); S->DriverTeam.Add(D.Name, T.Name); }
		}
	}
	Save();
}

FString UGPMSeasonSubsystem::CurrentCircuit() const
{
	if (!S || S->Calendar.Num() == 0) return TEXT("valmont");
	return S->Calendar[FMath::Clamp(S->Round, 0, S->Calendar.Num() - 1)];
}

int32 UGPMSeasonSubsystem::RecordRace(const TArray<FGPMFinish>& Order)
{
	if (!S || IsOver()) return 0;
	// meilleur tour : 1 point s'il est dans les 10 premiers
	int32 Fast = -1;
	for (int32 k = 0; k < Order.Num(); ++k)
		if (Order[k].BestLap > 0.f && (Fast < 0 || Order[k].BestLap < Order[Fast].BestLap)) Fast = k;
	FGPMRaceResult R;
	R.Circuit = CurrentCircuit();
	R.Winner = Order.Num() ? Order[0].Name : FString();
	R.Fastest = Fast >= 0 ? Order[Fast].Name : FString();
	for (int32 k = 0; k < Order.Num(); ++k)
	{
		const int32 P = (k < 10 ? Points[k] : 0) + (k == Fast && k < 10 ? 1 : 0);
		S->DriverPts.FindOrAdd(Order[k].Name) += P;
		S->TeamPts.FindOrAdd(Order[k].Team) += P;
		S->DriverTeam.Add(Order[k].Name, Order[k].Team);
		if (Order[k].bPlayer) { R.Pos = k + 1; R.Pts = P; }
	}
	S->Results.Add(R);
	++S->Round;
	Save();
	return R.Pts;
}

TArray<FGPMStanding> UGPMSeasonSubsystem::Drivers() const
{
	TArray<FGPMStanding> Out;
	if (!S) return Out;
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	for (const TPair<FString, int32>& P : S->DriverPts)
	{
		FGPMStanding Row; Row.Name = P.Key; Row.Pts = P.Value; Row.Team = S->DriverTeam.FindRef(P.Key);
		if (Data)
			for (const FGPMTeam& T : Data->Teams)
				if (T.Name == Row.Team)
				{
					Row.Color = T.Color;
					for (const FGPMDriver& D : T.Drivers) if (D.Name == Row.Name && D.bPlayer) Row.bPlayer = true;
				}
		if (Data && Data->Teams.Num() && Data->Teams[0].Drivers.Num() && Data->Teams[0].Drivers[0].Name == Row.Name) Row.bPlayer = true;
		Out.Add(Row);
	}
	Out.Sort([](const FGPMStanding& A, const FGPMStanding& B) { return A.Pts > B.Pts; });
	return Out;
}

TArray<FGPMStanding> UGPMSeasonSubsystem::Teams() const
{
	TArray<FGPMStanding> Out;
	if (!S) return Out;
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	for (const TPair<FString, int32>& P : S->TeamPts)
	{
		FGPMStanding Row; Row.Name = P.Key; Row.Pts = P.Value;
		if (Data) for (int32 t = 0; t < Data->Teams.Num(); ++t) if (Data->Teams[t].Name == P.Key) { Row.Color = Data->Teams[t].Color; Row.bPlayer = t == 0; }
		Out.Add(Row);
	}
	Out.Sort([](const FGPMStanding& A, const FGPMStanding& B) { return A.Pts > B.Pts; });
	return Out;
}

const TArray<FGPMRaceResult>& UGPMSeasonSubsystem::Results() const
{
	static const TArray<FGPMRaceResult> None;
	return S ? S->Results : None;
}

void UGPMSeasonSubsystem::Save()
{
	if (S) UGameplayStatics::SaveGameToSlot(S, Slot, 0);
}
