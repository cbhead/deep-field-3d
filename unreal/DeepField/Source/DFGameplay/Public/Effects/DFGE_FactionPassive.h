#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "DFGE_FactionPassive.generated.h"

// Faction passives (PROGRAMME §5.2 WS-02 "faction passive GEs"; B§1.5; Factions.cs `passive`) — five
// infinite C++ effects, one per faction, that WS-07's GA_Faction_* / the hero's UDFAbilitySet GRANT
// (WS-03 / WS-07 decide who carries one and when; WS-02 owns what it does). Each grants
// DF.Ability.Passive.<Id> for its life and takes its number from a Balance dial (balance.json; the
// sim's Balance.cs constant is the default), read at spec time by UDFMMC_BalanceDial — so a
// re-import re-tunes every passive and no class spells a number:
//
//   class                 grants                           dial                        default  effect
//   UDFGE_Passive_Forge   DF.Ability.Passive.BuildDiscount forgeBuildDiscount          0.9      tag only: WS-06's economy multiplies a build cost by Magnitude() when the buyer's ASC has the tag (Step.cs:375)
//   UDFGE_Passive_Ember   DF.Ability.Passive.BurnDuration  emberBurnDurationFactor     1.3      tag only: UDFStatusComponent stretches a burn the applier lands by the dial when the applier's ASC has the tag or DF.Faction.Ember (Step.cs:1120)
//   UDFGE_Passive_Tempest DF.Ability.Passive.ReloadSpeed   tempestRateFactor           1.12     UDFCombatSet.RateFactor x dial — fire and reload rate (Step.cs:548, 600)
//   UDFGE_Passive_Glacier DF.Ability.Passive.ChilledBonus  glacierChilledDamageFactor  1.25     UDFCombatSet.ChilledBonus x dial — the applier multiplies it into a hit on a target that carries DF.Status.Channel.Movement (Step.cs:562)
//   UDFGE_Passive_Specter DF.Ability.Passive.WeakPoints    (none)                      1        tag only: the hero's shots resolve weak-point zones (DF.SetByCaller.WeakPointFactor) — the sim has no number for it
//
// Apply one with ASC->ApplyGameplayEffectToSelf(Class->GetDefaultObject<UGameplayEffect>(), 1, ASC->MakeEffectContext());
// ClassForFaction(DF.Faction.<F>) picks the class for a faction row.
UCLASS(Abstract)
class DFGAMEPLAY_API UDFGE_FactionPassive : public UGameplayEffect
{
	GENERATED_BODY()

public:
	/** DF.Ability.Passive.<Id>, granted while the effect is active. */
	const FGameplayTag& GetPassiveTag() const { return PassiveTag; }
	/** The Balance dial behind the passive's number (NAME_None for a tag-only passive). */
	FName GetDial() const { return Dial; }
	/** The sim's constant for the dial: what Magnitude() returns without content. */
	float GetDialDefault() const { return DialDefault; }

	/** The passive's number now: DT_Balance's dial when content is loaded, else the sim default; 1 for a tag-only passive. */
	float Magnitude(const UObject* WorldContext) const;

	/** The passive class for DF.Faction.<F>, or null for a tag that is no faction. */
	static TSubclassOf<UDFGE_FactionPassive> ClassForFaction(const FGameplayTag& FactionTag);
	/** Every passive class in faction order: forge, ember, tempest, glacier, specter. */
	static TArray<TSubclassOf<UDFGE_FactionPassive>> AllClasses();

protected:
	/** Called by each subclass constructor: infinite duration, the granted passive tag, the dial. */
	void ConfigurePassive(const FGameplayTag& InPassiveTag, FName InDial, float InDialDefault);
	/** Adds a modifier on Attribute whose magnitude is the dial (UDFMMC_BalanceDial). */
	void AddDialModifier(const FGameplayAttribute& Attribute, EGameplayModOp::Type Op);

private:
	UPROPERTY(VisibleDefaultsOnly, Category = "DF|Passive") FGameplayTag PassiveTag;
	UPROPERTY(VisibleDefaultsOnly, Category = "DF|Passive") FName Dial;
	UPROPERTY(VisibleDefaultsOnly, Category = "DF|Passive") float DialDefault = 1.f;
};
