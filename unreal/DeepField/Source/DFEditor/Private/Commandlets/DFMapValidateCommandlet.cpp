#include "Commandlets/DFMapValidateCommandlet.h"

#include "Commandlets/DFLevelImportCommandlet.h"
#include "DFWorldCollision.h"
#include "DFWorldSubsystem.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "LaneGraph/DFLaneGraphAsset.h"
#include "LaneGraph/DFLevelFile.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFMapValidate, Log, All);

namespace
{
	constexpr float MaxClosableEdges = 8;
	constexpr float SpawnApronCm = 800.f;
	constexpr float SocketOffLaneCm = 350.f;
	constexpr float SegmentCm = 400.f;
	constexpr float GroundRangeCm = 1600.f;   // Nova, the longest ground tower at L1
	constexpr float AirRangeCm = 1500.f;      // Skywatch
	constexpr float EyeHeightCm = 160.f;      // "muzzle height" on the pad; a 1.6 m target on the lane
	constexpr int32 CoverageRequired = 3;

	const TCHAR* StatusText(int32 Status)
	{
		switch (Status)
		{
		case 0: return TEXT("PASS");
		case 1: return TEXT("FAIL");
		case 2: return TEXT("WARN");
		default: return TEXT("SKIP");
		}
	}

	bool IsTowerPad(const FDFSocketDef& Socket)
	{
		// Traps and barricade slots host no tower; they cover nothing.
		return Socket.Tag == EDFSocketTag::Ground || Socket.Tag == EDFSocketTag::Wall;
	}
}

UDFMapValidateCommandlet::UDFMapValidateCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	ShowErrorCount = true;
}

FString UDFMapValidateCommandlet::BaselinePath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../map-validation-baseline.tsv")));
}

FString UDFMapValidateCommandlet::ReportPath(const FString& MapId)
{
	return FPaths::Combine(FDFLevelFile::ContentLevelsDir(), TEXT("reports"), MapId + TEXT(".coverage.json"));
}

TSet<FString> UDFMapValidateCommandlet::LoadBaseline()
{
	TSet<FString> Out;
	TArray<FString> Lines;
	if (FFileHelper::LoadFileToStringArray(Lines, *BaselinePath()))
	{
		for (const FString& Line : Lines)
		{
			if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
			{
				continue;
			}
			TArray<FString> Cols;
			Line.ParseIntoArray(Cols, TEXT("\t"), false);
			if (Cols.Num() >= 2)
			{
				Out.Add(Cols[0].TrimStartAndEnd().ToLower() + TEXT("\t") + Cols[1].TrimStartAndEnd());
			}
		}
	}
	return Out;
}

int32 UDFMapValidateCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Named;
	ParseCommandLine(*Params, Tokens, Switches, Named);
	const bool bAll = Switches.ContainsByPredicate([](const FString& S) { return S.Equals(TEXT("all"), ESearchCase::IgnoreCase); });
	const FString* MapParam = Named.Find(TEXT("map"));

	TArray<FString> Maps;
	if (bAll)
	{
		Maps = FDFLevelFile::AllMapIds();
	}
	else if (MapParam && !MapParam->IsEmpty())
	{
		MapParam->ParseIntoArray(Maps, TEXT(","), true);
	}
	if (Maps.Num() == 0)
	{
		UE_LOG(LogDFMapValidate, Error, TEXT("usage: -run=DFMapValidate -map=<id>[,<id>] | -all"));
		return 1;
	}

	const TSet<FString> Baseline = LoadBaseline();
	int32 Errors = 0;
	for (const FString& RawMap : Maps)
	{
		const FString MapId = RawMap.ToLower();
		TArray<FResult> Results;
		if (!ValidateMap(MapId, Results))
		{
			++Errors;
			continue;
		}
		int32 Pass = 0, Fail = 0, Warn = 0, Skip = 0;
		for (FResult& R : Results)
		{
			if (R.Status == EStatus::Fail && Baseline.Contains(MapId + TEXT("\t") + R.Rule))
			{
				R.Status = EStatus::Warn;
				R.Notes.Add(TEXT("(in map-validation-baseline.tsv: warning, not error)"));
			}
			switch (R.Status)
			{
			case EStatus::Pass: ++Pass; break;
			case EStatus::Fail: ++Fail; break;
			case EStatus::Warn: ++Warn; break;
			case EStatus::Skip: ++Skip; break;
			}
			UE_LOG(LogDFMapValidate, Display, TEXT("[%s] %s/%s"), StatusText(static_cast<int32>(R.Status)), *MapId, *R.Rule);
			for (const FString& Note : R.Notes)
			{
				UE_LOG(LogDFMapValidate, Display, TEXT("        %s"), *Note);
			}
		}
		UE_LOG(LogDFMapValidate, Display, TEXT("DFMapValidate %s: %d pass, %d fail, %d warn, %d skip"), *MapId, Pass, Fail, Warn, Skip);
		Errors += Fail;
	}
	return Errors == 0 ? 0 : 1;
}

