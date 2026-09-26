#include "LaneGraph/DFLevelFile.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	bool ReadVec3(const TSharedPtr<FJsonValue>& Value, FVector& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Value.IsValid() || !Value->TryGetArray(Arr) || !Arr || Arr->Num() < 3)
		{
			return false;
		}
		const FVector Sim((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber());
		Out = FDFSimFrame::ToUnreal(Sim);
		return true;
	}

	bool ReadLayer(const FString& Text, EDFEnemyLayer& Out)
	{
		if (Text.Equals(TEXT("ground"), ESearchCase::IgnoreCase)) { Out = EDFEnemyLayer::Ground; return true; }
		if (Text.Equals(TEXT("air"), ESearchCase::IgnoreCase))    { Out = EDFEnemyLayer::Air;    return true; }
		return false;
	}

	bool ReadSocketTag(const FString& Text, EDFSocketTag& Out)
	{
		if (Text.Equals(TEXT("ground"), ESearchCase::IgnoreCase))    { Out = EDFSocketTag::Ground;    return true; }
		if (Text.Equals(TEXT("wall"), ESearchCase::IgnoreCase))      { Out = EDFSocketTag::Wall;      return true; }
		if (Text.Equals(TEXT("trap"), ESearchCase::IgnoreCase))      { Out = EDFSocketTag::Trap;      return true; }
		if (Text.Equals(TEXT("barricade"), ESearchCase::IgnoreCase)) { Out = EDFSocketTag::Barricade; return true; }
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>& ArrayField(const FJsonObject& Obj, const TCHAR* Field)
	{
		static const TArray<TSharedPtr<FJsonValue>> Empty;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		return Obj.TryGetArrayField(Field, Arr) && Arr ? *Arr : Empty;
	}
}

FString FDFLevelFile::ContentLevelsDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../content/levels")));
}

FString FDFLevelFile::ResolvePath(const FString& MapId, bool bPreferLegacy, bool* bOutLegacy)
{
	const FString Primary = FPaths::Combine(ContentLevelsDir(), MapId + TEXT(".level.json"));
	const FString Legacy = FPaths::Combine(ContentLevelsDir(), TEXT("legacy"), MapId + TEXT(".level.json"));
	const bool bPrimaryExists = FPaths::FileExists(Primary);
	const bool bLegacyExists = FPaths::FileExists(Legacy);
	if ((bPreferLegacy || !bPrimaryExists) && bLegacyExists)
	{
		if (bOutLegacy) { *bOutLegacy = true; }
		return Legacy;
	}
	if (bPrimaryExists)
	{
		if (bOutLegacy) { *bOutLegacy = false; }
		return Primary;
	}
	return FString();
}

TArray<FString> FDFLevelFile::AllMapIds()
{
	TSet<FString> Ids;
	for (const FString& Dir : { ContentLevelsDir(), FPaths::Combine(ContentLevelsDir(), TEXT("legacy")) })
	{
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *FPaths::Combine(Dir, TEXT("*.level.json")), true, false);
		for (const FString& File : Files)
		{
			Ids.Add(File.LeftChop(FString(TEXT(".level.json")).Len()));
		}
	}
	TArray<FString> Out = Ids.Array();
	Out.Sort();
	return Out;
}

