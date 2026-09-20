#include "Commandlets/DFLevelImportCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DFWorldCollision.h"
#include "Editor.h"
#include "EditorLevelUtils.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/PlayerStart.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "LaneGraph/DFLaneGraphAsset.h"
#include "LaneGraph/DFLaneGraphBuilder.h"
#include "LaneGraph/DFLevelFile.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "World/DFCore.h"
#include "World/DFHeroStation.h"
#include "World/DFLaneGate.h"
#include "World/DFLaneGraphInfo.h"
#include "World/DFOperatedGate.h"
#include "World/DFSocket.h"
#include "World/DFSpawnPortal.h"
#include "World/DFVehicleSpawn.h"
#include "World/DFWarpGate.h"
#include "World/DFWorldActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFLevelImport, Log, All);

namespace
{
	constexpr float FloorSizeMeters = 400.f;
	constexpr float PlayerStartLiftCm = 110.f;   // capsule half height + a little

	FString PlacementKey(const UClass* Class, FName Id)
	{
		return Class->GetName() + TEXT("/") + Id.ToString();
	}
}

UDFLevelImportCommandlet::UDFLevelImportCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	ShowErrorCount = true;
}

FString UDFLevelImportCommandlet::MapDisplayName(const FString& MapId)
{
	// foundry -> Foundry: the asset and level names (L_Foundry, DA_LaneGraph_Foundry).
	FString Name = MapId;
	if (Name.Len() > 0)
	{
		Name[0] = FChar::ToUpper(Name[0]);
	}
	return Name;
}

int32 UDFLevelImportCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Named;
	ParseCommandLine(*Params, Tokens, Switches, Named);

	const bool bAll = Switches.ContainsByPredicate([](const FString& S) { return S.Equals(TEXT("all"), ESearchCase::IgnoreCase); });
	const bool bLegacy = Switches.ContainsByPredicate([](const FString& S) { return S.Equals(TEXT("legacy"), ESearchCase::IgnoreCase); });
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
		UE_LOG(LogDFLevelImport, Error, TEXT("usage: -run=DFLevelImport -map=<id>[,<id>] | -all  [-legacy]   (levels in %s)"), *FDFLevelFile::ContentLevelsDir());
		return 1;
	}

	int32 Failed = 0;
	for (const FString& MapId : Maps)
	{
		FString Error;
		if (ImportMap(MapId.ToLower(), bLegacy, Error))
		{
			UE_LOG(LogDFLevelImport, Display, TEXT("imported %s"), *MapId);
		}
		else
		{
			UE_LOG(LogDFLevelImport, Error, TEXT("import of %s failed: %s"), *MapId, *Error);
			++Failed;
		}
	}
	UE_LOG(LogDFLevelImport, Display, TEXT("DFLevelImport: %d map(s), %d failed"), Maps.Num(), Failed);
	return Failed == 0 ? 0 : 1;
}

