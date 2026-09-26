#include "Attributes/DFStructureSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UDFStructureSet::UDFStructureSet()
{
	InitStructureHp(1.f);
	InitStructureMaxHp(1.f);
}

void UDFStructureSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.RepNotifyCondition = REPNOTIFY_Always;
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFStructureSet, StructureHp, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFStructureSet, StructureMaxHp, Params);
}

void UDFStructureSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (Attribute == GetStructureHpAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetStructureMaxHp());
	}
	else if (Attribute == GetStructureMaxHpAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
}

void UDFStructureSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	if (Attribute == GetStructureMaxHpAttribute() && GetStructureHp() > NewValue)
	{
		SetStructureHp(NewValue);
	}
	if (Attribute == GetStructureHpAttribute() && NewValue > 0.f)
	{
		bDestroyed = false;
	}
}

void UDFStructureSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	if (Data.EvaluatedData.Attribute != GetStructureHpAttribute())
	{
		return;
	}
	const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetEffectContext();
	const float NewHp = FMath::Clamp(GetStructureHp(), 0.f, GetStructureMaxHp());
	SetStructureHp(NewHp);
	if (Data.EvaluatedData.Magnitude < 0.f)
	{
		OnDamaged.Broadcast(Context.GetOriginalInstigator(), Context.GetEffectCauser(), &Data.EffectSpec,
			-Data.EvaluatedData.Magnitude, NewHp - Data.EvaluatedData.Magnitude, NewHp);
	}
	if (NewHp <= 0.f && !bDestroyed)
	{
		bDestroyed = true;
		OnDestroyed.Broadcast(Context.GetOriginalInstigator(), Context.GetEffectCauser(), &Data.EffectSpec,
			-Data.EvaluatedData.Magnitude, NewHp - Data.EvaluatedData.Magnitude, NewHp);
	}
}

void UDFStructureSet::OnRep_StructureHp(const FGameplayAttributeData& Old)    { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFStructureSet, StructureHp, Old); }
void UDFStructureSet::OnRep_StructureMaxHp(const FGameplayAttributeData& Old) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFStructureSet, StructureMaxHp, Old); }
