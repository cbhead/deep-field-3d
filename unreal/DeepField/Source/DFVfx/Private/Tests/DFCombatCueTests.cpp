// DF.Unit.Vfx.* — the placeholder combat cues (UDFCombatCueSubsystem): what each tower kind's messages
// draw and for how long, which round a host's landing belongs to, what a body's death takes away, that
// parts are pooled, and where nothing is drawn. The subsystem never starts itself in an automation test's
// world, so each test starts it and steps the world by hand; the messages are broadcast on the test world's
// bus exactly as a client re-broadcasts the host's. Numbers are towers.json's: a lance (Bolt) round flies
// 30 m/s, a skywatch (Flak) 45 m/s, a nova (Mortar) 14 m/s with a 3.2 m splash, arc is the Tesla, filament
// the Beam. A shot's Impact is the body's position; rounds and bolts end 0.8 m above it
// (DFTowerMath::ShotAimHeightCm), where the sim aims. A ProjectileLanded's Impact is where the host's round
// was when it landed, as ADFTower sends it. Run: unreal\deepfield.cmd test DF.Unit.Vfx

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/DFTargetable.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Content/DFContentSubsystem.h"
#include "Cues/DFCombatCueSubsystem.h"
#include "DFCueTestTarget.h"
#include "DFGameplayTags.h"
#include "DFWorldCollision.h"
#include "Messages/DFMessages.h"
#include "Testing/DFTestUtils.h"
#include "Towers/DFTower.h"
#include "Towers/DFTowerMath.h"

