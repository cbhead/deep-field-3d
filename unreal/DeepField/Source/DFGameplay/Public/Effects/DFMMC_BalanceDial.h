#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "DFMMC_BalanceDial.generated.h"

// The magnitude calculation behind every UDFGE_FactionPassive modifier: it reads the Balance dial
// the effect class names (DFBalance::Dial — DT_Balance when loaded, the sim's constant otherwise)
// at spec time, so the number is content, never a literal in a class, and a re-import re-tunes the
// passive without a rebuild. One class for every dial: the dial comes from the spec's effect
// definition, not from an MMC subclass per number.
UCLASS()
class DFGAMEPLAY_API UDFMMC_BalanceDial : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;
};
