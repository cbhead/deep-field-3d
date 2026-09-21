#pragma once

#include "Effects/DFGE_FactionPassive.h"
#include "DFGE_Passive_Specter.generated.h"

// Specter's passive: grants DF.Ability.Passive.WeakPoints only — with it the hero's shots resolve weak-point hit zones (DF.SetByCaller.WeakPointFactor from the enemy row). The sim has no number for it, so no dial and no modifier (UDFCombatSet.WeakPointBonus stays 1 unless a later dial says otherwise).
UCLASS()
class DFGAMEPLAY_API UDFGE_Passive_Specter : public UDFGE_FactionPassive
{
	GENERATED_BODY()

public:
	UDFGE_Passive_Specter();
};
