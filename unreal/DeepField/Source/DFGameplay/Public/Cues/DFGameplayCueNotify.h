#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "DFGameplayCueNotify.generated.h"

// The root of WS-02's cue bases (UDFGameplayCueNotify_Status / _Reaction). One thing lives here:
// the cue tag as a NAME. UGameplayCueNotify_Static::GameplayCueTag is an FGameplayTag, which no
// script can write (the struct's TagName is read-only from Python and Blueprint), so a generator
// such as Source/DFGameplay/Dev/make-l-test-status.py sets CueTagName instead and the class
// resolves it into GameplayCueTag (and the asset-registry mirror GameplayCueName) whenever it is
// edited, saved or loaded. Empty = the engine's own derivation from the asset name
// (GC_DF_Reaction_ThermalShock -> GameplayCue.DF.Reaction.ThermalShock). A name that is not a
// registered tag is an error in the log and leaves GameplayCueTag alone.
UCLASS(Abstract)
class DFGAMEPLAY_API UDFGameplayCueNotify : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	/** The cue tag by name (GameplayCue.DF.Status, GameplayCue.DF.Reaction.ThermalShock, ...). */
	UPROPERTY(EditDefaultsOnly, Category = "DF|Cue")
	FName CueTagName;

	/** True when GameplayCueTag is CueTagName (or CueTagName is empty). */
	bool ResolveCueTagName();

	virtual void PostInitProperties() override;
	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
