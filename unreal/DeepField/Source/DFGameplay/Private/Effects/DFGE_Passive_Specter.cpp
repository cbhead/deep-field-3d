#include "Effects/DFGE_Passive_Specter.h"

#include "Attributes/DFCombatSet.h"
#include "DFGameplayTags.h"

UDFGE_Passive_Specter::UDFGE_Passive_Specter()
{
	ConfigurePassive(DFTags::Ability_Passive_WeakPoints, NAME_None, 1.f);
}
