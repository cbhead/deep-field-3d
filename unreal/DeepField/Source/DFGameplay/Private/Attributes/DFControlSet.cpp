#include "Attributes/DFControlSet.h"

#include "Net/UnrealNetwork.h"

UDFControlSet::UDFControlSet()
{
	// 0 rates = "not set by content"; the status component then uses the resolver's defaults.
	InitCcResist(0.f);
	InitCcResistFill(0.f);
	InitCcResistDecay(0.f);
}

void UDFControlSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.RepNotifyCondition = REPNOTIFY_Always;
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFControlSet, CcResist, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFControlSet, CcResistFill, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFControlSet, CcResistDecay, Params);
}

void UDFControlSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (Attribute == GetCcResistAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 1.f);
	}
	else
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void UDFControlSet::OnRep_CcResist(const FGameplayAttributeData& Old)      { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFControlSet, CcResist, Old); }
void UDFControlSet::OnRep_CcResistFill(const FGameplayAttributeData& Old)  { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFControlSet, CcResistFill, Old); }
void UDFControlSet::OnRep_CcResistDecay(const FGameplayAttributeData& Old) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFControlSet, CcResistDecay, Old); }
