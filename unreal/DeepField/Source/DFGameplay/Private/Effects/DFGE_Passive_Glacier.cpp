#include "Effects/DFGE_Passive_Glacier.h"

#include "Attributes/DFCombatSet.h"
#include "DFGameplayTags.h"

UDFGE_Passive_Glacier::UDFGE_Passive_Glacier()
{
	ConfigurePassive(DFTags::Ability_Passive_ChilledBonus, TEXT("glacierChilledDamageFactor"), 1.25f);
	AddDialModifier(UDFCombatSet::GetChilledBonusAttribute(), EGameplayModOp::MultiplyCompound);
}
