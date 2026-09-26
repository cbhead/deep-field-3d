#pragma once

#include "CoreMinimal.h"
#include "Content/DFContentRows.h"
#include "Engine/DataTable.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DFContentSubsystem.generated.h"

class UDFContentDefinition;

// The one door to content. Rows come from /Game/DF/Data/Tables/DT_<Table> (written only by the
// JSON importer, WS-01); bindings come from the Asset Manager's primary assets. Missing rows are
// errors with the id in the message — never a silent default (the "wrong name = silent graybox"
// failure mode is what this class exists to make impossible).
UCLASS()
class DFCORE_API UDFContentSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UDFContentSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Loads every table under /Game/DF/Data/Tables. Returns false (and logs) if any expected table is missing. */
	bool LoadTables();

	const FDFTowerRow*           Tower(FName Id) const           { return Find<FDFTowerRow>(TEXT("towers"), Id); }
	const FDFTrapRow*            Trap(FName Id) const            { return Find<FDFTrapRow>(TEXT("traps"), Id); }
	const FDFEnemyRow*           Enemy(FName Id) const           { return Find<FDFEnemyRow>(TEXT("enemies"), Id); }
	const FDFStatusRow*          Status(FName Id) const          { return Find<FDFStatusRow>(TEXT("statuses"), Id); }
	const FDFReactionRow*        Reaction(FName Id) const        { return Find<FDFReactionRow>(TEXT("reactions"), Id); }
	const FDFFactionRow*         Faction(FName Id) const         { return Find<FDFFactionRow>(TEXT("factions"), Id); }
	const FDFWeaponRow*          Weapon(FName Id) const          { return Find<FDFWeaponRow>(TEXT("weapons"), Id); }
	const FDFMeleeRow*           Melee(FName Id) const           { return Find<FDFMeleeRow>(TEXT("melee"), Id); }
	const FDFMeleeAttachmentRow* MeleeAttachment(FName Id) const { return Find<FDFMeleeAttachmentRow>(TEXT("meleeAttachments"), Id); }
	const FDFAttachmentRow*      Attachment(FName Id) const      { return Find<FDFAttachmentRow>(TEXT("attachments"), Id); }
	const FDFAmmoRow*            Ammo(FName Id) const            { return Find<FDFAmmoRow>(TEXT("ammo"), Id); }
	const FDFConditionRow*       Condition(FName Id) const       { return Find<FDFConditionRow>(TEXT("conditions"), Id); }
	const FDFVehicleRow*         Vehicle(FName Id) const         { return Find<FDFVehicleRow>(TEXT("vehicles"), Id); }
	const FDFSectorRow*          Sector(FName Id) const          { return Find<FDFSectorRow>(TEXT("maps"), Id); }

	/** All wave groups of a map, in table order (waveIndex ascending, group order). */
	TArray<const FDFWaveGroupRow*> Waves(FName MapId) const;

	/** A Balance.cs dial by camelCase name (e.g. "startingMoney"). Logs and returns Default if missing. */
	float Balance(FName Dial, float Default = 0.f) const;

	/** Every id in a table, in table order. */
	TArray<FName> Ids(FName Table) const;

	/** The binding asset for a content id (loaded synchronously the first time). */
	UDFContentDefinition* Definition(const FPrimaryAssetType& Type, FName Id) const;

	/** True after LoadTables succeeded. */
	bool IsReady() const { return bReady; }

	template <typename TRow>
	const TRow* Find(FName Table, FName Id) const
	{
		if (const TObjectPtr<UDataTable>* DT = Tables.Find(Table); DT && *DT)
		{
			if (const TRow* Row = (*DT)->FindRow<TRow>(Id, TEXT("DFContent"), /*bWarnIfMissing*/ false))
			{
				return Row;
			}
		}
		ReportMissing(Table, Id);
		return nullptr;
	}

private:
	void ReportMissing(FName Table, FName Id) const;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UDataTable>> Tables;

	bool bReady = false;
	mutable TSet<FString> Reported;   // one log line per missing (table, id)
};
