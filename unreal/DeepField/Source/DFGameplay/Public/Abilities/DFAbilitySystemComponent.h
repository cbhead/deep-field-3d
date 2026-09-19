#pragma once

#include "AbilitySystemComponent.h"
#include "CoreMinimal.h"
#include "DFAbilitySystemComponent.generated.h"

class UDFDamageContext;

// C4 — the project ASC. Every DF actor with attributes (heroes, enemies, structures, vehicles)
// carries one. It adds the two things call sites repeat otherwise: set-by-caller effect
// application from a tag->magnitude map, and a damage effect context with the UDFDamageContext
// attached. Replication mode is Mixed for player-owned pawns and Minimal for AI (the owner
// sets it in its constructor); the default here is Minimal because most owners are enemies.
UCLASS(ClassGroup = (DF), meta = (BlueprintSpawnableComponent))
class DFGAMEPLAY_API UDFAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UDFAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The ASC on an actor, or null (UAbilitySystemGlobals lookup, cast to the DF class). */
	static UDFAbilitySystemComponent* FindOn(const AActor* Actor);

	/** A spec of EffectClass with every (tag -> magnitude) set by caller; the context is made here when not given. */
	FGameplayEffectSpecHandle MakeSetByCallerSpec(TSubclassOf<UGameplayEffect> EffectClass, const TMap<FGameplayTag, float>& Magnitudes,
		float Level = 1.f, FGameplayEffectContextHandle Context = FGameplayEffectContextHandle()) const;

	/** Apply EffectClass to this ASC with the magnitudes set by caller. Returns the active handle (invalid for Instant). */
	FActiveGameplayEffectHandle ApplySetByCallerEffectToSelf(TSubclassOf<UGameplayEffect> EffectClass, const TMap<FGameplayTag, float>& Magnitudes,
		float Level = 1.f, FGameplayEffectContextHandle Context = FGameplayEffectContextHandle());

	/** Apply EffectClass from this ASC to Target with the magnitudes set by caller. */
	FActiveGameplayEffectHandle ApplySetByCallerEffectToTarget(UAbilitySystemComponent* Target, TSubclassOf<UGameplayEffect> EffectClass,
		const TMap<FGameplayTag, float>& Magnitudes, float Level = 1.f, FGameplayEffectContextHandle Context = FGameplayEffectContextHandle());

	/** An effect context with Instigator/Causer set and the damage context attached (SourceObject slot). */
	FGameplayEffectContextHandle MakeDamageEffectContext(AActor* Instigator, AActor* Causer, const UDFDamageContext* Damage) const;

	/** Convenience: apply UDFGE_Damage to Target with BaseDamage as DF.SetByCaller.Damage and Damage attached. */
	FActiveGameplayEffectHandle ApplyDamageToTarget(UAbilitySystemComponent* Target, float BaseDamage, const UDFDamageContext* Damage,
		AActor* Instigator = nullptr, AActor* Causer = nullptr);
};
