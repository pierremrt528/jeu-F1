#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GPMTypes.h"
#include "GPMSetup.h"
#include "GPMGameMode.generated.h"

class AGPMTrack;
class AGPMCar;
class APlayerController;

/**
 * Mode de jeu de base. Étape 3 : charge un circuit (option ?circuit=<id>, comme le jeu web, ou commande GPM.Circuit <id>)
 * place la lumière et les 20 monoplaces sur la grille (19 IA, js/race.js), aspiration, contacts et classement en direct.
 */
UCLASS()
class GRANDPRIXMANAGER_API AGPMGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGPMGameMode();
	virtual void BeginPlay() override;

	/** Remplace le circuit affiché. */
	UFUNCTION(BlueprintCallable, Category = "GPM") void LoadCircuit(const FString& Id);
	virtual void Tick(float DeltaSeconds) override;

	/** Place (1 = en tête) d'une voiture au classement en direct. */
	int32 PositionOf(const AGPMCar* C) const { return Order.IndexOfByKey(C) + 1; }
	int32 NumCars() const { return Order.Num(); }
	/** Départ (race.js) : feux rouges allumés un par un, extinction au bout d'un délai aléatoire. */
	enum class EPhase : uint8 { Quali, QualiDone, Lights, Race, Done, Season, Factory, Home, Setup, Solo, Settings, Press, Mercato, Tuning, Prerace, Fired, Intro, Outro };
	/** Écrans de gestion (la course est figée derrière, caméra de survol). */
	bool InMenu() const { return Phase >= EPhase::Season && Phase <= EPhase::Fired; }
	bool bFutCar = false;                    // atelier : voiture de la prochaine ère (onglet « Voiture <année> » de l'usine)
	FString CarsEra;                         // ère des monoplaces construites
	int32 UiRequest = -1;
	AGPMCar* PlayerCar() const { return Car; }                    // tests (GPM.Test) : 0-3 onglet de la saison, 10 usine (voiture suivante), 11 usine
	EPhase Phase = EPhase::Lights;
	float LightsT = 0.f, GoAt = 5.f, Clock = 0.f, DoneT = 0.f;
	int32 LightsOn() const { return Phase == EPhase::Lights ? FMath::Clamp(FMath::FloorToInt(LightsT / 0.8f), 0, 5) : 0; }
	int32 TotalLaps = 5;
	float LapEstimate = 90.f;
	const TArray<AGPMCar*>& Standings() const { return Order; }
	/** Temps (ou estimation) à l'arrivée : arrivés au temps réel, les autres d'après leur retard. */
	float ResultTime(const AGPMCar* C) const;
	/** Écart à l'arrivée en clair : « +1.234 », « +1 tour », « Abandon (moteur) ». */
	FString ResultGap(const AGPMCar* C) const;
	/** Abandon d'une voiture (casse moteur, accident) : elle s'arrête, puis est évacuée ; classée derrière les autres, sans points. */
	void Retire(AGPMCar* C, const TCHAR* Why);
	/** Recommencer la séance en cours (même grille, même stratégie). */
	void Restart();
	/** Après les résultats : écran de saison (carrière) ou nouvelle course ; depuis l'écran de saison : GP suivant. */
	void Continue();
	bool bCareer = true;
	int32 LastPts = 0;
	float LastPrize = 0.f;
	float LastPartner = 0.f;   // dont l'Espace partenaires
	/** Qualifications : temps de chaque pilote (le joueur : son meilleur tour) et ordre de la grille. */
	TMap<AGPMCar*, float> QTimes;
	TArray<AGPMCar*> QOrder;
	void StartQuali();
	void StartRace();
	static float LapTimeFor(const FGPMTrackData* T, float Skill, float Perf);
	/** Stand : demander / annuler l'arrêt du joueur (touche B ou bouton STAND). */
	void ToggleBox();
	/** Voiture de sécurité (safety.js) : après un gros accident (ou GPM.SC / ?sc), file indienne, pénalité de 5 s si le joueur dépasse. */
	void RequestSafety(bool bForce, bool bVirtual = false);
	/** 0 piste verte, 1 voiture de sécurité, 2 elle rentre ce tour, 3 relance (le leader mène jusqu'à la ligne), 4 VSC, 5 fin de VSC. */
	uint8 SCState = 0;
	bool SafetyOut() const { return SCState >= 1 && SCState <= 3; }
	float GreenT = 0.f;   // bandeau « drapeau vert » après une neutralisation
	AGPMCar* SafetyCar() const;
	void TestPenalty(uint8 Kind, float Sec);   // tests (GPM.TestSC)
	/** Commissaires : dernier communiqué (HUD), pilote concerné, durée d'affichage ; enquête en cours. */
	FString StewardHead, StewardWhy;
	TWeakObjectPtr<AGPMCar> StewardCar;
	float StewardT = 0.f;
	bool bStewardInquiry = false;
	/** Message radio du muret affiché quelques secondes. */
	FString Radio; float RadioT = 0.f;
	void Say(const FString& Msg) { Radio = Msg; RadioT = 5.f; }
	static FString TyreName(TCHAR T) { return T == 'S' ? TEXT("tendres") : T == 'H' ? TEXT("durs") : T == 'W' ? TEXT("pluie") : T == 'I' ? TEXT("intermédiaires") : TEXT("mediums"); }
	static constexpr int32 Points[10] = { 25, 18, 15, 12, 10, 8, 6, 4, 2, 1 };

	// --- Menus (étape 15) ---
	/** Accueil : Carrière / Course solo / Réglages. */
	void GoHome();
	void NewCareer();
	void FinishSetup();
	// --- Pré-saison de janvier (GPMPreseason.cpp) : révélation de la monoplace, événements, trois journées d'essais jouables à missions ---
	bool bTesting = false, bTestAuto = false, bReveal = false, bRevealDialog = false;
	int32 PreStage() const;
	void StartReveal();
	void StartPreEvents();
	void StartTest();
	void SkipPreseason();
	void FinishPreseason();
	/** Textes du panneau de mission affiché pendant les essais : titre, objectif, état. */
	void TestLines(TArray<FString>& Out) const;
	static FString TestName(int32 Day);
	static FString TestGoal(int32 Day);
	/** Détruit les monoplaces et les recrée au prochain chargement de circuit (le plateau a changé : écurie rejointe, 11e écurie). */
	void RespawnCars();
	void ContinueCareer();
	void StartSolo();
	void OpenMercato();
	/** Fin des fenêtres à choix (presse, flash info, négociation). */
	void EndDialogs();
	int32 SetupStep = 0, SoloIdx = 0, SoloLaps = 5, MercatoPage = 0;
	int32 SoloWeather = 0;          // course solo : 0 au hasard, 1 sec, 2 pluie, 3 averse, 4 éclaircie, 5 pluie fine
	bool bSoloQuali = true, bNegoFromMercato = false, bChampShown = false;
	TArray<FString> LastLines;      // bilan de la course (direction, sponsors)
	// réglages (sauvegardés dans GameUserSettings.ini)
	bool bSetABS = true, bSetTC = true, bSetStab = true, bSetCockpit = false, bSetSound = true;
	/** Niveau des pilotes IA : 0 débutant … 4 expert (multiplie leur rythme, DIFFS de data.js). */
	int32 AILevel = 2;
	static float AIDiff(int32 L) { static const float K[5] = { 0.95f, 0.975f, 1.f, 1.012f, 1.025f }; return K[FMath::Clamp(L, 0, 4)]; }
	static const TCHAR* AILevelName(int32 L) { static const TCHAR* const Nm[5] = { TEXT("Débutant"), TEXT("Facile"), TEXT("Normal"), TEXT("Difficile"), TEXT("Expert") }; return Nm[FMath::Clamp(L, 0, 4)]; }
	void SetAILevel(int32 L);
	void SaveSettings();
	/** Clic d'un bouton des menus (son généré). */
	void UiClick();
	// --- Week-end comme le jeu web : réglages → qualifs (jouées ou simulées) → stratégie → course (ou simulée) ---
	/** Réglages de base (boutons rapides de l'écran de réglages) : ailerons et rapports de boîte. */
	struct FSetup { const TCHAR* Name; const TCHAR* Desc; int32 FW, RW, Gears; };
	static const FSetup Setups[5];
	/** Une stratégie : gomme et nombre de tours de chaque relais, temps estimé. */
	struct FPlan { TArray<TPair<TCHAR, int32>> Stints; float Time = 0.f; float MaxWear = 0.f; FString Name, Desc; };
	int32 SetupK = -1;                       // réglages faits pour ce week-end (parc fermé) ; -1 = pas encore (conseil de l'ingénieur)
	FGPMCarSetup Setup;                      // réglages de la voiture du joueur pour ce week-end
	int32 SetupRow = 0;                      // ligne choisie au clavier sur l'écran de réglages
	bool bGridReady = false;                 // qualifications faites pour ce Grand Prix
	bool bLongRace = false;                  // format Long (~22 min) au lieu de Sprint (~11 min)
	TArray<TPair<TCHAR, int32>> Stints;      // stratégie du joueur pour la course
	TArray<FPlan> Plans;                     // 3 stratégies proposées
	/** Carburant au départ choisi par le joueur : 0 très léger (−8 %) … 2 normal (+3 %) … 4 réservoir plein ; ravitaillement à chaque arrêt. */
	int32 FuelK = 2;
	static constexpr float Tank = 110.f;     // réservoir (kg, TANK de strategy.js)
	static const TCHAR* FuelName(int32 K) { static const TCHAR* const Nm[5] = { TEXT("Très léger"), TEXT("Léger"), TEXT("Normal"), TEXT("Marge"), TEXT("Plein") }; return Nm[FMath::Clamp(K, 0, 4)]; }
	/** Essence pour Laps tours de cette voiture (kg, consommation du motoriste comprise). */
	float FuelFor(const AGPMCar* C, float Laps) const;
	/** Essence du joueur au départ selon FuelK, pour le premier relais (ou toute la course sans arrêt). */
	float PlayerStartFuel(int32 K) const;
	/** Tours à couvrir jusqu'au prochain arrêt prévu (ravitaillement) ou jusqu'à l'arrivée. */
	float LapsToRefuel(const AGPMCar* C) const;
	void OpenTuning();
	void PlayQuali();
	void SimQuali();
	void OpenPrerace();
	void Go(bool bSim);
	void SimGrandPrix();
	int32 LapsFor() const;
	/** Tour simulé avec ces réglages (lapSim de tuning.js, étendu à tous les réglages). */
	float SetupLap(const FGPMCarSetup& S) const;
	/** Tour avec les réglages du joueur / tour avec la voiture de base. */
	float SetupRatio() const;
	/** Réglages conseillés par l'ingénieur pour ce circuit et cette voiture (calculés une fois). */
	const FGPMCarSetup& Advice() const;
	/** Réglages du week-end : ceux de l'ingénieur s'ils ne sont pas encore faits. */
	void EnsureSetup();
	/** Réglage de base correspondant aux ailerons et à la boîte (−1 : personnalisé). */
	int32 PresetOf(const FGPMCarSetup& S) const;
	void ApplyPreset(int32 K);
	/** Équilibre de la voiture en virage rapide : < 0 sous-vireuse, > 0 survireuse (écart d'adhérence avant/arrière). */
	float SetupBalance(const FGPMCarSetup& S) const;
	float BaseLap() const;
	float PlanTime(const TArray<TPair<TCHAR, int32>>& InStints, float LapTime, float* OutMaxWear = nullptr) const;
	TArray<FPlan> MakePlans() const;
	const FGPMTrackData* Circuit() const { return CurTrack(); }
	// --- Course complète (strategy.js) : météo, radio du muret, secteurs ---
	/** Météo du week-end (forecast) : pluie au départ, changement en cours de course, piste qui mouille puis sèche. */
	bool bRainStart = false;                 // il pleut au départ
	float WxChangeAt = -1.f;                 // part de la course où la météo change (−1 : jamais)
	bool bWxDone = false, bWxWarned = false;
	bool bRaining = false;
	float TrackWet = 0.f;                    // piste : 0 sèche … 1 détrempée
	float RainLevel = 0.9f;                  // pluie forte (0,9) ou fine (0,42 : piste humide, le domaine des intermédiaires)
	TCHAR RainTyre() const { return RainLevel < 0.6f ? TCHAR('I') : TCHAR('W'); }
	/** Pneus pour la pluie qu'il faut à cette voiture maintenant : 'W' pluie, 'I' intermédiaires, 0 slicks. */
	TCHAR WetTyreFor(const AGPMCar* C) const;
	float RainShow = 0.f;                    // pluie à l'écran (suit bRaining en douceur)
	void Forecast();
	FString WeatherLabel() const;            // « Sec », « Pluie », « Averse attendue », « Éclaircie attendue »
	/** Tour plus lent sur le mouillé (qualifications et courses simulées). */
	float WetLapK() const { return 1.f + 0.1f * TrackWet; }
	float RaceWetK() const;
	/** Fiabilité (GPMReliability.cpp) : risque d'ennui moteur sur une course, selon le motoriste et les pièces de fiabilité. */
	float EngineRisk(const AGPMCar* C) const;
	/** Radio du muret (3 boutons, touches 1 2 3 ou I M N) : 0 point complet, 1 rythme, 2 pneus du prochain arrêt. */
	void RadioCall(int32 K);
	TCHAR NextStopTyre(const AGPMCar* C) const;
	TCHAR ManualTyre = 0;                    // pneus demandés à la radio pour un arrêt hors stratégie
	bool bTyrePicked = false;                // le joueur a choisi lui-même les pneus du prochain arrêt
	/** Qualifications : secteurs S1 S2 S3 (0 = pas encore), couleur (1 violet, 2 vert, 3 jaune), records perso du week-end. */
	float QSec[3] = { 0.f, 0.f, 0.f };
	int32 QCol[3] = { 0, 0, 0 };
	float QPB[3] = { 0.f, 0.f, 0.f };
	float QPole() const;                     // meilleur temps des IA (la pole à battre)
	bool QDelta(float& Out) const;           // écart en direct avec la pole sur le tour en cours
	/** Touche chiffre 1 à 5, aussi sur un clavier AZERTY (& é " ' () et sur le pavé numérique. */
	static bool DigitPressed(const APlayerController* PC, int32 D);
	/** Images par seconde (moyenne glissante) et résolution adaptative en %. */
	float Fps = 60.f;
	int32 ScreenPct = 100;
	bool bPerfTest = false;                  // GPM.TestPerf : pas de résolution adaptative pendant la mesure

	// --- Cinématiques (étape 16, cine.js) : seule la caméra bouge ---
	enum class ECine : uint8 { None, Menu, Intro, Podium, Welcome, Meet, Sign, Crown };
	/** Cinématiques jouées dans l'atelier 3D (bienvenue, rendez-vous avec l'agent, signature, sacre). */
	bool RoomCine() const { return Cine == ECine::Welcome || Cine == ECine::Meet || Cine == ECine::Sign || Cine == ECine::Crown; }
	/** Rendez-vous au QG avec un pilote et son agent (première rencontre de la partie). FromTeam : écurie actuelle, vide si libre. */
	void StartMeet(const FString& Who, const FString& FromTeam);
	/** Signature d'un pilote : poignée de main avec le joueur, puis photo devant la voiture. */
	void StartSign(const FString& Who);
	ECine Cine = ECine::None;
	float CineT = 0.f;
	FString CineTitle, CineSub;
	void StartCine(ECine Mode);
	void EndCine();
	/** Plan en cours de la cinématique (survol : 0 à 3, tour d'honneur et sacre : 0 à 2). */
	int32 CineShot() const;
	/** Fondu au noir entre deux plans (0 : image, 1 : noir), dessiné par le HUD. */
	float CineCut() const;

