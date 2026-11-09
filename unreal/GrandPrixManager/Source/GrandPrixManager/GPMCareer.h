#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Dom/JsonObject.h"
#include "GPMTypes.h"
#include "GPMCareer.generated.h"

class UGPMSaveGame;

/** Un choix d'une fenêtre (presse, flash info, histoire, négociation). */
struct FGPMChoice
{
	FString Text, Out, Next;
	TSharedPtr<FJsonObject> Fx;
	int32 Gap = -1;
	double RiskP = 0.0;
	TSharedPtr<FJsonObject> RiskFx;
	FString RiskOut;
	int32 Action = -1;       // négociation : action de l'offre
	bool bOff = false;       // grisé (budget insuffisant)
};

/** Fenêtre à choix affichée par le HUD : une question de presse, un flash info, un épisode d'histoire ou une négociation. */
struct FGPMDialog
{
	enum class EKind : uint8 { Press, Event, Story, Nego };
	EKind Kind = EKind::Event;
	FString Kicker, Title, Text;
	FLinearColor Color = FLinearColor(0.9f, 0.24f, 0.18f);
	TArray<FGPMChoice> Choices;
	int32 Chosen = -1;
	FString Result, FxText, StoryId, Step;
	bool bBad = false;
	float Jit = 1.f;          // ampleur des effets (±25 %, jitter d'events.js)
};

/** Ce qu'il s'est passé pendant la course, pour la presse et les événements (ctx de press.js / eventCtx). */
struct FGPMRaceCtx
{
	int32 Pos = 10, Grid = 10, Stops = 1, MatePos = 0;
	bool bDnf = false, bFastest = false, bWet = false, bSC = false, bFlat = false;
	float Pen = 0.f;
	FString Circuit, Winner, Rival;
};

/** Pilote du marché des transferts. */
struct FGPMMarketDriver
{
	FString Name, From, Team;
	int32 Age = 25, Seat = -1;   // Seat : place dans l'écurie rivale (0/1), -1 = pilote libre
	float Spd = 75.f, Strat = 70.f, Tyre = 70.f, Pot = 60.f, Sponsor = 0.f;
	float Rating() const { return Spd * 0.6f + Strat * 0.15f + Tyre * 0.15f + Pot * 0.1f; }
};

/** Options de la création de l'écurie (career.js et les fichiers de config). */
struct FGPMTierOpt { FString Id, Name, Desc; float Budget = 70.f, Perf = 0.96f; int32 Rep = 50, Moral = 60; };
struct FGPMEngineOpt { FString Id, Name; int32 Perf = 85, Fiab = 80, Conso = 100; float Price = 20.f; };
struct FGPMColorOpt { FString Id, Name; FLinearColor Color; };
struct FGPMSponsorOpt { FString Id, Name, Sector, Goal; float Year = 10.f, Sign = 2.f, Bonus = 0.5f; int32 Rep = 0; };

/** Choix en cours pendant la création de l'écurie (indices dans les listes d'options). */
struct FGPMPick { int32 Name = 0, Pilot = 0, Tier = 1, Col1 = 0, Col2 = 6, Engine = 1, Mate = 2, Sponsor = 0; };

/** Négociation avec l'agent d'un pilote (nego.js). */
struct FGPMNego
{
	FGPMMarketDriver D;
	FString Pers, Role = TEXT("eq");
	int32 Interest = 50, Patience = 3, Years = 2;
	float Ask = 1.f, Demand = 1.f, Fee = 0.f;
	bool bWantN1 = false, bOpen = false;
};

/**
 * Carrière et vie de l'écurie : création de l'écurie (career.js), direction et objectifs, sponsors (sponsors.js),
 * presse (press.js), flash info et histoires à épisodes (events.js, stories.js), mercato et négociations (market.js, nego.js).
 * Tout l'état est dans la sauvegarde de la saison (UGPMSaveGame).
 */
UCLASS()
class GRANDPRIXMANAGER_API UGPMCareerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool IsCreated() const;
	/** Identité, motoriste, équipier et transferts appliqués aux écuries de data.json. */
	void ApplyIdentity();
	UGPMSaveGame* Save() const;

	// --- Création de l'écurie (6 écrans) ---
	TArray<FGPMTierOpt> Tiers;
	TArray<FGPMEngineOpt> Engines;
	TArray<FGPMColorOpt> Colors;
	TArray<FGPMSponsorOpt> Sponsors;
	TArray<FGPMMarketDriver> Market;
	TArray<FString> TeamNames, PilotNames;
	FGPMPick Pick;
	float PickBudget() const;
	bool PickValid(FString& Why) const;
	void CreateCareer();

	static float Salary(const FGPMMarketDriver& D);
	static float MateSkill(float Spd) { return 0.87f + Spd * 0.001f; }
	FString Display(const FString& Driver) const;

	// --- Après chaque course ---
	/** Direction, sponsors, contrats ponctuels ; prépare la presse et le flash info. Renvoie les lignes du bilan. */
	TArray<FString> AfterRace(const FGPMRaceCtx& Ctx);
	/** Bilan de fin de saison (objectifs, bonus ou licenciement). */
	TArray<FString> SeasonReview();
	/** Intersaison : salaires, motoriste, fin des contrats, transferts des IA. */
	void Intersaison();
	/** Objectifs de la direction : texte et atteint ou non. */
	TArray<TPair<FString, bool>> Objectives() const;
	static FString ConfText(int32 C) { return C >= 75 ? TEXT("excellente") : C >= 55 ? TEXT("bonne") : C >= 35 ? TEXT("fragile") : C >= 20 ? TEXT("en crise") : TEXT("rompue"); }
	bool IsFired() const;
	/** Bonus de performance pour la prochaine course (réponses à la presse, événements), puis remis à zéro. */
	float Boost(const TCHAR* Key) const;
	void ClearBoost();

	// --- Fenêtres à choix ---
	TArray<FGPMDialog> Queue;
	FGPMDialog* Current() { return Queue.Num() ? &Queue[0] : nullptr; }
	void Choose(int32 I);
	void Next();

	// --- Mercato ---
	TArray<FGPMMarketDriver> MarketList() const;
	FString Status(const FGPMMarketDriver& D) const;
	void OpenNego(const FGPMMarketDriver& D);
	FGPMNego Nego;
	void AIMercato(int32 Moves);

private:
	TArray<FGPMTeam> BaseTeams;
	TSharedPtr<FJsonObject> Life;
	TArray<FString> Outlets;
	TArray<FLinearColor> OutletColors;
	TMap<FString, FString> Ctx;          // contexte des textes et des conditions
	TSharedPtr<FJsonObject> StoryVars;

	void LoadOptions();
	void BuildCtx(const FGPMRaceCtx& R);
	bool Eval(const FString& Expr) const;
	FString Fill(const FString& Tpl) const;
	void ApplyFx(const TSharedPtr<FJsonObject>& Fx, float Jit);
	FString FxText(const TSharedPtr<FJsonObject>& Fx) const;
	FGPMChoice ReadChoice(const TSharedPtr<FJsonObject>& O) const;
	void BuildPress(const FGPMRaceCtx& R);
	void PickEvents();
	bool StoryDialog(const FString& Id, const FString& Step, int32 Ep);
	void EventDialog(const TSharedPtr<FJsonObject>& E);
	void NegoDialog(const FString& Line);
	void NegoAct(int32 Action);
	void Sign();
	int32 TeamPos() const;
	FGPMMarketDriver StatsOf(const FString& Name) const;
	void WriteSeats();
	void News(const FString& Msg);
};
