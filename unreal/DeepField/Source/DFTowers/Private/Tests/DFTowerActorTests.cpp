#include "Combat/DFTargetable.h"
#include "Content/DFContentSubsystem.h"
#include "DFTowerTestDummy.h"
#include "Misc/AutomationTest.h"
#include "Testing/DFTestUtils.h"
#include "Towers/DFTargetingComponent.h"
#include "Towers/DFTower.h"
#include "Towers/DFTowerMath.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.Unit.Tower.* (world half) — the targeting component over the registry, and ADFTower's fire loop.
// Test worlds have no game mode, so nothing ticks on its own: the tower is stepped by hand
// (StepTower), as DFWaveDirectorTests does with the director. A test world has no geometry, so sight
// traces never hit terrain; the Monolith rule is exercised through a sight-blocking dummy.

namespace DFTowerActorTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	ADFTowerTestDummy* Spawn(FDFTestWorld& World, int32 Id, float XMeters, float Remaining)
	{
		ADFTowerTestDummy* Dummy = World.SpawnActor<ADFTowerTestDummy>(FTransform(FVector(XMeters * 100.f, 0.f, 0.f)));
		if (Dummy)
		{
			Dummy->Id = Id;
			Dummy->Remaining = Remaining;
			UDFTargetRegistry::Get(World.GetWorld())->Register(Dummy);
		}
		return Dummy;
	}

	bool ContentReady(FAutomationTestBase& Test, FDFTestWorld& World)
	{
		UDFContentSubsystem* Content = World.GetSubsystem<UDFContentSubsystem>();
		if (!Test.TestNotNull(TEXT("content subsystem"), Content))
		{
			return false;
		}
		return Content->IsReady() || Test.TestTrue(TEXT("tables load"), Content->LoadTables());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerShotFlightTest, "DF.Unit.Tower.ShotFlightAndSplash", DFTowerActorTest::Flags)
