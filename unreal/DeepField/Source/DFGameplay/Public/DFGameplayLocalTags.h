#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

// WS-02's local tags (Config/Tags/DF_Gameplay.ini, C1 "own ini A"). They are ini tags, not
// native ones, so they cannot be static FNativeGameplayTags; every lookup goes through here
// and is cached after the first request. A tag missing from the ini comes back invalid and
// the caller skips the cue / factor rather than crashing — the ini is the contract-append.
namespace DFGameplayLocalTags
{
	/** DF.SetByCaller.AmmoFactor — ammo row DamageFactor, 1 when absent. */
	DFGAMEPLAY_API FGameplayTag SetByCaller_AmmoFactor();
	/** DF.SetByCaller.WeakPointFactor — hit-zone factor from the enemy row, 1 when absent. */
	DFGAMEPLAY_API FGameplayTag SetByCaller_WeakPointFactor();
	/** DF.SetByCaller.PackAPunchFactor — packDamagePerLevel^level, 1 when absent. */
	DFGAMEPLAY_API FGameplayTag SetByCaller_PackAPunchFactor();

	/** GameplayCue.DF.Status.<PascalId>.<Verb> where Verb is Applied / Removed / Tick. */
	DFGAMEPLAY_API FGameplayTag StatusCue(FName StatusId, const TCHAR* Verb);
	/** GameplayCue.DF.Reaction.<PascalId>. */
	DFGAMEPLAY_API FGameplayTag ReactionCue(FName ReactionId);

	/** The content id (lower camelCase) of a DF.Status.* / DF.Reaction.* tag: DF.Status.Chill -> "chill". */
	DFGAMEPLAY_API FName ContentIdFromTag(const FGameplayTag& Tag);
	/** "chill" -> "Chill" (the PascalCase leaf every C1 tag uses). */
	DFGAMEPLAY_API FString PascalCase(FName ContentId);
}