bool UDFMapValidateCommandlet::ValidateMap(const FString& MapId, TArray<FResult>& OutResults)
{
	FString MapName = MapId;
	MapName[0] = FChar::ToUpper(MapName[0]);
	const FString PersistentPackage = FString::Printf(TEXT("/Game/DF/Maps/%s/L_%s"), *MapName, *MapName);
	if (!FPackageName::DoesPackageExist(PersistentPackage))
	{
		UE_LOG(LogDFMapValidate, Error, TEXT("%s: %s does not exist — run -run=DFLevelImport -map=%s first"), *MapId, *PersistentPackage, *MapId);
		return false;
	}
	UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(PersistentPackage);
	if (!World)
	{
		UE_LOG(LogDFMapValidate, Error, TEXT("%s: could not load %s"), *MapId, *PersistentPackage);
		return false;
	}
	World->FlushLevelStreaming();

	UDFWorldSubsystem* Subsystem = World->GetSubsystem<UDFWorldSubsystem>();
	UDFLaneGraphAsset* Graph = Subsystem ? Subsystem->GetLaneGraph() : nullptr;
	if (!Graph)
	{
		UE_LOG(LogDFMapValidate, Error, TEXT("%s: no lane graph in %s (no ADFLaneGraphInfo, no DA_LaneGraph_%s)"), *MapId, *PersistentPackage, *MapName);
		return false;
	}
	const int32 Placed = Subsystem->GetSockets().Num();
	if (Placed != Graph->Sockets.Num())
	{
		UE_LOG(LogDFMapValidate, Warning, TEXT("%s: %d sockets placed in the world, %d in the lane graph — re-import"), *MapId, Placed, Graph->Sockets.Num());
	}

	OutResults.Add(CheckSealing(*Graph));
	OutResults.Add(CheckSpawnApron(*Graph));
	OutResults.Add(CheckSocketOffset(*Graph));
	OutResults.Add(CheckCorridor(*Graph));
	OutResults.Add(CheckCoverage(World, *Graph, MapId));
	OutResults.Add(CheckRouteIds(*Graph, MapId));
	return true;
}

FString UDFMapValidateCommandlet::WaveTablePath(const FString& MapId)
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../content/json"), FString::Printf(TEXT("waves_%s.json"), *MapId)));
}

