// Réglages détaillés de la voiture (écran de réglages du week-end) et leur effet sur la physique de AGPMCar.
#pragma once

#include "CoreMinimal.h"

/** Effet des réglages sur la physique (1 = voiture de base ; les IA roulent avec ces valeurs neutres). */
struct FGPMSetupFx
{
	float KdF = 1.f, KdR = 1.f;               // appui aéro à l'avant / à l'arrière
	float VTop = 1.f, Acc = 1.f;              // vitesse de pointe (traînée, rapports), accélération (rapports)
	float MuF = 1.f, MuR = 1.f;               // adhérence en virage de l'essieu avant / arrière
	float MuLong = 1.f;                       // adhérence au freinage et à l'accélération
	float Bias = 0.56f, BiasIdeal = 0.56f;    // part du freinage à l'avant, et la répartition idéale (selon l'appui)
	float BrakeP = 1.f;                       // force de freinage (pression dans les étriers)
	float Diff = 0.5f;                        // blocage du différentiel : motricité contre sous-virage à la remise des gaz
	float Kerb = 1.f, Bump = 1.f;             // adhérence sur les vibreurs, effet des bosses
	float Bottom = 0.f;                       // talonnage à haute vitesse (voiture basse et souple)
	float Wear = 1.f;                         // usure des pneus
};

/** Réglages détaillés de la voiture du joueur (écran de réglages du week-end) : un cran de 0 à 10 par réglage. */
struct FGPMCarSetup
{
	enum EKey { FrontWing, RearWing, Gears, BrakeBias, BrakePress, Diff, Susp, Arb, Ride, Camber, Press, Num };
	int32 V[Num] = { 5, 5, 5, 4, 8, 5, 5, 5, 5, 5, 5 };
	FGPMSetupFx Fx() const;
	/** Nom, explication et valeur affichée (« 56 % avant », « −2,5° »…) du réglage K. */
	static const TCHAR* Name(int32 K);
	static const TCHAR* Help(int32 K);
	FString Value(int32 K) const;
	/** Libellés des deux bouts de la réglette (« Moins d'appui » / « Plus d'appui »). */
	static const TCHAR* LowLabel(int32 K);
	static const TCHAR* HighLabel(int32 K);
	uint32 Hash() const { uint32 H = 0; for (int32 k = 0; k < Num; ++k) H = H * 13u + uint32(V[k]); return H; }
	bool operator==(const FGPMCarSetup& O) const { for (int32 k = 0; k < Num; ++k) if (V[k] != O.V[k]) return false; return true; }
};