bool FDFTowerShotFlightTest::RunTest(const FString& Parameters)
{
	using namespace DFTowerMath;
	// Step.cs StepTowerProjectiles: move speed x dt at the aim point; land within step + hit radius.
	FDFTowerShot Shot;
	Shot.SpeedMetersPerSecond = 30.f;
	const FVector Aim(1000.f, 0.f, 0.f);                          // 10 m away
	TestFalse(TEXT("1/30 s at 30 m/s is a 1 m step: 10 m away does not land"), AdvanceShot(Shot, Aim, 1.f / 30.f, 0.4f));
	TestEqual(TEXT("and it moved 1 m toward the aim"), Shot.PositionCm.X, 100.0, 1e-3);
	Shot.PositionCm = FVector(860.f, 0.f, 0.f);                  // 1.4 m out: step 1 m + radius 0.4 m
	TestTrue(TEXT("within step + hit radius: lands"), AdvanceShot(Shot, Aim, 1.f / 30.f, 0.4f));
	TestEqual(TEXT("a landing round does not move"), Shot.PositionCm.X, 860.0, 1e-3);

	// Nova: radius 3.2 m, falloff 0.35.
	TestEqual(TEXT("full damage at the centre"), SplashFactor(0.f, 3.2f, 0.35f), 1.f, 1e-5f);
	TestEqual(TEXT("the falloff at the rim"), SplashFactor(3.2f, 3.2f, 0.35f), 0.35f, 1e-5f);
	TestEqual(TEXT("halfway: 1 - 0.65 x 0.5"), SplashFactor(1.6f, 3.2f, 0.35f), 0.675f, 1e-5f);
	TestEqual(TEXT("beyond the rim: not hit"), SplashFactor(3.3f, 3.2f, 0.35f), 0.f, 1e-5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerLazySightTest, "DF.Unit.Tower.SightOnlyTracedForContenders", DFTowerActorTest::Flags)
bool FDFTowerLazySightTest::RunTest(const FString& Parameters)
{
	using namespace DFTowerMath;
	FDFTowerRow Row;
	Row.TargetLayers = { EDFEnemyLayer::Ground };
	TArray<FDFTargetCandidate> C;
	for (int32 i = 0; i < 6; ++i)
	{
		FDFTargetCandidate& E = C.AddDefaulted_GetRef();
		E.Id = i;
		E.PositionCm = FVector(100.f * (i + 1), 0.f, 0.f);
		E.RemainingToCore = 100.f - i;   // later candidates are closer to the core: every one is a contender
	}
	C[2].Layer = EDFEnemyLayer::Air;      // filtered before sight
	C[4].PositionCm.X = 5000.f;           // out of range before sight
	TArray<int32> Traced;
	const int32 Picked = PickTarget(Row, FVector::ZeroVector, 12.f, C, [&Traced](int32 i) { Traced.Add(i); return i == 5; });
	TestEqual(TEXT("candidate 5 is sight-blocked, so 3 wins"), Picked, 3);
	TestFalse(TEXT("the flyer was never traced"), Traced.Contains(2));
	TestFalse(TEXT("the out-of-range one was never traced"), Traced.Contains(4));

	// Same answer as the flag form, which is the sim's order.
	C[5].bSightBlocked = true;
	TestEqual(TEXT("the flag form agrees"), PickTarget(Row, FVector::ZeroVector, 12.f, C), 3);

	// A candidate that cannot beat the best so far is not traced.
	TArray<FDFTargetCandidate> D = C;
	D[5].RemainingToCore = 1000.f;
	Traced.Reset();
	PickTarget(Row, FVector::ZeroVector, 12.f, D, [&Traced](int32 i) { Traced.Add(i); return false; });
	TestFalse(TEXT("a loser is never traced"), Traced.Contains(5));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerTargetingComponentTest, "DF.Unit.Tower.TargetingOverRegistry", DFTowerActorTest::Flags)
bool FDFTowerTargetingComponentTest::RunTest(const FString& Parameters)
{
	using namespace DFTowerActorTest;
	FDFTestWorld World;
	AActor* Tower = World.SpawnActor<ADFTowerTestDummy>();   // any actor will do as the owner
	UDFTargetingComponent* Targeting = NewObject<UDFTargetingComponent>(Tower);
	Targeting->RegisterComponent();

	FDFTowerRow Row;
	Row.TargetLayers = { EDFEnemyLayer::Ground };
	ADFTowerTestDummy* Far = Spawn(World, 1, 8.f, 40.f);
	ADFTowerTestDummy* Near = Spawn(World, 2, 5.f, 20.f);
	ADFTowerTestDummy* Other = Spawn(World, 3, 6.f, 30.f);
	if (!TestNotNull(TEXT("dummies"), Far) || !TestNotNull(TEXT("dummies"), Near) || !TestNotNull(TEXT("dummies"), Other))
	{
		return false;
	}
	TestTrue(TEXT("the least left to walk wins"), Targeting->PickTarget(Row, 12.f) == Near);

	Near->bStealth = true;
	TestTrue(TEXT("stealth without a status component is never detected"), Targeting->PickTarget(Row, 12.f) == Other);
	Near->bStealth = false;

	// A Monolith 2 m along the line to Other, half a metre off it: Other is shielded.
	ADFTowerTestDummy* Monolith = Spawn(World, 9, 3.f, 999.f);
	Monolith->SetActorLocation(FVector(300.f, 50.f, 0.f));
	Monolith->bBlocksSight = true;
	Near->bDead = true;
	// Other (6 m) and Far (8 m) are both on the line behind it, so both are shielded, and the Monolith
	// itself, in the open, is what remains (its own body never blocks its own sight line).
	TestTrue(TEXT("everything behind a sight blocker is shielded: the blocker is the target"), Targeting->PickTarget(Row, 12.f) == Monolith);
	Near->bDead = false;
	UDFTargetRegistry::Get(World.GetWorld())->Unregister(Monolith);
	TestTrue(TEXT("with the blocker gone, the least left to walk wins again"), Targeting->PickTarget(Row, 12.f) == Near);

	UDFTargetRegistry::Get(World.GetWorld())->Unregister(Near);
	TestTrue(TEXT("an unregistered body is invisible to towers"), Targeting->PickTarget(Row, 12.f) == Other);

	// Night: the first shot at a new target waits; the same target does not wait again.
	FDFConditionRow Night;
	Night.AcquisitionDelaySeconds = 0.2f;
	Night.bAcquisitionDelayExemptsMarked = true;
	TestEqual(TEXT("a new target at night: 0.2 s"), Targeting->NoteTarget(Other, &Night), 0.2f, 1e-5f);
	TestEqual(TEXT("the same target: no delay"), Targeting->NoteTarget(Other, &Night), 0.f, 1e-5f);
	TestEqual(TEXT("losing the target clears the memory"), Targeting->NoteTarget(nullptr, &Night), 0.f, 1e-5f);
	TestEqual(TEXT("so the same one is new again"), Targeting->NoteTarget(Other, &Night), 0.2f, 1e-5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerFireLoopTest, "DF.Unit.Tower.LanceFiresAndLands", DFTowerActorTest::Flags)
bool FDFTowerFireLoopTest::RunTest(const FString& Parameters)
{
	using namespace DFTowerActorTest;
	FDFTestWorld World;
	if (!ContentReady(*this, World))
	{
		return false;
	}
	ADFTower* Tower = World.SpawnActor<ADFTower>();
	if (!TestNotNull(TEXT("tower"), Tower) || !TestTrue(TEXT("lance from the content tables"), Tower->InitializeTower(TEXT("lance"), TEXT("g1"), 1, 75)))
	{
		return false;
	}
	TestEqual(TEXT("three paths, no purchases"), Tower->GetPathLevels().Num(), 3);
	TestEqual(TEXT("range 12 m in clear weather"), Tower->GetRangeMeters(), 12.f, 1e-4f);
	Tower->SetActiveCondition(TEXT("fog"));
	TestEqual(TEXT("8.4 m in fog"), Tower->GetRangeMeters(), 8.4f, 1e-4f);
	Tower->SetActiveCondition(NAME_None);

	int32 Fired = 0;
	int32 Landed = 0;
	Tower->OnFired.AddLambda([&Fired](ADFTower*, AActor*) { ++Fired; });
	Tower->OnShotLanded.AddLambda([&Landed](ADFTower*, AActor*) { ++Landed; });

	Tower->StepTower(1.f / 30.f);
	TestEqual(TEXT("nothing to shoot: no fire"), Fired, 0);
	ADFTowerTestDummy* Drifter = Spawn(World, 1, 6.f, 30.f);
	Tower->StepTower(1.f / 30.f);
	TestEqual(TEXT("a body in range: one round"), Fired, 1);
	TestTrue(TEXT("the tower reports it as its target"), Tower->GetCurrentTarget() == Drifter);
	TestEqual(TEXT("one round in flight"), Tower->GetShotsInFlight(), 1);

	// 6 m at 30 m/s from 1.5 m up to 0.8 m up: lands within ~0.2 s. Step 10 frames.
	for (int32 i = 0; i < 10; ++i)
	{
		Tower->StepTower(1.f / 30.f);
	}
	TestEqual(TEXT("the round landed"), Landed, 1);
	TestEqual(TEXT("and only one was fired inside the 0.625 s cooldown"), Fired, 1);

	for (int32 i = 0; i < 12; ++i)   // past 1 / 1.6 s
	{
		Tower->StepTower(1.f / 30.f);
	}
	TestEqual(TEXT("the cooldown elapsed: a second round"), Fired, 2);

	// The second round left around step 20 and needs ~5 steps to arrive, so it is in flight now. Kill its
	// target: a round whose target dies before it arrives vanishes and lands nowhere (Step.cs).
	if (TestTrue(TEXT("the second round is in flight"), Tower->GetShotsInFlight() > 0))
	{
		const int32 LandedBefore = Landed;
		Drifter->bDead = true;
		for (int32 i = 0; i < 10; ++i)
		{
			Tower->StepTower(1.f / 30.f);
		}
		TestEqual(TEXT("a round whose target died lands nowhere"), Landed, LandedBefore);
		TestEqual(TEXT("and is gone"), Tower->GetShotsInFlight(), 0);
		TestEqual(TEXT("and nothing dead is fired at"), Fired, 2);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