bool FDFLevelFile::Load(const FString& Path, FDFLevelFile& Out, FString& OutError)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		OutError = FString::Printf(TEXT("cannot read %s"), *Path);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = FString::Printf(TEXT("%s is not valid JSON: %s"), *Path, *Reader->GetErrorMessage());
		return false;
	}

	Out = FDFLevelFile();
	Out.SourcePath = Path;
	Out.bLegacy = Path.Contains(TEXT("/legacy/"));
	Out.Schema = Root->GetStringField(TEXT("$schema"));
	Out.Id = FName(*Root->GetStringField(TEXT("id")));
	if (Out.Id.IsNone())
	{
		OutError = FString::Printf(TEXT("%s has no \"id\""), *Path);
		return false;
	}
	Out.TotalWaves = Root->HasTypedField<EJson::Number>(TEXT("totalWaves")) ? Root->GetIntegerField(TEXT("totalWaves")) : 10;
	{
		const TArray<TSharedPtr<FJsonValue>>& Field = ArrayField(*Root, TEXT("field"));
		if (Field.Num() >= 2)
		{
			Out.FieldMeters = FVector2D(Field[0]->AsNumber(), Field[1]->AsNumber());
		}
	}

	// Anchors: top level (legacy) or under "anchors" (MAP-AUTHORING §2.3).
	const TSharedPtr<FJsonObject>* Anchors = nullptr;
	const FJsonObject& AnchorObj = (Root->TryGetObjectField(TEXT("anchors"), Anchors) && Anchors && Anchors->IsValid()) ? **Anchors : *Root;
	// Every vector read fails the load with the record's id: a missing or malformed [x,y,z] must
	// never land silently at the origin (a socket at 0,0,0 validates, covers and misleads).
	if (!ReadVec3(AnchorObj.TryGetField(TEXT("heroSpawn")), Out.HeroSpawn))
	{
		OutError = FString::Printf(TEXT("%s: heroSpawn is missing or not [x,y,z]"), *Path);
		return false;
	}
	if (!ReadVec3(AnchorObj.TryGetField(TEXT("armory")), Out.Armory))
	{
		OutError = FString::Printf(TEXT("%s: armory is missing or not [x,y,z]"), *Path);
		return false;
	}

	// Routes
	for (const TSharedPtr<FJsonValue>& Value : ArrayField(*Root, TEXT("routes")))
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!Value->TryGetObject(Obj) || !Obj)
		{
			OutError = TEXT("routes[] entries must be objects");
			return false;
		}
		FDFLevelRoute Route;
		Route.Id = FName(*(*Obj)->GetStringField(TEXT("id")));
		if (!ReadLayer((*Obj)->GetStringField(TEXT("layer")), Route.Layer))
		{
			OutError = FString::Printf(TEXT("route '%s': unknown layer"), *Route.Id.ToString());
			return false;
		}
		for (const TSharedPtr<FJsonValue>& WP : ArrayField(**Obj, TEXT("waypoints")))
		{
			FVector P;
			if (!ReadVec3(WP, P))
			{
				OutError = FString::Printf(TEXT("route '%s': waypoint is not [x,y,z]"), *Route.Id.ToString());
				return false;
			}
			Route.Waypoints.Add(P);
		}
		if (Route.Waypoints.Num() < 2)
		{
			OutError = FString::Printf(TEXT("route '%s' needs at least two waypoints"), *Route.Id.ToString());
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Leg : ArrayField(**Obj, TEXT("teleportLegs")))
		{
			Route.TeleportLegs.Add(static_cast<int32>(Leg->AsNumber()));
		}
		(*Obj)->TryGetBoolField(TEXT("elevated"), Route.bElevated);
		Out.Routes.Add(MoveTemp(Route));
	}

	// Sockets: {"id","tag","pos"} or ["id", [x,y,z], "tag"]
	for (const TSharedPtr<FJsonValue>& Value : ArrayField(*Root, TEXT("sockets")))
	{
		FDFLevelSocket Socket;
		FString TagText;
		TSharedPtr<FJsonValue> Pos;
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Value->TryGetObject(Obj) && Obj)
		{
			Socket.Id = FName(*(*Obj)->GetStringField(TEXT("id")));
			TagText = (*Obj)->GetStringField(TEXT("tag"));
			Pos = (*Obj)->TryGetField(TEXT("pos"));
			(*Obj)->TryGetBoolField(TEXT("pad"), Socket.bPad);
		}
		else if (Value->TryGetArray(Arr) && Arr && Arr->Num() >= 3)
		{
			Socket.Id = FName(*(*Arr)[0]->AsString());
			Pos = (*Arr)[1];
			TagText = (*Arr)[2]->AsString();
		}
		else
		{
			OutError = FString::Printf(TEXT("sockets[%d] is neither {\"id\",\"tag\",\"pos\"} nor [\"id\", [x,y,z], \"tag\"]"), Out.Sockets.Num());
			return false;
		}
		if (Socket.Id.IsNone() || !ReadSocketTag(TagText, Socket.Tag))
		{
			OutError = FString::Printf(TEXT("socket '%s': bad id or tag '%s'"), *Socket.Id.ToString(), *TagText);
			return false;
		}
		if (!ReadVec3(Pos, Socket.Position))
		{
			OutError = FString::Printf(TEXT("socket '%s': pos is missing or not [x,y,z]"), *Socket.Id.ToString());
			return false;
		}
		Out.Sockets.Add(Socket);
	}

	// Stations: {"id","pos"} or ["id", [x,y,z]] — top level (legacy) or anchors.stations
	for (const TSharedPtr<FJsonValue>& Value : ArrayField(AnchorObj, TEXT("stations")))
	{
		FDFLevelStation Station;
		TSharedPtr<FJsonValue> Pos;
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Value->TryGetObject(Obj) && Obj)
		{
			Station.Id = FName(*(*Obj)->GetStringField(TEXT("id")));
			Pos = (*Obj)->TryGetField(TEXT("pos"));
		}
		else if (Value->TryGetArray(Arr) && Arr && Arr->Num() >= 2)
		{
			Station.Id = FName(*(*Arr)[0]->AsString());
			Pos = (*Arr)[1];
		}
		else
		{
			OutError = FString::Printf(TEXT("stations[%d] is neither {\"id\",\"pos\"} nor [\"id\", [x,y,z]]"), Out.Stations.Num());
			return false;
		}
		if (Station.Id.IsNone())
		{
			OutError = FString::Printf(TEXT("stations[%d] has no id"), Out.Stations.Num());
			return false;
		}
		if (!ReadVec3(Pos, Station.Position))
		{
			OutError = FString::Printf(TEXT("station '%s': pos is missing or not [x,y,z]"), *Station.Id.ToString());
			return false;
		}
		Out.Stations.Add(Station);
	}

	// Conditions: {"8": "fog"} under conditionSchedule (legacy) or conditions
	for (const TCHAR* Key : { TEXT("conditionSchedule"), TEXT("conditions") })
	{
		const TSharedPtr<FJsonObject>* Sched = nullptr;
		if (Root->TryGetObjectField(Key, Sched) && Sched && Sched->IsValid())
		{
			for (const auto& Pair : (*Sched)->Values)
			{
				Out.ConditionSchedule.Add(FCString::Atoi(*Pair.Key), FName(*Pair.Value->AsString()));
			}
		}
	}

	// Vehicles: {"id","defId","pos","yawDegrees"} or ["id","def",[x,y,z],yaw]
	for (const TSharedPtr<FJsonValue>& Value : ArrayField(*Root, TEXT("vehicles")))
	{
		FDFLevelVehicle Vehicle;
		float SimYaw = 0.f;
		TSharedPtr<FJsonValue> Pos;
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Value->TryGetObject(Obj) && Obj)
		{
			Vehicle.Id = FName(*(*Obj)->GetStringField(TEXT("id")));
			Vehicle.DefId = FName(*(*Obj)->GetStringField(TEXT("defId")));
			Pos = (*Obj)->TryGetField(TEXT("pos"));
			SimYaw = (*Obj)->HasField(TEXT("yawDegrees")) ? (*Obj)->GetNumberField(TEXT("yawDegrees")) : 0.f;
		}
		else if (Value->TryGetArray(Arr) && Arr && Arr->Num() >= 4)
		{
			Vehicle.Id = FName(*(*Arr)[0]->AsString());
			Vehicle.DefId = FName(*(*Arr)[1]->AsString());
			Pos = (*Arr)[2];
			SimYaw = (*Arr)[3]->AsNumber();
		}
		else
		{
			OutError = FString::Printf(TEXT("vehicles[%d] is neither {\"id\",\"defId\",\"pos\",\"yawDegrees\"} nor [\"id\",\"def\",[x,y,z],yaw]"), Out.Vehicles.Num());
			return false;
		}
		if (Vehicle.Id.IsNone())
		{
			OutError = FString::Printf(TEXT("vehicles[%d] has no id"), Out.Vehicles.Num());
			return false;
		}
		if (!ReadVec3(Pos, Vehicle.Position))
		{
			OutError = FString::Printf(TEXT("vehicle '%s': pos is missing or not [x,y,z]"), *Vehicle.Id.ToString());
			return false;
		}
		Vehicle.Yaw = FDFSimFrame::YawToUnreal(SimYaw);
		Out.Vehicles.Add(Vehicle);
	}

	// Lane node names, gates, levers (all object form in both formats)
	for (const TSharedPtr<FJsonValue>& Value : ArrayField(*Root, TEXT("laneNodeNames")))
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!Value->TryGetObject(Obj) || !Obj)
		{
			OutError = FString::Printf(TEXT("laneNodeNames[%d] is not an object"), Out.LaneNodeNames.Num());
			return false;
		}
		FDFLevelNodeName Name;
		Name.Id = FName(*(*Obj)->GetStringField(TEXT("id")));
		// A node id becomes the first half of a warp gate's "<node>@<edge>" key, and
		// ADFWarpGate::SplitStableId splits at the first '@' — so a node id may not carry one.
		if (Name.Id.ToString().Contains(TEXT("@"), ESearchCase::CaseSensitive))
		{
			OutError = FString::Printf(TEXT("laneNodeNames '%s': a node id may not contain '@' (warp gate keys are \"<node>@<edge>\")"), *Name.Id.ToString());
			return false;
		}
		if (!ReadVec3((*Obj)->TryGetField(TEXT("at")), Name.At))
		{
			OutError = FString::Printf(TEXT("laneNodeNames '%s': at is missing or not [x,y,z]"), *Name.Id.ToString());
			return false;
		}
		Out.LaneNodeNames.Add(Name);
	}
	for (const TSharedPtr<FJsonValue>& Value : ArrayField(*Root, TEXT("laneGates")))
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (Value->TryGetObject(Obj) && Obj)
		{
			FDFLevelLaneGate Gate;
			Gate.EdgeId = FName(*(*Obj)->GetStringField(TEXT("edgeId")));
			Gate.SocketId = FName(*(*Obj)->GetStringField(TEXT("socketId")));
			Out.LaneGates.Add(Gate);
		}
	}
	for (const TSharedPtr<FJsonValue>& Value : ArrayField(*Root, TEXT("operatedGates")))
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!Value->TryGetObject(Obj) || !Obj)
		{
			OutError = FString::Printf(TEXT("operatedGates[%d] is not an object"), Out.OperatedGates.Num());
			return false;
		}
		FDFLevelOperatedGate Lever;
		Lever.Id = FName(*(*Obj)->GetStringField(TEXT("id")));
		Lever.EdgeId = FName(*(*Obj)->GetStringField(TEXT("edgeId")));
		if (!ReadVec3((*Obj)->TryGetField(TEXT("at")), Lever.At))
		{
			OutError = FString::Printf(TEXT("operatedGate '%s': at is missing or not [x,y,z]"), *Lever.Id.ToString());
			return false;
		}
		Lever.Label = (*Obj)->GetStringField(TEXT("label"));
		Out.OperatedGates.Add(Lever);
	}
	return true;
}