namespace DFCombatCueTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** The content tables loaded and the world's cue subsystem started, or null (the failure says why). */
	UDFCombatCueSubsystem* Start(FAutomationTestBase& Test, FDFTestWorld& World)
	{
		UDFContentSubsystem* Content = World.GetSubsystem<UDFContentSubsystem>();
		if (!Test.TestNotNull(TEXT("content subsystem"), Content) || !(Content->IsReady() || Test.TestTrue(TEXT("tables load"), Content->LoadTables())))
		{
			return nullptr;
		}
		UDFCombatCueSubsystem* Cues = UDFCombatCueSubsystem::Get(World.GetWorld());
		if (!Test.TestNotNull(TEXT("a game world has the cue subsystem"), Cues))
		{
			return nullptr;
		}
		Test.TestFalse(TEXT("an automation test's world never starts it on its own"), Cues->IsDrawing());
		Cues->StartDrawing();
		return Test.TestTrue(TEXT("started"), Cues->IsDrawing()) ? Cues : nullptr;
	}

	FDFMsg_Shot Shot(const TCHAR* DefId, const FVector& Origin, const FVector& Impact, int32 StructureId = 1, int32 TargetId = 7)
	{
		FDFMsg_Shot Message;
		Message.StructureId = StructureId;
		Message.TargetId = TargetId;
		Message.DefId = DefId;
		Message.Origin = Origin;
		Message.Impact = Impact;
		return Message;
	}

	FVector AimAbove(const FVector& Position)
	{
		return Position + FVector(0.f, 0.f, DFTowerMath::ShotAimHeightCm);
	}

	/** Where a Mortar's disc sits over the ground it lies on: its underside SplashLiftCm up, half its thickness more. */
	constexpr double DiscAboveCm = DFCombatCue::SplashLiftCm + DFCombatCue::SplashThicknessCm * 0.5;

	/** The two ends of a cube stretched along its X (a bolt segment). */
	void EndsOf(const UStaticMeshComponent* Part, FVector& A, FVector& B)
	{
		const FTransform T = Part->GetComponentTransform();
		const FVector Half = T.GetUnitAxis(EAxis::X) * (T.GetScale3D().X * 50.0);
		A = T.GetLocation() - Half;
		B = T.GetLocation() + Half;
	}

	/** A bolt as its segments lie: how many segment ends touch its start and its end, and how far its kinks
	 *  (every other segment end) stray from the straight line between them. */
	struct FBoltShape
	{
		int32 AtStart = 0;
		int32 AtEnd = 0;
		double NearestKinkCm = TNumericLimits<double>::Max();
		double FurthestKinkCm = 0.0;
	};

	FBoltShape ShapeOf(const TArray<UStaticMeshComponent*>& Segments, const FVector& Start, const FVector& End)
	{
		FBoltShape Shape;
		for (const UStaticMeshComponent* Segment : Segments)
		{
			FVector A;
			FVector B;
			EndsOf(Segment, A, B);
			for (const FVector& Point : { A, B })
			{
				if (Point.Equals(Start, 0.5))
				{
					++Shape.AtStart;
				}
				else if (Point.Equals(End, 0.5))
				{
					++Shape.AtEnd;
				}
				else
				{
					const double Off = FMath::PointDistToSegment(Point, Start, End);
					Shape.NearestKinkCm = FMath::Min(Shape.NearestKinkCm, Off);
					Shape.FurthestKinkCm = FMath::Max(Shape.FurthestKinkCm, Off);
				}
			}
		}
		return Shape;
	}

	/** A 12 x 12 m slab that only DF_LaneSurface hits, its top face through At, climbing along +X at Grade. */
	void MakeLaneSurface(FDFTestWorld& World, const FVector& At, const FRotator& Grade)
	{
		AActor* Ramp = World.SpawnActor<AActor>(FTransform(Grade, At));
		UBoxComponent* Surface = NewObject<UBoxComponent>(Ramp, TEXT("Surface"));
		Ramp->SetRootComponent(Surface);
		Surface->SetBoxExtent(FVector(600.0, 600.0, 10.0));
		Surface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Surface->SetCollisionResponseToAllChannels(ECR_Ignore);
		Surface->SetCollisionResponseToChannel(DFCollision::LaneSurface, ECR_Block);
		Surface->RegisterComponent();
		Surface->SetWorldTransform(FTransform(Grade, At - Grade.RotateVector(FVector(0.0, 0.0, 10.0))));
	}

	/** A look part: nothing collides with it, overlaps it or navigates round it, and it casts no shadow. */
	bool IsInert(const UStaticMeshComponent* Part)
	{
		if (Part->GetCollisionEnabled() != ECollisionEnabled::NoCollision || Part->GetGenerateOverlapEvents()
			|| Part->CanEverAffectNavigation() || Part->CastShadow)
		{
			return false;
		}
		for (int32 Channel = ECC_WorldStatic; Channel <= ECC_GameTraceChannel18; ++Channel)
		{
			if (Part->GetCollisionResponseToChannel(static_cast<ECollisionChannel>(Channel)) != ECR_Ignore)
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFVfxRoundFlightTest, "DF.Unit.Vfx.RoundFliesAtRowSpeed", DFCombatCueTest::Flags)
bool FDFVfxRoundFlightTest::RunTest(const FString& Parameters)
{
	using namespace DFCombatCueTest;
	FDFTestWorld World;
	UDFCombatCueSubsystem* Cues = Start(*this, World);
	if (!Cues)
	{
		return false;
	}
	// From the muzzle to a body whose aim point is level with it, 6 m on: 6 m at 30 m/s is 0.2 s.
	const FVector Origin(0.f, 0.f, 150.f);
	const FVector Impact(600.f, 0.f, 70.f);
	World.MessageBus()->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("lance"), Origin, Impact));
	TestEqual(TEXT("one round"), Cues->NumCues(EDFCombatCue::Round), 1);
	const TArray<UStaticMeshComponent*> Ball = Cues->GetParts(EDFCombatCue::Round);
	if (!TestEqual(TEXT("one ball"), Ball.Num(), 1))
	{
		return false;
	}
	TestTrue(TEXT("it leaves the muzzle"), Ball[0]->GetComponentLocation().Equals(Origin, 0.1));
	TestTrue(TEXT("a look, nothing else"), IsInert(Ball[0]));
	TestTrue(TEXT("shown"), Ball[0]->IsVisible());

	World.Tick(0.1f);
	TestTrue(TEXT("halfway at half the flight"), Ball[0]->GetComponentLocation().Equals(FVector(300.f, 0.f, 150.f), 0.5));
	World.Tick(0.095f);
	TestEqual(TEXT("still in the air just before distance / speed"), Cues->NumCues(EDFCombatCue::Round), 1);
	World.Tick(0.01f);
	TestEqual(TEXT("landed after distance / speed"), Cues->NumCues(EDFCombatCue::Round), 0);
	const TArray<UStaticMeshComponent*> Flash = Cues->GetParts(EDFCombatCue::Flash);
	if (TestEqual(TEXT("a flash where it landed"), Flash.Num(), 1))
	{
		TestTrue(TEXT("at the aim point above Impact"), Flash[0]->GetComponentLocation().Equals(AimAbove(Impact), 0.1));
	}
	TestEqual(TEXT("the host's landing for it is owed"), Cues->NumOwedLandings(), 1);
	// The host's round lands a moment later (its body walked away): that landing is this round's, already shown.
	World.MessageBus()->Broadcast(DFTags::Message_ProjectileLanded, Shot(TEXT("lance"), Origin, FVector(640.f, 0.f, 150.f)));
	TestEqual(TEXT("its late landing draws nothing"), Cues->NumCues(EDFCombatCue::Flash), 1);
	TestEqual(TEXT("and is no longer owed"), Cues->NumOwedLandings(), 0);
	World.Tick(DFCombatCue::FlashSeconds + 0.01f);
	TestEqual(TEXT("the flash is brief"), Cues->NumCues(EDFCombatCue::Flash), 0);
	TestEqual(TEXT("every part put away"), Cues->GetPartsInUse(), 0);

	// The host's round landed first (its body walked toward the tower): this tower's cue at that body lands
	// there and then. A landing at another body moves nothing.
	World.MessageBus()->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("lance"), Origin, Impact, 1, 7));
	World.Tick(0.1f);                           // 3 m out
	const FVector Landed(290.f, 0.f, 150.f);   // the host's round met its body coming the other way, 2.9 m out
	World.MessageBus()->Broadcast(DFTags::Message_ProjectileLanded, Shot(TEXT("lance"), Origin, Landed, 1, 8));
	TestEqual(TEXT("another body's landing: still in the air"), Cues->NumCues(EDFCombatCue::Round), 1);
	World.MessageBus()->Broadcast(DFTags::Message_ProjectileLanded, Shot(TEXT("lance"), Origin, Landed, 1, 7));
	TestEqual(TEXT("its own: landed at once"), Cues->NumCues(EDFCombatCue::Round), 0);
	const TArray<UStaticMeshComponent*> Early = Cues->GetParts(EDFCombatCue::Flash);
	if (TestEqual(TEXT("one flash"), Early.Num(), 1))
	{
		TestTrue(TEXT("where the host's round landed"), Early[0]->GetComponentLocation().Equals(Landed, 0.1));
	}
	TestEqual(TEXT("nothing owed: the host has landed it"), Cues->NumOwedLandings(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFVfxLateLandingTest, "DF.Unit.Vfx.LateLandingKeepsTheNextRoundFlying", DFCombatCueTest::Flags)
bool FDFVfxLateLandingTest::RunTest(const FString& Parameters)
{
	using namespace DFCombatCueTest;
	FDFTestWorld World;
	UDFCombatCueSubsystem* Cues = Start(*this, World);
	if (!Cues)
	{
		return false;
	}
	UDFMessageBus* Bus = World.MessageBus();
	const FVector Origin(0.f, 0.f, 150.f);

	// A lance with its rate path bought fires every 0.265 s at a body 12 m off walking away at 3 m/s. Each cue
	// flies about 0.4 s to where the body was; each host round, homing on where it is, lands a little later,
	// after the next round has left. Its landing is its own round's, not the next one's.
	Bus->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("lance"), Origin, FVector(1200.f, 0.f, 70.f)));    // 12 m: 0.4 s
	World.Tick(0.265f);
	Bus->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("lance"), Origin, FVector(1279.5f, 0.f, 70.f)));   // 12.8 m: 0.4265 s
	World.Tick(0.14f);
	TestEqual(TEXT("the first round landed on its own at 0.4 s"), Cues->NumCues(EDFCombatCue::Flash), 1);
	const TArray<UStaticMeshComponent*> Second = Cues->GetParts(EDFCombatCue::Round);
	if (!TestEqual(TEXT("the second is in the air"), Second.Num(), 1))
	{
		return false;
	}
	const double WasAt = Second[0]->GetComponentLocation().X;
	// 0.41 s: the host's first round, 12.3 m out, comes within a step and 0.4 m of its body (13.2 m by then).
	Bus->Broadcast(DFTags::Message_ProjectileLanded, Shot(TEXT("lance"), Origin, FVector(1230.f, 0.f, 150.f)));
	TestEqual(TEXT("the first round's landing leaves the second in the air"), Cues->NumCues(EDFCombatCue::Round), 1);
	TestEqual(TEXT("and draws no second flash"), Cues->NumCues(EDFCombatCue::Flash), 1);
	TestEqual(TEXT("nothing owed now"), Cues->NumOwedLandings(), 0);
	World.Tick(0.1f);
	TestTrue(TEXT("the second flies on"), Cues->NumCues(EDFCombatCue::Round) == 1 && Second[0]->GetComponentLocation().X > WasAt + 250.0);
	World.Tick(0.2f);
	TestEqual(TEXT("and lands on its own at 0.69 s"), Cues->NumCues(EDFCombatCue::Round), 0);
	TestEqual(TEXT("its landing owed"), Cues->NumOwedLandings(), 1);
	const int32 Flashes = Cues->NumCues(EDFCombatCue::Flash);
	Bus->Broadcast(DFTags::Message_ProjectileLanded, Shot(TEXT("lance"), Origin, FVector(1320.f, 0.f, 150.f)));
	TestEqual(TEXT("which draws nothing when it comes"), Cues->NumCues(EDFCombatCue::Flash), Flashes);
	TestEqual(TEXT("and is paid"), Cues->NumOwedLandings(), 0);

	// A skywatch fires every 0.333 s, its flight time at its full 15 m: the host's landing for one round comes
	// after the next has left, and must not land that one a frame after launch.
	Bus->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("skywatch"), Origin, FVector(1500.f, 0.f, 70.f), 2, 9));
	World.Tick(0.34f);
	Bus->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("skywatch"), Origin, FVector(1510.f, 0.f, 70.f), 2, 9));
	World.Tick(1.f / 60.f);
	TestEqual(TEXT("one flak round landed, the next in the air"), Cues->NumCues(EDFCombatCue::Round), 1);
	Bus->Broadcast(DFTags::Message_ProjectileLanded, Shot(TEXT("skywatch"), Origin, FVector(1490.f, 0.f, 150.f), 2, 9));
	TestEqual(TEXT("the first's late landing leaves the next in the air"), Cues->NumCues(EDFCombatCue::Round), 1);
	TestEqual(TEXT("with no second flash"), Cues->NumCues(EDFCombatCue::Flash), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFVfxFreshRoundTest, "DF.Unit.Vfx.LandingNeverLandsAFreshRound", DFCombatCueTest::Flags)
