#pragma once

#include "Effects/DFGE_FactionPassive.h"
#include "DFGE_Passive_Glacier.generated.h"

// Glacier's passive: grants DF.Ability.Passive.ChilledBonus and multiplies UDFCombatSet.ChilledBonus by glacierChilledDamageFactor (1.25); the applier multiplies ChilledBonus into a hit on a target carrying DF.Status.Channel.Movement (Step.cs:562).
UCLASS()
class DFGAMEPLAY_API UDFGE_Passive_Glacier : public UDFGE_FactionPassive
{
	GENERATED_BODY()

public:
	UDFGE_Passive_Glacier();
};
