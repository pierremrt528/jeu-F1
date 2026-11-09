#include "GPMDataSubsystem.h"
#include "GrandPrixManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FLinearColor HexToColor(int64 Hex)
	{
		return FLinearColor(FColor((Hex >> 16) & 255, (Hex >> 8) & 255, Hex & 255));
	}

	void ReadFloats(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, TArray<float>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr;
		if (O->TryGetArrayField(Key, Arr))
		{
			Out.Reserve(Arr->Num());
			for (const TSharedPtr<FJsonValue>& V : *Arr) Out.Add((float)V->AsNumber());
		}
	}
}

namespace
{
	FLinearColor HexLin(const TSharedPtr<FJsonObject>& O, const TCHAR* K, FLinearColor Def)
	{
		double V; return O->TryGetNumberField(K, V) ? HexToColor((int64)V) : Def;
	}
}

void UGPMDataSubsystem::ReadTheme(const TSharedPtr<FJsonObject>& O, FGPMTheme& Th)
{
	Th.Horizon = HexLin(O, TEXT("horizon"), Th.Horizon);
	Th.Zenith = HexLin(O, TEXT("zenith"), Th.Zenith);
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (O->TryGetArrayField(TEXT("sun"), A) && A->Num() == 3)   // web (x, y haut, z) → Unreal (x, z, y)
		Th.Sun = FVector((*A)[0]->AsNumber(), (*A)[2]->AsNumber(), (*A)[1]->AsNumber()).GetSafeNormal();
	if (O->TryGetArrayField(TEXT("fog"), A) && A->Num() == 2) { Th.FogStart = (*A)[0]->AsNumber(); Th.FogEnd = (*A)[1]->AsNumber(); }
	O->TryGetStringField(TEXT("surface"), Th.Surface);
	O->TryGetStringField(TEXT("treeKind"), Th.TreeKind);
	double D;
	if (O->TryGetNumberField(TEXT("trees"), D)) Th.Trees = int32(D);
	if (O->TryGetArrayField(TEXT("treeHue"), A) && A->Num() == 2) Th.TreeHue = float(((*A)[0]->AsNumber() + (*A)[1]->AsNumber()) / 2);
	O->TryGetBoolField(TEXT("sea"), Th.bSea);
	const TSharedPtr<FJsonObject>* C = nullptr;
	if (O->TryGetObjectField(TEXT("city"), C))
	{
		if ((*C)->TryGetNumberField(TEXT("count"), D)) Th.City = int32(D);
		if ((*C)->TryGetNumberField(TEXT("hmin"), D)) Th.CityHMin = D;
		if ((*C)->TryGetNumberField(TEXT("hmax"), D)) Th.CityHMax = D;
	}
	const TSharedPtr<FJsonObject>* Mo = nullptr;
	if (O->TryGetObjectField(TEXT("mountains"), Mo))
	{
		auto Read = [&](const TCHAR* K, float& H0, float& H1, FLinearColor& Col)
		{
			const TArray<TSharedPtr<FJsonValue>>* M = nullptr;
			if ((*Mo)->TryGetArrayField(K, M) && M->Num() >= 3)
			{
				H0 = (*M)[0]->AsNumber(); H1 = (*M)[1]->AsNumber();
				Col = HexToColor((int64)(*M)[2]->AsNumber());
			}
		};
		Read(TEXT("far"), Th.FarH0, Th.FarH1, Th.MountFar);
		Read(TEXT("near"), Th.NearH0, Th.NearH1, Th.MountNear);
	}
}

FString UGPMDataSubsystem::DataDir()
{
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"));
}

TSharedPtr<FJsonObject> UGPMDataSubsystem::LoadFile(const FString& RelPath)
{
	if (const TSharedPtr<FJsonObject>* Found = Files.Find(RelPath)) return *Found;
	FString Text;
	TSharedPtr<FJsonObject> Obj;
	if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(DataDir(), RelPath)))
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		FJsonSerializer::Deserialize(Reader, Obj);
	}
	if (!Obj.IsValid()) UE_LOG(LogGPM, Warning, TEXT("Données introuvables ou invalides : %s"), *RelPath);
	Files.Add(RelPath, Obj);
	return Obj;
}

void UGPMDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (TSharedPtr<FJsonObject> Data = LoadFile(TEXT("data.json")))
	{
		for (const TSharedPtr<FJsonValue>& TV : Data->GetArrayField(TEXT("TEAMS")))
		{
			const TSharedPtr<FJsonObject> T = TV->AsObject();
			FGPMTeam Team;
			Team.Name = T->GetStringField(TEXT("name"));
			Team.Color = HexToColor((int64)T->GetNumberField(TEXT("color")));
			Team.Accent = HexToColor((int64)T->GetNumberField(TEXT("accent")));
			Team.Perf = T->GetNumberField(TEXT("perf"));
			for (const TSharedPtr<FJsonValue>& DV : T->GetArrayField(TEXT("drivers")))
			{
				const TSharedPtr<FJsonObject> D = DV->AsObject();
				FGPMDriver Driver;
				Driver.Name = D->GetStringField(TEXT("name"));
				D->TryGetBoolField(TEXT("player"), Driver.bPlayer);
				double Skill;
				if (D->TryGetNumberField(TEXT("skill"), Skill)) Driver.Skill = Skill;
				Team.Drivers.Add(Driver);
			}
			Teams.Add(Team);
		}
		for (const TSharedPtr<FJsonValue>& CV : Data->GetArrayField(TEXT("CIRCUITS")))
		{
			CircuitIds.Add(CV->AsObject()->GetStringField(TEXT("id")));
			CircuitDefs.Add(CircuitIds.Last(), CV->AsObject());
		}
	}

	if (TSharedPtr<FJsonObject> Strat = LoadFile(TEXT("strategy.json")))
	{
		for (const auto& Pair : Strat->GetObjectField(TEXT("TYRES"))->Values)
		{
			const TSharedPtr<FJsonObject> O = Pair.Value->AsObject();
			FGPMTyre Tyre;
			Tyre.Id = Pair.Key;
			Tyre.Name = O->GetStringField(TEXT("name"));
			Tyre.Color = FLinearColor(FColor::FromHex(O->GetStringField(TEXT("color"))));
			Tyre.Grip = O->GetNumberField(TEXT("grip"));
			Tyre.WearPerLap = O->GetNumberField(TEXT("wpl"));
			Tyres.Add(Tyre);
		}
	}

	if (TSharedPtr<FJsonObject> Season = LoadFile(TEXT("season.json")))
		for (const TSharedPtr<FJsonValue>& V : Season->GetArrayField(TEXT("CALENDAR"))) Calendar.Add(V->AsString());

	UE_LOG(LogGPM, Log, TEXT("Données chargées : %d écuries, %d circuits, %d gommes"), Teams.Num(), CircuitIds.Num(), Tyres.Num());
}

TArray<FString> UGPMDataSubsystem::GetCircuitIds() const
{
	return CircuitIds;
}

