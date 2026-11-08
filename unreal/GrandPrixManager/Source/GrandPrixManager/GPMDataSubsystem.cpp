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
			CircuitIds.Add(CV->AsObject()->GetStringField(TEXT("id")));
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
	if (const FGPMTrackData* Cached = TrackCache.Find(Id)) return Cached;
	TSharedPtr<FJsonObject> O = LoadFile(FString::Printf(TEXT("tracks/%s.json"), *Id));
	if (!O.IsValid()) return nullptr;

	FGPMTrackData T;
	T.Id = Id;
	T.Name = O->GetStringField(TEXT("name"));
	T.Length = O->GetNumberField(TEXT("length"));
	O->TryGetNumberField(TEXT("width"), T.Width);
	TArray<float> X, Y, Z;
	ReadFloats(O, TEXT("X"), X);
	ReadFloats(O, TEXT("Y"), Y);
	ReadFloats(O, TEXT("Z"), Z);
	T.Points.Reserve(X.Num());
	for (int32 i = 0; i < X.Num(); ++i) T.Points.Add(FGPMTrackData::ToWorld(X[i], Y.IsValidIndex(i) ? Y[i] : 0.f, Z[i]));
	ReadFloats(O, TEXT("WL"), T.WallLeft);
	ReadFloats(O, TEXT("WR"), T.WallRight);
	ReadFloats(O, TEXT("C"), T.Curvature);
	ReadFloats(O, TEXT("V"), T.AISpeed);

	Files.Remove(FString::Printf(TEXT("tracks/%s.json"), *Id));   // le JSON brut n'est plus utile
	UE_LOG(LogGPM, Log, TEXT("Circuit %s : %d points, %.0f m"), *T.Name, T.Num(), T.Length);
	return &TrackCache.Add(Id, MoveTemp(T));
}

TSharedPtr<FJsonValue> UGPMDataSubsystem::GetRaw(const FString& File, const FString& Key)
{
	TSharedPtr<FJsonObject> O = LoadFile(File + TEXT(".json"));
	return O.IsValid() ? O->TryGetField(Key) : nullptr;
}
