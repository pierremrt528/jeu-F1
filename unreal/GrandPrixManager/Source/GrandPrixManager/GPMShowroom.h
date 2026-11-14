#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GPMShowroom.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTextRenderComponent;
class UPointLightComponent;
class USpotLightComponent;
class AGPMCar;

/** Tenue d'un personnage de l'atelier (combinaison du pilote, costume de l'agent). */
struct FGPMOutfit
{
	FLinearColor Top = FLinearColor::White, Legs = FLinearColor::White, Stripe = FLinearColor::Black;
	FLinearColor Skin = FLinearColor(0.55f, 0.36f, 0.25f), Hair = FLinearColor(0.03f, 0.02f, 0.015f);
	bool bSuit = false;      // costume : chemise blanche, cravate, dossier sous le bras ; sinon casque à la main
};

/**
 * Atelier 3D derrière les écrans de gestion (showroom.js) : la voiture de l'écurie tourne sur un plateau au sol brillant,
 * mur circulaire avec rampes lumineuses et nom de l'écurie, plafond à panneaux de lumière ; deux personnages pour les
 * scènes du rendez-vous (le pilote et son agent arrivent) et de la signature (poignée de main, photo).
 * Construit sous le circuit, fermé de toutes parts, et caché pendant la course.
 */
UCLASS()
class GRANDPRIXMANAGER_API AGPMShowroom : public AActor
{
	GENERATED_BODY()

public:
	AGPMCar* DisplayCar() const { return Display; }
	AGPMShowroom();
	virtual void Tick(float DeltaSeconds) override;
	/** Construit la pièce et la voiture d'exposition (une fois). */
	void Build();
	/** Montre ou cache l'atelier (lumières comprises). */
	void SetShown(bool bShow);
	bool IsShown() const { return bShown; }
	/** Couleurs et nom de l'écurie du joueur : voiture, anneau du plateau, plinthe, halo au sol, nom au mur. */
	void SetTeam(const FLinearColor& Main, const FLinearColor& Accent, const FString& Name);
	/** Centre du plateau (monde, cm). */
	FVector Spot() const { return GetActorLocation(); }
	/** Scènes : 0 aucune, 1 rendez-vous, 2 signature ; T = temps écoulé (s). Place les personnages. */
	void PlayScene(int32 Kind, float T);
	/** Tenues des deux personnages (reconstruits si elles changent). */
	void SetCast(const FGPMOutfit& A, const FGPMOutfit& B);
	/** Tenue type : combinaison aux couleurs données, ou costume d'agent (bBusiness) ; teint et cheveux selon le nom. */
	static FGPMOutfit Outfit(const FLinearColor& Main, const FLinearColor& Accent, const FString& Who, bool bBusiness);
	/** Point visé pendant une scène (entre les deux personnages, à hauteur de poitrine). */
	FVector CastFocus() const;
	/**
	 * Caméra de l'atelier. Plan 0 : écran à panneau à gauche (voiture dans le tiers droit), 1 : écran plein (voiture au centre),
	 * 2 : bienvenue (tour de la voiture), 3 : rendez-vous, 4 : signature.
	 */
	void Camera(int32 Shot, float T, FVector& Eye, FVector& Look, float& Fov) const;

private:
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Room;      // sol, mur, plafond, panneau du nom (éclairés) ; halo au sol (section 1)
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Glow;      // rampes, plafond (sans éclairage) ; plinthe et filet à la couleur de l'écurie (section 1)
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Plate;     // plateau tournant ; anneau lumineux (section 1)
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> FigA;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> FigB;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> ArmA;      // bras droit (poignée de main)
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> ArmB;
	UPROPERTY() TObjectPtr<UTextRenderComponent> NameText;
	UPROPERTY() TObjectPtr<USpotLightComponent> KeyLight;
	UPROPERTY() TObjectPtr<UPointLightComponent> RimA;
	UPROPERTY() TObjectPtr<UPointLightComponent> RimB;
	UPROPERTY() TObjectPtr<UPointLightComponent> FillLight;
	UPROPERTY() TObjectPtr<AGPMCar> Display;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> LitMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GlowMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> AccentMID;
	UPROPERTY() TObjectPtr<UMaterialInterface> VertexMat;
	bool bShown = true, bBuilt = false;
	float Yaw = 0.f;
	FLinearColor TeamMain = FLinearColor::Red, TeamAccent = FLinearColor::White;
	FString TeamLabel;
	uint32 CastSig = 0;
	void BuildRoom();
	void BuildGlow();
	void BuildPlate();
	void BuildAccent(const FLinearColor& Neon);
	void BuildFigure(UProceduralMeshComponent* Body, UProceduralMeshComponent* Arm, const FGPMOutfit& O);
	UProceduralMeshComponent* MakePart(const TCHAR* PartName, bool bShadow);
	UMaterialInterface* Mat(UMaterialInstanceDynamic* M) const;
};
