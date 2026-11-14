#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GPMFactory.generated.h"

/** Une pièce d'un arbre de développement (TREES de garage.js). */
struct FGPMPart
{
	FString Id, Name, Desc, Tree;
	float Cost = 20.f;
	int32 Row = 1, Col = 2;                 // position dans l'arbre (rangée, colonne sur 3)
	TArray<FString> Req, ReqAny;            // toutes / au moins une
	TMap<FString, float> Fx;                // vtop, acc, mu, brake, kd, steer, fuel, rel…
};

struct FGPMTree { FString Id, Name; FLinearColor Color; TArray<FGPMPart> Parts; };

/** Multiplicateurs de la voiture du joueur (carUpgrades de garage.js) ; Pit en secondes gagnées à l'arrêt. */
struct FGPMUpgrades { float VTop = 1, Acc = 1, Mu = 1, Brake = 1, Kd = 1, Steer = 1, Fuel = 1, Tyre = 1, Pit = 0; };

/** Un bâtiment de l'écurie (INFRA d'infra.js), 3 niveaux. */
struct FGPMBuilding { FString Id, Name, Desc, Tag; TArray<FString> Lv; };

/** Un chef de pôle (staff.js) : réductions en % du prix, du délai et du risque des pièces de son pôle. */
struct FGPMChief
{
	FString Id, Dept, Name, From;
	int32 Cost = 0, Time = 0, Risk = 0;
	float Fee = 1.f;                        // salaire demandé par saison (M€)
	int32 Quality() const { return Cost + Time + Risk; }
	bool IsValid() const { return !Id.IsEmpty(); }
};

/** Un chef candidat : libre, ou en poste dans une écurie rivale (Club). */
struct FGPMChiefOffer { FGPMChief C; FString Club; bool bFresh = false; };

/**
 * QG de l'écurie : usine (garage.js : 5 pôles, pièces payées en M€ qui demandent 20 à 56 jours avec un risque de retard
 * puis d'échec), installations (infra.js : 7 bâtiments à 3 niveaux, un chantier à la fois) et chefs de pôle (staff.js :
 * contrats, marché des chefs libres, chefs des écuries rivales). L'état est gardé dans la sauvegarde de la saison.
 */
