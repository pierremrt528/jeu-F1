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
	int32 Pos = 10, Grid = 10, Stops = 1, MatePos = 0, RivalPos = 99;   // RivalPos : place du rival de la mission du sponsor
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
struct FGPMPick { int32 Name = 0, Pilot = 0, Tier = 1, Col1 = 0, Col2 = 6, Engine = 1, Mate = 2, Sponsor = 0, Join = -1; };   // Join : écurie existante rejointe (−1 : création)

/** Négociation avec l'agent d'un pilote (nego.js). */
struct FGPMNego
{
	FGPMMarketDriver D;
	FString Pers, Role = TEXT("eq");
	int32 Interest = 50, Patience = 3, Years = 2;
	float Ask = 1.f, Demand = 1.f, Fee = 0.f;
	float Pod = 0.f, Win = 0.f;   // primes de l'offre en cours (par podium, par victoire)
	float Clause = 0.f;           // clause libératoire de l'offre en cours (0 = aucune)
	bool bWantN1 = false, bOpen = false;
	bool bExtend = false;         // prolongation du contrat de l'équipier actuel
	bool bSwap = false;           // l'écurie du pilote récupère l'équipier actuel (sinon elle promeut un autre pilote)
	// offre composée à la carte (façon FIFA) : salaire, durée, statut, prime à la signature, primes de résultats,
	// prime d'objectif au championnat, clause libératoire ; l'agent répond terme par terme
	bool bBuilder = false;
	float OSal = 1.f;
	int32 OYears = 2, OSignL = 0, OBonusL = 0, OObj = 0, OClauseL = 0;
	FString ORole = TEXT("eq");
	float SignB = 0.f, ObjPrize = 0.f;   // offre acceptée : prime à la signature payée tout de suite, prime d'objectif en fin de saison
	int32 Obj = 0;                       // 1 top 10, 2 top 6, 3 top 3 au championnat des pilotes
	FString OLine;                       // dernière réponse de l'agent
};

/** Offre d'une écurie rivale pour l'équipier. */
struct FGPMRivalOffer { FString Team; float Fee = 0.f; };

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
	/** Options de carrière (réglages) : difficulté des négociations 0 facile, 1 normale, 2 dure ; durée de la carrière en saisons (0 = illimitée). */
	static int32 NegoLevel, CareerLen;

	bool IsCreated() const;
	/** Identité, motoriste, équipier et transferts appliqués aux écuries de data.json. */
	void ApplyIdentity();
	UGPMSaveGame* Save() const;

	// --- Création de l'écurie (6 écrans) ---
	TArray<FGPMTierOpt> Tiers;
	TArray<FGPMEngineOpt> Engines;
	TArray<FGPMColorOpt> Colors;
	TArray<FGPMSponsorOpt> Sponsors;
	TArray<FGPMSponsorOpt> SecSponsors;   // sponsors secondaires (3 au plus)
	/** Négociation des sponsors à la carte (GPMSponsorTalk.cpp) : montant fixe, durée, prime par GP, objectif de course. */
	struct FSpTalk { bool bSec = false; int32 Sp = -1; float Fix = 0.f; int32 Years = 2, BonusL = 2, GoalI = 0, Patience = 3; };
	FSpTalk SpTalk;
	struct FSpDeal { FString Id; float Fix = 0.f, Bonus = 0.f; FString Goal; int32 Until = 0; };
	static FSpDeal ParseDeal(const FString& S);
	/** Contrat en vigueur du sponsor principal (conditions de base s'il n'a pas été renégocié). */
	FSpDeal MainDeal() const;
	TArray<FSpDeal> SecDeals() const;
	bool CanTalkSponsor() const;
	void OpenSponsorTalk(int32 Sp, bool bSec);
	void OpenSecondary();
	static FString GoalName(const FString& Goal);
	float GoalChance(const FString& Goal) const;
	/** Académie des jeunes pilotes (GPMAcademy.cpp) : contrats juniors de 3 saisons, progression après chaque GP, pilote de réserve. */
	int32 AcadSlots() const;
	FGPMMarketDriver StatsOf(const FString& Name) const;
	float JuniorSal(const FString& Name) const;
	float PromoteCost() const;
	void EnsureScouts();
	bool SignJunior(const FString& Name);
	void ReleaseJunior(const FString& Name);
	void SetReserve(const FString& Name);
	bool PromoteJunior(const FString& Name);
	void TickAcademy();
	void AcademyYear();
	TArray<FGPMMarketDriver> Market;
	TArray<FString> TeamNames, PilotNames;
	FGPMPick Pick;
	float PickBudget() const;
	/** Les 10 écuries du monde du jeu telles qu'à l'origine (avant tout choix de carrière). */
	const TArray<FGPMTeam>& Originals() const { return OrigTeams; }
	/** Rejoindre l'écurie d'indice Idx : niveau, motoriste, sponsor et équipier qui vont avec sa voiture ; budget de départ. */
	void JoinPlan(int32 Idx, int32& OutTier, int32& OutEngine, int32& OutSponsor, FGPMMarketDriver& OutMate) const;
	float JoinBudget(int32 Idx) const;
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
	/** Ambitions de la direction (0 petite écurie, 1 milieu de grille, 2 écurie de pointe) : le niveau de départ, puis le classement de la saison précédente. */
	int32 Ambition() const;
	/** Place que la direction attend à chaque course selon ses ambitions (14, 9 ou 4). */
	int32 Expected() const { const int32 A = Ambition(); return A == 0 ? 14 : A == 2 ? 4 : 9; }
	static FString ConfText(int32 C) { return C >= 75 ? TEXT("excellente") : C >= 55 ? TEXT("bonne") : C >= 35 ? TEXT("fragile") : C >= 20 ? TEXT("en crise") : TEXT("rompue"); }
	bool IsFired() const;
	/** Après un licenciement : un repreneur rachète l'écurie et garde le joueur (budget divisé par deux, ambitions au plus bas). */
	void SecondChance();
	/** Mission du sponsor pour le prochain GP (makeMission d'events.js), proposée une fois par manche. */
	void EnsureMission();
	void AcceptMission();
	/** Bonus de performance pour la prochaine course (réponses à la presse, événements), puis remis à zéro. */
	float Boost(const TCHAR* Key) const;
	void ClearBoost();
	/** Pré-saison : jours gagnés (négatif : perdus) sur les recherches en cours ; événements de l'intersaison (bLaunch : la présentation de la voiture). */
	void ShiftDev(float Days);
	void PreEvents(bool bLaunch);

	// --- Boîte mail (mail.js) ---
	/** Ajoute un message en tête de la boîte (150 au plus). InTag : direction, presse, mercato, evenement, reglement, sponsor, usine, equipe. */
	void SendMail(const FString& InTag, const FString& InFrom, const FString& InSubj, const FString& InBody);
	int32 Unread() const;
	/** Après chaque GP : bilan de la direction et des sponsors, nouvelles de l'usine et du mercato. */
	void RaceMails(const FGPMRaceCtx& R, const TArray<FString>& Lines);

	// --- Fenêtres à choix ---
	TArray<FGPMDialog> Queue;
	FGPMDialog* Current() { return Queue.Num() ? &Queue[0] : nullptr; }
	void Choose(int32 I);
	void Next();

	// --- Mercato ---
	TArray<FGPMMarketDriver> MarketList() const;
	FString Status(const FGPMMarketDriver& D) const;
	void OpenNego(const FGPMMarketDriver& D, bool bExtend = false);
	/** Test : une écurie rivale fait tout de suite une offre pour l'équipier (GPM.TestMercato). */
	void DebugRivalOffer();
	/** Négociation de la prolongation du contrat de l'équipier. */
	void OpenExtend();
	bool CanExtend() const;
	/** Dernière saison du contrat d'un pilote d'une écurie rivale. */
	int32 UntilOf(const FString& Name) const;
	/** Indemnité de transfert que demanderait l'écurie d'un pilote sous contrat. */
	float FeeOf(const FGPMMarketDriver& D) const;
	FGPMNego Nego;
	FGPMRivalOffer Offer;
	/** Mis à vrai par une signature réussie (le jeu joue la scène de la signature puis le remet à faux). */
	bool bJustSigned = false;
	void AIMercato(int32 Moves);

