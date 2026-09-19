#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "DFDamageExecution.generated.h"

// C4 — the one damage execution. Captures DamageFactor from the source and FlatArmor /
// DamageTakenFactor from the target, reads DF.SetByCaller.Damage (base) and the factor
// magnitudes (DF.SetByCaller.AmmoFactor / WeakPointFactor / PackAPunchFactor, defaulting to
// the UDFDamageContext's values, then to 1), asks the context and the target's IDFArmorProfileSource
// for the arc, runs FDFDamageMath::Compute and writes the result into IncomingDamage.
// UDFHealthSet then splits shield and health. No number lives here.
UCLASS()
class DFGAMEPLAY_API UDFDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UDFDamageExecution();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
