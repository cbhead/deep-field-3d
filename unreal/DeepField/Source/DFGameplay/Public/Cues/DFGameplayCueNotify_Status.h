#pragma once

#include "CoreMinimal.h"
#include "Cues/DFGameplayCueNotify.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "DFGameplayCueNotify_Status.generated.h"

/** Every machine that ran a status cue: the target, the cue as fired (GameplayCue.DF.Status.<Id>.<Verb>), its parameters. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FDFStatusCueEvent, AActor* /*Target*/, const FGameplayTag& /*CueTag*/, const FGameplayCueParameters& /*Params*/);

// C5 cue base — the handler behind GameplayCue.DF.Status.<Id>.{Applied,Removed,Tick}.
// Content/DF/Gameplay/Cues/GC_DF_Status derives from it with the parent tag GameplayCue.DF.Status,
// so every status cue that has no leaf asset of its own lands here (the cue set walks up to the
// nearest registered parent); a per-status asset (GC_DF_Status_Burn_Applied, ...) derives from it
// too and overrides the leaf. The base owns no VFX or audio — WS-14's Niagara notifies and WS-13's
// DA_AudioCueMap subscribe to the same tags — it logs and broadcasts OnStatusCue so a test map or a
// DF.Func test can see that the cue ran. UDFStatusComponent fires Applied / Removed (RawMagnitude =
// the slot magnitude); UDFDamageExecution fires Tick from each DoT tick (RawMagnitude = the tick).
UCLASS(Blueprintable)
class DFGAMEPLAY_API UDFGameplayCueNotify_Status : public UDFGameplayCueNotify
{
	GENERATED_BODY()

public:
	/** Fired after every execution on this machine (all verbs, Tick included). */
	static FDFStatusCueEvent& OnStatusCue();

	/** The verb of a status cue tag ("Applied", "Removed", "Tick"), or empty. */
	static FString VerbOf(const FGameplayTag& CueTag);

	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
