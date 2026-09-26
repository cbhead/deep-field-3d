#pragma once

#include "CoreMinimal.h"
#include "Cues/DFGameplayCueNotify_Base.h"
#include "GameplayTagContainer.h"
#include "DFGameplayCueNotify_Status.generated.h"

// C5 cue family — the handler behind GameplayCue.DF.Status.<Id>.{Applied,Removed,Tick}. Registered
// natively for the family root GameplayCue.DF.Status (UDFGameplayCueNotify_Base::RegisterNativeCues),
// so every status cue with no asset of its own lands here; a per-status asset (a Blueprint child,
// GC_DF_Status_Burn_Applied say) registers its leaf and takes that one over. No VFX or audio here —
// WS-14's Niagara notifies and WS-13's DA_AudioCueMap subscribe to the same tags on UDFMessageBus —
// it logs and lets the base forward. UDFStatusComponent fires Applied / Removed (RawMagnitude = the
// slot magnitude); the Thermal / Toxin tick fires Tick (RawMagnitude = the tick damage).
UCLASS(Blueprintable)
class DFGAMEPLAY_API UDFGameplayCueNotify_Status : public UDFGameplayCueNotify_Base
{
	GENERATED_BODY()

public:
	/** The verb of a status cue tag ("Applied", "Removed", "Tick"), or empty. */
	static FString VerbOf(const FGameplayTag& CueTag);

	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
