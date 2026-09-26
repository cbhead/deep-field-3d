#include "Abilities/DFAbilitySet.h"

#include "Abilities/DFAbilitySystemComponent.h"
#include "Abilities/DFGameplayAbility.h"
#include "AttributeSet.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFAbilitySet, Log, All);

void FDFAbilitySet_GrantedHandles::AddAbilityHandle(const FGameplayAbilitySpecHandle& Handle)
{
	if (Handle.IsValid())
	{
		AbilityHandles.Add(Handle);
	}
}

void FDFAbilitySet_GrantedHandles::AddEffectHandle(const FActiveGameplayEffectHandle& Handle)
{
	if (Handle.IsValid())
	{
		EffectHandles.Add(Handle);
	}
}

void FDFAbilitySet_GrantedHandles::AddAttributeSet(UAttributeSet* Set)
{
	if (Set)
	{
		AttributeSets.Add(Set);
	}
}

void FDFAbilitySet_GrantedHandles::TakeFromAbilitySystem(UDFAbilitySystemComponent* ASC)
{
	if (!ASC)
	{
		return;
	}
	if (!ASC->IsOwnerActorAuthoritative())
	{
		return;
	}
	for (const FGameplayAbilitySpecHandle& Handle : AbilityHandles)
	{
		if (Handle.IsValid())
		{
			ASC->ClearAbility(Handle);
		}
	}
	for (const FActiveGameplayEffectHandle& Handle : EffectHandles)
	{
		if (Handle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}
	for (UAttributeSet* Set : AttributeSets)
	{
		if (Set)
		{
			ASC->RemoveSpawnedAttribute(Set);
		}
	}
	AbilityHandles.Reset();
	EffectHandles.Reset();
	AttributeSets.Reset();
}

void UDFAbilitySet::GiveToAbilitySystem(UDFAbilitySystemComponent* ASC, FDFAbilitySet_GrantedHandles* Out, UObject* SourceObject) const
{
	if (!ASC)
	{
		return;
	}
	if (!ASC->IsOwnerActorAuthoritative())
	{
		// Abilities and effects replicate down; a client granting them would desync its ASC.
		return;
	}

	// Attribute sets first so effects granted below can modify them.
	for (const FDFAbilitySet_AttributeSet& Entry : GrantedAttributeSets)
	{
		if (!Entry.AttributeSet)
		{
			UE_LOG(LogDFAbilitySet, Warning, TEXT("%s: empty attribute set entry"), *GetName());
			continue;
		}
		if (ASC->GetAttributeSet(Entry.AttributeSet))
		{
			continue;   // the owner already carries this set (e.g. a hero's health set)
		}
		UAttributeSet* Set = NewObject<UAttributeSet>(ASC->GetOwner(), Entry.AttributeSet);
		ASC->AddAttributeSetSubobject(Set);
		if (Out)
		{
			Out->AddAttributeSet(Set);
		}
	}

	for (const FDFAbilitySet_GameplayAbility& Entry : GrantedAbilities)
	{
		if (!Entry.Ability)
		{
			UE_LOG(LogDFAbilitySet, Warning, TEXT("%s: empty ability entry"), *GetName());
			continue;
		}
		UDFGameplayAbility* AbilityCDO = Entry.Ability->GetDefaultObject<UDFGameplayAbility>();
		FGameplayAbilitySpec Spec(AbilityCDO, Entry.Level);
		Spec.SourceObject = SourceObject;
		if (Entry.InputTag.IsValid())
		{
			Spec.GetDynamicSpecSourceTags().AddTag(Entry.InputTag);
		}
		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
		if (Out)
		{
			Out->AddAbilityHandle(Handle);
		}
	}

	for (const FDFAbilitySet_GameplayEffect& Entry : GrantedEffects)
	{
		if (!Entry.Effect)
		{
			UE_LOG(LogDFAbilitySet, Warning, TEXT("%s: empty effect entry"), *GetName());
			continue;
		}
		const UGameplayEffect* EffectCDO = Entry.Effect->GetDefaultObject<UGameplayEffect>();
		const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectToSelf(EffectCDO, Entry.Level, ASC->MakeEffectContext());
		if (Out)
		{
			Out->AddEffectHandle(Handle);
		}
	}
}