bool FDFVfxFreshRoundTest::RunTest(const FString& Parameters)
{
	using namespace DFCombatCueTest;
	FDFTestWorld World;
	UDFCombatCueSubsystem* Cues = Start(*this, World);
	if (!Cues)
	{
		return false;
	}
	UDFMessageBus* Bus = World.MessageBus();
	const FVector Origin(0.f, 0.f, 150.f);
	const FVector Impact(1200.f, 0.f, 70.f);

	// A round lands on its own and its host landing never comes (its tower was sold with the round in the air):
	// after its flight again and a grace, it is no longer waited for. (A world tick is clamped to 0.4 s,
	// MaxUndilatedFrameTime: step in quarter seconds.)
	Bus->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("lance"), Origin, Impact));
	World.Tick(0.25f, 2);
	TestEqual(TEXT("landed on its own, its landing owed"), Cues->NumOwedLandings(), 1);
	World.Tick(0.25f, 3);
	TestEqual(TEXT("still owed within its flight and the grace"), Cues->NumOwedLandings(), 1);
	World.Tick(0.25f);
	TestEqual(TEXT("a landing that never came is let go"), Cues->NumOwedLandings(), 0);
	World.Tick(0.2f);

	// A landing from 12.5 m out a frame after this tower's next round left cannot be that round's: the host's
	// round flew at least from the muzzle to where it landed. It is one this machine owes nothing for (a round
	// fired before it joined, or one it stopped waiting for), and it draws nothing.
	Bus->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("lance"), Origin, Impact));
	World.Tick(1.f / 60.f);
	Bus->Broadcast(DFTags::Message_ProjectileLanded, Shot(TEXT("lance"), Origin, FVector(1250.f, 0.f, 150.f)));
	TestEqual(TEXT("the fresh round stays in the air"), Cues->NumCues(EDFCombatCue::Round), 1);
	TestEqual(TEXT("and nothing flashes"), Cues->NumCues(EDFCombatCue::Flash), 0);

	// A landing it could be (6 m out after 0.22 s: its body came to meet it) lands it.
	World.Tick(0.2f);
	const FVector Met(600.f, 0.f, 150.f);
	Bus->Broadcast(DFTags::Message_ProjectileLanded, Shot(TEXT("lance"), Origin, Met));
	TestEqual(TEXT("landed"), Cues->NumCues(EDFCombatCue::Round), 0);
	const TArray<UStaticMeshComponent*> Flash = Cues->GetParts(EDFCombatCue::Flash);
	if (TestEqual(TEXT("one flash"), Flash.Num(), 1))
	{
		TestTrue(TEXT("where the host's round landed"), Flash[0]->GetComponentLocation().Equals(Met, 0.1));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFVfxBodyGoneTest, "DF.Unit.Vfx.RoundsAtAGoneBodyVanish", DFCombatCueTest::Flags)
