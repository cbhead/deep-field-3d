#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Content/DFContentRows.h"
#include "DFHeroCharacter.h"
#include "DFWeaponTestTarget.h"
#include "DFWorldCollision.h"
#include "Engine/CollisionProfile.h"
#include "EnhancedActionKeyMapping.h"
#include "GameFramework/WorldSettings.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Testing/DFTestUtils.h"
#include "Weapons/DFHeroWeaponComponent.h"
#include "Weapons/DFWeaponRules.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.Unit.Weapon.* — the hero's gun (B§1.2) against the sim (Step.cs ApplyPlayerHit, BeginReload and the
// reload timer) and the Godot trigger (Player.cs TickWeapons). Rows are the Appendix A1 literals
// (weapons.json: rifle 15 / 2.2 / 80 m / 24 / 1.9 s automatic, sidearm 7 / 3.0 / 60 m / 12 / 1.3 s),
// never WS-01's tables.

namespace DFHeroWeaponTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Tol = 1e-3f;

	FDFWeaponRow RifleRow()
	{
		FDFWeaponRow Row;
		Row.Damage = 15.f;
		Row.ShotsPerSecond = 2.2f;
		Row.RangeMeters = 80.f;
		Row.Applies = { TEXT("mark") };
		Row.MagazineSize = 24;
		Row.ReloadSeconds = 1.9f;
		Row.bAutomatic = true;
		return Row;
	}

	FDFWeaponRow SidearmRow()
	{
		FDFWeaponRow Row;
		Row.Damage = 7.f;
		Row.ShotsPerSecond = 3.f;
		Row.RangeMeters = 60.f;
		Row.MagazineSize = 12;
		Row.ReloadSeconds = 1.3f;
		Row.bAutomatic = false;
		return Row;
	}

	/** Runs the owner's gun for Frames steps of Dt with the trigger as given; counts what happened. */
	struct FRun
	{
		int32 Shots = 0;
		int32 ReloadsStarted = 0;
		int32 Reloads = 0;
		void Add(const FDFWeaponStep& Step)
		{
			Shots += Step.bFired ? 1 : 0;
			ReloadsStarted += Step.bReloadStarted ? 1 : 0;
			Reloads += Step.bReloaded ? 1 : 0;
		}
	};

	/** A press at t = 0 (a step of 0 s, as UDFHeroWeaponComponent::SetTriggerHeld runs it), then Frames - 1 held steps of Dt. */
	FRun HoldFor(FDFWeaponState& State, const FDFWeaponRules& Rules, int32 Frames, float Dt)
	{
		FRun Run;
		Run.Add(DFWeapon::Step(State, Rules, 0.f, true));
		for (int32 i = 1; i < Frames; ++i)
		{
			Run.Add(DFWeapon::Step(State, Rules, Dt, true));
		}
		return Run;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWeaponRulesFromRowTest, "DF.Unit.Weapon.RulesFromRow", DFHeroWeaponTest::Flags)
bool FDFWeaponRulesFromRowTest::RunTest(const FString&)
{
	using namespace DFHeroWeaponTest;
	const FDFWeaponRules Rules = DFWeapon::RulesFromRow(RifleRow());
	TestEqual(TEXT("damage 15"), Rules.Damage, 15.f, Tol);
	TestEqual(TEXT("2.2 shots a second"), Rules.ShotsPerSecond, 2.2f, Tol);
	TestEqual(TEXT("range 80 m, in cm"), Rules.RangeCm, 8000.f, Tol);
	TestEqual(TEXT("magazine 24"), Rules.MagazineSize, 24);
	TestEqual(TEXT("reload 1.9 s"), Rules.ReloadSeconds, 1.9f, Tol);
	TestTrue(TEXT("automatic"), Rules.bAutomatic);
	TestEqual(TEXT("a shot every 1/2.2 s"), DFWeapon::ShotInterval(Rules), 1.f / 2.2f, 1e-5f);

	const FDFWeaponState State = DFWeapon::Loaded(Rules);
	TestEqual(TEXT("loaded: a full magazine"), State.Rounds, 24);
	TestFalse(TEXT("loaded: not reloading"), DFWeapon::IsReloading(State));
	TestEqual(TEXT("loaded: ready to fire"), State.Cooldown, 0.f, Tol);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWeaponAutomaticRateTest, "DF.Unit.Weapon.AutomaticRate", DFHeroWeaponTest::Flags)
