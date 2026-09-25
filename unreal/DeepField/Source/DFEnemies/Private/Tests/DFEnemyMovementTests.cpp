#include "Misc/AutomationTest.h"
#include "Movement/DFEnemyMovement.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.Unit.EnemyMovement.* — the rules `Step.cs MoveEnemies` opens with, as pure functions. The
// component that gathers their inputs needs an actor and an ASC; the rules themselves must be
// provable without either, because what matters about them is an *order*, and an order is exactly
// what a spawned-actor test would obscure.

namespace DFEnemyMovementTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** A1's shade: 2.6 m/s, stealth, x1.45 unseen. */
	FDFEnemySpeedInputs Shade()
	{
		FDFEnemySpeedInputs In;
		In.RowSpeedMetersPerSec = 2.6f;
		In.bStealth = true;
		In.StealthSpeedBonus = 1.45f;
		return In;
	}

	/** A1's ram: 1.7 m/s, enrages below 35 % to x1.6. */
	FDFEnemySpeedInputs Ram()
	{
		FDFEnemySpeedInputs In;
		In.RowSpeedMetersPerSec = 1.7f;
		In.EnrageBelowHpFraction = 0.35f;
		In.EnrageSpeedFactor = 1.6f;
		return In;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFEnemySpeedOrderTest, "DF.Unit.EnemyMovement.SpeedRulesApplyInTheSimsOrder", DFEnemyMovementTest::Flags)