bool FDFVfxBodyGoneTest::RunTest(const FString& Parameters)
{
	using namespace DFCombatCueTest;
	FDFTestWorld World;
	UDFCombatCueSubsystem* Cues = Start(*this, World);
	if (!Cues)
	{
		return false;
	}
	UDFMessageBus* Bus = World.MessageBus();

	// A nova lobs at the lead body 14 m off (a second's flight), another nova at the body behind it, and a lance
	// kills the lead half a second in. The host drops the first shell unlanded, dealing nothing: its cue vanishes
	// where it is, with no disc claiming a splash and no flash.
	Bus->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("nova"), FVector(0.f, 0.f, 150.f), FVector(1400.f, 0.f, 70.f), 1, 7));
	Bus->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("nova"), FVector(0.f, 300.f, 150.f), FVector(1400.f, 300.f, 70.f), 2, 8));
	World.Tick(0.25f, 2);
	FDFMsg_Kill Kill;
	Kill.EnemyId = 7;
	Bus->Broadcast(DFTags::Message_EnemyKilled, Kill);
	TestEqual(TEXT("the dead body's shell is gone"), Cues->NumCues(EDFCombatCue::Round), 1);
	TestEqual(TEXT("no flash for it"), Cues->NumCues(EDFCombatCue::Flash), 0);
	TestEqual(TEXT("and no splash"), Cues->NumCues(EDFCombatCue::Splash), 0);
	TestEqual(TEXT("its ball put away"), Cues->GetParts(EDFCombatCue::Round).Num(), 1);
	World.Tick(0.255f, 2);
	TestEqual(TEXT("the other shell lands as it would"), Cues->NumCues(EDFCombatCue::Splash), 1);
	TestEqual(TEXT("with its flash"), Cues->NumCues(EDFCombatCue::Flash), 1);
	Bus->Broadcast(DFTags::Message_ProjectileLanded, Shot(TEXT("nova"), FVector(0.f, 300.f, 150.f), FVector(1390.f, 300.f, 150.f), 2, 8));
	TestEqual(TEXT("its host landing, a moment later, splashes no second disc"), Cues->NumCues(EDFCombatCue::Splash), 1);
	World.Tick(DFCombatCue::SplashSeconds + 0.01f);
	TestEqual(TEXT("every part put away"), Cues->GetPartsInUse(), 0);

	// A body that reaches the core takes the rounds at it with it.
	Bus->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("lance"), FVector(0.f, 0.f, 150.f), FVector(600.f, 0.f, 70.f), 3, 11));
	World.Tick(0.1f);
	FDFMsg_Enemy Leak;
	Leak.EnemyId = 11;
	Bus->Broadcast(DFTags::Message_EnemyLeaked, Leak);
	TestEqual(TEXT("a leaked body's round is gone"), Cues->NumCues(EDFCombatCue::Round), 0);
	TestEqual(TEXT("unlanded"), Cues->NumCues(EDFCombatCue::Flash), 0);

	// And a landing owed at a body that died will never come.
	Bus->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("lance"), FVector(0.f, 0.f, 150.f), FVector(600.f, 0.f, 70.f), 3, 12));
	World.Tick(0.25f);
	TestEqual(TEXT("landed on its own, its landing owed"), Cues->NumOwedLandings(), 1);
	Kill.EnemyId = 12;
	Bus->Broadcast(DFTags::Message_EnemyKilled, Kill);
	TestEqual(TEXT("its body died: nothing owed"), Cues->NumOwedLandings(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFVfxMortarTest, "DF.Unit.Vfx.MortarLobsAndSplashes", DFCombatCueTest::Flags)
bool FDFVfxMortarTest::RunTest(const FString& Parameters)
{
	using namespace DFCombatCueTest;
	FDFTestWorld World;
	UDFCombatCueSubsystem* Cues = Start(*this, World);
	if (!Cues)
	{
		return false;
	}
	// 14 m at 14 m/s: one second, on an arc 0.3 x 14 m = 4.2 m high at the middle.
	const FVector Origin(0.f, 0.f, 150.f);
	const FVector Impact(1400.f, 0.f, 70.f);
	World.MessageBus()->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("nova"), Origin, Impact));
	const TArray<UStaticMeshComponent*> Ball = Cues->GetParts(EDFCombatCue::Round);
	if (!TestEqual(TEXT("one round"), Ball.Num(), 1))
	{
		return false;
	}
	// (A world tick is clamped to 0.4 s, MaxUndilatedFrameTime: step in quarter seconds.)
	World.Tick(0.25f, 2);
	TestTrue(TEXT("halfway along, at the top of its lob"), Ball[0]->GetComponentLocation().Equals(FVector(700.f, 0.f, 150.f + 420.f), 1.0));
	World.Tick(0.255f, 2);
	TestEqual(TEXT("landed after the straight line's time"), Cues->NumCues(EDFCombatCue::Round), 0);
	const TArray<UStaticMeshComponent*> Disc = Cues->GetParts(EDFCombatCue::Splash);
	if (TestEqual(TEXT("a splash"), Disc.Num(), 1))
	{
		// No lane surface in this world: it lies level on the ground the body stood on.
		TestTrue(TEXT("on the ground under the landing"), Disc[0]->GetComponentLocation().Equals(Impact + FVector(0.0, 0.0, DiscAboveCm), 0.1));
		TestTrue(TEXT("level"), Disc[0]->GetUpVector().Equals(FVector::UpVector, 1e-4));
		TestEqual(TEXT("as wide as the row's 3.2 m radius"), Disc[0]->GetComponentScale().X, 6.4, 1e-4);
		TestTrue(TEXT("flat"), Disc[0]->GetComponentScale().Z < 0.1);
		TestTrue(TEXT("a look, nothing else"), IsInert(Disc[0]));
	}
	TestEqual(TEXT("and a flash"), Cues->NumCues(EDFCombatCue::Flash), 1);
	World.Tick(DFCombatCue::SplashSeconds + 0.01f);
	TestEqual(TEXT("the splash is brief"), Cues->NumCues(EDFCombatCue::Splash), 0);
	TestEqual(TEXT("every part put away"), Cues->GetPartsInUse(), 0);

	// A nova on a 5 m ledge (another tower) whose host shell landed early, on its flight line 1.2 m above the
	// body's aim point: the disc lies on the ground the body stood on, not 0.8 m under where the shell was.
	const FVector Ledge(0.f, 0.f, 650.f);
	World.MessageBus()->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("nova"), Ledge, Impact, 2, 7));   // 14.9 m: 1.06 s
	World.Tick(0.25f, 3);
	const FVector Early(1300.f, 0.f, 200.f);
	World.MessageBus()->Broadcast(DFTags::Message_ProjectileLanded, Shot(TEXT("nova"), Ledge, Early, 2, 7));
	TestEqual(TEXT("the early landing lands it"), Cues->NumCues(EDFCombatCue::Round), 0);
	const TArray<UStaticMeshComponent*> Low = Cues->GetParts(EDFCombatCue::Splash);
	if (TestEqual(TEXT("a splash"), Low.Num(), 1))
	{
		TestTrue(TEXT("under the landing, on the body's ground"), Low[0]->GetComponentLocation().Equals(FVector(Early.X, Early.Y, Impact.Z + DiscAboveCm), 0.1));
	}
	const TArray<UStaticMeshComponent*> Flash = Cues->GetParts(EDFCombatCue::Flash);
	if (TestEqual(TEXT("a flash"), Flash.Num(), 1))
	{
		TestTrue(TEXT("where the host's shell was"), Flash[0]->GetComponentLocation().Equals(Early, 0.1));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFVfxSplashSlopeTest, "DF.Unit.Vfx.SplashLiesAlongTheLane", DFCombatCueTest::Flags)
