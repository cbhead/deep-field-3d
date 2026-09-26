#pragma once

#include "Effects/DFGE_FactionPassive.h"
#include "DFGE_Passive_Tempest.generated.h"

// Tempest's passive: grants DF.Ability.Passive.ReloadSpeed and multiplies UDFCombatSet.RateFactor by tempestRateFactor (1.12) — fire and reload rate, as Step.cs:548 / 600 scale `rate`.
UCLASS()
class DFGAMEPLAY_API UDFGE_Passive_Tempest : public UDFGE_FactionPassive
{
	GENERATED_BODY()

public:
	UDFGE_Passive_Tempest();
};
