#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "DFAttributeSet.generated.h"

// C4 — the base every DF attribute set derives from. It only carries the accessor macro and the
// event signature; the sets themselves are one class per file under Attributes/ so a contract
// reader can open exactly the set C4 names. Replication is push-model (ADR-0004): GAS marks
// FGameplayAttributeData dirty itself (AttributeSet.cpp), so a set registers its properties with
// bIsPushBased and REPNOTIFY_Always and gets the cheap path for free.

/** Getter / setter / initter per attribute, per the engine's own recommended macro. */
#define DF_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/** Fired by a set when a gameplay effect moved a value it guards (damage, heal, depletion).
 *  Instigator / Causer come from the effect context (may be null for self-applied effects). */
DECLARE_MULTICAST_DELEGATE_SixParams(FDFAttributeEvent,
	AActor* /*Instigator*/, AActor* /*Causer*/, const FGameplayEffectSpec* /*Spec*/,
	float /*Magnitude*/, float /*OldValue*/, float /*NewValue*/);

UCLASS(Abstract)
class DFGAMEPLAY_API UDFAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

protected:
	/** Owner world (null on the CDO / in pure unit tests) — for the few sets that look at time. */
	UWorld* GetOwnerWorld() const;
};
