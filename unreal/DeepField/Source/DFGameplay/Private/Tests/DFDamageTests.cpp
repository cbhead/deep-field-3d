#include "Damage/DFDamageMath.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// C4 damage order with Appendix A1 numbers, no ASC.

namespace
{
	/** An aegis: 140 deg front arc x0.25, rear x1.5, facing +X, standing at the origin. */
	FDFDamageInput Aegis(float Base, const FVector& SourceLocation)
	{
		FDFDamageInput In;
		In.BaseDamage = Base;
		In.FrontArmorArcDegrees = 140.f;
		In.FrontArmorFactor = 0.25f;
		In.RearWeakFactor = 1.5f;
		In.bHasHitDirection = true;
		In.HitDot = FDFDamageMath::HitDot(SourceLocation, FVector::ZeroVector, FVector::ForwardVector);
		return In;
	}

	FVector FromAngle(float Degrees)
	{
		// A source standing 10 m away at Degrees off the target's facing (+X), in the ground plane.
		return FVector(FMath::Cos(FMath::DegreesToRadians(Degrees)), FMath::Sin(FMath::DegreesToRadians(Degrees)), 0.f) * 1000.f;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFDamageFrontArcAndRearTest, "DF.Unit.Damage.FrontArcAndRear", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFDamageFrontArcAndRearTest::RunTest(const FString& Parameters)
{
	// Lance bolt (8) into an aegis from straight ahead: inside the 140 deg arc -> x0.25 = 2.
	FDFDamageResult Front = FDFDamageMath::Compute(Aegis(8.f, FromAngle(0.f)));
	TestEqual(TEXT("front aspect"), Front.Aspect, EDFHitAspect::Front);
	TestTrue(TEXT("front: 8 x 0.25 = 2"), FMath::IsNearlyEqual(Front.Damage, 2.f, 1e-4f));

	// 69 deg off the nose is still inside the half-arc (70): armored.
	TestEqual(TEXT("69 deg is front"), FDFDamageMath::Compute(Aegis(8.f, FromAngle(69.f))).Aspect, EDFHitAspect::Front);
	// 71 deg is outside the arc but not behind the 150 deg rear threshold: full damage.
	FDFDamageResult Side = FDFDamageMath::Compute(Aegis(8.f, FromAngle(71.f)));
	TestEqual(TEXT("71 deg is side"), Side.Aspect, EDFHitAspect::Side);
	TestTrue(TEXT("side: 8"), FMath::IsNearlyEqual(Side.Damage, 8.f, 1e-4f));
	// 149 deg: still side.
	TestEqual(TEXT("149 deg is side"), FDFDamageMath::Compute(Aegis(8.f, FromAngle(149.f))).Aspect, EDFHitAspect::Side);
	// 151 deg and dead behind: rear x1.5 = 12.
	TestEqual(TEXT("151 deg is rear"), FDFDamageMath::Compute(Aegis(8.f, FromAngle(151.f))).Aspect, EDFHitAspect::Rear);
	FDFDamageResult Rear = FDFDamageMath::Compute(Aegis(8.f, FromAngle(180.f)));
	TestEqual(TEXT("rear aspect"), Rear.Aspect, EDFHitAspect::Rear);
	TestTrue(TEXT("rear: 8 x 1.5 = 12"), FMath::IsNearlyEqual(Rear.Damage, 12.f, 1e-4f));

	// The rear threshold is a dial: at 120 deg, 130 deg becomes rear.
	FDFDamageInput Dial = Aegis(8.f, FromAngle(130.f));
	Dial.RearThresholdDegrees = 120.f;
	TestEqual(TEXT("rear threshold from the dial"), FDFDamageMath::Compute(Dial).Aspect, EDFHitAspect::Rear);

	// No hit direction (DoT, reaction burst, splash centre): no aspect, no factor.
	FDFDamageInput Burst = Aegis(6.6f, FVector::ZeroVector);
	Burst.bHasHitDirection = false;
	FDFDamageResult NoDir = FDFDamageMath::Compute(Burst);
	TestEqual(TEXT("no direction: no aspect"), NoDir.Aspect, EDFHitAspect::None);
	TestTrue(TEXT("no direction: full 6.6"), FMath::IsNearlyEqual(NoDir.Damage, 6.6f, 1e-4f));
	// A source sitting on the target counts as straight ahead (HitDot 1) — the sim skipped the test; here it is Front.
	TestEqual(TEXT("HitDot on the target = 1"), FDFDamageMath::HitDot(FVector::ZeroVector, FVector::ZeroVector, FVector::ForwardVector), 1.f);

	// An unarmored target (drifter) ignores direction entirely.
	FDFDamageInput Drifter;
	Drifter.BaseDamage = 8.f;
	Drifter.bHasHitDirection = true;
	Drifter.HitDot = -1.f;
	FDFDamageResult Plain = FDFDamageMath::Compute(Drifter);
	TestEqual(TEXT("no arc: None"), Plain.Aspect, EDFHitAspect::None);
	TestTrue(TEXT("no arc: 8"), FMath::IsNearlyEqual(Plain.Damage, 8.f, 1e-4f));

	// Shred on an arc-armored target: the plating leaks x1.35 whatever the aspect (front 2 -> 2.7).
	FDFDamageInput Shredded = Aegis(8.f, FromAngle(0.f));
	Shredded.bTargetShredded = true;
	TestTrue(TEXT("shredded front: 8 x 0.25 x 1.35 = 2.7"), FMath::IsNearlyEqual(FDFDamageMath::Compute(Shredded).Damage, 2.7f, 1e-4f));
	Drifter.bTargetShredded = true;
	TestTrue(TEXT("shred leak needs an arc"), FMath::IsNearlyEqual(FDFDamageMath::Compute(Drifter).Damage, 8.f, 1e-4f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFDamageFlatArmorAndApTest, "DF.Unit.Damage.FlatArmorAndAp", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFDamageFlatArmorAndApTest::RunTest(const FString& Parameters)
{
	// Ram: flat armor 2. Sidearm 7 -> 5.
	FDFDamageInput Ram;
	Ram.BaseDamage = 7.f;
	Ram.FlatArmor = 2.f;
	TestTrue(TEXT("7 - 2 = 5"), FMath::IsNearlyEqual(FDFDamageMath::Compute(Ram).Damage, 5.f, 1e-4f));

	// AP ammo: x0.85 and ignores flat armor -> 5.95.
	FDFDamageInput Ap = Ram;
	Ap.AmmoFactor = 0.85f;
	Ap.bIgnoresFlatArmor = true;
	TestTrue(TEXT("ap: 7 x 0.85 = 5.95, no armor"), FMath::IsNearlyEqual(FDFDamageMath::Compute(Ap).Damage, 5.95f, 1e-4f));

	// Armor floors a hit at 0.5, never below (a 1-damage tick against 2 armor).
	FDFDamageInput Tiny = Ram;
	Tiny.BaseDamage = 1.f;
	TestTrue(TEXT("floored at 0.5"), FMath::IsNearlyEqual(FDFDamageMath::Compute(Tiny).Damage, 0.5f, 1e-4f));
	Tiny.PostArmorDamageFloor = 0.25f;
	TestTrue(TEXT("floor is a dial"), FMath::IsNearlyEqual(FDFDamageMath::Compute(Tiny).Damage, 0.25f, 1e-4f));

	// Hollow point: x1.3 only on unarmored targets. Drifter 7 -> 9.1; Ram (armored) 7 -> 5.
	FDFDamageInput Hp;
	Hp.BaseDamage = 7.f;
	Hp.UnarmoredBonusFactor = 1.3f;
	TestTrue(TEXT("hollow point on a drifter"), FMath::IsNearlyEqual(FDFDamageMath::Compute(Hp).Damage, 9.1f, 1e-4f));
	FDFDamageInput HpRam = Ram;
	HpRam.UnarmoredBonusFactor = 1.3f;
	TestTrue(TEXT("hollow point does nothing to a ram"), FMath::IsNearlyEqual(FDFDamageMath::Compute(HpRam).Damage, 5.f, 1e-4f));
	// An aegis (arc, no flat armor) counts as armored too.
	FDFDamageInput HpAegis = Hp;
	HpAegis.FrontArmorArcDegrees = 140.f;
	TestTrue(TEXT("an arc counts as armor"), FMath::IsNearlyEqual(FDFDamageMath::Compute(HpAegis).Damage, 7.f, 1e-4f));
	// Step.cs:545 reads `armored` off the enemy ROW, so shred cannot unarmor a ram for hollow point:
	// RowFlatArmor is the row, FlatArmor the shred-modified attribute (DF.Unit.Damage.ShredKeepsTargetArmored).
	FDFDamageInput HpShreddedRam = HpRam;
	HpShreddedRam.RowFlatArmor = 2.f;
	HpShreddedRam.FlatArmor = 0.f;
	TestTrue(TEXT("the row still says armored"), HpShreddedRam.IsArmored());
	TestTrue(TEXT("shredded ram takes 7, not 9.1"), FMath::IsNearlyEqual(FDFDamageMath::Compute(HpShreddedRam).Damage, 7.f, 1e-4f));

	// Source factors multiply before armor: DamageFactor 1.25 (glacier vs chilled) x weak point 2 x PaP 1.25.
	FDFDamageInput Stack = Ram;
	Stack.DamageFactor = 1.25f;
	Stack.WeakPointFactor = 2.f;
	Stack.PackAPunchFactor = 1.25f;
	TestTrue(TEXT("(7 x 1.25 x 2 x 1.25) - 2 = 19.875"), FMath::IsNearlyEqual(FDFDamageMath::Compute(Stack).Damage, 19.875f, 1e-4f));

	// Step.cs order: vulnerability BEFORE flat armor. Mark x1.25 on a ram: 7 x 1.25 - 2 = 6.75, not (7 - 2) x 1.25.
	FDFDamageInput Marked = Ram;
	Marked.DamageTakenFactor = 1.25f;
	TestTrue(TEXT("mark then armor: 6.75"), FMath::IsNearlyEqual(FDFDamageMath::Compute(Marked).Damage, 6.75f, 1e-4f));

	// Shred lowers the attribute the execution captures: FlatArmor 0 on a ram -> 7 (the leak needs an arc).
	FDFDamageInput ShredRam = Ram;
	ShredRam.FlatArmor = 0.f;
	ShredRam.bTargetShredded = true;
	TestTrue(TEXT("shredded ram takes the full 7"), FMath::IsNearlyEqual(FDFDamageMath::Compute(ShredRam).Damage, 7.f, 1e-4f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFDamageShieldBeforeHealthAndPoisonBypassTest, "DF.Unit.Damage.ShieldBeforeHealthAndPoisonBypass", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFDamageShieldBeforeHealthAndPoisonBypassTest::RunTest(const FString& Parameters)
{
	// Warden: 60 hp, shield 25. A 30 hit: shield soaks 25, health takes 5.
	FDFDamageInput Hit;
	Hit.BaseDamage = 30.f;
	Hit.Shield = 25.f;
	FDFDamageResult R = FDFDamageMath::Compute(Hit);
	TestTrue(TEXT("shield absorbs 25"), FMath::IsNearlyEqual(R.ShieldAbsorbed, 25.f, 1e-4f));
	TestTrue(TEXT("health takes 5"), FMath::IsNearlyEqual(R.ToHealth, 5.f, 1e-4f));

	// A 10 hit: all soaked.
	Hit.BaseDamage = 10.f;
	R = FDFDamageMath::Compute(Hit);
	TestTrue(TEXT("shield absorbs 10"), FMath::IsNearlyEqual(R.ShieldAbsorbed, 10.f, 1e-4f));
	TestTrue(TEXT("health takes 0"), FMath::IsNearlyEqual(R.ToHealth, 0.f, 1e-4f));

	// Poison: ignores armor and shield. A 3 dps tick (0.75 per 0.25 s) rots straight through.
	FDFDamageInput Poison;
	Poison.BaseDamage = 0.75f;
	Poison.Shield = 25.f;
	Poison.FlatArmor = 2.f;
	Poison.bIgnoresFlatArmor = true;
	Poison.bIgnoresShield = true;
	R = FDFDamageMath::Compute(Poison);
	TestTrue(TEXT("poison: no armor"), FMath::IsNearlyEqual(R.Damage, 0.75f, 1e-4f));
	TestTrue(TEXT("poison: shield untouched"), FMath::IsNearlyEqual(R.ShieldAbsorbed, 0.f, 1e-4f));
	TestTrue(TEXT("poison: health takes it all"), FMath::IsNearlyEqual(R.ToHealth, 0.75f, 1e-4f));

	// Burn would be soaked (it is refused at apply time anyway — DF.Unit.Status.BurnRejectedOnShield).
	FDFDamageInput Burn = Poison;
	Burn.bIgnoresFlatArmor = false;
	Burn.bIgnoresShield = false;
	Burn.BaseDamage = 1.5f;
	R = FDFDamageMath::Compute(Burn);
	TestTrue(TEXT("burn tick goes to the shield"), FMath::IsNearlyEqual(R.ShieldAbsorbed, 0.5f, 1e-4f));   // 1.5 - 2 armor -> floor 0.5

	// The split itself.
	const FDFShieldSplit Split = FDFDamageMath::SplitShield(40.f, 25.f, false);
	TestTrue(TEXT("split absorbed"), FMath::IsNearlyEqual(Split.Absorbed, 25.f, 1e-4f));
	TestTrue(TEXT("split to health"), FMath::IsNearlyEqual(Split.ToHealth, 15.f, 1e-4f));
	const FDFShieldSplit Bypass = FDFDamageMath::SplitShield(40.f, 25.f, true);
	TestTrue(TEXT("bypass absorbed 0"), FMath::IsNearlyEqual(Bypass.Absorbed, 0.f, 1e-4f));
	TestTrue(TEXT("bypass to health 40"), FMath::IsNearlyEqual(Bypass.ToHealth, 40.f, 1e-4f));
	const FDFShieldSplit None = FDFDamageMath::SplitShield(0.f, 25.f, false);
	TestTrue(TEXT("zero damage splits to nothing"), None.Absorbed == 0.f && None.ToHealth == 0.f);
	return true;
}

// Step.cs Damage() 2010-2066: arc -> vulnerability -> shred leak -> flat armor (floor 0.5) -> shield -> hp.
// gas.md had flat armor before vulnerability; the sim is the spec (ADR-0005).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFDamageOrderMatchesSimTest, "DF.Unit.Damage.OrderMatchesSim", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFDamageOrderMatchesSimTest::RunTest(const FString& Parameters)
{
	// The task's own example: mark x1.25 on 10 damage against 2 flat armor = 12.5 - 2 = 10.5, not (10 - 2) x 1.25 = 10.
	FDFDamageInput In;
	In.BaseDamage = 10.f;
	In.FlatArmor = 2.f;
	In.DamageTakenFactor = 1.25f;
	TestTrue(TEXT("mark before armor: 10.5"), FMath::IsNearlyEqual(FDFDamageMath::Compute(In).Damage, 10.5f, 1e-4f));

	// Arc before mark before shred leak before armor, on a marked, shredded ram hit from the front:
	// 10 x 0.35 (front) x 1.25 (mark) x 1.35 (shredded plating leaks) - 0 (shred took the 2 armor) = 5.90625.
	FDFDamageInput Ram;
	Ram.BaseDamage = 10.f;
	Ram.bHasHitDirection = true;
	Ram.HitDot = 1.f;
	Ram.FrontArmorArcDegrees = 150.f;
	Ram.FrontArmorFactor = 0.35f;
	Ram.RearWeakFactor = 2.2f;
	Ram.DamageTakenFactor = 1.25f;
	Ram.bTargetShredded = true;
	Ram.FlatArmor = 0.f;   // the Defense effect already drove the attribute 2 -> 0
	TestTrue(TEXT("front x mark x leak: 5.90625"), FMath::IsNearlyEqual(FDFDamageMath::Compute(Ram).Damage, 5.90625f, 1e-4f));
	// From behind without shred: 10 x 2.2 x 1.25 - 2 = 25.5.
	FDFDamageInput Rear = Ram;
	Rear.HitDot = -1.f;
	Rear.bTargetShredded = false;
	Rear.FlatArmor = 2.f;
	TestTrue(TEXT("rear x mark - armor: 25.5"), FMath::IsNearlyEqual(FDFDamageMath::Compute(Rear).Damage, 25.5f, 1e-4f));

	// The floor is applied after the mark: a 1.5 tick x 1.25 = 1.875 - 2 -> 0.5, and the shield split sees 0.5.
	FDFDamageInput Tick;
	Tick.BaseDamage = 1.5f;
	Tick.FlatArmor = 2.f;
	Tick.DamageTakenFactor = 1.25f;
	Tick.Shield = 25.f;
	const FDFDamageResult R = FDFDamageMath::Compute(Tick);
	TestTrue(TEXT("floored after mark"), FMath::IsNearlyEqual(R.Damage, 0.5f, 1e-4f));
	TestTrue(TEXT("shield soaks the floored tick"), FMath::IsNearlyEqual(R.ShieldAbsorbed, 0.5f, 1e-4f) && R.ToHealth == 0.f);

	// The three literals are dials with the sim's numbers as defaults.
	TestEqual(TEXT("rear threshold default 150"), DFDamageDefaults::RearThresholdDegrees, 150.f);
	TestEqual(TEXT("shred leak default 1.35"), DFDamageDefaults::ShredFrontArcLeakFactor, 1.35f);
	TestEqual(TEXT("post-armor floor default 0.5"), DFDamageDefaults::PostArmorDamageFloor, 0.5f);
	FDFDamageInput Dials = Ram;
	Dials.ShredFrontArcLeakFactor = 2.f;
	TestTrue(TEXT("leak dial honoured: 10 x 0.35 x 1.25 x 2 = 8.75"), FMath::IsNearlyEqual(FDFDamageMath::Compute(Dials).Damage, 8.75f, 1e-4f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
