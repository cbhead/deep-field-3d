#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "DFAbilitySet.generated.h"

class UAttributeSet;
class UDFAbilitySystemComponent;
class UDFGameplayAbility;
class UGameplayEffect;

// C4 — a bundle of abilities, effects and attribute sets granted together (the Lyra AbilitySet
// idea, written from the description): a hero's loadout, an enemy archetype's kit, a faction's
// passives. GiveToAbilitySystem grants everything and hands back the handles so the same bundle
// can be taken away as one unit (a faction change, an elite modifier ending, a vehicle exit).

USTRUCT(BlueprintType)
struct DFGAMEPLAY_API FDFAbilitySet_GameplayAbility
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly) TSubclassOf<UDFGameplayAbility> Ability;
	UPROPERTY(EditDefaultsOnly) int32 Level = 1;
	/** DF.Input.* the ability binds to (added to the spec's dynamic source tags; the input layer matches on it). */
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "DF.Input")) FGameplayTag InputTag;
};

USTRUCT(BlueprintType)
struct DFGAMEPLAY_API FDFAbilitySet_GameplayEffect
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly) TSubclassOf<UGameplayEffect> Effect;
	UPROPERTY(EditDefaultsOnly) float Level = 1.f;
};

USTRUCT(BlueprintType)
struct DFGAMEPLAY_API FDFAbilitySet_AttributeSet
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly) TSubclassOf<UAttributeSet> AttributeSet;
};

/** What one GiveToAbilitySystem granted; TakeFromAbilitySystem undoes exactly that. */
USTRUCT(BlueprintType)
struct DFGAMEPLAY_API FDFAbilitySet_GrantedHandles
{
	GENERATED_BODY()

	void AddAbilityHandle(const FGameplayAbilitySpecHandle& Handle);
	void AddEffectHandle(const FActiveGameplayEffectHandle& Handle);
	void AddAttributeSet(UAttributeSet* Set);

	/** Removes the abilities and effects and unregisters the attribute sets (server only). */
	void TakeFromAbilitySystem(UDFAbilitySystemComponent* ASC);

	bool IsEmpty() const { return AbilityHandles.Num() == 0 && EffectHandles.Num() == 0 && AttributeSets.Num() == 0; }

protected:
	UPROPERTY() TArray<FGameplayAbilitySpecHandle> AbilityHandles;
	UPROPERTY() TArray<FActiveGameplayEffectHandle> EffectHandles;
	UPROPERTY() TArray<TObjectPtr<UAttributeSet>> AttributeSets;
};

UCLASS(BlueprintType, Const)
class DFGAMEPLAY_API UDFAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Grants everything to ASC (server only). Out may be null; SourceObject is stamped on every ability spec. */
	void GiveToAbilitySystem(UDFAbilitySystemComponent* ASC, FDFAbilitySet_GrantedHandles* Out, UObject* SourceObject = nullptr) const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Abilities", meta = (TitleProperty = Ability))
	TArray<FDFAbilitySet_GameplayAbility> GrantedAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "Effects", meta = (TitleProperty = Effect))
	TArray<FDFAbilitySet_GameplayEffect> GrantedEffects;

	UPROPERTY(EditDefaultsOnly, Category = "Attributes", meta = (TitleProperty = AttributeSet))
	TArray<FDFAbilitySet_AttributeSet> GrantedAttributeSets;
};