bool UDFLevelImportCommandlet::ImportMap(const FString& MapId, bool bPreferLegacy, FString& OutError)
{
	bool bLegacyUsed = false;
	const FString Path = FDFLevelFile::ResolvePath(MapId, bPreferLegacy, &bLegacyUsed);
	if (Path.IsEmpty())
	{
		OutError = FString::Printf(TEXT("no level file for '%s' in %s (or its legacy/)"), *MapId, *FDFLevelFile::ContentLevelsDir());
		return false;
	}
	FDFLevelFile Level;
	if (!FDFLevelFile::Load(Path, Level, OutError))
	{
		return false;
	}
	UE_LOG(LogDFLevelImport, Display, TEXT("%s: reading %s%s (%d routes, %d sockets, %d stations, %d vehicles)"),
		*MapId, *Path, bLegacyUsed ? TEXT(" [legacy]") : TEXT(""), Level.Routes.Num(), Level.Sockets.Num(), Level.Stations.Num(), Level.Vehicles.Num());

	const FString MapName = MapDisplayName(MapId);
	const FString PersistentPackage = FString::Printf(TEXT("/Game/DF/Maps/%s/L_%s"), *MapName, *MapName);
	const FString GameplayPackage = PersistentPackage + TEXT("_Gameplay");
	const FString AssetName = FString::Printf(TEXT("DA_LaneGraph_%s"), *MapName);
	const FString AssetPackage = TEXT("/Game/DF/Data/Defs/LaneGraphs/") + AssetName;

	// 1. The world: the persistent level (created once) with the gameplay sublevel in it.
	bool bPersistentCreated = false;
	UWorld* World = LoadOrCreatePersistentLevel(PersistentPackage, Level, bPersistentCreated);
	if (!World)
	{
		OutError = FString::Printf(TEXT("could not load or create %s"), *PersistentPackage);
		return false;
	}
	bool bSublevelAdded = false;
	ULevel* GameplayLevel = FindOrAddGameplaySublevel(World, GameplayPackage, bSublevelAdded);
	if (!GameplayLevel)
	{
		OutError = FString::Printf(TEXT("could not create or load the gameplay sublevel %s"), *GameplayPackage);
		return false;
	}

	// 2. The lane graph, derived from the routes and projected onto the ground.
	bool bAssetCreated = false;
	UDFLaneGraphAsset* Asset = FindOrCreateLaneGraphAsset(AssetPackage, AssetName, bAssetCreated);
	if (!Asset)
	{
		OutError = FString::Printf(TEXT("could not create %s"), *AssetPackage);
		return false;
	}
	Asset->Modify();
	if (!UDFLaneGraphBuilder::BuildFromLevel(Level, *Asset, OutError))
	{
		return false;
	}
	ProjectAsset(World, Level, *Asset);
	for (const FString& Miss : Asset->NearMisses)
	{
		UE_LOG(LogDFLevelImport, Warning, TEXT("%s: near miss — %s"), *MapId, *Miss);
	}
	UE_LOG(LogDFLevelImport, Display, TEXT("%s: lane graph %d nodes, %d edges, %d itineraries, %d gates, %d levers"),
		*MapId, Asset->Nodes.Num(), Asset->Edges.Num(), Asset->Itineraries.Num(), Asset->Gates.Num(), Asset->OperatedGates.Num());

	// 3. The actors, keyed by id.
	FPlacement P;
	P.World = World;
	P.Level = GameplayLevel;
	TArray<ADFWorldActor*> Duplicates;
	for (AActor* Actor : GameplayLevel->Actors)
	{
		if (ADFWorldActor* Placed = Cast<ADFWorldActor>(Actor))
		{
			const FString Key = PlacementKey(Placed->GetClass(), Placed->GetStableId());
			if (P.Existing.Contains(Key))
			{
				// Two actors in the level answer to one key: a level saved under an older keying
				// (warp gates by node id alone) or a hand-made duplicate. Only one can be the one
				// a re-import updates; the other could never be found by its id again, so it is
				// removed with the stale ones — said out loud, never dropped from the map silently.
				UE_LOG(LogDFLevelImport, Warning, TEXT("%s: two actors carry the key %s; %s is removed as a duplicate"), *MapId, *Key, *Placed->GetName());
				Duplicates.Add(Placed);
				continue;
			}
			P.Existing.Add(Key, Placed);
		}
	}
	PlaceActors(P, Level, *Asset);
	if (P.Collisions > 0)
	{
		OutError = FString::Printf(TEXT("%d placement key(s) used twice in one import (see the errors above); nothing saved"), P.Collisions);
		return false;
	}
	for (ADFWorldActor* Duplicate : Duplicates)
	{
		World->EditorDestroyActor(Duplicate, true);
		++P.Removed;
	}
	for (const auto& Pair : P.Existing)
	{
		if (!P.Touched.Contains(Pair.Value))
		{
			UE_LOG(LogDFLevelImport, Display, TEXT("%s: removing %s (id no longer in the level file)"), *MapId, *Pair.Key);
			World->EditorDestroyActor(Pair.Value, true);
			++P.Removed;
		}
	}
	// What the level now holds, per class — the count a reviewer checks (four warp gates on Toaster).
	TMap<FString, int32> PerClass;
	for (AActor* Placed : P.Touched)
	{
		++PerClass.FindOrAdd(Placed->GetClass()->GetName());
	}
	TArray<FString> Counts;
	for (const auto& Pair : PerClass)
	{
		Counts.Add(FString::Printf(TEXT("%s %d"), *Pair.Key, Pair.Value));
	}
	Counts.Sort();
	UE_LOG(LogDFLevelImport, Display, TEXT("%s: actors spawned %d, moved %d, removed %d (%s)"), *MapId, P.Spawned, P.Moved, P.Removed, *FString::Join(Counts, TEXT(", ")));

	// 4. Save: the asset, the sublevel, and the persistent level when it changed. A checked-out
	//    tree has every committed .uasset/.umap read-only (LFS `lockable`, see the header): clear
	//    that on exactly the files about to be written, before the first save.
	const bool bSavePersistent = bPersistentCreated || bSublevelAdded;
	if (!MakePackageWritable(AssetPackage, TEXT("the lane graph asset"))
		|| !MakePackageWritable(GameplayPackage, TEXT("the gameplay sublevel"))
		|| (bSavePersistent && !MakePackageWritable(PersistentPackage, TEXT("the persistent level"))))
	{
		OutError = TEXT("a package to save is read-only and could not be made writable (see above)");
		return false;
	}
	if (!SaveAsset(Asset, OutError))
	{
		return false;
	}
	GameplayLevel->MarkPackageDirty();
	if (!FEditorFileUtils::SaveLevel(GameplayLevel))
	{
		OutError = FString::Printf(TEXT("saving %s failed"), *GameplayPackage);
		return false;
	}
	if (bSavePersistent)
	{
		if (!UEditorLoadingAndSavingUtils::SaveMap(World, PersistentPackage))
		{
			OutError = FString::Printf(TEXT("saving %s failed"), *PersistentPackage);
			return false;
		}
	}
	UE_LOG(LogDFLevelImport, Display, TEXT("%s: saved %s, %s%s"), *MapId, *AssetPackage, *GameplayPackage,
		bSavePersistent ? *(TEXT(", ") + PersistentPackage) : TEXT(""));
	return true;
}

