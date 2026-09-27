// DF.Unit.Vfx.* — the placeholder combat cues (UDFCombatCueSubsystem): what each tower kind's messages
// draw and for how long, that parts are pooled, and where nothing is drawn. The subsystem never starts
// itself in an automation test's world, so each test starts it and steps the world by hand; the messages
// are broadcast on the test world's bus exactly as a client re-broadcasts the host's. Numbers are
// towers.json's: a lance (Bolt) round flies 30 m/s, a nova (Mortar) 14 m/s with a 3.2 m splash, arc is
// the Tesla, filament the Beam. A shot's Impact is the body's position; rounds and bolts end 0.8 m above
// it (DFTowerMath::ShotAimHeightCm), where the sim aims. Run: unreal\deepfield.cmd test DF.Unit.Vfx

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/DFTargetable.h"
#include "Components/StaticMeshComponent.h"
#include "Content/DFContentSubsystem.h"
#include "Cues/DFCombatCueSubsystem.h"
#include "DFCueTestTarget.h"
#include "DFGameplayTags.h"
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

	/** The two ends of a cube stretched along its X (a bolt segment). */
	void EndsOf(const UStaticMeshComponent* Part, FVector& A, FVector& B)
	{
		const FTransform T = Part->GetComponentTransform();
		const FVector Half = T.GetUnitAxis(EAxis::X) * (T.GetScale3D().X * 50.0);
		A = T.GetLocation() - Half;
		B = T.GetLocation() + Half;
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
	World.Tick(DFCombatCue::FlashSeconds + 0.01f);
	TestEqual(TEXT("the flash is brief"), Cues->NumCues(EDFCombatCue::Flash), 0);
	TestEqual(TEXT("every part put away"), Cues->GetPartsInUse(), 0);

	// The host's round landed first (its body walked toward the tower): this tower's cue at that body lands
	// there and then. A landing at another body moves nothing.
	World.MessageBus()->Broadcast(DFTags::Message_TowerFired, Shot(TEXT("lance"), Origin, Impact, 1, 7));
	World.Tick(0.05f);
	const FVector Landed(350.f, 0.f, 150.f);
	World.MessageBus()->Broadcast(DFTags::Message_ProjectileLanded, Shot(TEXT("lance"), Origin, Landed, 1, 8));
	TestEqual(TEXT("another body's landing: still in the air"), Cues->NumCues(EDFCombatCue::Round), 1);
	World.MessageBus()->Broadcast(DFTags::Message_ProjectileLanded, Shot(TEXT("lance"), Origin, Landed, 1, 7));
	TestEqual(TEXT("its own: landed at once"), Cues->NumCues(EDFCombatCue::Round), 0);
	const TArray<UStaticMeshComponent*> Early = Cues->GetParts(EDFCombatCue::Flash);
	if (TestEqual(TEXT("one flash"), Early.Num(), 1))
	{
		TestTrue(TEXT("where the host's round landed"), Early[0]->GetComponentLocation().Equals(Landed, 0.1));
	}
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
		TestTrue(TEXT("on the ground under the landing"), Disc[0]->GetComponentLocation().Equals(Impact, 0.1));
		TestEqual(TEXT("as wide as the row's 3.2 m radius"), Disc[0]->GetComponentScale().X, 6.4, 1e-4);
		TestTrue(TEXT("flat"), Disc[0]->GetComponentScale().Z < 0.1);
		TestTrue(TEXT("a look, nothing else"), IsInert(Disc[0]));
	}
	TestEqual(TEXT("and a flash"), Cues->NumCues(EDFCombatCue::Flash), 1);
	World.Tick(DFCombatCue::SplashSeconds + 0.01f);
	TestEqual(TEXT("the splash is brief"), Cues->NumCues(EDFCombatCue::Splash), 0);
	TestEqual(TEXT("every part put away"), Cues->GetPartsInUse(), 0);
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
	// than 60 cm from the straight line.
	int32 AtMuzzle = 0;
	int32 AtBody = 0;
	bool bNear = true;
	for (const UStaticMeshComponent* Segment : Segments)
	{
		FVector A;
		FVector B;
		EndsOf(Segment, A, B);
		AtMuzzle += (A.Equals(Origin, 0.5) || B.Equals(Origin, 0.5)) ? 1 : 0;
		AtBody += (A.Equals(End, 0.5) || B.Equals(End, 0.5)) ? 1 : 0;
		bNear &= FMath::PointDistToSegment(A, Origin, End) <= 60.5 && FMath::PointDistToSegment(B, Origin, End) <= 60.5;
		TestTrue(TEXT("a segment is a look, nothing else"), IsInert(Segment));
	}
	TestEqual(TEXT("one segment leaves the muzzle"), AtMuzzle, 1);
	TestEqual(TEXT("one reaches the body's aim point"), AtBody, 1);
	TestTrue(TEXT("every kink near the line"), bNear);

	World.Tick(0.1f);
	TestEqual(TEXT("still showing at 0.1 s"), Cues->NumCues(EDFCombatCue::Arc), 1);
	World.Tick(0.05f);
	TestEqual(TEXT("gone by 0.15 s"), Cues->NumCues(EDFCombatCue::Arc), 0);
	TestEqual(TEXT("every part put away"), Cues->GetPartsInUse(), 0);
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
