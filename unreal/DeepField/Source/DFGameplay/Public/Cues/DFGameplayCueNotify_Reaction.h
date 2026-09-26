#pragma once

#include "CoreMinimal.h"
#include "Cues/DFGameplayCueNotify_Base.h"
#include "DFGameplayCueNotify_Reaction.generated.h"

// C5 cue family — the handler behind GameplayCue.DF.Reaction.<Id>, executed by UDFStatusComponent on
// the server when a reaction fires (RawMagnitude = the burst damage after the damage order,
// NormalizedMagnitude = the row's BurstFraction, Instigator = the applier). Registered natively for
// the family root GameplayCue.DF.Reaction, so every reaction is handled without an asset; WS-14 adds
// the Niagara half by a Blueprint child per leaf (GC_DF_Reaction_ThermalShock) that registers that
// leaf. The base pulses a debug sphere and an on-screen line in the palette's Danger swatch (no
// colour literal, ADR-0017) — what L_Test_Status shows — logs, and lets UDFGameplayCueNotify_Base
// forward the cue to UDFMessageBus.
UCLASS(Blueprintable)
class DFGAMEPLAY_API UDFGameplayCueNotify_Reaction : public UDFGameplayCueNotify_Base
{
	GENERATED_BODY()

public:
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
