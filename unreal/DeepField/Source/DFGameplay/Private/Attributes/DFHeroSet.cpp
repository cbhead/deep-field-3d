#include "Attributes/DFHeroSet.h"

#include "Net/UnrealNetwork.h"

UDFHeroSet::UDFHeroSet()
{
	InitRegenPerSecond(0.f);
	InitRegenDelay(0.f);
	InitBleedoutSeconds(0.f);
}

void UDFHeroSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.RepNotifyCondition = REPNOTIFY_Always;
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFHeroSet, RegenPerSecond, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFHeroSet, RegenDelay, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFHeroSet, BleedoutSeconds, Params);
}

void UDFHeroSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	NewValue = FMath::Max(NewValue, 0.f);
}

void UDFHeroSet::OnRep_RegenPerSecond(const FGameplayAttributeData& Old)  { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFHeroSet, RegenPerSecond, Old); }
void UDFHeroSet::OnRep_RegenDelay(const FGameplayAttributeData& Old)      { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFHeroSet, RegenDelay, Old); }
void UDFHeroSet::OnRep_BleedoutSeconds(const FGameplayAttributeData& Old) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFHeroSet, BleedoutSeconds, Old); }