bool UDFLevelImportCommandlet::MakeWritable(const FString& Filename, const TCHAR* Purpose)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (Filename.IsEmpty() || !PlatformFile.FileExists(*Filename) || !PlatformFile.IsReadOnly(*Filename))
	{
		return true;
	}
	if (!PlatformFile.SetReadOnly(*Filename, false))
	{
		UE_LOG(LogDFLevelImport, Error, TEXT("%s is read-only and could not be made writable (%s)"), *Filename, Purpose);
		return false;
	}
	UE_LOG(LogDFLevelImport, Display, TEXT("made %s writable (%s; it was read-only)"), *Filename, Purpose);

	// .gitattributes marks Content/**/*.uasset and *.umap `lockable` (ADR-0010): git-lfs checks them
	// out read-only and `git lfs lock` is what makes one writable. Clearing the bit keeps a
	// re-import working on a plain checkout; it takes no lock, and the lock model must not be
	// broken silently — so one line, every time.
	const FString Ext = FPaths::GetExtension(Filename).ToLower();
	const FString ContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	if ((Ext == TEXT("uasset") || Ext == TEXT("umap")) && FPaths::IsUnderDirectory(Filename, ContentDir))
	{
		FString RepoRelative = Filename;
		const FString RepoRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../..")));
		FPaths::MakePathRelativeTo(RepoRelative, *(RepoRoot / TEXT("")));
		UE_LOG(LogDFLevelImport, Warning, TEXT("LFS reminder (ADR-0010): %s is lockable and no lock was taken — `git lfs lock %s` before editing a shared level, `git lfs unlock` at session end"), *Filename, *RepoRelative);
	}
	return true;
}

bool UDFLevelImportCommandlet::MakePackageWritable(const FString& PackageName, const TCHAR* Purpose)
{
	FString Filename;
	if (!FPackageName::DoesPackageExist(PackageName, &Filename))
	{
		return true;   // not on disk yet: the save creates it writable
	}
	return MakeWritable(FPaths::ConvertRelativePathToFull(Filename), Purpose);
}

