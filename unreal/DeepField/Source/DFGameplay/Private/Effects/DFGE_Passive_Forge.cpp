#include "Effects/DFGE_Passive_Forge.h"

#include "Attributes/DFCombatSet.h"
#include "DFGameplayTags.h"

UDFGE_Passive_Forge::UDFGE_Passive_Forge()
{
	ConfigurePassive(DFTags::Ability_Passive_BuildDiscount, TEXT("forgeBuildDiscount"), 0.9f);
}
