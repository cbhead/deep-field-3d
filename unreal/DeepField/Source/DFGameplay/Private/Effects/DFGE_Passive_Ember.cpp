#include "Effects/DFGE_Passive_Ember.h"

#include "Attributes/DFCombatSet.h"
#include "DFGameplayTags.h"

UDFGE_Passive_Ember::UDFGE_Passive_Ember()
{
	ConfigurePassive(DFTags::Ability_Passive_BurnDuration, TEXT("emberBurnDurationFactor"), 1.3f);
}
