#include "Effects/DFGE_Passive_Tempest.h"

#include "Attributes/DFCombatSet.h"
#include "DFGameplayTags.h"

UDFGE_Passive_Tempest::UDFGE_Passive_Tempest()
{
	ConfigurePassive(DFTags::Ability_Passive_ReloadSpeed, TEXT("tempestRateFactor"), 1.12f);
	AddDialModifier(UDFCombatSet::GetRateFactorAttribute(), EGameplayModOp::MultiplyCompound);
}
