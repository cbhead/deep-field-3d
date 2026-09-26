#pragma once

#include "Attributes/DFAttributeSet.h"
#include "DFHealthSet.generated.h"

// C4 — UDFHealthSet {Health, MaxHealth, Shield, MaxShield, ShieldRegen, FlatArmor,
// DamageTakenFactor, IncomingDamage (meta), IncomingHeal (meta)}.
//
// The damage execution (UDFDamageExecution) never touches Health: it writes the post-armor,
// post-vulnerability amount into IncomingDamage and this set consumes it in
// PostGameplayEffectExecute — shield first, then health — unless the effect's UDFDamageContext
// says the hit ignores the shield (poison). Keeping the split here rather than in the execution
// means two hits landing in the same frame each see the shield the previous one left, exactly
// as the sim's sequential Damage() did.
UCLASS()
class DFGAMEPLAY_API UDFHealthSet : public UDFAttributeSet
{
	GENERATED_BODY()

public:
	UDFHealthSet();

	DF_ATTRIBUTE_ACCESSORS(UDFHealthSet, Health)
	DF_ATTRIBUTE_ACCESSORS(UDFHealthSet, MaxHealth)
	DF_ATTRIBUTE_ACCESSORS(UDFHealthSet, Shield)
	DF_ATTRIBUTE_ACCESSORS(UDFHealthSet, MaxShield)
	DF_ATTRIBUTE_ACCESSORS(UDFHealthSet, ShieldRegen)
	DF_ATTRIBUTE_ACCESSORS(UDFHealthSet, FlatArmor)
	DF_ATTRIBUTE_ACCESSORS(UDFHealthSet, DamageTakenFactor)
	DF_ATTRIBUTE_ACCESSORS(UDFHealthSet, IncomingDamage)
	DF_ATTRIBUTE_ACCESSORS(UDFHealthSet, IncomingHeal)

	/** Health went down through IncomingDamage (Magnitude = total taken, shield included). */
	mutable FDFAttributeEvent OnDamaged;
	/** Health went up through IncomingHeal. */
	mutable FDFAttributeEvent OnHealed;
	/** Health reached 0 (fires once until Health is raised above 0 again). */
	mutable FDFAttributeEvent OnHealthDepleted;
	/** Shield went from > 0 to 0 under damage — the ShieldPopped message hook. */
	mutable FDFAttributeEvent OnShieldDepleted;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_Shield(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_MaxShield(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_ShieldRegen(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_FlatArmor(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_DamageTakenFactor(const FGameplayAttributeData& Old);

private:
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "DF|Health", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Health;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "DF|Health", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MaxHealth;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Shield, Category = "DF|Health", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Shield;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxShield, Category = "DF|Health", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MaxShield;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ShieldRegen, Category = "DF|Health", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData ShieldRegen;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FlatArmor, Category = "DF|Health", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData FlatArmor;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageTakenFactor, Category = "DF|Health", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData DamageTakenFactor;

	// Meta attributes: server-only scratch, consumed and zeroed in PostGameplayEffectExecute.
	UPROPERTY(BlueprintReadOnly, Category = "DF|Health", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData IncomingDamage;
	UPROPERTY(BlueprintReadOnly, Category = "DF|Health", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData IncomingHeal;

	/** OnHealthDepleted fires once per depletion, not once per hit on a corpse. */
	bool bHealthDepleted = false;
};