UDFMapValidateCommandlet::FResult UDFMapValidateCommandlet::CheckRouteIds(const UDFLaneGraphAsset& Graph, const FString& MapId)
{
	// CONTRACTS/map-authoring-3d.md "Route ids are referenced by the wave tables": every routeId in
	// waves_<map>.json names an itinerary in the lane graph. Read from the JSON (the source of truth,
	// ADR-0005), not the imported DataTable, so a stale DT_Waves cannot hide a rename.
	FResult R;
	R.Rule = TEXT("routeIds");
	const FString WavePath = WaveTablePath(MapId);
	const FString LevelPath = FDFLevelFile::ResolvePath(MapId, /*bPreferLegacy*/ false);
	const FString LevelName = LevelPath.IsEmpty() ? MapId + TEXT(".level.json") : FPaths::GetCleanFilename(LevelPath);
	if (!FPaths::FileExists(WavePath))
	{
		R.Status = EStatus::Skip;
		R.Notes.Add(FString::Printf(TEXT("no wave table at %s"), *WavePath));
		return R;
	}

	FString Text;
	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	const bool bRead = FFileHelper::LoadFileToString(Text, *WavePath);
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!bRead || !FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()
		|| !Root->TryGetArrayField(TEXT("rows"), Rows) || !Rows)
	{
		R.Status = EStatus::Fail;
		R.Notes.Add(FString::Printf(TEXT("%s is not a readable wave table (no \"rows\" array)"), *WavePath));
		return R;
	}

	TSet<FName> Checked;
	for (const TSharedPtr<FJsonValue>& Value : *Rows)
	{
		const TSharedPtr<FJsonObject>* Row = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Row) || !Row) { continue; }
		FString RowId;
		FString RouteText;
		(*Row)->TryGetStringField(TEXT("id"), RowId);
		(*Row)->TryGetStringField(TEXT("routeId"), RouteText);
		const FName RouteId(*RouteText);
		if (RouteId.IsNone())
		{
			R.Status = EStatus::Fail;
			R.Notes.Add(FString::Printf(TEXT("waves_%s.json row %s has no routeId"), *MapId, *RowId));
			continue;
		}
		Checked.Add(RouteId);
		if (!Graph.FindItinerary(RouteId))
		{
			R.Status = EStatus::Fail;
			R.Notes.Add(FString::Printf(TEXT("waves_%s.json row %s names route '%s', which is no itinerary in %s — rename it in both files in the same PR"),
				*MapId, *RowId, *RouteId.ToString(), *LevelName));
		}
	}
	TArray<FString> Names;
	for (const FName& Id : Checked) { Names.Add(Id.ToString()); }
	Names.Sort();
	R.Notes.Insert(FString::Printf(TEXT("%d row(s), route(s) {%s} against %d itinerary(ies) in %s"),
		Rows->Num(), *FString::Join(Names, TEXT(", ")), Graph.Itineraries.Num(), *LevelName), 0);
	return R;
}

UDFMapValidateCommandlet::FResult UDFMapValidateCommandlet::CheckSealing(const UDFLaneGraphAsset& Graph)
{
	FResult R;
	R.Rule = TEXT("sealing");
	const TArray<FName> Closable = Graph.ClosableEdges();
	R.Notes.Add(FString::Printf(TEXT("%d closable edge(s)"), Closable.Num()));
	if (!Graph.EverySpawnReachesCore())
	{
		R.Status = EStatus::Fail;
		R.Notes.Add(TEXT("a spawn cannot reach the core with everything open"));
		return R;
	}
	if (Closable.Num() > MaxClosableEdges)
	{
		R.Status = EStatus::Fail;
		R.Notes.Add(FString::Printf(TEXT("over the eight-gate cap (%d)"), Closable.Num()));
		return R;
	}
	// Rule 11 as RFC-0001 rules it (it follows the sim): a closable SET may seal — the runtime
	// refuses the last closure — so the validator REPORTS sealing combinations and FAILS only a
	// single-edge seal or a break in monotonic reachability.
	// Every subset, as Sim.Harness gate 57 enumerates "every configuration a map allows". A
	// combination that seals is legal content as long as the runtime refuses its LAST closure
	// (Switchyard's b1+b2 is exactly that: gate 22c expects the second barricade to be refused),
	// so those are reported, not failed. What fails: a single closable edge that seals on its own
	// (a gate that can never be shut is inert content), and a break in monotonicity (opening a
	// gate from a legal configuration must never make the map less connected — that is what lets
	// a player always get back to neutral, rule 12 of MAP-AUTHORING §4).
	const int32 Configurations = 1 << Closable.Num();
	TArray<bool> Legal;
	Legal.SetNum(Configurations);
	for (int32 Mask = 0; Mask < Configurations; ++Mask)
	{
		TSet<FName> Closed;
		for (int32 g = 0; g < Closable.Num(); ++g)
		{
			if (Mask & (1 << g)) { Closed.Add(Closable[g]); }
		}
		Legal[Mask] = !Graph.WouldSeal(Closed);
		if (!Legal[Mask])
		{
			TArray<FString> Names;
			for (const FName& N : Closed) { Names.Add(N.ToString()); }
			R.Notes.Add(FString::Printf(TEXT("closing {%s} would seal a spawn from the core: the runtime refuses the last of them"), *FString::Join(Names, TEXT(", "))));
		}
	}
	for (int32 g = 0; g < Closable.Num(); ++g)
	{
		if (!Legal[1 << g])
		{
			R.Status = EStatus::Fail;
			R.Notes.Add(FString::Printf(TEXT("%s can never be closed: closing it alone seals"), *Closable[g].ToString()));
		}
	}
	for (int32 Mask = 0; Mask < Configurations; ++Mask)
	{
		if (!Legal[Mask]) { continue; }
		for (int32 g = 0; g < Closable.Num(); ++g)
		{
			if ((Mask & (1 << g)) && !Legal[Mask & ~(1 << g)])
			{
				R.Status = EStatus::Fail;
				R.Notes.Add(FString::Printf(TEXT("opening %s from a legal configuration made the map less connected"), *Closable[g].ToString()));
			}
		}
	}
	return R;
}

