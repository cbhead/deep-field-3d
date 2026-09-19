#pragma once

#include "CoreMinimal.h"
#include "Damage/DFArmorProfile.h"
#include "Damage/DFDamageMath.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "DFDamageContext.generated.h"

struct FDFAmmoRow;
struct FDFEnemyRow;
struct FDFStatusRow;

// Everything a damage hit knows that is not an attribute or a set-by-caller magnitude (C4).
// The applier builds one (UDFDamageContext::Make), fills what it knows — where the shot came
// from, the ammo, the zone, the status behind a DoT tick — and attaches it to the effect
// context as the SourceObject. UDFDamageExecution reads it back; UDFHealthSet reads
// bIgnoresShield from it when it splits shield and health. A hit with no context is a plain
// bullet: no aspect, no ammo, flat armor and shield both apply.
//
// It is a transient UObject rather than a custom FGameplayEffectContext subclass because the
// latter needs an AbilitySystemGlobals subclass in DefaultGame.ini (INT-owned); this needs no
// config and still travels with the spec on the server, which is the only side that executes.
// The effect context holds it only weakly (SourceObject is a TWeakObjectPtr): an applier of a
// lasting effect (a DoT, an aura) must keep it referenced from a UPROPERTY for the effect's life
// — UDFStatusComponent does that per channel; an instant hit is safe on the call stack.
UCLASS(BlueprintType)
class DFGAMEPLAY_API UDFDamageContext : public UObject
{
	GENERATED_BODY()

public:
	/** DF.Damage.Type.* (Kinetic, Thermal, Toxin, ...). */
	UPROPERTY(BlueprintReadWrite) FGameplayTag DamageType;
	/** DF.Damage.Source.* (Tower, Hero, Melee, Trap, Reaction, ...). */
	UPROPERTY(BlueprintReadWrite) FGameplayTag DamageSource;
	/** Hit zone the applier resolved: body / weak_n / plate_n / shield. */
	UPROPERTY(BlueprintReadWrite) FName Zone = TEXT("body");
	/** DF.Status.* behind a DoT tick (fires GameplayCue.DF.Status.<id>.Tick from the execution). */
	UPROPERTY(BlueprintReadWrite) FGameplayTag StatusTag;
	/** DF.Reaction.* behind a burst. */
	UPROPERTY(BlueprintReadWrite) FGameplayTag ReactionTag;

	// Source-side factors (also accepted as DF.SetByCaller.* magnitudes on the spec; the spec wins when present).
	UPROPERTY(BlueprintReadWrite) float AmmoFactor = 1.f;
	UPROPERTY(BlueprintReadWrite) float UnarmoredBonusFactor = 1.f;
	UPROPERTY(BlueprintReadWrite) float WeakPointFactor = 1.f;
	UPROPERTY(BlueprintReadWrite) float PackAPunchFactor = 1.f;
	UPROPERTY(BlueprintReadWrite) bool bIgnoresFlatArmor = false;
	UPROPERTY(BlueprintReadWrite) bool bIgnoresShield = false;

	// Where the hit came from (world). Without it the hit has no aspect (DoT, reactions, splash centre).
	UPROPERTY(BlueprintReadWrite) bool bHasSourceLocation = false;
	UPROPERTY(BlueprintReadWrite) FVector SourceLocation = FVector::ZeroVector;

	// The target's arc, when the applier chose to supply it; otherwise the execution asks the target (IDFArmorProfileSource).
	UPROPERTY(BlueprintReadWrite) bool bHasTargetArmor = false;
	UPROPERTY(BlueprintReadWrite) FDFArmorProfile TargetArmor;

	// Rule dials (Step.cs literals; ReadBalance overrides them from content when it is loaded).
	UPROPERTY(BlueprintReadWrite) float RearArcDegrees = DFDamageDefaults::RearArcDegrees;
	UPROPERTY(BlueprintReadWrite) float ShreddedFrontArcFactor = DFDamageDefaults::ShreddedFrontArcFactor;
	UPROPERTY(BlueprintReadWrite) float MinDamageAfterArmor = DFDamageDefaults::MinDamageAfterArmor;

	/** A fresh context owned by Outer (the applying actor / component) with the Balance dials read. */
	static UDFDamageContext* Make(UObject* Outer, const FGameplayTag& InDamageType = FGameplayTag(), const FGameplayTag& InDamageSource = FGameplayTag());

	/** The context attached to an effect, or null. */
	static const UDFDamageContext* FromContext(const FGameplayEffectContextHandle& Handle);

	/** Attach to an effect context (SourceObject slot). */
	void AttachTo(FGameplayEffectContextHandle& Handle) const;

	void SetSourceLocation(const FVector& InSourceLocation);
	void SetTargetArmor(const FDFEnemyRow& Row, const FVector& TargetForward);
	void SetAmmo(const FDFAmmoRow& Row);
	/** DoT tick: poison ignores armor and shield, burn neither. */
	void SetStatus(const FDFStatusRow& Row, const FGameplayTag& InStatusTag);

	/** Overrides the three rule dials from Balance (rearArcDegrees, shreddedFrontArcFactor, minDamageAfterArmor) when content is loaded. */
	void ReadBalance(const UObject* WorldContext);

	/** Copies the context's share of the inputs (factors, flags, dials, arc, direction) onto In. */
	void FillInput(FDFDamageInput& In, const FVector& TargetLocation, const FDFArmorProfile* TargetProfile) const;
};