bool FDFVfxSplashSlopeTest::RunTest(const FString& Parameters)
{
	// Foundry's lanes climb 13-25 %. A level 6.4 m disc there would hang 0.8 m over the lane at its downhill rim
	// and sink as far into it uphill: it lies along the DF_LaneSurface under the landing, as an in-lane pad does.
	using namespace DFCombatCueTest;
	FDFTestWorld World;
	UDFCombatCueSubsystem* Cues = Start(*this, World);
	if (!Cues)
	{
		return false;
	}
	const FVector At(4000.0, -3000.0, 0.0);
	const FRotator Grade(14.f, 0.f, 0.f);   // a 25 % climb along +X
	MakeLaneSurface(World, At, Grade);
	const FVector Normal = Grade.RotateVector(FVector::UpVector);

	// A shell at a body standing on the slope at At lands on its own at the body's aim point.
	World.MessageBus()->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("nova"), At + FVector(-1000.0, 0.0, 300.0), At));
	World.Tick(0.25f, 3);
	TestEqual(TEXT("landed"), Cues->NumCues(EDFCombatCue::Round), 0);
	const TArray<UStaticMeshComponent*> Disc = Cues->GetParts(EDFCombatCue::Splash);
	if (!TestEqual(TEXT("a splash"), Disc.Num(), 1))
	{
		return false;
	}
	const double Dot = FVector::DotProduct(Disc[0]->GetUpVector(), Normal);
	TestTrue(FString::Printf(TEXT("it lies along the 25 %% grade (up . normal = %.5f)"), Dot), Dot > 0.9999);
	const FVector Offset = Disc[0]->GetComponentLocation() - At;
	const double Above = FVector::DotProduct(Offset, Normal);
	TestEqual(TEXT("just over the lane, under the landing"), Above, DiscAboveCm, 0.5);
	TestTrue(TEXT("centred where the body stood"), (Offset - Normal * Above).Size() < 1.0);
	TestTrue(TEXT("a look, nothing else"), IsInert(Disc[0]));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFVfxTeslaTest, "DF.Unit.Vfx.TeslaArcIsBrief", DFCombatCueTest::Flags)
