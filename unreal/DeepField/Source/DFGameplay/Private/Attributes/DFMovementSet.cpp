#include "Attributes/DFMovementSet.h"

#include "Net/UnrealNetwork.h"

UDFMovementSet::UDFMovementSet()
{
	InitBaseSpeed(0.f);
	InitSpeedFactor(1.f);
}

void UDFMovementSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.RepNotifyCondition = REPNOTIFY_Always;
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFMovementSet, BaseSpeed, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFMovementSet, SpeedFactor, Params);
}

void UDFMovementSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (Attribute == GetBaseSpeedAttribute() || Attribute == GetSpeedFactorAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void UDFMovementSet::OnRep_BaseSpeed(const FGameplayAttributeData& Old)   { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFMovementSet, BaseSpeed, Old); }
void UDFMovementSet::OnRep_SpeedFactor(const FGameplayAttributeData& Old) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFMovementSet, SpeedFactor, Old); }
