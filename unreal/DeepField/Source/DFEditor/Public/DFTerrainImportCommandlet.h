#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DFTerrainHeightmap.h"

#include "DFTerrainImportCommandlet.generated.h"

class ALandscape;
class UPhysicalMaterial;
class UWorld;

/**
 * DFTerrainImport — the Landscape half of the terrain lane (ADR-0018, PROGRAMME.md §3.2).
 *
 *   UnrealEditor-Cmd DeepField.uproject -run=DFTerrainImport -map=foundry [-terraindir=<dir>] [-verifyonly]
 *
 * Reads unreal/content/terrain/out/<map>_height.{json,png} (written by build_heightmap.py), creates or
 * replaces the streaming sublevel /Game/DF/Maps/<Map>/L_<Map>_Terrain with one ALandscape imported from
 * the heightmap (100 cm quads at 1 m/sample, sim (0,0) at the Unreal origin, Z range from the json, the
 * PM_Grass physical material, Nanite flagged on, collision on), then makes sure the persistent level
 * /Game/DF/Maps/<Map>/L_<Map> exists (a minimal one is created if WS-09 has not made it yet) and lists the
 * terrain sublevel as always-loaded. Finally it reloads the terrain level and checks the landscape's
 * bounds and two sample heights against the json, so a frame or scale mistake fails the run.
 *
 * The Landscape is a product, never a source: edit the terrain.json (or record a sculpt delta) and re-run.
 */
UCLASS()
class DFEDITOR_API UDFTerrainImportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UDFTerrainImportCommandlet();

	virtual int32 Main(const FString& Params) override;

	/** /Game/DF/Maps/<Map>/L_<Map>_Terrain for a map id (first letter upper-cased). */
	static FString TerrainLevelPath(const FString& MapId);
	/** /Game/DF/Maps/<Map>/L_<Map>. */
	static FString PersistentLevelPath(const FString& MapId);
	/** "foundry" -> "Foundry" (level and folder names use the capitalised map id). */
	static FString MapFolderName(const FString& MapId);

private:
	/** Create (or load) the physical material every landscape gets by default. */
	UPhysicalMaterial* EnsureDefaultPhysicalMaterial() const;
	/** New blank world with one imported ALandscape, saved as the terrain sublevel. */
	bool BuildTerrainLevel(const FString& MapId, const FDFTerrainHeightmap& Heightmap, const FDFTerrainLandscapeLayout& Layout, UPhysicalMaterial* PhysMat) const;
	/** Make sure the persistent level exists and streams the terrain sublevel. */
	bool EnsurePersistentLevel(const FString& MapId, const FDFTerrainHeightmap& Heightmap) const;
	/** Reload the terrain level and compare the landscape with the json; logs the bounds. */
	bool VerifyTerrainLevel(const FString& MapId, const FDFTerrainHeightmap& Heightmap, const FDFTerrainLandscapeLayout& Layout) const;
};
