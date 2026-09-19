#include "Attributes/DFHealthSet.h"

#include "Damage/DFDamageContext.h"
#include "Damage/DFDamageMath.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UDFHealthSet::UDFHealthSet()
{
	// Neutral defaults: a set on an actor nobody initialised is harmless (factor 1, no armor, no
	// shield). The real numbers come from the content rows through the owning actor's init.
	InitHealth(1.f);
	InitMaxHealth(1.f);
	InitShield(0.f);
	InitMaxShield(0.f);
	InitShieldRegen(0.f);
	InitFlatArmor(0.f);
	InitDamageTakenFactor(1.f);
	InitIncomingDamage(0.f);
	InitIncomingHeal(0.f);
}

void UDFHealthSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.RepNotifyCondition = REPNOTIFY_Always;
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFHealthSet, Health, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFHealthSet, MaxHealth, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFHealthSet, Shield, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFHealthSet, MaxShield, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFHealthSet, ShieldRegen, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFHealthSet, FlatArmor, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UDFHealthSet, DamageTakenFactor, Params);
}

void UDFHealthSet::OnRep_Health(const FGameplayAttributeData& Old)            { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFHealthSet, Health, Old); }
void UDFHealthSet::OnRep_MaxHealth(const FGameplayAttributeData& Old)         { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFHealthSet, MaxHealth, Old); }
void UDFHealthSet::OnRep_Shield(const FGameplayAttributeData& Old)            { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFHealthSet, Shield, Old); }
void UDFHealthSet::OnRep_MaxShield(const FGameplayAttributeData& Old)         { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFHealthSet, MaxShield, Old); }
void UDFHealthSet::OnRep_ShieldRegen(const FGameplayAttributeData& Old)       { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFHealthSet, ShieldRegen, Old); }
void UDFHealthSet::OnRep_FlatArmor(const FGameplayAttributeData& Old)         { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFHealthSet, FlatArmor, Old); }
void UDFHealthSet::OnRep_DamageTakenFactor(const FGameplayAttributeData& Old) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDFHealthSet, DamageTakenFactor, Old); }

void UDFHealthSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
	else if (Attribute == GetShieldAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxShield());
	}
	else if (Attribute == GetMaxShieldAttribute() || Attribute == GetShieldRegenAttribute() || Attribute == GetFlatArmorAttribute())
	{
		// Shred drives FlatArmor negative on an unarmored target; the sim clamps at 0 too.
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetDamageTakenFactorAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void UDFHealthSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UDFHealthSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UDFHealthSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	if (Attribute == GetMaxHealthAttribute() && GetHealth() > NewValue)
	{
		SetHealth(NewValue);
	}
	else if (Attribute == GetMaxShieldAttribute() && GetShield() > NewValue)
	{
		SetShield(NewValue);
	}
	if (Attribute == GetHealthAttribute() && NewValue > 0.f)
	{
		bHealthDepleted = false;
	}
}

void UDFHealthSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetEffectContext();
	AActor* Instigator = Context.GetOriginalInstigator();
	AActor* Causer = Context.GetEffectCauser();

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Amount = GetIncomingDamage();
		SetIncomingDamage(0.f);
		if (Amount <= 0.f)
		{
			return;
		}

		// Poison (and anything else the context flags) rots through a shield; everything else
		// is soaked by it first. The flag rides on the effect context so a plain set-by-caller
		// damage effect with no context behaves like a bullet.
		const UDFDamageContext* DamageContext = UDFDamageContext::FromContext(Context);
		const bool bIgnoresShield = DamageContext && DamageContext->bIgnoresShield;

		const float OldShield = GetShield();
		const float OldHealth = GetHealth();
		const FDFShieldSplit Split = FDFDamageMath::SplitShield(Amount, OldShield, bIgnoresShield);
		if (Split.Absorbed > 0.f)
		{
			SetShield(OldShield - Split.Absorbed);
		}
		const float NewHealth = FMath::Clamp(OldHealth - Split.ToHealth, 0.f, GetMaxHealth());
		SetHealth(NewHealth);

		OnDamaged.Broadcast(Instigator, Causer, &Data.EffectSpec, Amount, OldHealth, NewHealth);
		if (OldShield > 0.f && GetShield() <= 0.f)
		{
			OnShieldDepleted.Broadcast(Instigator, Causer, &Data.EffectSpec, Split.Absorbed, OldShield, 0.f);
		}
		if (NewHealth <= 0.f && !bHealthDepleted)
		{
			bHealthDepleted = true;
			OnHealthDepleted.Broadcast(Instigator, Causer, &Data.EffectSpec, Amount, OldHealth, NewHealth);
		}
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingHealAttribute())
	{
		const float Amount = GetIncomingHeal();
		SetIncomingHeal(0.f);
		if (Amount <= 0.f)
		{
			return;
		}
		const float OldHealth = GetHealth();
		const float NewHealth = FMath::Clamp(OldHealth + Amount, 0.f, GetMaxHealth());
		SetHealth(NewHealth);
		OnHealed.Broadcast(Instigator, Causer, &Data.EffectSpec, NewHealth - OldHealth, OldHealth, NewHealth);
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetShieldAttribute())
	{
		SetShield(FMath::Clamp(GetShield(), 0.f, GetMaxShield()));
	}
}