UWorld* UDFLevelImportCommandlet::LoadOrCreatePersistentLevel(const FString& PackageName, const FDFLevelFile& Level, bool& bOutCreated)
{
	bOutCreated = false;
	if (FPackageName::DoesPackageExist(PackageName))
	{
		UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(PackageName);
		if (World)
		{
			World->FlushLevelStreaming();
		}
		return World;
	}

	UWorld* World = GEditor->NewMap();
	if (!World)
	{
		return nullptr;
	}
	bOutCreated = true;

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SP.ObjectFlags |= RF_Transactional;

	// The floor: 400 m of DF_Graybox at z = 0 (a 1 m cube scaled, top face at grade), until a Landscape replaces it.
	if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(FVector(0.f, 0.f, -50.f), FRotator::ZeroRotator, SP);
		Floor->SetActorLabel(TEXT("Floor"));
		Floor->SetMobility(EComponentMobility::Static);
		Floor->GetStaticMeshComponent()->SetStaticMesh(Cube);
		Floor->SetActorScale3D(FVector(FloorSizeMeters, FloorSizeMeters, 1.f));
		Floor->GetStaticMeshComponent()->SetCollisionProfileName(DFCollision::GrayboxProfile());
		Floor->GetStaticMeshComponent()->RecreatePhysicsState();
	}
	else
	{
		UE_LOG(LogDFLevelImport, Error, TEXT("/Engine/BasicShapes/Cube is missing; the level has no floor"));
	}

	FVector Start = Level.HeroSpawn;
	ProjectToGround(World, Start);
	APlayerStart* PlayerStart = World->SpawnActor<APlayerStart>(Start + FVector(0.f, 0.f, PlayerStartLiftCm), FRotator::ZeroRotator, SP);
	PlayerStart->SetActorLabel(TEXT("PlayerStart"));

	ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0.f, 0.f, 500.f), FRotator(-55.f, -30.f, 0.f), SP);
	Sun->SetActorLabel(TEXT("Sun"));
	Sun->GetComponent()->SetIntensity(8.f);
	Sun->GetComponent()->SetAtmosphereSunLight(true);

	ASkyAtmosphere* Sky = World->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator, SP);
	Sky->SetActorLabel(TEXT("SkyAtmosphere"));

	ASkyLight* SkyLight = World->SpawnActor<ASkyLight>(FVector(0.f, 0.f, 400.f), FRotator::ZeroRotator, SP);
	SkyLight->SetActorLabel(TEXT("SkyLight"));
	SkyLight->GetLightComponent()->SetRealTimeCaptureEnabled(true);

	AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(FVector::ZeroVector, FRotator::ZeroRotator, SP);
	Fog->SetActorLabel(TEXT("HeightFog"));

	if (!UEditorLoadingAndSavingUtils::SaveMap(World, PackageName))
	{
		UE_LOG(LogDFLevelImport, Error, TEXT("could not save the new persistent level %s"), *PackageName);
		return nullptr;
	}
	return World;
}

ULevel* UDFLevelImportCommandlet::FindOrAddGameplaySublevel(UWorld* World, const FString& PackageName, bool& bOutAdded)
{
	bOutAdded = false;
	ULevelStreaming* Streaming = nullptr;
	for (ULevelStreaming* Candidate : World->GetStreamingLevels())
	{
		if (Candidate && Candidate->GetWorldAssetPackageName() == PackageName)
		{
			Streaming = Candidate;
			break;
		}
	}

	if (!Streaming)
	{
		bOutAdded = true;
		if (FPackageName::DoesPackageExist(PackageName))
		{
			// The sublevel exists on disk but the persistent level forgot it (hand-edited): re-add it.
			Streaming = UEditorLevelUtils::AddLevelToWorld(World, *PackageName, ULevelStreamingAlwaysLoaded::StaticClass());
		}
		else
		{
			FString Filename;
			if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, Filename, FPackageName::GetMapPackageExtension()))
			{
				return nullptr;
			}
			UEditorLevelUtils::FCreateNewStreamingLevelForWorldParams Params(ULevelStreamingAlwaysLoaded::StaticClass(), Filename);
			Params.bUseSaveAs = false;
			// One .umap per sublevel: OFPA (ADR-0006) would scatter the importer's actors into
			// __ExternalActors__, which the ownership globs do not cover yet — see the WS-09 log.
			Params.bUseExternalActors = false;
			Streaming = UEditorLevelUtils::CreateNewStreamingLevelForWorld(*World, Params);
		}
	}
	if (!Streaming)
	{
		return nullptr;
	}

	ULevel* Level = Streaming->GetLoadedLevel();
	if (!Level)
	{
		Streaming->SetShouldBeLoaded(true);
		Streaming->SetShouldBeVisible(true);
		World->FlushLevelStreaming();
		Level = Streaming->GetLoadedLevel();
	}
	if (Level)
	{
		World->SetCurrentLevel(Level);
	}
	return Level;
}

