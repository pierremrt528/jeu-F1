#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GPMSeason.generated.h"

/** Résultat du joueur sur un Grand Prix (season.results du jeu web). */
USTRUCT()
struct FGPMRaceResult
{
	GENERATED_BODY()
	UPROPERTY() FString Circuit;
	UPROPERTY() int32 Pos = 0;
	UPROPERTY() int32 Pts = 0;
	UPROPERTY() FString Winner;
	UPROPERTY() FString Fastest;
};

/** Sauvegarde de la saison (gpm-season) : calendrier, manche en cours, points des pilotes et des écuries. */
UCLASS()
class GRANDPRIXMANAGER_API UGPMSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() int32 Version = 1;
	UPROPERTY() int32 Year = 2027;
	UPROPERTY() int32 Round = 0;
	UPROPERTY() TArray<FString> Calendar;
	UPROPERTY() TMap<FString, int32> DriverPts;
	UPROPERTY() TMap<FString, int32> TeamPts;
	UPROPERTY() TMap<FString, FString> DriverTeam;
	UPROPERTY() TArray<FGPMRaceResult> Results;
	// Écurie et usine (team.js, garage.js)
	UPROPERTY() float Budget = 70.f;        // M€
	UPROPERTY() int32 Rep = 50;
	UPROPERTY() int32 Moral = 60;
	UPROPERTY() TArray<FString> Owned;      // pièces installées
	UPROPERTY() TMap<FString, FString> DevPart;   // pôle → pièce en cours
	UPROPERTY() TMap<FString, float> DevLeft;   // pôle → jours restants / total
	UPROPERTY() TMap<FString, float> DevTot;
	UPROPERTY() TArray<FString> Delayed;    // pièces déjà retardées une fois
	UPROPERTY() TMap<FString, float> RivalDev;    // progrès des écuries rivales (perf)
	UPROPERTY() TArray<FString> News;       // messages de l'usine depuis le dernier GP

	// Carrière (career.js) : identité de l'écurie, motoriste, équipier et son contrat
	UPROPERTY() bool bCreated = false;
	UPROPERTY() FString TeamName;
	UPROPERTY() FString PlayerName;
	UPROPERTY() FString Tier = TEXT("mid");
	UPROPERTY() FString Engine;
	UPROPERTY() FString Sponsor;
	UPROPERTY() int32 Col1 = 0;
	UPROPERTY() int32 Col2 = 6;
	UPROPERTY() float BasePerf = 0.96f;
	UPROPERTY() float Fuel = 1.f;
	UPROPERTY() FString MateName;
	UPROPERTY() float MateSpd = 80.f;
	UPROPERTY() float MateStrat = 75.f;
	UPROPERTY() float MateTyre = 74.f;
	UPROPERTY() float MatePot = 60.f;
	UPROPERTY() float MateSal = 1.f;
	UPROPERTY() int32 MateYears = 1;
	UPROPERTY() int32 MateSince = 2027;
	UPROPERTY() FString MateRole = TEXT("eq");
	UPROPERTY() TMap<FString, FString> Seats;     // « écurie|0 » → pilote (écuries rivales, après les transferts)
	UPROPERTY() TMap<FString, float> Skill;       // niveau des pilotes rivaux et libres
	UPROPERTY() TArray<FString> Free;             // pilotes libres
	UPROPERTY() TArray<FString> Broken;           // discussions rompues jusqu'au GP suivant
	// Vie de l'écurie (events.js, press.js, stories.js) : direction, bonus de course, sponsors ponctuels
	UPROPERTY() int32 Conf = 60;
	UPROPERTY() TMap<FString, float> Boost;       // effets pour la prochaine course (vtop, acc, mu…)
	UPROPERTY() TArray<FString> DealName;
	UPROPERTY() TArray<float> DealGp;
	UPROPERTY() TArray<int32> DealN;
	UPROPERTY() TArray<FString> Flags;
	UPROPERTY() TArray<FString> SeenEv;
	UPROPERTY() TArray<FString> SeenSt;
	UPROPERTY() TArray<int32> PressSeen;
	UPROPERTY() FString StoryId;
	UPROPERTY() FString StoryStep;
	UPROPERTY() int32 StoryDue = 0;
	UPROPERTY() int32 StoryEp = 0;
	UPROPERTY() int32 StoryVar = 0;
	UPROPERTY() int32 GridPen = 0;
	UPROPERTY() bool bReviewed = false;
	UPROPERTY() bool bFired = false;
	// Installations (infra.js) : niveau de chaque bâtiment, chantier en cours (un seul à la fois)
	UPROPERTY() TMap<FString, int32> InfraLv;
	UPROPERTY() FString BuildId;
	UPROPERTY() int32 BuildLeft = 0;
	UPROPERTY() int32 BuildTot = 0;
	// Chefs de pôle (staff.js) : chef engagé par pôle et son contrat, chefs libres, chefs des écuries rivales
	UPROPERTY() bool bStaffInit = false;
	UPROPERTY() TMap<FString, FString> ChiefOf;      // pôle → id du chef
	UPROPERTY() TMap<FString, float> ChiefSal;       // pôle → salaire par saison (M€)
	UPROPERTY() TMap<FString, int32> ChiefUntil;     // pôle → dernière saison du contrat
	UPROPERTY() TArray<FString> ChiefFree;           // ids des chefs libres (tous pôles)
	UPROPERTY() TArray<FString> ChiefFresh;          // nouveaux venus sur le marché depuis le dernier GP
	UPROPERTY() TMap<FString, FString> RivalChief;   // « écurie|pôle » → id
	UPROPERTY() TArray<FString> ChiefGen;            // chefs générés : « id|pôle|nom|origine|coût|délai|risque »
	UPROPERTY() int32 ChiefSeq = 0;
	// Mission du sponsor pour le prochain Grand Prix (makeMission d'events.js)
	UPROPERTY() int32 MisRound = -1;
	UPROPERTY() FString MisKind;
	UPROPERTY() FString MisText;
	UPROPERTY() FString MisWho;
	UPROPERTY() FString MisSponsor;
	UPROPERTY() int32 MisV = 0;
	UPROPERTY() float MisGain = 0.f;
	UPROPERTY() bool bMisAcc = false;
	// Courbes de performance (season.hist) : indice de chaque écurie au départ puis après chaque GP, PerfN valeurs par relevé
	UPROPERTY() int32 PerfN = 0;
	UPROPERTY() TArray<float> PerfHist;
	// Contrat de l'équipier : primes par podium et par victoire
	UPROPERTY() float MatePod = 0.f;
	UPROPERTY() float MateWin = 0.f;
};

