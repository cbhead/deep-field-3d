#pragma once

#include "Attributes/DFAttributeSet.h"
#include "DFHeroSet.generated.h"

// C4 — UDFHeroSet {RegenPerSecond, RegenDelay, BleedoutSeconds}: the Balance.cs hero dials
// (playerRegenPerSecond 10, playerRegenDelaySeconds 6, bleedoutSeconds 30) as attributes so
// faction passives and tiers can scale them. The regen / bleedout timers are the hero's (WS-03).
UCLASS()
class DFGAMEPLAY_API UDFHeroSet : public UDFAttributeSet
{
	GENERATED_BODY()

public:
	UDFHeroSet();

	DF_ATTRIBUTE_ACCESSORS(UDFHeroSet, RegenPerSecond)
	DF_ATTRIBUTE_ACCESSORS(UDFHeroSet, RegenDelay)
	DF_ATTRIBUTE_ACCESSORS(UDFHeroSet, BleedoutSeconds)

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

protected:
	UFUNCTION() void OnRep_RegenPerSecond(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_RegenDelay(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_BleedoutSeconds(const FGameplayAttributeData& Old);

private:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RegenPerSecond, Category = "DF|Hero", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData RegenPerSecond;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RegenDelay, Category = "DF|Hero", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData RegenDelay;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BleedoutSeconds, Category = "DF|Hero", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData BleedoutSeconds;
};
