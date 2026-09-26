#pragma once

#include "Attributes/DFAttributeSet.h"
#include "DFStructureSet.generated.h"

// C4 — UDFStructureSet {StructureHp, StructureMaxHp}: towers, traps, barricade, walls,
// containers, vehicles. Structures take siege / ram damage as a plain AddBase on StructureHp
// (no armor, no shield, no statuses), so there is no meta attribute here.
UCLASS()
class DFGAMEPLAY_API UDFStructureSet : public UDFAttributeSet
{
	GENERATED_BODY()

public:
	UDFStructureSet();

	DF_ATTRIBUTE_ACCESSORS(UDFStructureSet, StructureHp)
	DF_ATTRIBUTE_ACCESSORS(UDFStructureSet, StructureMaxHp)

	/** StructureHp went down through a gameplay effect. */
	mutable FDFAttributeEvent OnDamaged;
	/** StructureHp reached 0 (once per depletion). */
	mutable FDFAttributeEvent OnDestroyed;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:
	UFUNCTION() void OnRep_StructureHp(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_StructureMaxHp(const FGameplayAttributeData& Old);

private:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StructureHp, Category = "DF|Structure", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData StructureHp;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StructureMaxHp, Category = "DF|Structure", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData StructureMaxHp;

	bool bDestroyed = false;
};