UDFLaneGraphAsset* UDFLevelImportCommandlet::FindOrCreateLaneGraphAsset(const FString& PackageName, const FString& AssetName, bool& bOutCreated)
{
	bOutCreated = false;
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		return nullptr;
	}
	Package->FullyLoad();
	UDFLaneGraphAsset* Asset = FindObject<UDFLaneGraphAsset>(Package, *AssetName);
	if (!Asset)
	{
		Asset = NewObject<UDFLaneGraphAsset>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Asset);
		bOutCreated = true;
	}
	return Asset;
}

bool UDFLevelImportCommandlet::ProjectToGround(UWorld* World, FVector& InOutPosition)
{
	const FVector Start = InOutPosition + FVector(0.f, 0.f, 50000.f);
	const FVector End = InOutPosition - FVector(0.f, 0.f, 50000.f);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DFLevelImportProject), /*bTraceComplex*/ false);
	if (World->LineTraceSingleByChannel(Hit, Start, End, DFCollision::LaneSurface, Params))
	{
		InOutPosition.Z = Hit.ImpactPoint.Z;
		return true;
	}
	return false;
}

void UDFLevelImportCommandlet::ProjectAsset(UWorld* World, const FDFLevelFile& Level, UDFLaneGraphAsset& Asset)
{
	// What gets dropped onto the ground: anything authored AT grade. A legacy file is flat (y = 0
	// is the ground, anything above it is a deck the map builds), so only z == 0 points project;
	// an authored 3D file projects every ground-layer point unless the route says elevated. Air
	// lanes keep their height (legacy: absolute; 3D: AGL, which is the same thing over a flat floor).
	const auto WantsProjection = [&Level](const FVector& P, EDFEnemyLayer Layer, bool bElevated)
	{
		if (Layer == EDFEnemyLayer::Air || bElevated)
		{
			return false;
		}
		return Level.bLegacy ? FMath::IsNearlyZero(P.Z) : true;
	};

	int32 Projected = 0;
	int32 Missed = 0;
	const auto Project = [&](FVector& P)
	{
		if (ProjectToGround(World, P)) { ++Projected; } else { ++Missed; }
	};

	TMap<FName, bool> RouteElevated;
	for (const FDFLevelRoute& Route : Level.Routes)
	{
		RouteElevated.Add(Route.Id, Route.bElevated);
	}
	for (FDFLaneNode& Node : Asset.Nodes)
	{
		if (WantsProjection(Node.Position, Node.Layer, false))
		{
			Project(Node.Position);
		}
	}
	for (FDFLaneEdge& Edge : Asset.Edges)
	{
		if (Edge.IsWarp()) { continue; }
		bool bElevated = false;
		for (const FDFLaneItinerary& It : Asset.Itineraries)
		{
			if (Asset.EdgesOf(It).Contains(Asset.EdgeIndexOf(Edge.Id)) && RouteElevated.FindRef(It.Id))
			{
				bElevated = true;
			}
		}
		bool bChanged = false;
		for (FVector& WP : Edge.Waypoints)
		{
			if (WantsProjection(WP, Edge.Layer, bElevated))
			{
				Project(WP);
				bChanged = true;
			}
		}
		if (bChanged)
		{
			Edge.LengthMeters = UDFLaneGraphAsset::WalkedLengthMeters(Edge);
		}
	}
	for (FDFSocketDef& Socket : Asset.Sockets)
	{
		if (Socket.Tag != EDFSocketTag::Wall && WantsProjection(Socket.Position, EDFEnemyLayer::Ground, false))
		{
			Project(Socket.Position);
		}
	}
	for (FDFStationDef& Station : Asset.Stations)
	{
		if (WantsProjection(Station.Position, EDFEnemyLayer::Ground, false)) { Project(Station.Position); }
	}
	for (FDFVehicleSpawnDef& Vehicle : Asset.VehicleSpawns)
	{
		if (WantsProjection(Vehicle.Position, EDFEnemyLayer::Ground, false)) { Project(Vehicle.Position); }
	}
	for (FDFOperatedGateDef& Lever : Asset.OperatedGates)
	{
		if (WantsProjection(Lever.At, EDFEnemyLayer::Ground, false)) { Project(Lever.At); }
	}
	for (FDFTraversalDef& T : Asset.Traversal)
	{
		if (WantsProjection(T.Position, EDFEnemyLayer::Ground, false)) { Project(T.Position); }
	}
	UE_LOG(LogDFLevelImport, Display, TEXT("%s: projected %d points onto DF_LaneSurface (%d had nothing below them)"), *Asset.MapId.ToString(), Projected, Missed);
}

