#pragma once

#include "Attributes/DFAttributeSet.h"
#include "DFCombatSet.generated.h"

// C4 — UDFCombatSet {DamageFactor, RateFactor, RangeFactor, ChilledBonus, WeakPointBonus}.
// Faction passives (WS-07) and the Overclock feed (WS-16) write here; the damage execution
// captures DamageFactor from the source. ChilledBonus is Glacier's "×1.25 vs Movement-channel
// targets" and WeakPointBonus Specter's — both multiply into the execution only when the
// applier says the condition held (it knows the target; the source set does not).
UCLASS()
class DFGAMEPLAY_API UDFCombatSet : public UDFAttributeSet
{
	GENERATED_BODY()

public:
	UDFCombatSet();

	DF_ATTRIBUTE_ACCESSORS(UDFCombatSet, DamageFactor)
	DF_ATTRIBUTE_ACCESSORS(UDFCombatSet, RateFactor)
	DF_ATTRIBUTE_ACCESSORS(UDFCombatSet, RangeFactor)
	DF_ATTRIBUTE_ACCESSORS(UDFCombatSet, ChilledBonus)
	DF_ATTRIBUTE_ACCESSORS(UDFCombatSet, WeakPointBonus)

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

protected:
	UFUNCTION() void OnRep_DamageFactor(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_RateFactor(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_RangeFactor(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_ChilledBonus(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_WeakPointBonus(const FGameplayAttributeData& Old);

private:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageFactor, Category = "DF|Combat", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData DamageFactor;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RateFactor, Category = "DF|Combat", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData RateFactor;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RangeFactor, Category = "DF|Combat", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData RangeFactor;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ChilledBonus, Category = "DF|Combat", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData ChilledBonus;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_WeakPointBonus, Category = "DF|Combat", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData WeakPointBonus;
};
