#include "Effects/DFGE_Damage.h"

#include "Damage/DFDamageExecution.h"

UDFGE_Damage::UDFGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UDFDamageExecution::StaticClass();
	Executions.Add(Execution);
}
