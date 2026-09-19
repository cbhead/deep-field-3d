#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DFLevelImportCommandlet.generated.h"

class ADFWorldActor;
class UDFLaneGraphAsset;
class ULevel;
struct FDFLevelFile;

// -run=DFEditor.DFLevelImport -map=<id>[,<id>] [-all] [-legacy]
//
// (Spelled with the module: the engine resolves -run= before PostEngineInit modules load, and
// DFEditor loads at PostEngineInit; "Module.Commandlet" makes it load DFEditor first. The bare
// -run=DFLevelImport works once INT moves DFEditor to the Default loading phase.)
//
// unreal/content/levels/<map>.level.json -> DA_LaneGraph_<Map> + L_<Map>_Gameplay (WS-09; C6,
// CONTRACTS/map-authoring-3d.md §1). The persistent L_<Map> is created only when missing (a 400 m
// DF_Graybox floor, a PlayerStart at heroSpawn, sun, sky and fog — what make_dev_level.py does);
// after that it belongs to the map's WS-10x owner and the importer only adds the sublevel to it.
//
// Re-import updates in place: every placed actor is found by class + stable id and moved; ids that
// vanished from the file are removed; nothing is ever recreated under a new GUID. Ground positions
// are projected onto whatever blocks DF_LaneSurface (the flat floor today, the Landscape later).
UCLASS()
class UDFLevelImportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UDFLevelImportCommandlet();

	virtual int32 Main(const FString& Params) override;

private:
	struct FPlacement
	{
		UWorld* World = nullptr;
		ULevel* Level = nullptr;
		TMap<FString, ADFWorldActor*> Existing;   // "<class>/<id>" -> actor already in the level
		TSet<AActor*> Touched;
		int32 Spawned = 0;
		int32 Moved = 0;
		int32 Removed = 0;
	};

	bool ImportMap(const FString& MapId, bool bPreferLegacy, FString& OutError);

	UWorld* LoadOrCreatePersistentLevel(const FString& PackageName, const FDFLevelFile& Level, bool& bOutCreated);
	ULevel* FindOrAddGameplaySublevel(UWorld* World, const FString& PackageName, bool& bOutAdded);
	UDFLaneGraphAsset* FindOrCreateLaneGraphAsset(const FString& PackageName, const FString& AssetName, bool& bOutCreated);

	/** Drop the point onto the first DF_LaneSurface hit below it; false (and unchanged) when there is nothing to hit. */
	static bool ProjectToGround(UWorld* World, FVector& InOutPosition);
	void ProjectAsset(UWorld* World, const FDFLevelFile& Level, UDFLaneGraphAsset& Asset);

	void PlaceActors(FPlacement& P, const FDFLevelFile& Level, UDFLaneGraphAsset& Asset);

	template <typename T>
	T* FindOrSpawn(FPlacement& P, FName Id, const FVector& Location, const FRotator& Rotation);

	static bool SaveAsset(UObject* Asset, FString& OutError);
	static FString MapDisplayName(const FString& MapId);
};
