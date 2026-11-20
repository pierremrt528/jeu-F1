#include "GPMSeason.h"
#include "GrandPrixManager.h"
#include "GPMDataSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"

// Sauvegarde : gpm-season.sav. Copies de secours à côté, dans Saved/SaveGames :
//  - gpm-season-secours.sav : la partie telle qu'elle était au dernier lancement du jeu (rechargée seule si le fichier est abîmé) ;
//  - gpm-season-precedent.sav : l'état juste avant la dernière écriture ;
//  - gpm-season-ancienne-carriere.sav : la carrière effacée par « Recommencer la carrière ».
// Les tests automatiques (jeu lancé avec une commande GPM.Test…) travaillent sur une copie, gpm-test.sav : la vraie partie n'est jamais touchée.
namespace
{
	const TCHAR* RealSlot = TEXT("gpm-season");
	FString SaveSlot = RealSlot;
	constexpr int32 SaveVersion = 2;
	FString SlotFile(const FString& Slot) { return FPaths::ProjectSavedDir() / TEXT("SaveGames") / Slot + TEXT(".sav"); }
	bool CopySlot(const FString& From, const FString& To)
	{
		IFileManager& FM = IFileManager::Get();
		return FM.FileExists(*SlotFile(From)) && FM.Copy(*SlotFile(To), *SlotFile(From), true, true) == COPY_OK;
	}
}

void UGPMSeasonSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UGPMDataSubsystem>();
	Super::Initialize(Collection);
	// tests automatiques : une copie de la partie, pour ne jamais abîmer la vraie
	if (FString(FCommandLine::Get()).Contains(TEXT("GPM.Test")))
	{
		SaveSlot = TEXT("gpm-test");
		IFileManager::Get().Delete(*SlotFile(SaveSlot), false, true, true);
		CopySlot(RealSlot, SaveSlot);
		UE_LOG(LogGPM, Log, TEXT("Sauvegarde : test automatique, copie de travail %s"), *SlotFile(SaveSlot));
	}
	const bool bHadFile = IFileManager::Get().FileExists(*SlotFile(SaveSlot));
	S = Cast<UGPMSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
	if (bHadFile && (!S || S->Calendar.Num() == 0))
	{
		// fichier abîmé (coupure pendant l'écriture, disque plein) : on le met de côté et on reprend la copie de secours
		UE_LOG(LogGPM, Warning, TEXT("Sauvegarde illisible : essai de la copie de secours"));
		CopySlot(SaveSlot, SaveSlot + TEXT("-abimee"));
		for (const TCHAR* Suffix : { TEXT("-precedent"), TEXT("-secours") })
		{
			S = Cast<UGPMSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot + Suffix, 0));
			if (S && S->Calendar.Num() > 0)
			{
				UE_LOG(LogGPM, Warning, TEXT("Sauvegarde reprise depuis %s%s.sav"), *SaveSlot, Suffix);
				UGameplayStatics::SaveGameToSlot(S, SaveSlot, 0);   // le fichier principal est réécrit tout de suite
				break;
			}
			S = nullptr;
		}
	}
	else if (S && S->Calendar.Num() > 0) CopySlot(SaveSlot, SaveSlot + TEXT("-secours"));   // la partie est saine : copie de secours du lancement
	if (!S || S->Calendar.Num() == 0) NewSeason();
	if (S->Version > SaveVersion) UE_LOG(LogGPM, Warning, TEXT("Sauvegarde écrite par une version plus récente du jeu (%d > %d)"), S->Version, SaveVersion);
	UE_LOG(LogGPM, Log, TEXT("Saison %d : manche %d / %d"), S->Year, S->Round + 1, S->Calendar.Num());
}

void UGPMSeasonSubsystem::NewSeason(bool bNextYear)
{
	const int32 Y = S && bNextYear ? S->Year + 1 : 2027;
	if (S && bNextYear)
	{
		// intersaison : l'écurie continue, la saison repart de zéro
		S = DuplicateObject<UGPMSaveGame>(S, this);
		S->Round = 0; S->Results.Reset(); S->DriverPts.Reset(); S->TeamPts.Reset(); S->DriverTeam.Reset(); S->News.Reset();
		S->bReviewed = false; S->SeenEv.Reset(); S->StoryId.Reset(); S->GridPen = 0;
		S->PerfHist.Reset(); S->PerfN = 0; S->MisRound = -1; S->bMisAcc = false;
		S->FinStart = S->Budget; S->FinPrize = S->FinPartner = 0.f;   // bilan financier : nouvelle saison
		S->PreStage = 0; S->TestDay = 0; S->TestDays = 0.f; S->TestRes.Reset(); S->TestReport.Reset();   // la saison commence en janvier par la pré-saison
	}
	else S = Cast<UGPMSaveGame>(UGameplayStatics::CreateSaveGameObject(UGPMSaveGame::StaticClass()));
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

void UGPMSeasonSubsystem::Reset()
{
	// la carrière effacée reste récupérable à la main (gpm-season-ancienne-carriere.sav)
	if (S && S->bCreated) CopySlot(SaveSlot, SaveSlot + TEXT("-ancienne-carriere"));
	UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
	S = nullptr;
	NewSeason();
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
		if (!Order[k].bOut && Order[k].BestLap > 0.f && (Fast < 0 || Order[k].BestLap < Order[Fast].BestLap)) Fast = k;
	FGPMRaceResult R;
	R.Circuit = CurrentCircuit();
	R.Winner = Order.Num() ? Order[0].Name : FString();
	R.Fastest = Fast >= 0 ? Order[Fast].Name : FString();
	for (int32 k = 0; k < Order.Num(); ++k)
	{
		const int32 P = Order[k].bOut ? 0 : (k < 10 ? Points[k] : 0) + (k == Fast && k < 10 ? 1 : 0);   // pas de points après un abandon
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
		if (Data) for (int32 t = 0; t < Data->Teams.Num(); ++t) if (Data->Teams[t].Name == P.Key) { Row.Color = Data->Teams[t].Color; Row.bPlayer = Row.bPlayer || t == 0; }
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
	if (!S) return;
	S->Version = SaveVersion;
	CopySlot(SaveSlot, SaveSlot + TEXT("-precedent"));   // l'état d'avant, au cas où l'écriture serait coupée
	if (!UGameplayStatics::SaveGameToSlot(S, SaveSlot, 0))
		UE_LOG(LogGPM, Error, TEXT("Sauvegarde impossible (disque plein ?) : %s"), *SlotFile(SaveSlot));
}