UDFMapValidateCommandlet::FResult UDFMapValidateCommandlet::CheckSpawnApron(const UDFLaneGraphAsset& Graph)
{
	FResult R;
	R.Rule = TEXT("spawnApron");
	for (const FDFLaneNode& Node : Graph.Nodes)
	{
		if (Node.Kind != EDFLaneNodeKind::Spawn) { continue; }
		for (const FDFSocketDef& Socket : Graph.Sockets)
		{
			const float D = FVector::Dist(Socket.Position, Node.Position);
			if (D < SpawnApronCm)
			{
				R.Status = EStatus::Fail;
				R.Notes.Add(FString::Printf(TEXT("socket %s is %.1f m from spawn %s (< 8 m)"), *Socket.SocketId.ToString(), D / 100.f, *Node.Id.ToString()));
			}
		}
	}
	return R;
}

float UDFMapValidateCommandlet::DistanceToPolylineCm(const FVector& P, const TArray<FVector>& Points)
{
	float Best = TNumericLimits<float>::Max();
	for (int32 i = 0; i + 1 < Points.Num(); ++i)
	{
		Best = FMath::Min(Best, static_cast<float>(FVector::Dist(P, FMath::ClosestPointOnSegment(P, Points[i], Points[i + 1]))));
	}
	return Best;
}

FVector UDFMapValidateCommandlet::PointAlongPolyline(const TArray<FVector>& Points, double DistanceCm)
{
	double Walked = 0.0;
	for (int32 i = 0; i + 1 < Points.Num(); ++i)
	{
		const double Leg = FVector::Dist(Points[i], Points[i + 1]);
		if (Walked + Leg >= DistanceCm || i + 2 == Points.Num())
		{
			const double T = Leg > 0.0 ? FMath::Clamp((DistanceCm - Walked) / Leg, 0.0, 1.0) : 0.0;
			return FMath::Lerp(Points[i], Points[i + 1], T);
		}
		Walked += Leg;
	}
	return Points.Num() > 0 ? Points.Last() : FVector::ZeroVector;
}

