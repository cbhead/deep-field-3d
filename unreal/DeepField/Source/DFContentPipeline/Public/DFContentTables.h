#pragma once

#include "CoreMinimal.h"

class UScriptStruct;

// The one place that maps a JSON table to its row struct and its DataTable asset (CONTRACTS/content-rows.md).
// UDFContentSubsystem::LoadTables (DFCore) spells the asset names the same way — DT_<Table> with the first
// letter raised, DT_Waves_<map> for waves_<map>.json — so a change here is a change there.
namespace DFContentTables
{
	/** /Game/DF/Data/Tables */
	DFCONTENTPIPELINE_API const TCHAR* Root();

	/** Row struct for a table name ("towers", "waves_foundry"); nullptr for a table this pipeline does not know. */
	DFCONTENTPIPELINE_API UScriptStruct* RowStructFor(const FString& Table);

	/** "towers" -> "DT_Towers", "meleeAttachments" -> "DT_MeleeAttachments", "waves_foundry" -> "DT_Waves_foundry". */
	DFCONTENTPIPELINE_API FString AssetNameFor(const FString& Table);

	/** /Game/DF/Data/Tables/DT_Towers */
	DFCONTENTPIPELINE_API FString PackageNameFor(const FString& Table);

	/** /Game/DF/Data/Tables/DT_Towers.DT_Towers */
	DFCONTENTPIPELINE_API FString ObjectPathFor(const FString& Table);

	/** True for waves_<map> tables (one DataTable per map, all sharing FDFWaveGroupRow). */
	DFCONTENTPIPELINE_API bool IsWavesTable(const FString& Table);

	/** <repo>/unreal/content/json, derived from the project directory. */
	DFCONTENTPIPELINE_API FString DefaultJsonDir();

	/** <repo>/unreal/content/content-ids.json */
	DFCONTENTPIPELINE_API FString ContentIdsPath();

	/** Every <table>.json in a directory, by file stem, sorted. */
	DFCONTENTPIPELINE_API TArray<FString> TablesIn(const FString& JsonDir);

	/** The JSON table a PrimaryAssetType from DefaultGame.ini binds to ("Tower" -> "towers"); empty if none. */
	DFCONTENTPIPELINE_API FString TableForPrimaryAssetType(FName Type);
}
