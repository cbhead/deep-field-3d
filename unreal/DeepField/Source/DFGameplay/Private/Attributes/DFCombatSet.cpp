#include "Attributes/DFCombatSet.h"

#include "Net/UnrealNetwork.h"

UDFCombatSet::UDFCombatSet()
{
	InitDamageFactor(1.f);
	InitRateFactor(1.f);
	InitRangeFactor(1.f);
	InitChilledBonus(1.f);
	InitWeakPointBonus(1.f);
}

void UDFCombatSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.RepNotifyCondition = REPNOTIFY_Always;
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFCombatSet, DamageFactor, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFCombatSet, RateFactor, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFCombatSet, RangeFactor, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFCombatSet, ChilledBonus, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFCombatSet, WeakPointBonus, Params);
}

void UDFCombatSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	// Factors are multipliers; a negative one would flip damage into healing.
	NewValue = FMath::Max(NewValue, 0.f);
}

void UDFCombatSet::OnRep_DamageFactor(const FGameplayAttributeData& Old)   { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFCombatSet, DamageFactor, Old); }
void UDFCombatSet::OnRep_RateFactor(const FGameplayAttributeData& Old)     { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFCombatSet, RateFactor, Old); }
void UDFCombatSet::OnRep_RangeFactor(const FGameplayAttributeData& Old)    { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFCombatSet, RangeFactor, Old); }
void UDFCombatSet::OnRep_ChilledBonus(const FGameplayAttributeData& Old)   { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFCombatSet, ChilledBonus, Old); }
void UDFCombatSet::OnRep_WeakPointBonus(const FGameplayAttributeData& Old) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFCombatSet, WeakPointBonus, Old); }
