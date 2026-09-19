#include "Abilities/DFAbilitySystemComponent.h"

#include "AbilitySystemGlobals.h"
#include "DFGameplayTags.h"
#include "Damage/DFDamageContext.h"
#include "Effects/DFGE_Damage.h"

UDFAbilitySystemComponent::UDFAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
	ReplicationMode = EGameplayEffectReplicationMode::Minimal;
}

UDFAbilitySystemComponent* UDFAbilitySystemComponent::FindOn(const AActor* Actor)
{
	return Cast<UDFAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor, /*LookForComponent*/ true));
}

FGameplayEffectSpecHandle UDFAbilitySystemComponent::MakeSetByCallerSpec(TSubclassOf<UGameplayEffect> EffectClass, const TMap<FGameplayTag, float>& Magnitudes,
	float Level, FGameplayEffectContextHandle Context) const
{
	if (!EffectClass)
	{
		return FGameplayEffectSpecHandle();
	}
	if (!Context.IsValid())
	{
		Context = MakeEffectContext();
	}
	FGameplayEffectSpecHandle Spec = MakeOutgoingSpec(EffectClass, Level, Context);
	if (Spec.IsValid())
	{
		for (const TPair<FGameplayTag, float>& Pair : Magnitudes)
		{
			Spec.Data->SetSetByCallerMagnitude(Pair.Key, Pair.Value);
		}
	}
	return Spec;
}

FActiveGameplayEffectHandle UDFAbilitySystemComponent::ApplySetByCallerEffectToSelf(TSubclassOf<UGameplayEffect> EffectClass, const TMap<FGameplayTag, float>& Magnitudes,
	float Level, FGameplayEffectContextHandle Context)
{
	const FGameplayEffectSpecHandle Spec = MakeSetByCallerSpec(EffectClass, Magnitudes, Level, Context);
	return Spec.IsValid() ? ApplyGameplayEffectSpecToSelf(*Spec.Data) : FActiveGameplayEffectHandle();
}

FActiveGameplayEffectHandle UDFAbilitySystemComponent::ApplySetByCallerEffectToTarget(UAbilitySystemComponent* Target, TSubclassOf<UGameplayEffect> EffectClass,
	const TMap<FGameplayTag, float>& Magnitudes, float Level, FGameplayEffectContextHandle Context)
{
	if (!Target)
	{
		return FActiveGameplayEffectHandle();
	}
	const FGameplayEffectSpecHandle Spec = MakeSetByCallerSpec(EffectClass, Magnitudes, Level, Context);
	return Spec.IsValid() ? ApplyGameplayEffectSpecToTarget(*Spec.Data, Target) : FActiveGameplayEffectHandle();
}

FGameplayEffectContextHandle UDFAbilitySystemComponent::MakeDamageEffectContext(AActor* Instigator, AActor* Causer, const UDFDamageContext* Damage) const
{
	FGameplayEffectContextHandle Context = MakeEffectContext();
	Context.AddInstigator(Instigator ? Instigator : GetOwnerActor(), Causer ? Causer : GetAvatarActor());
	if (Damage)
	{
		Damage->AttachTo(Context);
	}
	return Context;
}

FActiveGameplayEffectHandle UDFAbilitySystemComponent::ApplyDamageToTarget(UAbilitySystemComponent* Target, float BaseDamage, const UDFDamageContext* Damage,
	AActor* Instigator, AActor* Causer)
{
	TMap<FGameplayTag, float> Magnitudes;
	Magnitudes.Add(DFTags::SetByCaller_Damage, BaseDamage);
	return ApplySetByCallerEffectToTarget(Target, UDFGE_Damage::StaticClass(), Magnitudes, 1.f, MakeDamageEffectContext(Instigator, Causer, Damage));
}
