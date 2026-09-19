#include "Damage/DFDamageMath.h"

EDFHitAspect FDFDamageMath::Aspect(float HitDot, float FrontArmorArcDegrees, float RearThresholdDegrees)
{
	if (FrontArmorArcDegrees <= 0.f)
	{
		return EDFHitAspect::None;
	}
	// Cosine decreases as the angle grows, so the comparisons invert (Step.cs Damage()).
	const float CosHalfArc = FMath::Cos(FMath::DegreesToRadians(FrontArmorArcDegrees * 0.5f));
	const float CosRear = FMath::Cos(FMath::DegreesToRadians(RearThresholdDegrees));
	if (HitDot >= CosHalfArc)
	{
		return EDFHitAspect::Front;
	}
	if (HitDot <= CosRear)
	{
		return EDFHitAspect::Rear;
	}
	return EDFHitAspect::Side;
}

float FDFDamageMath::HitDot(const FVector& SourceLocation, const FVector& TargetLocation, const FVector& TargetForward)
{
	const FVector ToSource = SourceLocation - TargetLocation;
	// The sim skips the arc test when the source is on the target (DoT, reactions): 1 cm here.
	if (ToSource.SizeSquared() <= 1.f)
	{
		return 1.f;
	}
	return FVector::DotProduct(ToSource.GetSafeNormal(), TargetForward.GetSafeNormal());
}

FDFShieldSplit FDFDamageMath::SplitShield(float Amount, float Shield, bool bIgnoresShield)
{
	FDFShieldSplit Out;
	if (Amount <= 0.f)
	{
		return Out;
	}
	if (Shield > 0.f && !bIgnoresShield)
	{
		Out.Absorbed = FMath::Min(Shield, Amount);
	}
	Out.ToHealth = Amount - Out.Absorbed;
	return Out;
}

FDFDamageResult FDFDamageMath::Compute(const FDFDamageInput& In)
{
	FDFDamageResult Out;

	// 1. Source factors.
	float Amount = In.BaseDamage * In.DamageFactor * In.AmmoFactor * In.WeakPointFactor * In.PackAPunchFactor;
	if (!In.IsArmored())
	{
		Amount *= In.UnarmoredBonusFactor;
	}

	// 2. Directional armor (Aegis / Ram): reduced inside the front arc, amplified from behind.
	if (In.bHasHitDirection)
	{
		Out.Aspect = Aspect(In.HitDot, In.FrontArmorArcDegrees, In.RearThresholdDegrees);
		if (Out.Aspect == EDFHitAspect::Front)
		{
			Amount *= In.FrontArmorFactor;
		}
		else if (Out.Aspect == EDFHitAspect::Rear)
		{
			Amount *= In.RearWeakFactor;
		}
	}

	// 3. Vulnerability (mark) — before flat armor, as Step.cs orders it: the mark amplifies the
	//    hit and the armor then takes its flat bite out of the amplified number.
	Amount *= In.DamageTakenFactor;

	// 4. Shred on an arc-armored target: the plating leaks regardless of aspect.
	if (In.bTargetShredded && In.FrontArmorArcDegrees > 0.f)
	{
		Amount *= In.ShredFrontArcLeakFactor;
	}

	// 5. Flat armor per hit (the attribute already carries the shred delta), floored.
	if (In.FlatArmor > 0.f && Amount > 0.f && !In.bIgnoresFlatArmor)
	{
		Amount = FMath::Max(In.PostArmorDamageFloor, Amount - In.FlatArmor);
	}

	Out.Damage = FMath::Max(Amount, 0.f);

	// 6. Preview of the shield/health split (UDFHealthSet performs the real one).
	const FDFShieldSplit Split = SplitShield(Out.Damage, In.Shield, In.bIgnoresShield);
	Out.ShieldAbsorbed = Split.Absorbed;
	Out.ToHealth = Split.ToHealth;
	return Out;
}
