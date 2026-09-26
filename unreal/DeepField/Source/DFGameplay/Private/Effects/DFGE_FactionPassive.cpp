#include "Effects/DFGE_FactionPassive.h"

#include "DFBalanceDial.h"
#include "DFGameplayTags.h"
#include "Effects/DFGE_Passive_Ember.h"
#include "Effects/DFGE_Passive_Forge.h"
#include "Effects/DFGE_Passive_Glacier.h"
#include "Effects/DFGE_Passive_Specter.h"
#include "Effects/DFGE_Passive_Tempest.h"
#include "Effects/DFMMC_BalanceDial.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

float UDFGE_FactionPassive::Magnitude(const UObject* WorldContext) const
{
	return Dial.IsNone() ? DialDefault : DFBalance::Dial(WorldContext, Dial, DialDefault);
}

TSubclassOf<UDFGE_FactionPassive> UDFGE_FactionPassive::ClassForFaction(const FGameplayTag& FactionTag)
{
	if (FactionTag == DFTags::Faction_Forge)   { return UDFGE_Passive_Forge::StaticClass(); }
	if (FactionTag == DFTags::Faction_Ember)   { return UDFGE_Passive_Ember::StaticClass(); }
	if (FactionTag == DFTags::Faction_Tempest) { return UDFGE_Passive_Tempest::StaticClass(); }
	if (FactionTag == DFTags::Faction_Glacier) { return UDFGE_Passive_Glacier::StaticClass(); }
	if (FactionTag == DFTags::Faction_Specter) { return UDFGE_Passive_Specter::StaticClass(); }
	return nullptr;
}

TArray<TSubclassOf<UDFGE_FactionPassive>> UDFGE_FactionPassive::AllClasses()
{
	return {
		UDFGE_Passive_Forge::StaticClass(),
		UDFGE_Passive_Ember::StaticClass(),
		UDFGE_Passive_Tempest::StaticClass(),
		UDFGE_Passive_Glacier::StaticClass(),
		UDFGE_Passive_Specter::StaticClass(),
	};
}

void UDFGE_FactionPassive::ConfigurePassive(const FGameplayTag& InPassiveTag, FName InDial, float InDialDefault)
{
	PassiveTag = InPassiveTag;
	Dial = InDial;
	DialDefault = InDialDefault;
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// A default subobject, as UDFGE_StatusBase does: the CDO owns it the way a loaded asset's component is owned.
	UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("DFPassiveTag"));
	GEComponents.Add(TargetTags);
	FInheritedTagContainer Granted;
	Granted.AddTag(InPassiveTag);
	TargetTags->SetAndApplyTargetTagChanges(Granted);
}

void UDFGE_FactionPassive::AddDialModifier(const FGameplayAttribute& Attribute, EGameplayModOp::Type Op)
{
	FCustomCalculationBasedFloat Custom;
	Custom.CalculationClassMagnitude = UDFMMC_BalanceDial::StaticClass();
	Custom.Coefficient = FScalableFloat(1.f);
	Custom.PreMultiplyAdditiveValue = FScalableFloat(0.f);
	Custom.PostMultiplyAdditiveValue = FScalableFloat(0.f);
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = Attribute;
	Modifier.ModifierOp = Op;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Custom);
	Modifiers.Add(Modifier);
}
