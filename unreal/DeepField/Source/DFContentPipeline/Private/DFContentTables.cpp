#include "DFContentTables.h"

#include "Content/DFContentRows.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace DFContentTables
{
	namespace
	{
		const TCHAR* GRoot = TEXT("/Game/DF/Data/Tables");
		const TCHAR* GWavesPrefix = TEXT("waves_");

		// Built on first use: StaticStruct() needs the UObject system, which is not up during static init.
		const TMap<FString, UScriptStruct*>& Specs()
		{
			static const TMap<FString, UScriptStruct*> Map = {
				{ TEXT("towers"),           FDFTowerRow::StaticStruct() },
				{ TEXT("traps"),            FDFTrapRow::StaticStruct() },
				{ TEXT("enemies"),          FDFEnemyRow::StaticStruct() },
				{ TEXT("statuses"),         FDFStatusRow::StaticStruct() },
				{ TEXT("reactions"),        FDFReactionRow::StaticStruct() },
				{ TEXT("factions"),         FDFFactionRow::StaticStruct() },
				{ TEXT("weapons"),          FDFWeaponRow::StaticStruct() },
				{ TEXT("melee"),            FDFMeleeRow::StaticStruct() },
				{ TEXT("meleeAttachments"), FDFMeleeAttachmentRow::StaticStruct() },
				{ TEXT("attachments"),      FDFAttachmentRow::StaticStruct() },
				{ TEXT("ammo"),             FDFAmmoRow::StaticStruct() },
				{ TEXT("conditions"),       FDFConditionRow::StaticStruct() },
				{ TEXT("vehicles"),         FDFVehicleRow::StaticStruct() },
				{ TEXT("maps"),             FDFSectorRow::StaticStruct() },
				{ TEXT("balance"),          FDFBalanceRow::StaticStruct() },
			};
			return Map;
		}

		// DefaultGame.ini PrimaryAssetTypesToScan -> table. Types without a table (levels) are not bindings.
		const TMap<FName, FString>& PrimaryAssetTables()
		{
			static const TMap<FName, FString> Map = {
				{ TEXT("Tower"),     TEXT("towers") },
				{ TEXT("Trap"),      TEXT("traps") },
				{ TEXT("Enemy"),     TEXT("enemies") },
				{ TEXT("Status"),    TEXT("statuses") },
				{ TEXT("Reaction"),  TEXT("reactions") },
				{ TEXT("Faction"),   TEXT("factions") },
				{ TEXT("Weapon"),    TEXT("weapons") },
				{ TEXT("Melee"),     TEXT("melee") },
				{ TEXT("Ammo"),      TEXT("ammo") },
				{ TEXT("Condition"), TEXT("conditions") },
				{ TEXT("Vehicle"),   TEXT("vehicles") },
				{ TEXT("Map"),       TEXT("maps") },
			};
			return Map;
		}
	}

	const TCHAR* Root()
	{
		return GRoot;
	}

	bool IsWavesTable(const FString& Table)
	{
		return Table.StartsWith(GWavesPrefix, ESearchCase::CaseSensitive) && Table.Len() > FCString::Strlen(GWavesPrefix);
	}

	UScriptStruct* RowStructFor(const FString& Table)
	{
		if (IsWavesTable(Table))
		{
			return FDFWaveGroupRow::StaticStruct();
		}
		UScriptStruct* const* Found = Specs().Find(Table);
		return Found ? *Found : nullptr;
	}

	FString AssetNameFor(const FString& Table)
	{
		if (IsWavesTable(Table))
		{
			// The map id keeps its JSON spelling: the subsystem looks up DT_Waves_<maps row name>.
			return FString(TEXT("DT_Waves_")) + Table.Mid(FCString::Strlen(GWavesPrefix));
		}
		FString Name = Table;
		Name[0] = FChar::ToUpper(Name[0]);
		return TEXT("DT_") + Name;
	}

	FString PackageNameFor(const FString& Table)
	{
		return FString(GRoot) + TEXT("/") + AssetNameFor(Table);
	}

	FString ObjectPathFor(const FString& Table)
	{
		const FString Asset = AssetNameFor(Table);
		return FString(GRoot) + TEXT("/") + Asset + TEXT(".") + Asset;
	}

	FString DefaultJsonDir()
	{
		// ProjectDir is unreal/DeepField/; the text source of truth lives beside it in unreal/content/.
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("content"), TEXT("json")));
	}

	FString ContentIdsPath()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("content"), TEXT("content-ids.json")));
	}

	TArray<FString> TablesIn(const FString& JsonDir)
	{
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *FPaths::Combine(JsonDir, TEXT("*.json")), /*Files*/ true, /*Directories*/ false);
		for (FString& File : Files)
		{
			File = FPaths::GetBaseFilename(File);
		}
		Files.Sort();
		return Files;
	}

	FString TableForPrimaryAssetType(FName Type)
	{
		const FString* Found = PrimaryAssetTables().Find(Type);
		return Found ? *Found : FString();
	}
}
