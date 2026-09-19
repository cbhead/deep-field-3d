#pragma once

#include "CoreMinimal.h"
#include "DFDamageMath.generated.h"

// C4 damage order as a pure function, so DF.Unit.Damage.* can pin the formula without an ASC:
//   source factors (DamageFactor, ammo, unarmored bonus, weak point, Pack-a-Punch)
//   -> target front-arc / rear factor (hit direction vs facing, enemy row arc numbers)
//   -> shredded-plate leak (Defense channel active on an arc-armored target)
//   -> flat armor unless the hit ignores it (floor at MinDamageAfterArmor)
//   -> DamageTakenFactor (Vulnerability channel)
//   -> shield then health unless the hit ignores the shield (SplitShield; UDFHealthSet calls it).
// Every input is a row value, an attribute or a set-by-caller magnitude; the three that were
// literals in Step.cs (rear threshold 150 deg, shredded leak x1.35, post-armor floor 0.5) are
// DFDamageDefaults, overridable per hit by the Balance dials UDFDamageContext::ReadBalance reads.

namespace DFDamageDefaults
{
	/** Step.cs EnemyDef.CosRearThreshold: past this angle from the facing a hit is "from behind". */
	constexpr float RearArcDegrees = 150.f;
	/** Step.cs Damage(): shredded plating leaks — an arc-armored target under shred takes x1.35. */
	constexpr float ShreddedFrontArcFactor = 1.35f;
	/** Step.cs Damage(): flat armor never reduces a hit below this. */
	constexpr float MinDamageAfterArmor = 0.5f;
	/** B§1.6: Tether is refused at this Mass and above. */
	constexpr float TetherImmuneMass = 6.f;
}

UENUM(BlueprintType)
enum class EDFHitAspect : uint8
{
	None,   // no direction, or the target has no arc
	Front,  // inside the front arc: FrontArmorFactor applied
	Side,   // between the arc and the rear threshold: no factor
	Rear,   // behind the rear threshold: RearWeakFactor applied
};

USTRUCT(BlueprintType)
struct DFGAMEPLAY_API FDFDamageInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BaseDamage = 0.f;

	// Source factors.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DamageFactor = 1.f;          // UDFCombatSet on the source
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AmmoFactor = 1.f;            // FDFAmmoRow.DamageFactor
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float UnarmoredBonusFactor = 1.f;  // FDFAmmoRow.UnarmoredBonusFactor, only vs unarmored
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float WeakPointFactor = 1.f;       // FDFWeakPointRow.Factor of the zone hit
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PackAPunchFactor = 1.f;      // packDamagePerLevel ^ level
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIgnoresFlatArmor = false;    // ammo ap, poison
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIgnoresShield = false;       // poison

	// Target profile.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasHitDirection = false;
	/** dot(unit vector target->source, target facing); 1 = hit from straight ahead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float HitDot = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FrontArmorArcDegrees = 0.f;  // FDFEnemyRow
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FrontArmorFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RearWeakFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RearArcDegrees = DFDamageDefaults::RearArcDegrees;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FlatArmor = 0.f;             // UDFHealthSet on the target (shred already applied)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bTargetShredded = false;      // DF.Status.Channel.Defense on the target
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ShreddedFrontArcFactor = DFDamageDefaults::ShreddedFrontArcFactor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinDamageAfterArmor = DFDamageDefaults::MinDamageAfterArmor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DamageTakenFactor = 1.f;     // UDFHealthSet on the target
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Shield = 0.f;                // UDFHealthSet on the target (for the split preview)

	/** Step.cs: "armored" = flat armor or a front arc; hollow-point's bonus applies only when it is false. */
	bool IsArmored() const { return FlatArmor > 0.f || FrontArmorArcDegrees > 0.f; }
};

USTRUCT(BlueprintType)
struct DFGAMEPLAY_API FDFShieldSplit
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) float Absorbed = 0.f;
	UPROPERTY(BlueprintReadOnly) float ToHealth = 0.f;
};

USTRUCT(BlueprintType)
struct DFGAMEPLAY_API FDFDamageResult
{
	GENERATED_BODY()
	/** The amount that reaches the shield/health split (what the execution writes to IncomingDamage). */
	UPROPERTY(BlueprintReadOnly) float Damage = 0.f;
	UPROPERTY(BlueprintReadOnly) EDFHitAspect Aspect = EDFHitAspect::None;
	UPROPERTY(BlueprintReadOnly) float ShieldAbsorbed = 0.f;
	UPROPERTY(BlueprintReadOnly) float ToHealth = 0.f;
};

struct DFGAMEPLAY_API FDFDamageMath
{
	static FDFDamageResult Compute(const FDFDamageInput& In);

	/** Shield soaks first unless the hit ignores it; the remainder goes to health. */
	static FDFShieldSplit SplitShield(float Amount, float Shield, bool bIgnoresShield);

	/** Which aspect a hit lands on: cosine compare, no Acos, exactly as Step.cs. */
	static EDFHitAspect Aspect(float HitDot, float FrontArmorArcDegrees, float RearArcDegrees);

	/** dot(unit(SourceLocation - TargetLocation), TargetForward); returns 1 when the source sits on the target (DoT, reactions). */
	static float HitDot(const FVector& SourceLocation, const FVector& TargetLocation, const FVector& TargetForward);
};
