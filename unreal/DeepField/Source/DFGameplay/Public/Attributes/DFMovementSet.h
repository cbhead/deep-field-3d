#pragma once

#include "Attributes/DFAttributeSet.h"
#include "DFMovementSet.generated.h"

// C4 — UDFMovementSet {BaseSpeed, SpeedFactor}. Statuses on the Movement channel multiply
// SpeedFactor (UDFGE_Status_Movement, compound); slope factors are the movement component's
// business and never land here. Effective speed = BaseSpeed * SpeedFactor.
UCLASS()
class DFGAMEPLAY_API UDFMovementSet : public UDFAttributeSet
{
	GENERATED_BODY()

public:
	UDFMovementSet();

	DF_ATTRIBUTE_ACCESSORS(UDFMovementSet, BaseSpeed)
	DF_ATTRIBUTE_ACCESSORS(UDFMovementSet, SpeedFactor)

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

protected:
	UFUNCTION() void OnRep_BaseSpeed(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_SpeedFactor(const FGameplayAttributeData& Old);

private:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BaseSpeed, Category = "DF|Movement", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData BaseSpeed;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpeedFactor, Category = "DF|Movement", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData SpeedFactor;
};
