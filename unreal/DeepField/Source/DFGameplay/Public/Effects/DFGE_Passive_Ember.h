#pragma once

#include "Effects/DFGE_FactionPassive.h"
#include "DFGE_Passive_Ember.generated.h"

// Ember's passive: grants DF.Ability.Passive.BurnDuration; UDFStatusComponent reads emberBurnDurationFactor (1.3) itself and applies it to a burn whose applier carries the tag (or DF.Faction.Ember), before the refresh branch (Step.cs:1120). No attribute: duration is the slot's.
UCLASS()
class DFGAMEPLAY_API UDFGE_Passive_Ember : public UDFGE_FactionPassive
{
	GENERATED_BODY()

public:
	UDFGE_Passive_Ember();
};