UDFMapValidateCommandlet::FResult UDFMapValidateCommandlet::CheckSocketOffset(const UDFLaneGraphAsset& Graph)
{
	FResult R;
	R.Rule = TEXT("socketOffset");
	for (const FDFSocketDef& Socket : Graph.Sockets)
	{
		if (!IsTowerPad(Socket)) { continue; }   // traps and barricades are laid IN the lane by design
		float Nearest = TNumericLimits<float>::Max();
		FName NearestEdge;
		for (const FDFLaneEdge& Edge : Graph.Edges)
		{
			if (Edge.IsWarp()) { continue; }
			const float D = DistanceToPolylineCm(Socket.Position, Edge.Waypoints);
			if (D < Nearest)
			{
				Nearest = D;
				NearestEdge = Edge.Id;
			}
		}
		if (Nearest < SocketOffLaneCm)
		{
			R.Status = EStatus::Fail;
			R.Notes.Add(FString::Printf(TEXT("socket %s is %.2f m off %s (< 3.5 m)"), *Socket.SocketId.ToString(), Nearest / 100.f, *NearestEdge.ToString()));
		}
	}
	return R;
}

UDFMapValidateCommandlet::FResult UDFMapValidateCommandlet::CheckCorridor(const UDFLaneGraphAsset& Graph)
{
	FResult R;
	R.Rule = TEXT("corridor");
	R.Status = EStatus::Skip;
	R.Notes.Add(TEXT("no navmesh: the 3.4 m corridor walk (rule 2) needs a baked navmesh and a Lane area class; not checked, not passed"));
	return R;
}