bool FDFWeaponAutomaticRateTest::RunTest(const FString&)
{
	using namespace DFHeroWeaponTest;
	const FDFWeaponRules Rules = DFWeapon::RulesFromRow(RifleRow());

	// The press fires at once: nothing waits for the next tick.
	FDFWeaponState State = DFWeapon::Loaded(Rules);
	TestTrue(TEXT("the press fires the first round"), DFWeapon::Step(State, Rules, 0.f, true).bFired);
	TestEqual(TEXT("and spends it"), State.Rounds, 23);
	TestFalse(TEXT("held, the next frame does not fire again"), DFWeapon::Step(State, Rules, 1.f / 60.f, true).bFired);

	// Held for 10 s: shots at 0, 1/2.2, 2/2.2, ... — 22 of them before 10 s, whatever the frame rate,
	// because the cooldown carries the part of a frame it overran into the next shot.
	for (const float Fps : { 60.f, 30.f, 144.f, 7.f })
	{
		FDFWeaponState Held = DFWeapon::Loaded(Rules);
		const int32 Frames = FMath::FloorToInt32(10.f * Fps - 0.5f) + 1;   // the last step lands just short of 10 s
		const FRun Run = HoldFor(Held, Rules, Frames, 1.f / Fps);
		TestEqual(*FString::Printf(TEXT("%.0f fps: 22 shots in 10 s at 2.2 a second"), Fps), Run.Shots, 22);
		TestEqual(*FString::Printf(TEXT("%.0f fps: 22 rounds spent"), Fps), Held.Rounds, 2);
	}

	// A trigger left up does not bank shots: after a long pause the next press fires once, then waits.
	FDFWeaponState Rested = DFWeapon::Loaded(Rules);
	DFWeapon::Step(Rested, Rules, 0.f, true);
	for (int32 i = 0; i < 300; ++i)
	{
		DFWeapon::Step(Rested, Rules, 1.f / 60.f, false);
	}
	const FRun AfterRest = HoldFor(Rested, Rules, 27, 1.f / 60.f);   // 0.43 s: less than one interval after the press
	TestEqual(TEXT("5 s of rest, then 0.43 s held: one shot, no burst"), AfterRest.Shots, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWeaponSemiAutomaticTest, "DF.Unit.Weapon.SemiAutomatic", DFHeroWeaponTest::Flags)
bool FDFWeaponSemiAutomaticTest::RunTest(const FString&)
{
	using namespace DFHeroWeaponTest;
	const FDFWeaponRules Rules = DFWeapon::RulesFromRow(SidearmRow());

	// Player.cs: a semi-automatic gun wants a click each; holding it fires once.
	FDFWeaponState Held = DFWeapon::Loaded(Rules);
	TestEqual(TEXT("held for 3 s: one shot"), HoldFor(Held, Rules, 180, 1.f / 60.f).Shots, 1);

	// Clicked every other frame for 2 s: the cooldown (1/3 s) still caps it at 3 a second, and a
	// click during the cooldown is lost, as in Godot.
	FDFWeaponState Clicked = DFWeapon::Loaded(Rules);
	FRun Run;
	for (int32 i = 0; i < 120; ++i)
	{
		Run.Add(DFWeapon::Step(Clicked, Rules, 1.f / 60.f, (i % 2) == 0));
	}
	TestTrue(*FString::Printf(TEXT("clicked for 2 s: 6 or 7 shots at 3 a second (%d)"), Run.Shots), Run.Shots >= 6 && Run.Shots <= 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWeaponMagazineReloadTest, "DF.Unit.Weapon.MagazineAndReload", DFHeroWeaponTest::Flags)
bool FDFWeaponMagazineReloadTest::RunTest(const FString&)
{
	using namespace DFHeroWeaponTest;
	const FDFWeaponRules Rules = DFWeapon::RulesFromRow(RifleRow());
	constexpr float Dt = 1.f / 60.f;

	// Empty the magazine with the trigger held: 24 shots, the last at 23 intervals (10.45 s).
	FDFWeaponState State = DFWeapon::Loaded(Rules);
	const FRun Run = HoldFor(State, Rules, 629, Dt);   // to 10.47 s
	TestEqual(TEXT("24 shots empty the magazine"), Run.Shots, 24);
	TestEqual(TEXT("empty"), State.Rounds, 0);
	TestEqual(TEXT("no reload yet: the dry pull waits for the cooldown, as Step.cs does"), Run.ReloadsStarted, 0);

	// Still held: the next pull finds the magazine empty and starts the reload instead of firing.
	FRun Dry;
	for (int32 i = 0; i < 40 && Dry.ReloadsStarted == 0; ++i)
	{
		Dry.Add(DFWeapon::Step(State, Rules, Dt, true));
	}
	TestEqual(TEXT("an empty magazine's pull starts the reload"), Dry.ReloadsStarted, 1);
	TestEqual(TEXT("and fires nothing"), Dry.Shots, 0);
	TestTrue(TEXT("reloading"), DFWeapon::IsReloading(State));
	TestEqual(TEXT("for the row's 1.9 s"), State.ReloadLeft, 1.9f, Tol);

	// Busy hands: nothing fires during the reload, though the trigger is held; on the step it ends the
	// magazine is full and the held trigger fires again.
	int32 ShotsWhileReloading = 0;
	int32 StepsToReload = 0;
	FDFWeaponStep Last;
	while (StepsToReload < 200)
	{
		Last = DFWeapon::Step(State, Rules, Dt, true);
		++StepsToReload;
		if (Last.bReloaded)
		{
			break;
		}
		ShotsWhileReloading += Last.bFired ? 1 : 0;
	}
	TestEqual(TEXT("nothing fires while reloading"), ShotsWhileReloading, 0);
	TestTrue(TEXT("the reload ends"), Last.bReloaded);
	TestEqual(TEXT("after 1.9 s"), static_cast<float>(StepsToReload) * Dt, 1.9f, Dt + Tol);
	TestTrue(TEXT("the held trigger fires on the step it ends"), Last.bFired);
	TestEqual(TEXT("a full magazine less that shot"), State.Rounds, 23);
	TestFalse(TEXT("not reloading"), DFWeapon::IsReloading(State));

	// R: not when full, and a partial magazine keeps its rounds until the reload ends.
	FDFWeaponState Manual = DFWeapon::Loaded(Rules);
	TestFalse(TEXT("no reload with a full magazine (Step.cs BeginReload)"), DFWeapon::BeginReload(Manual, Rules));
	Manual.Rounds = 19;
	TestTrue(TEXT("a reload with 19 of 24"), DFWeapon::BeginReload(Manual, Rules));
	TestFalse(TEXT("no second reload while reloading"), DFWeapon::BeginReload(Manual, Rules));
	TestFalse(TEXT("halfway: not done"), DFWeapon::AdvanceReload(Manual, Rules, 0.95f));
	TestEqual(TEXT("halfway: the fraction"), DFWeapon::ReloadFraction(Manual, Rules), 0.5f, Tol);
	TestEqual(TEXT("halfway: the magazine still holds 19"), Manual.Rounds, 19);
	TestFalse(TEXT("halfway: the trigger does nothing"), DFWeapon::Step(Manual, Rules, 0.f, true).bFired);
	TestTrue(TEXT("done at 1.9 s"), DFWeapon::AdvanceReload(Manual, Rules, 0.95f));
	TestEqual(TEXT("done: 24"), Manual.Rounds, 24);
	TestEqual(TEXT("done: fraction back to 0"), DFWeapon::ReloadFraction(Manual, Rules), 0.f, Tol);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWeaponHostChecksTest, "DF.Unit.Weapon.HostChecks", DFHeroWeaponTest::Flags)
bool FDFWeaponHostChecksTest::RunTest(const FString&)
{
	using namespace DFHeroWeaponTest;
	const FDFWeaponRules Rules = DFWeapon::RulesFromRow(RifleRow());

	// The rate (§3.2 rule 2): a burst of two goes through (one shot early after one late), a third at
	// once does not; the budget refills at 2.2 × 1.05 a second.
	FDFWeaponState State = DFWeapon::Loaded(Rules);
	FDFShotBudget Budget = DFWeapon::FullBudget();
	TestTrue(TEXT("first shot"), DFWeapon::HostAcceptShot(State, Budget, Rules) == EDFShotVerdict::Accepted);
	TestTrue(TEXT("second at once: jitter"), DFWeapon::HostAcceptShot(State, Budget, Rules) == EDFShotVerdict::Accepted);
	TestTrue(TEXT("third at once: too fast"), DFWeapon::HostAcceptShot(State, Budget, Rules) == EDFShotVerdict::TooFast);
	TestEqual(TEXT("a refused shot spends no round"), State.Rounds, 22);
	DFWeapon::RefillBudget(Budget, Rules, 1.f / (2.2f * DFWeapon::RateSlack) + 1e-4f);
	TestTrue(TEXT("one interval later: accepted"), DFWeapon::HostAcceptShot(State, Budget, Rules) == EDFShotVerdict::Accepted);

	// A client firing twice as fast for 10 s gets about 2.2 × 1.05 a second through, plus the burst.
	FDFWeaponRules Bottomless = Rules;
	Bottomless.MagazineSize = 1000;
	FDFWeaponState Cheat = DFWeapon::Loaded(Bottomless);
	FDFShotBudget CheatBudget = DFWeapon::FullBudget();
	int32 Accepted = 0;
	const float Interval = 1.f / (2.f * Rules.ShotsPerSecond);
	for (int32 i = 0; i < 44; ++i)
	{
		DFWeapon::RefillBudget(CheatBudget, Bottomless, i == 0 ? 0.f : Interval);
		Accepted += DFWeapon::HostAcceptShot(Cheat, CheatBudget, Bottomless) == EDFShotVerdict::Accepted ? 1 : 0;
	}
	const int32 Allowed = FMath::FloorToInt32(43.f * Interval * Rules.ShotsPerSecond * DFWeapon::RateSlack + DFWeapon::BurstShots);
	TestTrue(*FString::Printf(TEXT("double rate for 10 s: %d accepted, at most %d"), Accepted, Allowed), Accepted <= Allowed && Accepted >= Allowed - 1);

	// The magazine: an empty one refuses; the host's reload refuses too, except in its last 0.2 s,
	// which the owner's earlier start has already finished.
	FDFWeaponState Empty = DFWeapon::Loaded(Rules);
	Empty.Rounds = 0;
	FDFShotBudget Plenty = DFWeapon::FullBudget();
	TestTrue(TEXT("empty: refused"), DFWeapon::HostAcceptShot(Empty, Plenty, Rules) == EDFShotVerdict::Empty);
	TestTrue(TEXT("empty: the host can start the reload"), DFWeapon::BeginReload(Empty, Rules));
	TestTrue(TEXT("1.9 s left: reloading"), DFWeapon::HostAcceptShot(Empty, Plenty, Rules) == EDFShotVerdict::Reloading);
	DFWeapon::AdvanceReload(Empty, Rules, 1.9f - 0.15f);
	TestTrue(TEXT("0.15 s left: the grace finishes it and the shot fires"), DFWeapon::HostAcceptShot(Empty, Plenty, Rules) == EDFShotVerdict::Accepted);
	TestFalse(TEXT("the reload is over"), DFWeapon::IsReloading(Empty));
	TestEqual(TEXT("a full magazine less that shot"), Empty.Rounds, 23);

	// The origin: within 2 m of the host's eye.
	const FVector Eye(100.f, 200.f, 160.f);
	TestTrue(TEXT("origin at the eye"), DFWeapon::IsOriginPlausible(Eye, Eye));
	TestTrue(TEXT("origin 1.5 m off: lag, accepted"), DFWeapon::IsOriginPlausible(Eye + FVector(150.f, 0.f, 0.f), Eye));
	TestFalse(TEXT("origin 2.5 m off: refused"), DFWeapon::IsOriginPlausible(Eye + FVector(0.f, 250.f, 0.f), Eye));
	TestFalse(TEXT("origin across the map: refused"), DFWeapon::IsOriginPlausible(FVector(9000.f, 0.f, 0.f), Eye));
	return true;
}

namespace DFHeroWeaponTest
{
	/** A world with a shooter at the origin facing +X (inside a box that blocks DF_Weapon, which its own shots must ignore) and a target ahead. */
	struct FRange
	{
		FDFTestWorld World;
		AActor* Shooter = nullptr;
		UDFHeroWeaponComponent* Weapon = nullptr;
		ADFWeaponTestTarget* Target = nullptr;

		~FRange()
		{
			// Ends the play NotifyBeginPlay started, before FDFTestWorld tears the world down (CleanupWorld warns otherwise).
			if (UWorld* W = World.GetWorld(); W && W->HasBegunPlay())
			{
				W->EndPlay(EEndPlayReason::Quit);
			}
		}

		bool Init(float TargetDistanceCm)
		{
			// No game mode: the world settings are what flips the begun-play flag, so spawned actors and
			// registered components begin play (see DFStatusComponentTests).
			World.GetWorld()->GetWorldSettings()->NotifyBeginPlay();

			Shooter = World.SpawnActor<AActor>();
			UBoxComponent* ShooterBody = NewObject<UBoxComponent>(Shooter, TEXT("Body"));
			ShooterBody->InitBoxExtent(FVector(40.f, 40.f, 90.f));
			ShooterBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			ShooterBody->SetCollisionResponseToAllChannels(ECR_Block);
			Shooter->SetRootComponent(ShooterBody);
			ShooterBody->RegisterComponent();
			Weapon = NewObject<UDFHeroWeaponComponent>(Shooter, TEXT("Weapon"));
			Weapon->SetWeaponRowOverride(RifleRow());
			Weapon->RegisterComponent();

			Target = World.SpawnActor<ADFWeaponTestTarget>(FTransform(FVector(TargetDistanceCm, 0.f, 0.f)));
			if (!Target)
			{
				return false;
			}
			Target->HealthSet->InitMaxHealth(100.f);
			Target->HealthSet->InitHealth(100.f);
			World.Tick(1.f / 60.f, 2);   // the new bodies into the physics scene's query structure
			return Weapon->HasWeapon();
		}

		float Health() const { return Target->HealthSet->GetHealth(); }

		AActor* SpawnWall(float AtCm)
		{
			AActor* Wall = World.SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Wall, TEXT("Wall"));
			Box->InitBoxExtent(FVector(10.f, 200.f, 200.f));
			Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);   // DF_Weapon takes its ini default
			Wall->SetRootComponent(Box);
			Box->RegisterComponent();
			Wall->SetActorLocation(FVector(AtCm, 0.f, 0.f));
			World.Tick(1.f / 60.f, 2);
			return Wall;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWeaponServerShotTest, "DF.Unit.Weapon.ServerShotDamages", DFHeroWeaponTest::Flags)
bool FDFWeaponServerShotTest::RunTest(const FString&)
{
	using namespace DFHeroWeaponTest;
	FRange Range;
	if (!TestTrue(TEXT("a range with a shooter and a target"), Range.Init(1000.f)))
	{
		return false;
	}
	const FVector Forward(1.f, 0.f, 0.f);

	// A shot reported from the shooter's eye, straight at the target 10 m away: the host traces it past
	// the shooter's own body and the target takes the rifle's 15.
	TestTrue(TEXT("accepted"), Range.Weapon->HostReceiveShot(FVector::ZeroVector, Forward) == EDFShotVerdict::Accepted);
	TestEqual(TEXT("the target took 15"), Range.Health(), 85.f, Tol);
	TestEqual(TEXT("the host spent a round"), Range.Weapon->GetAmmoInMagazine(), 23);

	// Refused shots do nothing: an origin 5 m above the eye, and no direction.
	TestTrue(TEXT("an origin 5 m off: refused"), Range.Weapon->HostReceiveShot(FVector(0.f, 0.f, 500.f), Forward) == EDFShotVerdict::BadShot);
	TestTrue(TEXT("no direction: refused"), Range.Weapon->HostReceiveShot(FVector::ZeroVector, FVector::ZeroVector) == EDFShotVerdict::BadShot);
	TestEqual(TEXT("refused shots hurt nothing"), Range.Health(), 85.f, Tol);
	TestEqual(TEXT("refused shots spend nothing"), Range.Weapon->GetAmmoInMagazine(), 23);

	// A miss spends its round (shots, not hits) and hurts nothing.
	Range.World.Tick(0.5f);   // one interval of budget back
	TestTrue(TEXT("a shot wide of the target: accepted"), Range.Weapon->HostReceiveShot(FVector::ZeroVector, FVector(0.f, 1.f, 0.f)) == EDFShotVerdict::Accepted);
	TestEqual(TEXT("a miss hurts nothing"), Range.Health(), 85.f, Tol);
	TestEqual(TEXT("a miss spends a round"), Range.Weapon->GetAmmoInMagazine(), 22);

	// A dead body takes no more.
	Range.World.Tick(0.5f);
	Range.Target->bDead = true;
	TestTrue(TEXT("a shot at a dead body: accepted"), Range.Weapon->HostReceiveShot(FVector::ZeroVector, Forward) == EDFShotVerdict::Accepted);
	TestEqual(TEXT("a dead body takes nothing"), Range.Health(), 85.f, Tol);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWeaponWallsAndRangeTest, "DF.Unit.Weapon.WallsAndRange", DFHeroWeaponTest::Flags)
bool FDFWeaponWallsAndRangeTest::RunTest(const FString&)
{
	using namespace DFHeroWeaponTest;
	const FVector Forward(1.f, 0.f, 0.f);

	// World geometry blocks DF_Weapon by default (DefaultEngine.ini), so a shot stops on a wall.
	{
		FRange Range;
		if (!TestTrue(TEXT("a range"), Range.Init(1000.f)))
		{
			return false;
		}
		Range.SpawnWall(500.f);
		TestTrue(TEXT("a shot into the wall: accepted"), Range.Weapon->HostReceiveShot(FVector::ZeroVector, Forward) == EDFShotVerdict::Accepted);
		TestEqual(TEXT("the target behind the wall is untouched"), Range.Health(), 100.f, Tol);
	}

	// The host traces the row's 80 m × 1.15: a target at 90 m is hit, one at 95 m is out of reach.
	{
		FRange Near;
		if (!TestTrue(TEXT("a range at 90 m"), Near.Init(9000.f)))
		{
			return false;
		}
		Near.Weapon->HostReceiveShot(FVector::ZeroVector, Forward);
		TestEqual(TEXT("90 m: inside 80 m × 1.15"), Near.Health(), 85.f, Tol);
	}
	{
		FRange Far;
		if (!TestTrue(TEXT("a range at 95 m"), Far.Init(9500.f)))
		{
			return false;
		}
		Far.Weapon->HostReceiveShot(FVector::ZeroVector, Forward);
		TestEqual(TEXT("95 m: out of reach"), Far.Health(), 100.f, Tol);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWeaponHeroCarriesRifleTest, "DF.Unit.Weapon.HeroCarriesRifle", DFHeroWeaponTest::Flags)
bool FDFWeaponHeroCarriesRifleTest::RunTest(const FString&)
{
	using namespace DFHeroWeaponTest;
	const ADFHeroCharacter* Hero = GetDefault<ADFHeroCharacter>();
	const UDFHeroWeaponComponent* Weapon = Hero->GetWeapon();
	if (!TestNotNull(TEXT("the hero has a gun"), Weapon))
	{
		return false;
	}
	TestTrue(TEXT("the rifle"), Weapon->GetWeaponId() == FName(TEXT("rifle")));
	TestTrue(TEXT("replicated, for its RPCs"), Weapon->GetIsReplicated());

	// The placeholder gun: on the camera, seen by its owner only, and in nobody's way.
	const UCameraComponent* Camera = Hero->GetFirstPersonCamera();
	ADFHeroCharacter* HeroForLookup = GetMutableDefault<ADFHeroCharacter>();   // GetDefaultSubobjectByName is not const; read only
	for (const TCHAR* Name : { TEXT("GunBody"), TEXT("GunBarrel") })
	{
		const UStaticMeshComponent* Part = Cast<UStaticMeshComponent>(HeroForLookup->GetDefaultSubobjectByName(Name));
		if (!TestNotNull(*FString::Printf(TEXT("%s exists"), Name), Part))
		{
			continue;
		}
		TestNotNull(*FString::Printf(TEXT("%s has a mesh"), Name), Part->GetStaticMesh().Get());
		// The basic shapes' own DefaultMaterial has no "Color": the slot must hold BasicShapeMaterial, or the tint does nothing.
		const UMaterialInterface* Material = Part->GetMaterial(0);
		TestTrue(*FString::Printf(TEXT("%s wears the tintable BasicShapeMaterial (%s)"), Name, *GetNameSafe(Material)),
			Material != nullptr && Material->GetFName() == FName(TEXT("BasicShapeMaterial")));
		TestTrue(*FString::Printf(TEXT("%s: only the owner sees it"), Name), Part->bOnlyOwnerSee != 0);
		TestFalse(*FString::Printf(TEXT("%s: casts no shadow"), Name), Part->CastShadow != 0);
		TestTrue(*FString::Printf(TEXT("%s: no collision"), Name), Part->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
		TestTrue(*FString::Printf(TEXT("%s: ignores DF_Weapon"), Name), Part->GetCollisionResponseToChannel(DFCollision::Weapon) == ECR_Ignore);
		TestTrue(*FString::Printf(TEXT("%s: rides on the camera"), Name), Part->GetAttachParent() != nullptr && Part->GetAttachParent()->GetAttachParent() == Camera);
	}

	// C16: IA_Fire on LMB and the right trigger, IA_Reload on R and the top face button.
	const UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/DF/Core/Input/IMC_DF_Default.IMC_DF_Default"));
	if (!TestNotNull(TEXT("IMC_DF_Default"), Context))
	{
		return false;
	}
	auto IsMapped = [Context](const TCHAR* ActionName, const FKey& Key)
	{
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			if (Mapping.Action != nullptr && Mapping.Action->GetFName() == FName(ActionName) && Mapping.Key == Key)
			{
				return true;
			}
		}
		return false;
	};
	TestTrue(TEXT("IA_Fire on the left mouse button"), IsMapped(TEXT("IA_Fire"), EKeys::LeftMouseButton));
	TestTrue(TEXT("IA_Fire on the right trigger"), IsMapped(TEXT("IA_Fire"), EKeys::Gamepad_RightTrigger));
	TestTrue(TEXT("IA_Reload on R"), IsMapped(TEXT("IA_Reload"), EKeys::R));
	TestTrue(TEXT("IA_Reload on the top face button"), IsMapped(TEXT("IA_Reload"), EKeys::Gamepad_FaceButton_Top));
	return true;
}

#endif
