#pragma once

#include "CoreMinimal.h"
#include "Cues/DFGameplayCueNotify.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "DFGameplayCueNotify_Reaction.generated.h"

/** Every machine that ran a reaction cue: the target, the cue as fired (GameplayCue.DF.Reaction.<Id>), its parameters. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FDFReactionCueEvent, AActor* /*Target*/, const FGameplayTag& /*CueTag*/, const FGameplayCueParameters& /*Params*/);

// C5 cue base — the handler behind GameplayCue.DF.Reaction.<Id>, executed by UDFStatusComponent on
// the server when a reaction fires (RawMagnitude = the burst damage after the damage order,
// NormalizedMagnitude = the row's BurstFraction, Instigator = the applier). Content/DF/Gameplay/
// Cues/GC_DF_Reaction (tag GameplayCue.DF.Reaction) is the fallback for every reaction;
// GC_DF_Reaction_ThermalShock is the leaf L_Test_Status shows. No VFX here (WS-14 adds the Niagara
// half by reparenting or overriding the leaf asset): the base logs, pulses a debug sphere and an
// on-screen line in the palette's Danger colour, and broadcasts OnReactionCue for the test rig.
UCLASS(Blueprintable)
class DFGAMEPLAY_API UDFGameplayCueNotify_Reaction : public UDFGameplayCueNotify
{
	GENERATED_BODY()

public:
	/** Fired after every execution on this machine. */
	static FDFReactionCueEvent& OnReactionCue();

	/** Seconds the debug sphere / string stay (0 = no debug draw). */
	UPROPERTY(EditDefaultsOnly, Category = "DF|Cue")
	float DebugDrawSeconds = 1.f;

	/** Debug sphere radius (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "DF|Cue")
	float DebugRadiusCm = 120.f;

	/** Also print the line on screen (GEngine debug messages). */
	UPROPERTY(EditDefaultsOnly, Category = "DF|Cue")
	bool bOnScreenMessage = true;

	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
