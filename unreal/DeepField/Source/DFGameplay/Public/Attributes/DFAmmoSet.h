#pragma once

#include "Attributes/DFAttributeSet.h"
#include "DFAmmoSet.generated.h"

// C4 — UDFAmmoSet {Magazine, MagazineSize, ReloadSeconds}. Predicted on the owning client for
// fire/reload (the weapon abilities are LocalPredicted); Magazine is clamped to [0, MagazineSize].
UCLASS()
class DFGAMEPLAY_API UDFAmmoSet : public UDFAttributeSet
{
	GENERATED_BODY()

public:
	UDFAmmoSet();

	DF_ATTRIBUTE_ACCESSORS(UDFAmmoSet, Magazine)
	DF_ATTRIBUTE_ACCESSORS(UDFAmmoSet, MagazineSize)
	DF_ATTRIBUTE_ACCESSORS(UDFAmmoSet, ReloadSeconds)

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

protected:
	UFUNCTION() void OnRep_Magazine(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_MagazineSize(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_ReloadSeconds(const FGameplayAttributeData& Old);

private:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Magazine, Category = "DF|Ammo", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Magazine;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MagazineSize, Category = "DF|Ammo", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MagazineSize;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ReloadSeconds, Category = "DF|Ammo", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData ReloadSeconds;
};