bool FDFVfxTeslaTest::RunTest(const FString& Parameters)
{
	using namespace DFCombatCueTest;
	FDFTestWorld World;
	UDFCombatCueSubsystem* Cues = Start(*this, World);
	if (!Cues)
	{
		return false;
	}
	const FVector Origin(0.f, 0.f, 150.f);
	const FVector Impact(500.f, 0.f, 0.f);
	const FVector End = AimAbove(Impact);
	World.MessageBus()->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("arc"), Origin, Impact));
	TestEqual(TEXT("one bolt"), Cues->NumCues(EDFCombatCue::Arc), 1);
	TestEqual(TEXT("and a flash on the body"), Cues->NumCues(EDFCombatCue::Flash), 1);
	const TArray<UStaticMeshComponent*> Segments = Cues->GetParts(EDFCombatCue::Arc);
	if (!TestEqual(TEXT("made of the bolt's segments"), Segments.Num(), DFCombatCue::ArcSegments))
	{
		return false;
	}
	// A jagged line from the muzzle to the aim point: one segment starts at each end, and no kink strays more
	// than 60 cm (15 % of 5 m, capped) from the straight line.
	const double Reach = DFCombatCue::ArcReachCm(static_cast<float>(FVector::Dist(Origin, End)));
	TestEqual(TEXT("a 5 m bolt reaches the cap"), Reach, 60.0, 1e-3);
	const FBoltShape Bolt = ShapeOf(Segments, Origin, End);
	TestEqual(TEXT("one segment leaves the muzzle"), Bolt.AtStart, 1);
	TestEqual(TEXT("one reaches the body's aim point"), Bolt.AtEnd, 1);
	TestTrue(FString::Printf(TEXT("every kink near the line (%.1f cm at most)"), Bolt.FurthestKinkCm), Bolt.FurthestKinkCm <= Reach + 0.5);
	for (const UStaticMeshComponent* Segment : Segments)
	{
		TestTrue(TEXT("a segment is a look, nothing else"), IsInert(Segment));
	}

	World.Tick(0.1f);
	TestEqual(TEXT("still showing at 0.1 s"), Cues->NumCues(EDFCombatCue::Arc), 1);
	World.Tick(0.05f);
	TestEqual(TEXT("gone by 0.15 s"), Cues->NumCues(EDFCombatCue::Arc), 0);
	TestEqual(TEXT("every part put away"), Cues->GetPartsInUse(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFVfxTeslaShapeTest, "DF.Unit.Vfx.TeslaKinksStayWithinReach", DFCombatCueTest::Flags)
bool FDFVfxTeslaShapeTest::RunTest(const FString& Parameters)
{
	// Every kink of every bolt, as it is thrown and as it flickers into its second shape, lies within its reach
	// of the straight line (a kink drawn in a square rather than round the line reached 1.41 x that about half
	// the time), and at least a quarter of it off, so a bolt never reads as a rod. A long bolt reaches the 60 cm
	// cap; a short one 15 % of its length. Under automation the jitter is seeded, so this is the same 200 bolt
	// shapes on every run, and the same bolt twice from the same start.
	using namespace DFCombatCueTest;
	FDFTestWorld World;
	UDFCombatCueSubsystem* Cues = Start(*this, World);
	if (!Cues)
	{
		return false;
	}
	const FVector Origin(0.f, 0.f, 150.f);
	const FVector Far(500.f, 0.f, 0.f);     // 5 m to the aim point: 60 cm
	const FVector Near(200.f, 0.f, 70.f);   // 2 m, level: 30 cm
	int32 Bolts = 0;
	int32 Ends = 0;
	double Furthest = 0.0;
	double Nearest = TNumericLimits<double>::Max();
	for (int32 i = 0; i < 100; ++i)
	{
		const FVector Impact = (i % 2 == 0) ? Far : Near;
		const FVector End = AimAbove(Impact);
		const double Reach = DFCombatCue::ArcReachCm(static_cast<float>(FVector::Dist(Origin, End)));
		World.MessageBus()->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("arc"), Origin, Impact));
		for (int32 Look = 0; Look < 2; ++Look)
		{
			const FBoltShape Bolt = ShapeOf(Cues->GetParts(EDFCombatCue::Arc), Origin, End);
			++Bolts;
			Ends += (Bolt.AtStart == 1 && Bolt.AtEnd == 1) ? 1 : 0;
			Furthest = FMath::Max(Furthest, Bolt.FurthestKinkCm / Reach);
			Nearest = FMath::Min(Nearest, Bolt.NearestKinkCm / Reach);
			World.Tick(DFCombatCue::ArcSeconds * 0.5f + 0.01f);   // past the flicker, then past its end
		}
		TestEqual(TEXT("each bolt gone before the next"), Cues->GetPartsInUse(), 0);
	}
	TestEqual(TEXT("every bolt shape runs muzzle to body"), Ends, Bolts);
	TestTrue(FString::Printf(TEXT("no kink past its reach (at most %.4f of it)"), Furthest), Furthest <= 1.0 + 1e-3);
	TestTrue(FString::Printf(TEXT("none within a quarter of it (at least %.4f)"), Nearest), Nearest >= DFCombatCue::ArcMinKinkFraction - 1e-3);

	// The same start, the same bolt.
	auto FirstBolt = [&]()
	{
		Cues->StopDrawing();
		Cues->StartDrawing();
		World.MessageBus()->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("arc"), Origin, Far));
		TArray<FTransform> Out;
		for (const UStaticMeshComponent* Segment : Cues->GetParts(EDFCombatCue::Arc))
		{
			Out.Add(Segment->GetComponentTransform());
		}
		return Out;
	};
	const TArray<FTransform> First = FirstBolt();
	const TArray<FTransform> Again = FirstBolt();
	bool bSame = First.Num() == DFCombatCue::ArcSegments && Again.Num() == First.Num();
	for (int32 i = 0; bSame && i < First.Num(); ++i)
	{
		bSame &= First[i].Equals(Again[i], 1e-3);
	}
	TestTrue(TEXT("a test's bolts are the same on every run"), bSame);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFVfxBeamTest, "DF.Unit.Vfx.BeamHoldsItsTarget", DFCombatCueTest::Flags)
