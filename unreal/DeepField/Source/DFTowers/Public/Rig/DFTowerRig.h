#pragma once

#include "CoreMinimal.h"
#include "DFTowerRig.generated.h"

/**
 * C9 rig limits (rig.md): how far and how fast a turret turns. Canonical home DA_Tower_<id>.Rig,
 * copied from game/assets/structures/manifest.json `rig` blocks. Angles are degrees in the tower's
 * own frame: yaw 0 is the actor's forward, pitch 0 is level, up is positive.
 */
USTRUCT(BlueprintType)
struct DFTOWERS_API FDFTowerRigLimits
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rig") float YawMinDeg = -180.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rig") float YawMaxDeg = 180.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rig") float PitchMinDeg = -10.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rig") float PitchMaxDeg = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rig") float TraverseDegPerSec = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rig") float ElevateDegPerSec = 60.f;
	/** Turns the whole way round, taking the short way across the +-180 seam. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rig") bool bUnlimitedYaw = true;
	/** Multiplies the pitch applied to the Pitch part: -1 for a mesh whose barrel rises with a negative
	 *  Unreal pitch. The manifest's pitchSign (-1 everywhere) is Godot's convention, not this one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rig") float PitchSign = 1.f;
};

/** The rig math, pure, so the component, the HUD and the tests share one answer. */
namespace DFTowerRig
{
	/** Aura / support spin (rig.md): idle and engaged rates, degrees per second. */
	constexpr float SpinIdleDegPerSec = 25.f;
	constexpr float SpinEngagedDegPerSec = 110.f;
	/** Night: a turret searches this long before it locks on a new target (rig.md). */
	constexpr float SearchWobbleSeconds = 0.2f;
	/** Within this many degrees of the (clamped) aim on both axes, the turret is settled. */
	constexpr float SettledToleranceDeg = 0.5f;

	/**
	 * The manifest's rig block for a turreted tower until DA_Tower_<id> carries it (WS-30's importer):
	 * lance +-180 / -8..26 / 90 / 60, nova +-180 / 22..70 / 45 / 30, skywatch +-180 / 6..82 / 200 / 140,
	 * filament +-180 / -12..78 / 220 / 160 (rig.md). Anything else gets the struct's defaults.
	 */
	DFTOWERS_API FDFTowerRigLimits ManifestLimitsFor(FName TowerId);

	/** Yaw and pitch (degrees, in the tower's frame) that point from PivotCm at TargetCm, for a tower
	 *  whose actor yaw is TowerYawDeg. Yaw in (-180, 180]. X = yaw, Y = pitch. */
	DFTOWERS_API FVector2D DesiredAngles(const FVector& PivotCm, float TowerYawDeg, const FVector& TargetCm);

	/** The aim clamped to what the rig can reach (a limited yaw, the pitch band). */
	DFTOWERS_API FVector2D ClampToLimits(const FVector2D& Desired, const FDFTowerRigLimits& Limits);

	/**
	 * One step of the turret toward Desired: yaw at TraverseDegPerSec, pitch at ElevateDegPerSec, each
	 * clamped to the limits; an unlimited yaw takes the short way across the seam, a limited one never
	 * crosses it. True when settled on the clamped aim (within SettledToleranceDeg on both axes).
	 */
	DFTOWERS_API bool Slew(float& YawDeg, float& PitchDeg, const FVector2D& Desired, const FDFTowerRigLimits& Limits, float DeltaSeconds);
}
