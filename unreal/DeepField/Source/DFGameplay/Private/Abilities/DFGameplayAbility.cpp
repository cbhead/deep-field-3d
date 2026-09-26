#include "Abilities/DFGameplayAbility.h"

#include "Abilities/DFAbilitySystemComponent.h"

UDFGameplayAbility::UDFGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// PROGRAMME.md §3.3: fire / reload / melee / ability are LocalPredicted with predictive cues.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
}

UDFAbilitySystemComponent* UDFGameplayAbility::GetDFAbilitySystemComponent() const
{
	return Cast<UDFAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
}

FGameplayEffectSpecHandle UDFGameplayAbility::MakeSetByCallerSpec(TSubclassOf<UGameplayEffect> EffectClass, const TMap<FGameplayTag, float>& Magnitudes) const
{
	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(EffectClass, GetAbilityLevel());
	if (Spec.IsValid())
	{
		for (const TPair<FGameplayTag, float>& Pair : Magnitudes)
		{
			Spec.Data->SetSetByCallerMagnitude(Pair.Key, Pair.Value);
		}
	}
	return Spec;
}

FActiveGameplayEffectHandle UDFGameplayAbility::ApplySetByCallerToOwner(TSubclassOf<UGameplayEffect> EffectClass, const TMap<FGameplayTag, float>& Magnitudes)
{
	const FGameplayEffectSpecHandle Spec = MakeSetByCallerSpec(EffectClass, Magnitudes);
	if (!Spec.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	return ApplyGameplayEffectSpecToOwner(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), Spec);
}

FActiveGameplayEffectHandle UDFGameplayAbility::ApplySetByCallerToTarget(UAbilitySystemComponent* Target, TSubclassOf<UGameplayEffect> EffectClass, const TMap<FGameplayTag, float>& Magnitudes)
{
	UAbilitySystemComponent* Source = GetAbilitySystemComponentFromActorInfo();
	const FGameplayEffectSpecHandle Spec = MakeSetByCallerSpec(EffectClass, Magnitudes);
	if (!Target || !Source || !Spec.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	return Source->ApplyGameplayEffectSpecToTarget(*Spec.Data, Target, GetCurrentActivationInfo().GetActivationPredictionKey());
}

float UDFGameplayAbility::GetSetByCaller(const FGameplayEffectSpec& Spec, const FGameplayTag& Tag, float Default)
{
	return Spec.GetSetByCallerMagnitude(Tag, /*WarnIfNotFound*/ false, Default);
}

float UDFGameplayAbility::GetSetByCaller(const FGameplayEffectSpecHandle& Spec, const FGameplayTag& Tag, float Default)
{
	return Spec.IsValid() ? GetSetByCaller(*Spec.Data, Tag, Default) : Default;
}