bool FDFVfxBeamTest::RunTest(const FString& Parameters)
{
	using namespace DFCombatCueTest;
	FDFTestWorld World;
	UDFCombatCueSubsystem* Cues = Start(*this, World);
	if (!Cues)
	{
		return false;
	}
	// Spawned after the start: the subsystem hears the spawn, as a client hears a tower replicate in.
	ADFTower* Tower = World.SpawnActor<ADFTower>();
	if (!TestNotNull(TEXT("tower"), Tower) || !TestTrue(TEXT("filament from the content tables"), Tower->InitializeTower(TEXT("filament"), TEXT("g1"), 1, 0)))
	{
		return false;
	}
	ADFCueTestTarget* Body = World.SpawnActor<ADFCueTestTarget>(FTransform(FVector(600.f, 0.f, 0.f)));
	if (!TestNotNull(TEXT("body"), Body))
	{
		return false;
	}
	Body->Id = 1;
	Body->Remaining = 10.f;
	UDFTargetRegistry::Get(World.GetWorld())->Register(Body);

	World.Tick(1.f / 30.f);
	TestEqual(TEXT("no target, no beam"), Cues->NumCues(EDFCombatCue::Beam), 0);

	// The host steps the tower (a test world does not tick it); the beam is drawn from what replicates.
	Tower->StepTower(1.f / 30.f);
	if (!TestTrue(TEXT("the tower holds the body"), Tower->GetCurrentTarget() == Body))
	{
		return false;
	}
	TestEqual(TEXT("BeamHeld: a spark where it locked on"), Cues->NumCues(EDFCombatCue::Flash), 1);
	World.Tick(1.f / 30.f);
	TArray<UStaticMeshComponent*> Beam = Cues->GetParts(EDFCombatCue::Beam);
	if (!TestEqual(TEXT("one beam"), Beam.Num(), 1))
	{
		return false;
	}
	const FVector Muzzle(0.f, 0.f, DFTowerMath::ShotMuzzleHeightCm);
	const FVector Aim = Body->GetAimPoint();
	TestTrue(TEXT("centred between the muzzle and the aim point"), Beam[0]->GetComponentLocation().Equals((Muzzle + Aim) * 0.5, 0.1));
	TestEqual(TEXT("as long as the gap"), Beam[0]->GetComponentScale().Z, FVector::Dist(Muzzle, Aim) / 100.0, 1e-3);
	TestTrue(TEXT("along it"), Beam[0]->GetComponentTransform().GetUnitAxis(EAxis::Z).Equals((Aim - Muzzle).GetSafeNormal(), 1e-3));
	TestTrue(TEXT("a look, nothing else"), IsInert(Beam[0]));
	const double Cold = Beam[0]->GetComponentScale().X;
	TestEqual(TEXT("as thick as its heat says"), Cold, FMath::Lerp(DFCombatCue::BeamMinCm, DFCombatCue::BeamMaxCm, Tower->GetHeat() / 255.f) / 100.0, 1e-4);

	// Two seconds on the same body: the ramp climbs, and the beam thickens with the replicated Heat.
	for (int32 i = 0; i < 60; ++i)
	{
		Tower->StepTower(1.f / 30.f);
	}
	World.Tick(1.f / 30.f);
	TestTrue(TEXT("the ramp climbed"), Tower->GetHeat() > 100);
	Beam = Cues->GetParts(EDFCombatCue::Beam);
	if (TestEqual(TEXT("still one beam"), Beam.Num(), 1))
	{
		const double Hot = Beam[0]->GetComponentScale().X;
		TestTrue(TEXT("thicker than at the start"), Hot > Cold);
		TestEqual(TEXT("as thick as its heat says"), Hot, FMath::Lerp(DFCombatCue::BeamMinCm, DFCombatCue::BeamMaxCm, Tower->GetHeat() / 255.f) / 100.0, 1e-4);
	}

	Body->bDead = true;
	Tower->StepTower(1.f / 30.f);
	World.Tick(1.f / 30.f);
	TestEqual(TEXT("the target gone, the beam goes"), Cues->NumCues(EDFCombatCue::Beam), 0);
	TestEqual(TEXT("and its part is put away"), Cues->GetParts(EDFCombatCue::Beam).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFVfxPoolTest, "DF.Unit.Vfx.PartsArePooled", DFCombatCueTest::Flags)
bool FDFVfxPoolTest::RunTest(const FString& Parameters)
{
	using namespace DFCombatCueTest;
	FDFTestWorld World;
	UDFCombatCueSubsystem* Cues = Start(*this, World);
	if (!Cues)
	{
		return false;
	}
	auto Volley = [&World]()
	{
		for (int32 i = 0; i < 3; ++i)
		{
			const float Y = 200.f * i;
			World.MessageBus()->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("lance"), FVector(0.f, Y, 150.f), FVector(600.f, Y, 70.f), i + 1, i + 1));
		}
	};
	auto RunOut = [&World]()
	{
		World.Tick(0.25f);                               // every 0.2 s round lands
		World.Tick(DFCombatCue::SplashSeconds + 0.01f);  // and every flash, disc and bolt is done
	};

	Volley();
	TestEqual(TEXT("three rounds in the air"), Cues->NumCues(EDFCombatCue::Round), 3);
	TestEqual(TEXT("three balls made"), Cues->GetPoolSize(), 3);
	const TArray<UStaticMeshComponent*> First = Cues->GetParts(EDFCombatCue::Round);
	World.Tick(0.25f);
	TestEqual(TEXT("three flashes"), Cues->NumCues(EDFCombatCue::Flash), 3);
	TestEqual(TEXT("each flash took its round's ball"), Cues->GetPoolSize(), 3);
	World.Tick(DFCombatCue::SplashSeconds + 0.01f);
	TestEqual(TEXT("all put away"), Cues->GetPartsInUse(), 0);
	bool bHidden = true;
	for (const UStaticMeshComponent* Part : First)
	{
		bHidden &= IsValid(Part) && !Part->IsVisible();
	}
	TestTrue(TEXT("a part put away is hidden, not destroyed"), bHidden);

	Volley();
	TestEqual(TEXT("the second volley makes nothing"), Cues->GetPoolSize(), 3);
	const TArray<UStaticMeshComponent*> Second = Cues->GetParts(EDFCombatCue::Round);
	bool bSame = Second.Num() == First.Num();
	for (const UStaticMeshComponent* Part : Second)
	{
		bSame &= First.Contains(Part);
	}
	TestTrue(TEXT("it flies the first volley's balls"), bSame);
	RunOut();

	// A Tesla bolt: four segments, and its flash takes a waiting ball.
	World.MessageBus()->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("arc"), FVector(0.f, 0.f, 150.f), FVector(500.f, 0.f, 0.f)));
	TestEqual(TEXT("the bolt adds its segments only"), Cues->GetPoolSize(), 3 + DFCombatCue::ArcSegments);
	RunOut();
	World.MessageBus()->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("arc"), FVector(0.f, 0.f, 150.f), FVector(500.f, 0.f, 0.f)));
	TestEqual(TEXT("the next bolt makes nothing"), Cues->GetPoolSize(), 3 + DFCombatCue::ArcSegments);
	RunOut();
	TestEqual(TEXT("all put away"), Cues->GetPartsInUse(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFVfxWhereTest, "DF.Unit.Vfx.OnlyInGameWorldsThatDraw", DFCombatCueTest::Flags)
bool FDFVfxWhereTest::RunTest(const FString& Parameters)
{
	using namespace DFCombatCueTest;
	{
		FDFTestWorld Editor(EWorldType::Editor);
		TestNull(TEXT("an editor world has no cue subsystem"), UDFCombatCueSubsystem::Get(Editor.GetWorld()));
	}

	FDFTestWorld World;
	UDFContentSubsystem* Content = World.GetSubsystem<UDFContentSubsystem>();
	if (!TestNotNull(TEXT("content subsystem"), Content) || !(Content->IsReady() || TestTrue(TEXT("tables load"), Content->LoadTables())))
	{
		return false;
	}
	UDFCombatCueSubsystem* Cues = UDFCombatCueSubsystem::Get(World.GetWorld());
	if (!TestNotNull(TEXT("a game world has one"), Cues))
	{
		return false;
	}
	// An automation test's world is left alone unless the test asks: a shot there draws nothing.
	TestFalse(TEXT("not drawing on its own"), Cues->IsDrawing());
	const FDFMsg_Shot Lance = Shot(TEXT("lance"), FVector(0.f, 0.f, 150.f), FVector(600.f, 0.f, 70.f));
	World.MessageBus()->Broadcast(DFTags::Message_TowerFired, Lance);
	World.Tick();
	TestEqual(TEXT("no round"), Cues->NumCues(EDFCombatCue::Round), 0);
	TestEqual(TEXT("no part made"), Cues->GetPoolSize(), 0);

	// Started, then stopped: everything is put away and later shots draw nothing.
	Cues->StartDrawing();
	World.MessageBus()->Broadcast(DFTags::Message_TowerFired, Lance);
	TestEqual(TEXT("drawing once started"), Cues->NumCues(EDFCombatCue::Round), 1);
	Cues->StopDrawing();
	TestFalse(TEXT("stopped"), Cues->IsDrawing());
	TestEqual(TEXT("its round gone"), Cues->NumCues(EDFCombatCue::Round), 0);
	TestEqual(TEXT("and the pool with it"), Cues->GetPoolSize(), 0);
	World.MessageBus()->Broadcast(DFTags::Message_TowerFired, Lance);
	World.Tick();
	TestEqual(TEXT("a shot after stopping draws nothing"), Cues->NumCues(EDFCombatCue::Round), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFVfxNoRowTest, "DF.Unit.Vfx.IgnoresShotsWithoutARow", DFCombatCueTest::Flags)
bool FDFVfxNoRowTest::RunTest(const FString& Parameters)
{
	using namespace DFCombatCueTest;
	FDFTestWorld World;
	UDFCombatCueSubsystem* Cues = Start(*this, World);
	if (!Cues)
	{
		return false;
	}
	const FVector Origin(0.f, 0.f, 150.f);
	const FVector Impact(600.f, 0.f, 70.f);
	UDFMessageBus* Bus = World.MessageBus();

	Bus->Broadcast(DFTags::Message_TowerFired, Shot(TEXT(""), Origin, Impact));
	// An id towers.json does not have: the content subsystem reports it (once), the cues draw nothing.
	AddExpectedMessage(TEXT("has no row 'nope'"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
	Bus->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("nope"), Origin, Impact));
	Bus->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("nope"), Origin, Impact));
	Bus->Broadcast(DFTags::Message_BeamHeld, Shot(TEXT("nope"), Origin, Impact));
	// Rows with no shot to draw: a barricade and an aura never fire; a Beam's TowerFired is not its beam.
	Bus->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("barricade"), Origin, Impact));
	Bus->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("detector"), Origin, Impact));
	Bus->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("filament"), Origin, Impact));
	// A landing with nothing in the air.
	Bus->Broadcast(DFTags::Message_ProjectileLanded, Shot(TEXT("lance"), Origin, Impact));
	World.Tick();
	TestEqual(TEXT("nothing drawn"), Cues->GetPartsInUse(), 0);
	TestEqual(TEXT("nothing made"), Cues->GetPoolSize(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
