#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/** How a structure hurts a body: shared by ADFTower and ADFTrap (Step.cs Damage / ApplyStatus). */
namespace DFTowerDamage
{
	/** UDFGE_Damage on Target's ability system, with a UDFDamageContext naming Source (a Tower, a Trap)
	 *  and the instigator's location. Nothing happens without an ability system or for Amount <= 0. */
	void ApplyDamage(AActor* Instigator, AActor* Target, float Amount, const FGameplayTag& DamageType, const FGameplayTag& DamageSource);

	/** Each status id through the target's UDFStatusComponent (none: nothing happens). */
	void ApplyStatuses(AActor* Instigator, AActor* Target, TConstArrayView<FName> StatusIds);
}