UDFMapValidateCommandlet::FResult UDFMapValidateCommandlet::CheckCoverage(UWorld* World, const UDFLaneGraphAsset& Graph, const FString& MapId)
{
	FResult R;
	R.Rule = TEXT("coverage");

	TArray<FSegment> Segments;
	for (const FDFLaneEdge& Edge : Graph.Edges)
	{
		if (Edge.IsWarp() || Edge.Waypoints.Num() < 2) { continue; }
		const double LengthCm = Edge.LengthMeters * 100.0;
		if (LengthCm <= 0.0) { continue; }
		const FDFLaneNode* From = Graph.FindNode(Edge.From);
		const bool bFromSpawn = From && From->Kind == EDFLaneNodeKind::Spawn;
		const int32 Count = FMath::Max(1, FMath::CeilToInt(LengthCm / SegmentCm));
		for (int32 k = 0; k < Count; ++k)
		{
			const double D = FMath::Min((k + 0.5) * SegmentCm, LengthCm);
			FSegment S;
			S.Edge = Edge.Id;
			S.T = static_cast<float>(D / LengthCm);
			S.Position = PointAlongPolyline(Edge.Waypoints, D);
			S.bAir = Edge.Layer == EDFEnemyLayer::Air;
			// The first 8 m after a gate or an arrival pad is spawn apron: nothing has to cover it.
			S.bApron = bFromSpawn && D < SpawnApronCm;
			Segments.Add(S);
		}
	}

	int32 Traces = 0;
	int32 Blocked = 0;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DFMapValidateSight), /*bTraceComplex*/ false);
	for (FSegment& S : Segments)
	{
		const float Range = S.bAir ? AirRangeCm : GroundRangeCm;
		const FVector Target = S.Position + FVector(0.f, 0.f, EyeHeightCm);
		for (const FDFSocketDef& Socket : Graph.Sockets)
		{
			if (!IsTowerPad(Socket)) { continue; }
			// Air-capable = every pad for now; the tower table's TargetLayers decides later.
			if (FVector::Dist(Socket.Position, S.Position) > Range) { continue; }
			const FVector Muzzle = Socket.Position + FVector(0.f, 0.f, EyeHeightCm);
			FHitResult Hit;
			++Traces;
			if (World->LineTraceSingleByChannel(Hit, Muzzle, Target, DFCollision::Sight, Params))
			{
				++Blocked;
				continue;
			}
			S.Covering.Add(Socket.SocketId);
		}
	}

	// The report: every segment, the dead ones, and what each socket can see. It is committed
	// beside the level file, so nothing in it may change between two runs on the same input — no
	// timestamp; git's own history says when it was produced.
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("map"), MapId);
	Root->SetNumberField(TEXT("segmentMeters"), SegmentCm / 100.f);
	Root->SetNumberField(TEXT("required"), CoverageRequired);
	TArray<TSharedPtr<FJsonValue>> SegmentValues;
	TArray<TSharedPtr<FJsonValue>> DeadValues;
	TMap<FName, TArray<TSharedPtr<FJsonValue>>> PerSocket;
	int32 Dead = 0;
	int32 Apron = 0;
	for (const FSegment& S : Segments)
	{
		const FString Key = FString::Printf(TEXT("%s@%.3f"), *S.Edge.ToString(), S.T);
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("edge"), S.Edge.ToString());
		Obj->SetNumberField(TEXT("t"), S.T);
		Obj->SetStringField(TEXT("layer"), S.bAir ? TEXT("air") : TEXT("ground"));
		Obj->SetBoolField(TEXT("apron"), S.bApron);
		const FVector Sim = FDFSimFrame::ToSim(S.Position);
		Obj->SetArrayField(TEXT("positionMetres"), { MakeShared<FJsonValueNumber>(Sim.X), MakeShared<FJsonValueNumber>(Sim.Y), MakeShared<FJsonValueNumber>(Sim.Z) });
		TArray<TSharedPtr<FJsonValue>> Covering;
		for (const FName& Id : S.Covering)
		{
			Covering.Add(MakeShared<FJsonValueString>(Id.ToString()));
			PerSocket.FindOrAdd(Id).Add(MakeShared<FJsonValueString>(Key));
		}
		Obj->SetArrayField(TEXT("coveringSockets"), Covering);
		SegmentValues.Add(MakeShared<FJsonValueObject>(Obj));

		if (S.bApron)
		{
			++Apron;
		}
		else if (S.Covering.Num() < CoverageRequired)
		{
			++Dead;
			TSharedRef<FJsonObject> DeadObj = MakeShared<FJsonObject>();
			DeadObj->SetStringField(TEXT("edge"), S.Edge.ToString());
			DeadObj->SetNumberField(TEXT("t"), S.T);
			DeadObj->SetNumberField(TEXT("count"), S.Covering.Num());
			DeadValues.Add(MakeShared<FJsonValueObject>(DeadObj));
			if (R.Notes.Num() < 12)
			{
				R.Notes.Add(FString::Printf(TEXT("dead ground: %s t=%.2f (%s) sees %d pad(s)"), *S.Edge.ToString(), S.T, S.bAir ? TEXT("air") : TEXT("ground"), S.Covering.Num()));
			}
		}
	}
	Root->SetArrayField(TEXT("segments"), SegmentValues);
	Root->SetArrayField(TEXT("deadSegments"), DeadValues);
	TSharedRef<FJsonObject> PerSocketObj = MakeShared<FJsonObject>();
	for (const FDFSocketDef& Socket : Graph.Sockets)
	{
		if (!IsTowerPad(Socket)) { continue; }
		const TArray<TSharedPtr<FJsonValue>>* Seen = PerSocket.Find(Socket.SocketId);
		PerSocketObj->SetArrayField(Socket.SocketId.ToString(), Seen ? *Seen : TArray<TSharedPtr<FJsonValue>>());
		if (!Seen)
		{
			R.Notes.Add(FString::Printf(TEXT("socket %s covers nothing (rule 6)"), *Socket.SocketId.ToString()));
		}
	}
	Root->SetObjectField(TEXT("perSocket"), PerSocketObj);

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root, Writer);
	const FString Path = ReportPath(MapId);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	// The report is text (never LFS-lockable), but the validator writes nothing else, and a
	// read-only report must not fail the rule silently: clear the bit the same way the importer does.
	if (!UDFLevelImportCommandlet::MakeWritable(Path, TEXT("the dead-ground report")) || !FFileHelper::SaveStringToFile(Json, *Path))
	{
		R.Status = EStatus::Fail;
		R.Notes.Add(FString::Printf(TEXT("could not write %s"), *Path));
		return R;
	}

	R.Notes.Insert(FString::Printf(TEXT("%d segments (%d apron), %d dead, %d sight traces (%d blocked); report %s"),
		Segments.Num(), Apron, Dead, Traces, Blocked, *Path), 0);
	if (Dead > 0)
	{
		R.Status = EStatus::Fail;
	}
	return R;
}
