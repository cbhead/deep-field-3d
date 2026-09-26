#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DFLevelImportCommandlet.generated.h"

class ADFWorldActor;
class UDFLaneGraphAsset;
class ULevel;
struct FDFLevelFile;

// -run=DFLevelImport -map=<id>[,<id>] [-all] [-legacy]        (or -run=DFEditor.DFLevelImport)
//
// (DFEditor loads at the Default phase, so the bare name resolves; the module-qualified spelling
// still works — the WS-09 logs before 2026-09-20 use it, from when DFEditor loaded PostEngineInit.)
//
// unreal/content/levels/<map>.level.json -> DA_LaneGraph_<Map> + L_<Map>_Gameplay (WS-09; C6,
// CONTRACTS/map-authoring-3d.md §1). The persistent L_<Map> is created only when missing (a 400 m
// DF_Graybox floor, a PlayerStart at heroSpawn, sun, sky and fog — what make_dev_level.py does);
// after that it belongs to the map's WS-10x owner and the importer only adds the sublevel to it.
//
// Re-import updates in place: every placed actor is found by class + stable id and moved; ids that
// vanished from the file are removed; nothing is ever recreated under a new GUID. Ground positions
// are projected onto whatever blocks DF_LaneSurface (the flat floor today, the Landscape later).
//
// LFS etiquette (ADR-0010): unreal/.gitattributes marks every Content/**/*.uasset and *.umap
// `lockable`, so a checked-out tree has them READ-ONLY until `git lfs lock <path>` — and a save
// onto one fails ("Cannot remove ... as it is read only"). The commandlet clears the read-only
// bit on the packages it is about to save (DA_LaneGraph_<Map>, L_<Map>_Gameplay, and L_<Map>
// only when it adds the sublevel), logs each file it made writable, and prints a one-line
// reminder for each lockable one: it did NOT take the lock. The etiquette stands: before
// re-importing a shared level, `git lfs lock unreal/DeepField/Content/DF/Maps/<Map>/L_<Map>_Gameplay.umap`
// (and L_<Map>.umap when it will be saved), and `git lfs unlock` at session end (PROGRAMME §6.5).
UCLASS()
class UDFLevelImportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UDFLevelImportCommandlet();

	virtual int32 Main(const FString& Params) override;

	/** Clear the read-only bit on a file about to be overwritten (a checked-out LFS `lockable`
	 *  file, see above). Logs what it made writable and, for a lockable file, the lock reminder.
	 *  True when the file is writable afterwards or is not on disk at all. */
	static bool MakeWritable(const FString& Filename, const TCHAR* Purpose);

	/** MakeWritable for the file a package is saved to; true for a package not yet on disk. */
	static bool MakePackageWritable(const FString& PackageName, const TCHAR* Purpose);

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
		int32 Collisions = 0;                     // one placement key used by two records in one import
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