const FGPMTrackData* UGPMDataSubsystem::GetTrack(const FString& Id)
{
	if (const TSharedPtr<FGPMTrackData>* Cached = TrackCache.Find(Id)) return Cached->Get();
	TSharedPtr<FJsonObject> O = LoadFile(FString::Printf(TEXT("tracks/%s.json"), *Id));
	if (!O.IsValid()) return nullptr;

	TSharedPtr<FGPMTrackData> Ptr = MakeShared<FGPMTrackData>();
	FGPMTrackData& T = *Ptr;
	T.Id = Id;
	T.Name = O->GetStringField(TEXT("name"));
	if (const TSharedPtr<FJsonObject>* Def = CircuitDefs.Find(Id))
	{
		(*Def)->TryGetStringField(TEXT("type"), T.Type);
		const TSharedPtr<FJsonObject>* Th = nullptr;
		if ((*Def)->TryGetObjectField(TEXT("theme"), Th)) ReadTheme(*Th, T.Theme);
	}
	T.Length = O->GetNumberField(TEXT("length"));
	O->TryGetNumberField(TEXT("width"), T.Width);
	TArray<float> X, Y, Z;
	ReadFloats(O, TEXT("X"), X);
	ReadFloats(O, TEXT("Y"), Y);
	ReadFloats(O, TEXT("Z"), Z);
	T.Points.Reserve(X.Num());
	for (int32 i = 0; i < X.Num(); ++i) T.Points.Add(FGPMTrackData::ToWorld(X[i], Y.IsValidIndex(i) ? Y[i] : 0.f, Z[i]));
	T.WX = X; T.WY = Y; T.WZ = Z;
	if (T.WY.Num() != X.Num()) T.WY.Init(0.f, X.Num());
	{
		const int32 N = X.Num();
		T.TX.SetNum(N); T.TZ.SetNum(N); T.H.SetNum(N);
		for (int32 i = 0; i < N; ++i)
		{
			const float Tx = X[(i + 1) % N] - X[(i - 1 + N) % N], Tz = Z[(i + 1) % N] - Z[(i - 1 + N) % N];
			const float L = FMath::Max(1e-4f, FMath::Sqrt(Tx * Tx + Tz * Tz));
			T.TX[i] = Tx / L; T.TZ[i] = Tz / L; T.H[i] = FMath::Atan2(T.TX[i], T.TZ[i]);
		}
	}
	ReadFloats(O, TEXT("WL"), T.WallLeft);
	ReadFloats(O, TEXT("WR"), T.WallRight);
	ReadFloats(O, TEXT("C"), T.Curvature);
	ReadFloats(O, TEXT("V"), T.AISpeed);
	if (const TSharedPtr<FJsonObject>* Pit = nullptr; O->TryGetObjectField(TEXT("pit"), Pit))
	{
		double D;
		if ((*Pit)->TryGetNumberField(TEXT("half"), D)) T.PitHalf = D;
		if ((*Pit)->TryGetNumberField(TEXT("lat"), D)) T.PitLat = D;
		if ((*Pit)->TryGetNumberField(TEXT("loss"), D)) T.PitLoss = D;
		if ((*Pit)->TryGetNumberField(TEXT("e0"), D)) T.PitE0 = int32(D);
		if ((*Pit)->TryGetNumberField(TEXT("e1"), D)) T.PitE1 = int32(D);
		if ((*Pit)->TryGetNumberField(TEXT("x0"), D)) T.PitX0 = int32(D);
		if ((*Pit)->TryGetNumberField(TEXT("x1"), D)) T.PitX1 = int32(D);
	}
	{
		// trajectoire : courbure brute lissée sur ±22 points, −260 × courbure, bornée (comme track.js)
		const int32 N = X.Num();
		TArray<float> Raw; Raw.SetNum(N); T.RL.SetNum(N);
		for (int32 i = 0; i < N; ++i)
		{
			const float D = T.H[(i + 2) % N] - T.H[(i - 2 + N) % N];
			Raw[i] = FMath::Atan2(FMath::Sin(D), FMath::Cos(D)) / 8.f;
		}
		const float Lim = T.Width / 2.f - 2.2f;
		for (int32 i = 0; i < N; ++i)
		{
			float Sum = 0.f;
			for (int32 k = -22; k <= 22; ++k) Sum += Raw[((i + k) % N + N) % N];
			T.RL[i] = FMath::Clamp(-Sum / 45.f * 260.f, -Lim, Lim);
		}
	}

	Files.Remove(FString::Printf(TEXT("tracks/%s.json"), *Id));   // le JSON brut n'est plus utile
	UE_LOG(LogGPM, Log, TEXT("Circuit %s : %d points, %.0f m"), *T.Name, T.Num(), T.Length);
	TrackCache.Add(Id, Ptr);
	return Ptr.Get();
}

const FGPMCarModel* UGPMDataSubsystem::GetCarModel()
{
	if (CarModel.IsValid()) return CarModel->P.Num() ? CarModel.Get() : nullptr;
	CarModel = MakeShared<FGPMCarModel>();
	TSharedPtr<FJsonObject> O = LoadFile(TEXT("car.json"));
	if (!O.IsValid()) return nullptr;
	TArray<float> P, N, K;
	ReadFloats(O, TEXT("P"), P);
	ReadFloats(O, TEXT("N"), N);
	ReadFloats(O, TEXT("K"), K);
	const int32 Cnt = FMath::Min3(P.Num(), N.Num(), K.Num() * 3) / 3 / 3 * 3;   // multiple de 3 sommets
	FGPMCarModel& M = *CarModel;
	// three.js (x, y haut, z avant, en m) → Unreal (X avant, Y à droite = −x, Z haut, en cm)
	for (int32 i = 0; i < Cnt; ++i)
	{
		M.P.Add(FVector(P[i * 3 + 2], -P[i * 3], P[i * 3 + 1]) * 100.f);
		M.N.Add(FVector(N[i * 3 + 2], -N[i * 3], N[i * 3 + 1]));
		M.K.Add(int32(K[i]));
	}
	const TArray<TSharedPtr<FJsonValue>>* Pal;
	if (O->TryGetArrayField(TEXT("palette"), Pal))
		for (const TSharedPtr<FJsonValue>& V : *Pal)
		{
			const FColor C = FColor::FromHex(V->AsString());   // valeurs déjà linéaires (three.js)
			M.Fixed.Add(FLinearColor(C.R / 255.f, C.G / 255.f, C.B / 255.f));
		}
	Files.Remove(TEXT("car.json"));
	UE_LOG(LogGPM, Log, TEXT("Monoplace : %d triangles"), M.P.Num() / 3);
	return M.P.Num() ? &M : nullptr;
}

TSharedPtr<FJsonValue> UGPMDataSubsystem::GetRaw(const FString& File, const FString& Key)
{
	TSharedPtr<FJsonObject> O = LoadFile(File + TEXT(".json"));
	return O.IsValid() ? O->TryGetField(Key) : nullptr;
}