private:
	TArray<FGPMTeam> BaseTeams;
	TArray<FGPMTeam> OrigTeams;
	TSharedPtr<FJsonObject> Life;
	TArray<FString> Outlets;
	TArray<FLinearColor> OutletColors;
	TMap<FString, FString> Ctx;          // contexte des textes et des conditions
	TSharedPtr<FJsonObject> StoryVars;
	TArray<FString> PressLog;            // questions et réponses de la conférence de presse en cours (un seul message à la fin)

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
	void OfferDialog(const FString& Line);
	void BuildAct(int32 Action);
	float OfferValue(float& OutCost) const;   // valeur de l'offre pour le pilote, rapportée à ce qu'il attend (1 = juste)
	float ObjChance(int32 Lvl) const;
public:
	static float ObjPrizeOf(int32 Lvl, float Sal) { return Lvl <= 0 ? 0.f : FMath::RoundToFloat(Sal * (Lvl == 1 ? 0.3f : Lvl == 2 ? 0.5f : 0.8f) * 10.f) / 10.f; }
private:
	void NegoAct(int32 Action);
	void Sign();
	// mercato : offres des rivaux pour l'équipier, pilote de réserve, carrières (âge, progression, retraites)
	void RivalOfferCheck(const FGPMRaceCtx& R);
	void OfferAct(int32 Action);
	void SponsorAct(int32 Action);
	void SponsorDialog(const FString& Line);
	void MateLeaves(const FString& Team, float Gain);
	FString PromoteReserve(bool bPayNow);
	FString BestFree(float MaxSal) const;
	float SkillOf(const FString& Name) const;
	FString FillSeat(int32 T, int32 Seat, bool bRookie);
	TArray<FString> AgeDrivers();
	TArray<FString> Rookies(int32 N);
	FGPMMarketDriver MateAsDriver() const;
	void MakeOffer(const FString& Team);
	int32 TeamPos() const;
	void WriteSeats();
	void News(const FString& Msg);
};