private:
	UPROPERTY() TObjectPtr<AGPMTrack> Track;
	UPROPERTY() TObjectPtr<AGPMCar> Car;               // le joueur
	UPROPERTY() TArray<TObjectPtr<AGPMCar>> Cars;      // les 20 voitures
	TArray<AGPMCar*> Order;
	void SetupWorld();
	void ApplyTheme(const FGPMTrackData* T);
	UPROPERTY() TObjectPtr<class ADirectionalLight> Sun;
	UPROPERTY() TObjectPtr<class ASkyLight> SkyLight;
	UPROPERTY() TObjectPtr<class AExponentialHeightFog> Fog;
	UPROPERTY() TObjectPtr<class AGPMScenery> Scenery;
	UPROPERTY() TObjectPtr<class UGPMEngineSound> EngineSound;
	// sons de la course (GPMSound.cpp) : voiture la plus proche, chocs, feux, radio, foule
	void UpdateAudio(float Dt);
	const AGPMCar* SndNear = nullptr;
	float SndNearD = 0.f, SndHit = 0.f, SndContact = 0.f, SndLeadT = 0.f;
	int32 SndLights = 0, SndPos = 0;
	EPhase SndPhase = EPhase::Home;
	bool bSndFinished = false;
	FString SndRadio;
	void PlaceCar();
	void Aspiration(float Dt);
	void Collide();
	void Strategy(float Dt);
	void EndQuali();
	void RecordResult();
	void ApplyCareer();
	bool bRecorded = false;
	void Park(AGPMCar* C, bool bPark);
	const FGPMTrackData* CurTrack() const;
	void UpdateSafety(float Dt);
	UPROPERTY() TObjectPtr<AGPMCar> SC;
	float SCT = 0.f, SCEnding = 0.f;
	bool bSCUsed = false;
	int32 SCCount = 0, SCLeadLap = 0, SCPassed = 0;   // neutralisations de la course ; tour du leader au début de la relance
	TWeakObjectPtr<AGPMCar> SCLeader;
	TSet<AGPMCar*> SCAhead;
	TCHAR PickTyre(int32 LapsLeft) const;
	int32 GridSlot = 9;
	int32 Pending = 0;   // après le chargement d'un circuit : 0 = qualifs (test), 1 = accueil, 2 = QG (bienvenue), 3 = grille en fond, 4 = réglages, 5 = stratégie
	bool bBootMenu = false;
	TSet<FString> IntroSeen;
	void StageGrid();
	void RefreshRoster();
	void LoadSettings();
	void ApplySettings();
	void UpdateCine(float Dt);
	void UpdatePerf(float Dt);
	float FrameAvg = 1.f / 60.f, PerfT = 0.f, PerfLogT = 0.f, PerfWorst = 0.f;
	int32 PerfHitch = 0;   // images de plus de 25 ms depuis la dernière ligne du journal (à-coups)
	UPROPERTY() TObjectPtr<class ACameraActor> CineCam;
	FVector CineFwd = FVector::ForwardVector;   // cap lissé de la voiture pendant le tour d'honneur
	FVector CineAim = FVector::ZeroVector;      // point visé lissé (cadreur)
	int32 CineShotSeen = -1;
	int32 CrownYear = -1;                        // saison dont le sacre a déjà été joué
	void CineLens(float Focus, float Fstop);    // flou de profondeur de la caméra des cinématiques (Focus 0 : net partout)
	bool bTyreWarn = false;
	// fiabilité : part de la course où le moteur de la voiture faiblit, voitures déjà annoncées à la radio ; crevaison du joueur
	void RollReliability();
	bool EngineTrouble(const AGPMCar* C);
	TMap<const AGPMCar*, float> EngineFailAt;
	TSet<const AGPMCar*> EngineTold;
	TSet<const AGPMCar*> EngineBlown;        // moteurs qui casseront (abandon) au lieu de seulement faiblir
	bool bHadFlat = false;
	void TestTick(float Dt);
	void EndTest(const TCHAR* Why);
	void ReturnToSeason();
	TArray<float> TestLaps;
	TArray<bool> TestClean;
	int32 TestLapSeen = 0, TestWanted = 3;
	bool TestDirty = false, TestEnded = false;
	float TestRef = 0.f, TestHurt = 0.f, TestEngineAt = -1.f;
	// commissaires (GPMPenalty.cpp) : limites de piste du joueur, accrochages ; tour de qualification annulé
	void TrackLimits(float Dt);
	void ContactPenalty(AGPMCar* Guilty, AGPMCar* Victim, float Close);
	/** Sanction : Kind 0 = secondes de pénalité, 1 = drive-through, 2 = stop-and-go ; Delay > 0 : enquête annoncée d'abord. */
	void Penalize(AGPMCar* C, uint8 Kind, float Sec, const FString& Why, float Delay = 0.f);
	void Decide(AGPMCar* C, uint8 Kind, float Sec, const FString& Why);
	void Stewards(float Dt);
	/** Arrêt dans le carré : secondes de pénalité à purger avant le travail des mécaniciens (stop-and-go : arrêt de 10 s). */
	float ServeAtStop(AGPMCar* C);
	struct FInquiry { TWeakObjectPtr<AGPMCar> C; uint8 Kind = 0; float Sec = 0.f, At = 0.f; FString Why; };
	TArray<FInquiry> Inquiries;
	bool bQInvalid = false;
	int32 FuelWarn = 0;   // 1 : essence juste annoncée, 2 : panne sèche
	// captures d'écran automatiques (Saved/Captures, une par écran et par partie) pour régler l'interface et les graphismes
	void AutoCapture(float Dt);
	TSet<FString> Captured;
	EPhase CapPhase = EPhase::Lights;
	float CapT = 0.f;
	bool bShadersBusy = false;   // matériaux en cours de compilation (damier gris) : pas de capture d'écran
	void BaseUps();
	void ApplySetup();
	mutable FGPMCarSetup AdviceCache;
	mutable uint32 AdviceKey = 0;
	void SimRace();
	bool bNoIntro = false;
	// météo, dégâts, radio, secteurs
	void ResetWeather();
	void UpdateWeather(float Dt);
	void UpdateSky(float Dt);
	void AddDamage(AGPMCar* C, float Amount, bool bRear);
	bool WantBox(const AGPMCar* C) const;
	FString RadioBrief() const;
	void UpdateSectors();
	int32 SectorColour(int32 K) const;
	float QSplit[2] = { 0.f, 0.f };
	float QPB0[3] = { 0.f, 0.f, 0.f };
	TArray<float> QCum;                      // temps de référence cumulé le long du tour (profil de vitesse)
	float SkyGloom = 0.f, SkyGloomSet = -1.f, SkyCaptured = 0.f, WetSet = -1.f;
	float FogBase = 0.004f;
	FLinearColor FogTint = FLinearColor(0.36f, 0.42f, 0.48f);
	UPROPERTY() TObjectPtr<class AGPMSky> SkyDome;
	UPROPERTY() TObjectPtr<class AGPMDebris> Debris;
	UPROPERTY() TObjectPtr<class AGPMRain> Rain3D;   // gerbes d'eau et gouttes en 3D (GPMRain)
	// atelier 3D derrière les écrans de gestion (GPMShowroom), sous le circuit
	UPROPERTY() TObjectPtr<class AGPMShowroom> Showroom;
	bool bRoomOn = false;
	int32 RoomScene = 0, RoomShot = -1;
	FVector RoomEye = FVector::ZeroVector, RoomLook = FVector::ZeroVector;
	float RoomFov = 62.f;
	TSet<FString> MetDrivers;
	bool RoomWanted() const;
	void UpdateRoom();
};
