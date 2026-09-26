#include "Attributes/DFAmmoSet.h"

#include "Net/UnrealNetwork.h"

UDFAmmoSet::UDFAmmoSet()
{
	InitMagazine(0.f);
	InitMagazineSize(0.f);
	InitReloadSeconds(0.f);
}

void UDFAmmoSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.RepNotifyCondition = REPNOTIFY_Always;
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFAmmoSet, Magazine, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFAmmoSet, MagazineSize, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFAmmoSet, ReloadSeconds, Params);
}

void UDFAmmoSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (Attribute == GetMagazineAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMagazineSize());
	}
	else
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void UDFAmmoSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	if (Attribute == GetMagazineSizeAttribute() && GetMagazine() > NewValue)
	{
		SetMagazine(NewValue);
	}
}

void UDFAmmoSet::OnRep_Magazine(const FGameplayAttributeData& Old)      { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFAmmoSet, Magazine, Old); }
void UDFAmmoSet::OnRep_MagazineSize(const FGameplayAttributeData& Old)  { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFAmmoSet, MagazineSize, Old); }
void UDFAmmoSet::OnRep_ReloadSeconds(const FGameplayAttributeData& Old) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFAmmoSet, ReloadSeconds, Old); }
