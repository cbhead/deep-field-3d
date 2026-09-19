#pragma once

#include "Attributes/DFAttributeSet.h"
#include "DFControlSet.generated.h"

// C4 — UDFControlSet {CcResist, CcResistFill, CcResistDecay}. The gauge itself lives in
// FDFStatusResolver (so it is testable without a world); UDFStatusComponent mirrors it into
// CcResist every tick so UI / AI can read it as an attribute, and reads the two rates from
// here when they were set by content (Balance ccResistFillPerSecond / ccResistDecayPerSecond).
UCLASS()
class DFGAMEPLAY_API UDFControlSet : public UDFAttributeSet
{
	GENERATED_BODY()

public:
	UDFControlSet();

	DF_ATTRIBUTE_ACCESSORS(UDFControlSet, CcResist)
	DF_ATTRIBUTE_ACCESSORS(UDFControlSet, CcResistFill)
	DF_ATTRIBUTE_ACCESSORS(UDFControlSet, CcResistDecay)

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

protected:
	UFUNCTION() void OnRep_CcResist(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_CcResistFill(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_CcResistDecay(const FGameplayAttributeData& Old);

private:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CcResist, Category = "DF|Control", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData CcResist;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CcResistFill, Category = "DF|Control", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData CcResistFill;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CcResistDecay, Category = "DF|Control", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData CcResistDecay;
};