template <typename T>
T* UDFLevelImportCommandlet::FindOrSpawn(FPlacement& P, FName Id, const FVector& Location, const FRotator& Rotation)
{
	const FString Key = PlacementKey(T::StaticClass(), Id);
	T* Actor = nullptr;
	if (ADFWorldActor** Found = P.Existing.Find(Key))
	{
		Actor = Cast<T>(*Found);
	}
	if (Actor)
	{
		Actor->Modify();
		Actor->SetActorLocationAndRotation(Location, Rotation);
		++P.Moved;
	}
	else
	{
		FActorSpawnParameters SP;
		SP.OverrideLevel = P.Level;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SP.ObjectFlags |= RF_Transactional;
		Actor = P.World->SpawnActor<T>(T::StaticClass(), Location, Rotation, SP);
		++P.Spawned;
	}
	if (P.Touched.Contains(Actor))
	{
		// Two records of this import resolved to one placement key, so the second silently
		// overwrote the first (a warp pad that is both an arrival and a departure, keyed by node
		// id alone, was exactly this). Counted; the import fails instead of losing an actor.
		UE_LOG(LogDFLevelImport, Error, TEXT("placement key %s used twice in one import"), *Key);
		++P.Collisions;
	}
	Actor->SetStableId(Id);
	P.Touched.Add(Actor);
	return Actor;
}

