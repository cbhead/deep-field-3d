#pragma once

#include "Abilities/GameplayAbility.h"
#include "CoreMinimal.h"
#include "DFGameplayAbility.generated.h"

class UDFAbilitySystemComponent;

// C4 — the ability base. LocalPredicted and instanced per actor by default (fire / reload /
// melee / faction abilities all predict; anything server-only overrides the policy in its own
// constructor). Cost, cooldown, damage and radius come from content rows through the
// DF.SetByCaller.* tags: an ability reads its row, fills a tag->magnitude map and applies the
// effect through the helpers here, so no GE asset holds a number.
UCLASS(Abstract)
class DFGAMEPLAY_API UDFGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UDFGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The owning ASC as the DF class (null before InitAbilityActorInfo or on a foreign ASC). */
	UDFAbilitySystemComponent* GetDFAbilitySystemComponent() const;

	/** A spec of EffectClass from this ability's owner with the magnitudes set by caller (level = ability level). */
	FGameplayEffectSpecHandle MakeSetByCallerSpec(TSubclassOf<UGameplayEffect> EffectClass, const TMap<FGameplayTag, float>& Magnitudes) const;

	/** Apply EffectClass to the owner with the magnitudes set by caller. */
	FActiveGameplayEffectHandle ApplySetByCallerToOwner(TSubclassOf<UGameplayEffect> EffectClass, const TMap<FGameplayTag, float>& Magnitudes);

	/** Apply EffectClass to a target ASC with the magnitudes set by caller. */
	FActiveGameplayEffectHandle ApplySetByCallerToTarget(UAbilitySystemComponent* Target, TSubclassOf<UGameplayEffect> EffectClass, const TMap<FGameplayTag, float>& Magnitudes);

	/** A set-by-caller magnitude on a spec, or Default when absent (never warns). */
	static float GetSetByCaller(const FGameplayEffectSpec& Spec, const FGameplayTag& Tag, float Default = 0.f);
	static float GetSetByCaller(const FGameplayEffectSpecHandle& Spec, const FGameplayTag& Tag, float Default = 0.f);
};