bool FDFEnemySpeedOrderTest::RunTest(const FString& Parameters)
{
	using namespace DFEnemyMovementTest;

	// Plain: the row's own speed, untouched.
	FDFEnemySpeedInputs Drifter;
	Drifter.RowSpeedMetersPerSec = 2.5f;
	TestEqual(TEXT("an unmodified drifter walks at its row speed"), UDFEnemyMovement::ResolveSpeed(Drifter), 2.5f);

	// Stealth pays only while unseen — that is what stops being ignored from being free.
	TestTrue(TEXT("an unrevealed shade is faster"), FMath::IsNearlyEqual(UDFEnemyMovement::ResolveSpeed(Shade()), 2.6f * 1.45f, 1e-4f));
	FDFEnemySpeedInputs Seen = Shade();
	Seen.bRevealed = true;
	TestTrue(TEXT("a revealed one is not"), FMath::IsNearlyEqual(UDFEnemyMovement::ResolveSpeed(Seen), 2.6f, 1e-4f));

	// The Movement channel multiplies whatever is left (chill 0.65).
	FDFEnemySpeedInputs Chilled = Shade();
	Chilled.StatusSpeedFactor = 0.65f;
	TestTrue(TEXT("chill multiplies the stealth-boosted speed"), FMath::IsNearlyEqual(UDFEnemyMovement::ResolveSpeed(Chilled), 2.6f * 1.45f * 0.65f, 1e-4f));

	// Enrage is a threshold on the row's own fraction, not a ramp.
	FDFEnemySpeedInputs Healthy = Ram();
	Healthy.HpFraction = 0.36f;
	TestTrue(TEXT("a ram above the threshold is not enraged"), FMath::IsNearlyEqual(UDFEnemyMovement::ResolveSpeed(Healthy), 1.7f, 1e-4f));
	FDFEnemySpeedInputs Hurt = Ram();
	Hurt.HpFraction = 0.35f;
	TestTrue(TEXT("at the threshold it is"), FMath::IsNearlyEqual(UDFEnemyMovement::ResolveSpeed(Hurt), 1.7f * 1.6f, 1e-4f));

	// **The order that matters.** Enrage is applied before the two that zero the speed, so a frozen
	// Ram is still frozen however badly it is losing — panic does not beat being frozen solid. If
	// enrage were applied last, or control first, this would be 2.72 rather than 0.
	FDFEnemySpeedInputs FrozenAndLosing = Hurt;
	FrozenAndLosing.bHardControlled = true;
	TestEqual(TEXT("a frozen, enraged ram does not move"), UDFEnemyMovement::ResolveSpeed(FrozenAndLosing), 0.f);

	FDFEnemySpeedInputs SiegingAndLosing = Hurt;
	SiegingAndLosing.bSieging = true;
	TestEqual(TEXT("nor does one busy demolishing something"), UDFEnemyMovement::ResolveSpeed(SiegingAndLosing), 0.f);

	// Everything at once, in the sim's order: 2.6 x 1.45 x 0.65, then enrage, then not stopped.
	FDFEnemySpeedInputs Everything = Shade();
	Everything.StatusSpeedFactor = 0.65f;
	Everything.EnrageBelowHpFraction = 0.5f;
	Everything.EnrageSpeedFactor = 2.f;
	Everything.HpFraction = 0.2f;
	TestTrue(TEXT("stealth, then status, then enrage"), FMath::IsNearlyEqual(UDFEnemyMovement::ResolveSpeed(Everything), 2.6f * 1.45f * 0.65f * 2.f, 1e-3f));

	// A speed can never come out negative, whatever content says.
	FDFEnemySpeedInputs Nonsense;
	Nonsense.RowSpeedMetersPerSec = -5.f;
	TestEqual(TEXT("a negative row speed is not a reverse gear"), UDFEnemyMovement::ResolveSpeed(Nonsense), 0.f);
	FDFEnemySpeedInputs NegativeFactor = Ram();
	NegativeFactor.StatusSpeedFactor = -1.f;
	TestEqual(TEXT("nor is a negative speed factor"), UDFEnemyMovement::ResolveSpeed(NegativeFactor), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFEnemyBurrowTest, "DF.Unit.EnemyMovement.BurrowCycleIsDistanceNotTime", DFEnemyMovementTest::Flags)
bool FDFEnemyBurrowTest::RunTest(const FString& Parameters)
{
	// balance.json: a 14 m cycle, 8 m of it under. Distance-based, so the cycle is the same at any
	// frame rate and survives a save — a time-based one would drift between host and client and
	// would put a Mole somewhere different after a resume.
	constexpr float Cycle = 14.f;
	constexpr float Under = 8.f;

	TestTrue(TEXT("a mole starts under"), UDFEnemyMovement::IsBurrowedAt(0.f, Cycle, Under));
	TestTrue(TEXT("still under just before the boundary"), UDFEnemyMovement::IsBurrowedAt(7.99f, Cycle, Under));
	TestFalse(TEXT("surfaced exactly at 8 m"), UDFEnemyMovement::IsBurrowedAt(8.f, Cycle, Under));
	TestFalse(TEXT("and stays up to the end of the cycle"), UDFEnemyMovement::IsBurrowedAt(13.99f, Cycle, Under));
	TestTrue(TEXT("under again on the next cycle"), UDFEnemyMovement::IsBurrowedAt(14.f, Cycle, Under));
	TestTrue(TEXT("and the cycle repeats far down the lane"), UDFEnemyMovement::IsBurrowedAt(14.f * 37.f + 3.f, Cycle, Under));
	TestFalse(TEXT("surfaced far down the lane too"), UDFEnemyMovement::IsBurrowedAt(14.f * 37.f + 9.f, Cycle, Under));

	// A knockback moves it backward through the cycle, because the cycle is a function of distance
	// rather than of elapsed time: shoved from 9 m (up) back to 3 m, a Mole is under again.
	TestFalse(TEXT("up at 9 m"), UDFEnemyMovement::IsBurrowedAt(9.f, Cycle, Under));
	TestTrue(TEXT("and under again once knocked back to 3 m"), UDFEnemyMovement::IsBurrowedAt(3.f, Cycle, Under));

	// Nothing burrows without both dials, and a negative distance is not a cycle position.
	TestFalse(TEXT("no cycle, no burrowing"), UDFEnemyMovement::IsBurrowedAt(3.f, 0.f, Under));
	TestFalse(TEXT("no burrowed span, no burrowing"), UDFEnemyMovement::IsBurrowedAt(3.f, Cycle, 0.f));
	TestTrue(TEXT("a negative distance clamps to the cycle start"), UDFEnemyMovement::IsBurrowedAt(-5.f, Cycle, Under));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