/** Une ligne de classement. */
struct FGPMStanding { FString Name, Team; int32 Pts = 0; FLinearColor Color = FLinearColor::White; bool bPlayer = false; };

/** Arrivée d'un pilote, pour enregistrer une course. */
struct FGPMFinish { FString Name, Team; float BestLap = 0.f; bool bPlayer = false; };

/**
 * Saison / carrière (season.js) : calendrier des 22 GP, points 25-18-15… + 1 pour le meilleur tour (top 10),
 * classements pilotes et écuries, sauvegarde locale. La gestion (budget, garage, presse…) viendra aux étapes 12-14.
 */
UCLASS()
class GRANDPRIXMANAGER_API UGPMSeasonSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Nouvelle saison ; bNextYear : on garde l'écurie, l'usine et les contrats (intersaison). */
	void NewSeason(bool bNextYear = false);
	/** Recommencer la carrière (efface la sauvegarde). */
	void Reset();
	bool IsOver() const { return S && S->Round >= S->Calendar.Num(); }
	int32 Round() const { return S ? S->Round : 0; }
	int32 NumRounds() const { return S ? S->Calendar.Num() : 0; }
	int32 Year() const { return S ? S->Year : 2027; }
	FString CurrentCircuit() const;
	/** Enregistre l'arrivée (dans l'ordre) et passe à la manche suivante ; renvoie les points du joueur. */
	int32 RecordRace(const TArray<FGPMFinish>& Order);
	TArray<FGPMStanding> Drivers() const;
	TArray<FGPMStanding> Teams() const;
	const TArray<FGPMRaceResult>& Results() const;
	void Save();
	UGPMSaveGame* State() const { return S; }

	static constexpr int32 Points[10] = { 25, 18, 15, 12, 10, 8, 6, 4, 2, 1 };

private:
	UPROPERTY() TObjectPtr<UGPMSaveGame> S;
};