UCLASS()
class GRANDPRIXMANAGER_API UGPMFactorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// --- Recherche ---
	const TArray<FGPMTree>& Trees() const { return TreeList; }
	const FGPMPart* Part(const FString& Id) const;
	/** Valeurs de base d'une pièce (sans bâtiments ni chef). */
	static float Price(const FGPMPart& P) { return FMath::RoundToFloat(P.Cost / 5.f * 10.f) / 10.f; }
	static int32 Delay(const FGPMPart& P) { return P.Cost <= 20 ? 20 : P.Cost <= 55 ? 36 : 56; }
	static int32 FailPct(const FGPMPart& P) { return P.Cost <= 20 ? 6 : P.Cost <= 55 ? 10 : 16; }
	/** Valeurs réelles : Centre de R&D, Soufflerie et chef du pôle. */
	float PriceOf(const FGPMPart& P) const;
	int32 DelayOf(const FGPMPart& P) const;
	int32 FailOf(const FGPMPart& P) const;
	int32 LateOf(const FGPMPart& P) const;
	/** Recherches simultanées : 2/3/4 selon le niveau de départ, + les Ateliers. */
	int32 Workshops() const;
	/** Pièces en même temps dans un pôle (2 avec la Soufflerie niveau 3). */
	int32 PerDept() const;
	int32 Running() const;
	int32 RunningIn(const FString& Tree) const;
	float DaysLeft(const FGPMPart& P) const;
	float DaysTotal(const FGPMPart& P) const;

	enum class EState : uint8 { Owned, Running, Available, TooExpensive, Busy, Locked };
	EState StateOf(const FGPMPart& P) const;
	/** Lance la recherche (paie le prix). */
	bool Start(const FString& Id);
	/** Fait avancer les recherches de N jours ; les messages vont dans les actualités de la saison. */
	void Tick(int32 Days);
	FGPMUpgrades Upgrades() const;
	float Budget() const;
	void AddMoney(float M);

	// --- Installations ---
	const TArray<FGPMBuilding>& Buildings() const { return BuildingList; }
	int32 Level(const FString& Id) const;
	static float BuildCost(const FString& Id, int32 Lv);
	static int32 BuildDays(int32 Lv);
	/** Peut-on lancer le niveau suivant ? Sinon, la raison. */
	bool CanBuild(const FString& Id, FString& Why) const;
	bool Build(const FString& Id);
	/** Le chantier avance de N jours ; renvoie le message de fin de chantier (ou vide). */
	FString TickInfra(int32 Days);
	/** Effets des bâtiments. */
	float PartnerIncome() const;    // M€ par GP (Espace partenaires + réputation)
	float PrizeMul() const;         // primes de course (Centre médias)
	float MateGrowth() const;       // progression de l'équipier (Académie)
	float SalaryCut() const;        // salaires des pilotes (Académie)
	float PitBonus() const;         // secondes gagnées à l'arrêt (Centre des mécaniciens), négatif

	// --- Chefs de pôle ---
	/** Pôles dans l'ordre des arbres : id et nom. */
	static const TArray<TPair<FString, FString>>& Depts();
	static FString DeptName(const FString& Dept);
	FGPMChief ChiefById(const FString& Id) const;
	FGPMChief ChiefOf(const FString& Dept) const;
	/** Candidats d'un pôle : chefs libres puis chefs des écuries rivales (meilleurs d'abord). */
	TArray<FGPMChiefOffer> Candidates(const FString& Dept) const;
	/** 1 - réduction du chef du pôle : Which 0 prix, 1 délai, 2 risque. */
	float ChiefMul(const FString& Dept, int32 Which) const;
	/** Rythme de développement d'une écurie rivale selon ses chefs (0,7 … 1,2). */
	float RivalDevMul(const FString& Team) const;
	/** Salaire par saison demandé pour un contrat de N saisons. */
	static float ChiefAsk(const FGPMChief& C, int32 Years);
	/** Indemnité à verser à l'écurie rivale qui emploie ce chef. */
	static float ChiefFee(const FGPMChief& C) { return FMath::RoundToFloat(C.Fee * 1.6f * 10.f) / 10.f; }
	/** Indemnité de départ du chef en poste (moitié des saisons restantes après celle-ci). */
	float ReleaseCost(const FString& Dept) const;
	/** Coût immédiat d'une signature : première saison de salaire + indemnités. */
	float HireCost(const FGPMChiefOffer& O, int32 Years) const;
	bool Hire(const FGPMChiefOffer& O, int32 Years);
	/** Prolonge le contrat du chef d'une saison, au même salaire (3 saisons d'avance au plus). */
	bool Extend(const FString& Dept);
	/** Marché des chefs créé à la première visite (chefs libres, chefs des rivaux). */
	void EnsureStaff();
	/** Après chaque GP : des chefs libres partent ou sont engagés par un rival, d'autres arrivent. */
	void RotateStaff();
	/** Intersaison : salaires des chefs pour la nouvelle saison, fins de contrat. Renvoie le total payé. */
	float RenewStaff(int32 NewYear);

	// --- Courbes de performance ---
	/** Indice de performance de la voiture du joueur (perfFactor de garage.js). */
	float PlayerPerf() const;
	/** Relevé de l'indice de chaque écurie (au départ de la saison, puis après chaque GP). */
	void RecordPerf();
	void EnsurePerfStart();

private:
	TArray<FGPMTree> TreeList;
	TArray<FGPMBuilding> BuildingList;
	TArray<FGPMChief> ChiefList;
	class UGPMSaveGame* Save() const;
	void Persist() const;
	void LoadQG();
	FGPMChief NewChief(const FString& Dept, int32 Q, const FString& From);
	FString DevKey(const FGPMPart& P) const;
};
