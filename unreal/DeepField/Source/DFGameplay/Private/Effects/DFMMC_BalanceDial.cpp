#include "Effects/DFMMC_BalanceDial.h"

#include "Effects/DFGE_FactionPassive.h"
#include "GameplayEffect.h"

float UDFMMC_BalanceDial::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	const UDFGE_FactionPassive* Passive = Cast<UDFGE_FactionPassive>(Spec.Def.Get());
	if (!Passive)
	{
		return 1.f;   // not a passive: a neutral factor
	}
	// The world reaches the content subsystem through whoever the spec names; a granter always makes
	// the context on its own ASC (MakeEffectContext names the owner as instigator).
	const FGameplayEffectContextHandle& Context = Spec.GetContext();
	const UObject* WorldContext = Context.GetInstigator();
	if (!WorldContext)
	{
		WorldContext = Context.GetEffectCauser();
	}
	if (!WorldContext)
	{
		WorldContext = Context.GetSourceObject();
	}
	return Passive->Magnitude(WorldContext);
}