void UDFLevelImportCommandlet::PlaceActors(FPlacement& P, const FDFLevelFile& Level, UDFLaneGraphAsset& Asset)
{
	// The one info actor that names the graph.
	ADFLaneGraphInfo* Info = nullptr;
	for (AActor* Actor : P.Level->Actors)
	{
		if (ADFLaneGraphInfo* Candidate = Cast<ADFLaneGraphInfo>(Actor))
		{
			Info = Candidate;
			break;
		}
	}
	if (!Info)
	{
		FActorSpawnParameters SP;
		SP.OverrideLevel = P.Level;
		SP.ObjectFlags |= RF_Transactional;
		Info = P.World->SpawnActor<ADFLaneGraphInfo>(FVector::ZeroVector, FRotator::ZeroRotator, SP);
		Info->SetActorLabel(TEXT("LaneGraphInfo"));
	}
	Info->Modify();
	Info->MapId = Level.Id;
	Info->LaneGraph = &Asset;

	for (const FDFSocketDef& Socket : Asset.Sockets)
	{
		ADFSocket* Actor = FindOrSpawn<ADFSocket>(P, Socket.SocketId, Socket.Position, FRotator(0.f, Socket.PadYaw, 0.f));
		Actor->Tag = Socket.Tag;
		Actor->PadYaw = Socket.PadYaw;
		Actor->PadSlopePercent = Socket.PadSlopePercent;
	}

	for (const FDFLaneNode& Node : Asset.Nodes)
	{
		switch (Node.Kind)
		{
		case EDFLaneNodeKind::Core:
			FindOrSpawn<ADFCore>(P, Node.Id, Node.Position, FRotator::ZeroRotator);
			break;
		case EDFLaneNodeKind::Spawn:
		{
			// A route start is a portal; a warp arrival pad is a warp gate (placed below).
			const bool bStartsRoute = Asset.Itineraries.ContainsByPredicate([&Node](const FDFLaneItinerary& It) { return It.Via.Num() > 0 && It.Via[0] == Node.Id; });
			if (bStartsRoute)
			{
				ADFSpawnPortal* Portal = FindOrSpawn<ADFSpawnPortal>(P, Node.Id, Node.Position, FRotator::ZeroRotator);
				Portal->Layer = Node.Layer;
			}
			break;
		}
		default:
			break;
		}
	}

	for (const FDFLaneEdge& Edge : Asset.Edges)
	{
		if (!Edge.IsWarp())
		{
			continue;
		}
		// One actor per warp END, keyed "<node>@<edge>" (ADFWarpGate::StableIdFor): a pad that is
		// the arrival of one warp and the departure of another gets two actors, one per role.
		const FDFLaneNode* From = Asset.FindNode(Edge.From);
		const FDFLaneNode* To = Asset.FindNode(Edge.To);
		if (From)
		{
			ADFWarpGate* Gate = FindOrSpawn<ADFWarpGate>(P, ADFWarpGate::StableIdFor(From->Id, Edge.Id), From->Position, FRotator::ZeroRotator);
			Gate->bArrival = false;
		}
		if (To)
		{
			ADFWarpGate* Gate = FindOrSpawn<ADFWarpGate>(P, ADFWarpGate::StableIdFor(To->Id, Edge.Id), To->Position, FRotator::ZeroRotator);
			Gate->bArrival = true;
		}
	}

	for (const FDFLaneGateDef& Gate : Asset.Gates)
	{
		const FDFSocketDef* Socket = Asset.Sockets.FindByPredicate([&Gate](const FDFSocketDef& S) { return S.SocketId == Gate.SocketId; });
		const FVector At = Socket ? Socket->Position : FVector::ZeroVector;
		ADFLaneGate* Actor = FindOrSpawn<ADFLaneGate>(P, Gate.SocketId, At, FRotator::ZeroRotator);
		Actor->EdgeId = Gate.EdgeId;
	}

	for (const FDFOperatedGateDef& Lever : Asset.OperatedGates)
	{
		ADFOperatedGate* Actor = FindOrSpawn<ADFOperatedGate>(P, Lever.Id, Lever.At, FRotator::ZeroRotator);
		Actor->EdgeId = Lever.EdgeId;
		Actor->Label = Lever.Label;
		Actor->CooldownSeconds = Lever.CooldownSeconds;
		Actor->ReachMeters = Lever.ReachMeters;
		Actor->BodyCheckMeters = Lever.BodyCheckMeters;
	}

	for (const FDFStationDef& Station : Asset.Stations)
	{
		FindOrSpawn<ADFHeroStation>(P, Station.Id, Station.Position, FRotator::ZeroRotator);
	}

	for (const FDFVehicleSpawnDef& Vehicle : Asset.VehicleSpawns)
	{
		ADFVehicleSpawn* Actor = FindOrSpawn<ADFVehicleSpawn>(P, Vehicle.Id, Vehicle.Position, FRotator(0.f, Vehicle.Yaw, 0.f));
		Actor->VehicleId = Vehicle.VehicleId;
	}
}

bool UDFLevelImportCommandlet::SaveAsset(UObject* Asset, FString& OutError)
{
	UPackage* Package = Asset->GetOutermost();
	FString Filename;
	if (!FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), Filename, FPackageName::GetAssetPackageExtension()))
	{
		OutError = FString::Printf(TEXT("no filename for %s"), *Package->GetName());
		return false;
	}
	Package->MarkPackageDirty();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	Args.Error = GError;
	if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
	{
		OutError = FString::Printf(TEXT("saving %s failed"), *Filename);
		return false;
	}
	return true;
}
