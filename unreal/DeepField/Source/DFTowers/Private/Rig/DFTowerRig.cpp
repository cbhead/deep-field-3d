#include "Rig/DFTowerRig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DFTowerRig)

namespace DFTowerRig
{
	namespace
	{
		FDFTowerRigLimits Make(float PitchMin, float PitchMax, float Traverse, float Elevate, float PitchSign)
		{
			FDFTowerRigLimits L;
			L.YawMinDeg = -180.f;
			L.YawMaxDeg = 180.f;
			L.bUnlimitedYaw = true;
			L.PitchMinDeg = PitchMin;
			L.PitchMaxDeg = PitchMax;
			L.TraverseDegPerSec = Traverse;
			L.ElevateDegPerSec = Elevate;
			L.PitchSign = PitchSign;
			return L;
		}

		/** Move Current toward Target by at most MaxStep (both plain numbers, no wrap). */
		float StepToward(float Current, float Target, float MaxStep)
		{
			return Current + FMath::Clamp(Target - Current, -MaxStep, MaxStep);
		}
	}

	FDFTowerRigLimits ManifestLimitsFor(FName TowerId)
	{
		// game/assets/structures/manifest.json `rig` blocks. Every block says pitchSign -1: that is Godot's
		// convention (a positive X rotation tips the barrel down). Unreal's pitch is + up already, so the
		// sign here is +1; a mesh authored the other way round sets it in its DA_Tower_<id>.
		if (TowerId == TEXT("lance"))    { return Make(-8.f, 26.f, 90.f, 60.f, 1.f); }
		if (TowerId == TEXT("nova"))     { return Make(22.f, 70.f, 45.f, 30.f, 1.f); }
		if (TowerId == TEXT("skywatch")) { return Make(6.f, 82.f, 200.f, 140.f, 1.f); }
		if (TowerId == TEXT("filament")) { return Make(-12.f, 78.f, 220.f, 160.f, 1.f); }
		return FDFTowerRigLimits();
	}

	FVector2D DesiredAngles(const FVector& PivotCm, float TowerYawDeg, const FVector& TargetCm)
	{
		const FVector To = TargetCm - PivotCm;
		const double Flat = FVector2D(To.X, To.Y).Size();
		if (Flat < UE_KINDA_SMALL_NUMBER && FMath::Abs(To.Z) < UE_KINDA_SMALL_NUMBER)
		{
			return FVector2D::ZeroVector;
		}
		const float WorldYaw = FMath::RadiansToDegrees(static_cast<float>(FMath::Atan2(To.Y, To.X)));
		const float Yaw = FRotator::NormalizeAxis(WorldYaw - TowerYawDeg);
		const float Pitch = FMath::RadiansToDegrees(static_cast<float>(FMath::Atan2(To.Z, Flat)));
		return FVector2D(Yaw, Pitch);
	}

	FVector2D ClampToLimits(const FVector2D& Desired, const FDFTowerRigLimits& Limits)
	{
		const float Yaw = Limits.bUnlimitedYaw
			? FRotator::NormalizeAxis(static_cast<float>(Desired.X))
			: FMath::Clamp(static_cast<float>(Desired.X), Limits.YawMinDeg, Limits.YawMaxDeg);
		const float Pitch = FMath::Clamp(static_cast<float>(Desired.Y), Limits.PitchMinDeg, Limits.PitchMaxDeg);
		return FVector2D(Yaw, Pitch);
	}

	bool Slew(float& YawDeg, float& PitchDeg, const FVector2D& Desired, const FDFTowerRigLimits& Limits, float DeltaSeconds)
	{
		const FVector2D Aim = ClampToLimits(Desired, Limits);
		const float YawStep = FMath::Max(0.f, Limits.TraverseDegPerSec) * DeltaSeconds;
		const float PitchStep = FMath::Max(0.f, Limits.ElevateDegPerSec) * DeltaSeconds;

		float YawRemaining = 0.f;
		if (Limits.bUnlimitedYaw)
		{
			const float Delta = FMath::FindDeltaAngleDegrees(YawDeg, static_cast<float>(Aim.X));   // the short way
			YawDeg = FRotator::NormalizeAxis(YawDeg + FMath::Clamp(Delta, -YawStep, YawStep));
			YawRemaining = FMath::FindDeltaAngleDegrees(YawDeg, static_cast<float>(Aim.X));
		}
		else
		{
			YawDeg = FMath::Clamp(StepToward(YawDeg, static_cast<float>(Aim.X), YawStep), Limits.YawMinDeg, Limits.YawMaxDeg);
			YawRemaining = static_cast<float>(Aim.X) - YawDeg;
		}
		PitchDeg = FMath::Clamp(StepToward(PitchDeg, static_cast<float>(Aim.Y), PitchStep), Limits.PitchMinDeg, Limits.PitchMaxDeg);
		const float PitchRemaining = static_cast<float>(Aim.Y) - PitchDeg;
		return FMath::Abs(YawRemaining) <= SettledToleranceDeg && FMath::Abs(PitchRemaining) <= SettledToleranceDeg;
	}
}
