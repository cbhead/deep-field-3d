#pragma once

#include "CoreMinimal.h"
#include "Content/DFContentRows.h"
#include "DFMatchTypes.generated.h"

// Match-state value types shared by every layer: DFMatch replicates them, DFGameplay computes
// with them, DFUI shows them (C12). Mirrors of the sim's records, kept in DFCore so none of those
// modules needs another to name them. Append-only, like the rest of DFCore.

/** World.cs MatchPhase, same order. */
UENUM(BlueprintType)
enum class EDFMatchPhase : uint8 { Intermission, Wave, Victory, Defeat };

/** Gunsmith.cs WeaponBuild: the attachment chosen per slot, the loaded ammo, the Pack-a-Punch level. */
USTRUCT(BlueprintType)
struct DFCORE_API FDFWeaponBuild
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<EDFAttachmentSlot, FName> Attachments;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName AmmoId = TEXT("standard");
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 PackLevel = 0;                                // uncapped

	bool operator==(const FDFWeaponBuild& Other) const
	{
		return AmmoId == Other.AmmoId && PackLevel == Other.PackLevel && Attachments.OrderIndependentCompareEqual(Other.Attachments);
	}
	bool operator!=(const FDFWeaponBuild& Other) const { return !(*this == Other); }
};
