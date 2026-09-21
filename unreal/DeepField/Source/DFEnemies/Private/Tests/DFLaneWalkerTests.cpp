#include "Misc/AutomationTest.h"
#include "Movement/DFLaneWalker.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.Unit.LaneWalker.* — walking an itinerary, against Step.cs MoveEnemies and ADR-0018's terrain
// rules. A hand-built graph rather than an imported map: these are about the walk, and a fixture
// that states its own geometry is one a failure can be read off.

namespace DFLaneWalkerTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float M = 100.f;   // cm per metre

	FDFLaneNode Node(const TCHAR* Id, const FVector& At, EDFLaneNodeKind Kind = EDFLaneNodeKind::Junction)
	{
		FDFLaneNode N;
		N.Id = Id;
		N.Position = At;
		N.Kind = Kind;
		return N;
	}

	FDFLaneEdge Walk(const TCHAR* From, const TCHAR* To, TArray<FVector> Waypoints)
	{
		FDFLaneEdge E;
		E.Id = FName(*FString::Printf(TEXT("%s-%s"), From, To));
		E.From = From;
		E.To = To;
		E.Waypoints = MoveTemp(Waypoints);
		E.LengthMeters = FDFLaneWalker::SegmentLengthsCm(E).Num() > 0 ? 0.f : 0.f;
		float Cm = 0.f;
		for (float L : FDFLaneWalker::SegmentLengthsCm(E)) { Cm += L; }
		E.LengthMeters = Cm / M;
		return E;
	}

	/** spawn --100 m--> mid --100 m--> core, flat, plus a 100 m detour mid -> side -> core. */
	UDFLaneGraphAsset* Straight()
	{
		UDFLaneGraphAsset* G = NewObject<UDFLaneGraphAsset>(GetTransientPackage());
		G->MapId = TEXT("walkerFixture");
		G->Nodes = {
			Node(TEXT("spawn"), FVector(0, 0, 0), EDFLaneNodeKind::Spawn),
			Node(TEXT("mid"), FVector(100 * M, 0, 0)),
			Node(TEXT("side"), FVector(100 * M, 100 * M, 0)),
			Node(TEXT("core"), FVector(200 * M, 0, 0), EDFLaneNodeKind::Core),
		};
		G->Edges = {
			Walk(TEXT("spawn"), TEXT("mid"), { FVector(0, 0, 0), FVector(50 * M, 0, 0), FVector(100 * M, 0, 0) }),
			Walk(TEXT("mid"), TEXT("core"), { FVector(100 * M, 0, 0), FVector(200 * M, 0, 0) }),
			Walk(TEXT("mid"), TEXT("side"), { FVector(100 * M, 0, 0), FVector(100 * M, 100 * M, 0) }),
			Walk(TEXT("side"), TEXT("core"), { FVector(100 * M, 100 * M, 0), FVector(200 * M, 0, 0) }),
		};
		FDFLaneItinerary Direct;
		Direct.Id = TEXT("ground");
		Direct.Via = { TEXT("spawn"), TEXT("mid"), TEXT("core") };
		FDFLaneItinerary Long;
		Long.Id = TEXT("scenic");
		Long.Via = { TEXT("spawn"), TEXT("mid"), TEXT("side"), TEXT("core") };
		G->Itineraries = { Direct, Long };
		G->RebuildIndex();
		return G;
	}

	/** The real routing struct, with a barricade's hp behind every shut edge so siege pricing has
	 *  something to price. */
	struct FRouting : FDFLaneRouting
	{
		explicit FRouting(const UDFLaneGraphAsset& G, float BarricadeHp = 300.f)
		{
			BlockingHpOf = [BarricadeHp](int32) { return BarricadeHp; };
			Rebuild(G);
		}
	};

	FDFLaneWalkerParams Params(float Speed = 10.f)
	{
		FDFLaneWalkerParams P;
		P.SpeedMetersPerSec = Speed;
		return P;
	}

	/** Step until the walker leaves the lane or Frames run out; returns the events seen. */
	TArray<FDFWalkEvent> Run(const UDFLaneGraphAsset& G, const FDFLaneItinerary& It, const FDFLaneWalkerParams& P,
		FRouting& R, FDFLaneWalkerState& S, int32 Frames = 1200, float Dt = 1.f / 30.f)
	{
		TArray<FDFWalkEvent> All;
		for (int32 I = 0; I < Frames && S.IsWalking(); ++I)
		{
			FDFLaneWalker::Advance(G, It, P, Dt, R, S, All);
		}
		return All;
	}

	int32 CountOf(const TArray<FDFWalkEvent>& Events, EDFWalkEventKind Kind)
	{
		int32 N = 0;
		for (const FDFWalkEvent& E : Events) { N += (E.Kind == Kind) ? 1 : 0; }
		return N;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFLaneWalkerWalksTest, "DF.Unit.LaneWalker.WalksToTheCore", DFLaneWalkerTest::Flags)
bool FDFLaneWalkerWalksTest::RunTest(const FString& Parameters)
{
	using namespace DFLaneWalkerTest;
	UDFLaneGraphAsset* G = Straight();
	const FDFLaneItinerary& It = *G->FindItinerary(TEXT("ground"));
	FRouting R(*G);
	FDFLaneWalkerState S;
	if (!TestTrue(TEXT("begins on the first edge"), FDFLaneWalker::Begin(*G, It, 0.f, S)))
	{
		return false;
	}
	TestEqual(TEXT("which is spawn-mid"), G->Edges[S.EdgeIndex].Id, FName(TEXT("spawn-mid")));
	TestTrue(TEXT("and starts at the spawn"), FDFLaneWalker::LocationOf(*G, S).Equals(FVector::ZeroVector, 1.f));

	// 200 m of lane at 10 m/s is 20 s = 600 frames at 30 Hz. Run past that: a fixture sized to the
	// exact answer measures the float error in 1/30 rather than the thing under test.
	const TArray<FDFWalkEvent> Events = Run(*G, It, Params(10.f), R, S, 640);
	TestEqual(TEXT("reached the core once"), CountOf(Events, EDFWalkEventKind::ReachedCore), 1);
	TestEqual(TEXT("entered mid-core on the way"), CountOf(Events, EDFWalkEventKind::EnteredEdge), 1);
	TestEqual(TEXT("never stranded"), CountOf(Events, EDFWalkEventKind::Stranded), 0);
	TestFalse(TEXT("no longer walking"), S.IsWalking());
	TestEqual(TEXT("nothing left to walk once leaked"), FDFLaneWalker::RemainingToCoreMeters(*G, It, S, R.EdgeOpen), 0.f);

	// It took the time the distance implies, not less: 200 m at 10 m/s, inside one frame's slack.
	TestTrue(TEXT("travelled the lane's length"), FMath::IsNearlyEqual(S.TotalTraveledCm, 200.f * M, 2.f * M));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFLaneWalkerSlopeTest, "DF.Unit.LaneWalker.SlopeSlowsTheClimb", DFLaneWalkerTest::Flags)
bool FDFLaneWalkerSlopeTest::RunTest(const FString& Parameters)
{
	using namespace DFLaneWalkerTest;
	const FDFLaneWalkerParams P = Params();

	// The factor itself, at and either side of the threshold.
	TestEqual(TEXT("flat"), FDFLaneWalker::SlopeFactor(0.f, P), 1.f);
	TestEqual(TEXT("a 14 % climb is still flat walking"), FDFLaneWalker::SlopeFactor(0.14f, P), 1.f);
	TestEqual(TEXT("exactly 15 % is not yet a climb"), FDFLaneWalker::SlopeFactor(0.15f, P), 1.f);
	TestEqual(TEXT("16 % is"), FDFLaneWalker::SlopeFactor(0.16f, P), P.SlopeSpeedUp);
	TestEqual(TEXT("and it is symmetric going down"), FDFLaneWalker::SlopeFactor(-0.16f, P), P.SlopeSpeedDown);
	TestEqual(TEXT("a gentle descent is flat walking"), FDFLaneWalker::SlopeFactor(-0.1f, P), 1.f);

	// Grade off real geometry: 100 m along, 30 m up is 30 %.
	FDFLaneEdge Hill = Walk(TEXT("a"), TEXT("b"), { FVector(0, 0, 0), FVector(100 * M, 0, 30 * M) });
	TestTrue(TEXT("a 30 m rise over 100 m is a 30 % grade"), FMath::IsNearlyEqual(FDFLaneWalker::SegmentGrade(Hill, 0), 0.3f, 1e-4f));
	FDFLaneEdge Down = Walk(TEXT("a"), TEXT("b"), { FVector(0, 0, 30 * M), FVector(100 * M, 0, 0) });
	TestTrue(TEXT("and the same fall is -30 %"), FMath::IsNearlyEqual(FDFLaneWalker::SegmentGrade(Down, 0), -0.3f, 1e-4f));
	FDFLaneEdge Vertical = Walk(TEXT("a"), TEXT("b"), { FVector(0, 0, 0), FVector(0, 0, 10 * M) });
	TestEqual(TEXT("a vertical segment has no grade to act on"), FDFLaneWalker::SegmentGrade(Vertical, 0), 0.f);

	// And it shows up in the walk: the same 100 m climbed takes longer than walked flat.
	auto TimeToCross = [](const FVector& End)
	{
		UDFLaneGraphAsset* G = NewObject<UDFLaneGraphAsset>(GetTransientPackage());
		G->Nodes = { Node(TEXT("a"), FVector::ZeroVector, EDFLaneNodeKind::Spawn), Node(TEXT("b"), End, EDFLaneNodeKind::Core) };
		G->Edges = { Walk(TEXT("a"), TEXT("b"), { FVector::ZeroVector, End }) };
		FDFLaneItinerary It;
		It.Id = TEXT("r");
		It.Via = { TEXT("a"), TEXT("b") };
		G->Itineraries = { It };
		G->RebuildIndex();
		FRouting R(*G);
		FDFLaneWalkerState S;
		FDFLaneWalker::Begin(*G, G->Itineraries[0], 0.f, S);
		int32 Frames = 0;
		TArray<FDFWalkEvent> Events;
		while (S.IsWalking() && Frames < 4000)
		{
			FDFLaneWalker::Advance(*G, G->Itineraries[0], Params(10.f), 1.f / 30.f, R, S, Events);
			++Frames;
		}
		return Frames;
	};
	const int32 Flat = TimeToCross(FVector(100 * M, 0, 0));
	const int32 Up = TimeToCross(FVector(100 * M, 0, 30 * M));
	const int32 DownFrames = TimeToCross(FVector(100 * M, 0, -30 * M));
	TestTrue(TEXT("the climb takes longer than the flat"), Up > Flat);
	TestTrue(TEXT("and the descent is quicker"), DownFrames < Flat);

	// A climb costs twice over and the fixture has to say both: the slope is longer than its
	// footprint (100 m along and 30 m up is a 104.4 m walk) *and* it is walked at x0.85. Comparing
	// against the flat crossing alone was wrong by exactly the slant.
	const float SlantMeters = FMath::Sqrt(100.f * 100.f + 30.f * 30.f);
	const float UpExpected = SlantMeters / (10.f * 0.85f) * 30.f;
	const float DownExpected = SlantMeters / (10.f * 1.1f) * 30.f;
	TestTrue(FString::Printf(TEXT("the climb took %d frames, expected about %.0f"), Up, UpExpected), FMath::Abs(Up - UpExpected) <= 2.f);
	TestTrue(FString::Printf(TEXT("the descent took %d frames, expected about %.0f"), DownFrames, DownExpected), FMath::Abs(DownFrames - DownExpected) <= 2.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFLaneWalkerStrandedTest, "DF.Unit.LaneWalker.StrandedRatherThanLeaked", DFLaneWalkerTest::Flags)
bool FDFLaneWalkerStrandedTest::RunTest(const FString& Parameters)
{
	using namespace DFLaneWalkerTest;
	UDFLaneGraphAsset* G = Straight();
	const FDFLaneItinerary& It = *G->FindItinerary(TEXT("ground"));
	FRouting R(*G);

	// Shut both ways out of mid. The no-sealing rule makes this unreachable in a real map; the
	// walker must still stop at the node rather than leak, vanish, or walk through.
	R.EdgeOpen[G->EdgeIndexOf(TEXT("mid-core"))] = false;
	R.EdgeOpen[G->EdgeIndexOf(TEXT("mid-side"))] = false;
	R.Rebuild(*G);

	FDFLaneWalkerState S;
	FDFLaneWalker::Begin(*G, It, 0.f, S);
	// spawn-mid is 100 m = 10 s at 10 m/s; give it more than the exact answer.
	TArray<FDFWalkEvent> Events = Run(*G, It, Params(10.f), R, S, 400);

	TestEqual(TEXT("stranded, once"), CountOf(Events, EDFWalkEventKind::Stranded), 1);
	TestEqual(TEXT("never reached the core"), CountOf(Events, EDFWalkEventKind::ReachedCore), 0);
	TestTrue(TEXT("still on the lane, so still shootable"), S.IsWalking() && S.bStranded);
	TestTrue(TEXT("standing at the junction"), FDFLaneWalker::LocationOf(*G, S).Equals(FVector(100 * M, 0, 0), 1.f));

	// A cut-off enemy sorts last in a targeting order, never first (World.cs).
	TestEqual(TEXT("remaining distance is the sort-last sentinel"), FDFLaneWalker::RemainingToCoreMeters(*G, It, S, R.EdgeOpen), TNumericLimits<float>::Max());

	// Open the detour: it unstrands and goes, without a second Stranded event.
	R.EdgeOpen[G->EdgeIndexOf(TEXT("mid-side"))] = true;
	R.Rebuild(*G);
	// The detour is longer than the road it replaces — mid-side 100 m plus side-core 141 m is
	// 241 m, 24 s of walking — which is the whole point of a detour and was what the first version
	// of this fixture got wrong by budgeting it the direct route's 200 m.
	Events = Run(*G, It, Params(10.f), R, S, 800);
	TestEqual(TEXT("unstranded once"), CountOf(Events, EDFWalkEventKind::Unstranded), 1);
	TestEqual(TEXT("and reached the core the long way"), CountOf(Events, EDFWalkEventKind::ReachedCore), 1);
	TestFalse(TEXT("no longer stranded"), S.bStranded);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFLaneWalkerSiegeTest, "DF.Unit.LaneWalker.SiegeWalksAtTheWallAndSortsFirst", DFLaneWalkerTest::Flags)
bool FDFLaneWalkerSiegeTest::RunTest(const FString& Parameters)
{
	using namespace DFLaneWalkerTest;
	// The inversion this test exists to catch: a Ram at a barricade must be the NEAREST threat, not
	// a stranded one sorting last. Step.cs routes any StructureDps > 0 enemy against tables that
	// see through gates and prices the wall as hp / dps x speed x bias, so it always has an edge.
	// Without that, the walker flags it stranded, RemainingToCore reports cut off, and every tower
	// in range ignores the enemy breaking the player's wall — silently, which is the worst part.
	UDFLaneGraphAsset* G = Straight();
	const FDFLaneItinerary& It = *G->FindItinerary(TEXT("ground"));

	FDFLaneWalkerParams Ram = Params(10.f);
	Ram.StructureDps = 14.f;       // A1's ram
	const FDFLaneWalkerParams Drifter = Params(10.f);

	// Both ways out of mid are shut, as in the stranded fixture.
	FRouting R(*G);
	R.EdgeOpen[G->EdgeIndexOf(TEXT("mid-core"))] = false;
	R.EdgeOpen[G->EdgeIndexOf(TEXT("mid-side"))] = false;
	R.Rebuild(*G);

	// The drifter strands, as it should: nothing open, nothing it can do about it.
	FDFLaneWalkerState Walker;
	FDFLaneWalker::Begin(*G, It, 0.f, Walker);
	Run(*G, It, Drifter, R, Walker, 400);
	TestTrue(TEXT("a drifter strands at the shut gate"), Walker.bStranded);
	TestEqual(TEXT("and sorts last"), FDFLaneWalker::RemainingToCoreMeters(*G, It, Walker, R.EdgeOpen), TNumericLimits<float>::Max());

	// The ram does not: it takes the blocked edge and starts breaching.
	FDFLaneWalkerState Sieger;
	FDFLaneWalker::Begin(*G, It, 0.f, Sieger);
	const TArray<FDFWalkEvent> Events = Run(*G, It, Ram, R, Sieger, 400);
	TestFalse(TEXT("a ram is never stranded at a wall"), Sieger.bStranded);
	TestEqual(TEXT("it announced the breach once"), CountOf(Events, EDFWalkEventKind::BreachStarted), 1);
	TestTrue(TEXT("on the edge the barricade shuts"), G->Edges[Sieger.EdgeIndex].Id == FName(TEXT("mid-core")) || G->Edges[Sieger.EdgeIndex].Id == FName(TEXT("mid-side")));

	// The whole point: it reports a real, small distance, so targeting picks it first.
	const float Remaining = FDFLaneWalker::RemainingToCoreMeters(*G, It, Sieger, R.EdgeOpen);
	TestTrue(FString::Printf(TEXT("the ram reports a real distance (%.1f m), not the cut-off sentinel"), Remaining), Remaining < 1000.f);
	TestTrue(TEXT("and is nearer the core than the stranded drifter"), Remaining < FDFLaneWalker::RemainingToCoreMeters(*G, It, Walker, R.EdgeOpen));

	// The bias is a real lever, not decoration: a wall in front of a short detour is worth going
	// round, the same wall in front of a long one is not. Open the detour and make the barricade
	// cheap to break, then expensive, and watch the choice change.
	FRouting Detour(*G, /*BarricadeHp*/ 50.f);
	Detour.EdgeOpen[G->EdgeIndexOf(TEXT("mid-core"))] = false;
	Detour.Rebuild(*G);
	int32 Cursor = 1;
	const int32 Cheap = FDFLaneWalker::ChooseNextEdge(*G, It, Ram, TEXT("mid"), Detour, Cursor);

	FRouting Tough(*G, /*BarricadeHp*/ 100000.f);
	Tough.EdgeOpen[G->EdgeIndexOf(TEXT("mid-core"))] = false;
	Tough.Rebuild(*G);
	Cursor = 1;
	const int32 Expensive = FDFLaneWalker::ChooseNextEdge(*G, It, Ram, TEXT("mid"), Tough, Cursor);

	TestEqual(TEXT("a cheap wall is worth breaking"), G->Edges[Cheap].Id, FName(TEXT("mid-core")));
	TestEqual(TEXT("a hard one is worth walking round"), G->Edges[Expensive].Id, FName(TEXT("mid-side")));

	// And a drifter never prices a wall at all: shut is shut.
	Cursor = 1;
	TestEqual(TEXT("a drifter takes the open detour whatever the wall costs"), G->Edges[FDFLaneWalker::ChooseNextEdge(*G, It, Drifter, TEXT("mid"), Cheap >= 0 ? Detour : Detour, Cursor)].Id, FName(TEXT("mid-side")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFLaneWalkerWarpTest, "DF.Unit.LaneWalker.WarpCrossesAtAnySpeed", DFLaneWalkerTest::Flags)
bool FDFLaneWalkerWarpTest::RunTest(const FString& Parameters)
{
	using namespace DFLaneWalkerTest;
	// spawn --walk--> gate ==warp==> pad --walk--> core
	UDFLaneGraphAsset* G = NewObject<UDFLaneGraphAsset>(GetTransientPackage());
	G->Nodes = {
		Node(TEXT("spawn"), FVector::ZeroVector, EDFLaneNodeKind::Spawn),
		Node(TEXT("gate"), FVector(50 * M, 0, 0)),
		Node(TEXT("pad"), FVector(500 * M, 0, 0), EDFLaneNodeKind::Spawn),
		Node(TEXT("core"), FVector(550 * M, 0, 0), EDFLaneNodeKind::Core),
	};
	FDFLaneEdge Warp;
	Warp.Id = TEXT("gate-pad");
	Warp.From = TEXT("gate");
	Warp.To = TEXT("pad");
	Warp.Kind = EDFLaneEdgeKind::Warp;
	Warp.Waypoints = { FVector(50 * M, 0, 0), FVector(500 * M, 0, 0) };
	G->Edges = { Walk(TEXT("spawn"), TEXT("gate"), { FVector::ZeroVector, FVector(50 * M, 0, 0) }), Warp, Walk(TEXT("pad"), TEXT("core"), { FVector(500 * M, 0, 0), FVector(550 * M, 0, 0) }) };
	FDFLaneItinerary It;
	It.Id = TEXT("warped");
	It.Via = { TEXT("spawn"), TEXT("gate"), TEXT("pad"), TEXT("core") };
	G->Itineraries = { It };
	G->RebuildIndex();
	FRouting R(*G);

	FDFLaneWalkerState S;
	FDFLaneWalker::Begin(*G, G->Itineraries[0], 0.f, S);
	TArray<FDFWalkEvent> Events = Run(*G, G->Itineraries[0], Params(10.f), R, S, 600);
	TestEqual(TEXT("warped once"), CountOf(Events, EDFWalkEventKind::Warped), 1);
	TestEqual(TEXT("and reached the core"), CountOf(Events, EDFWalkEventKind::ReachedCore), 1);
	TestTrue(TEXT("the warp's 450 m cost no walking"), FMath::IsNearlyEqual(S.TotalTraveledCm, 100.f * M, 2.f * M));

	// A frozen enemy standing on a departure pad still goes: a zero-length span is the one place
	// this model cannot park anyone (Step.cs). Walk it to the gate, then freeze it.
	FDFLaneWalkerState Frozen;
	FDFLaneWalker::Begin(*G, G->Itineraries[0], 0.f, Frozen);
	TArray<FDFWalkEvent> Before;
	for (int32 I = 0; I < 200 && CountOf(Before, EDFWalkEventKind::Warped) == 0; ++I)
	{
		FDFLaneWalker::Advance(*G, G->Itineraries[0], Params(10.f), 1.f / 30.f, R, Frozen, Before);
	}
	TestEqual(TEXT("it warped on the way"), CountOf(Before, EDFWalkEventKind::Warped), 1);

	FDFLaneWalkerState AtGate;
	FDFLaneWalker::Begin(*G, G->Itineraries[0], 0.f, AtGate);
	AtGate.EdgeIndex = G->EdgeIndexOf(TEXT("gate-pad"));
	TArray<FDFWalkEvent> Stopped;
	FDFLaneWalker::Advance(*G, G->Itineraries[0], Params(0.f), 1.f / 30.f, R, AtGate, Stopped);
	TestEqual(TEXT("speed 0 still crosses the warp"), CountOf(Stopped, EDFWalkEventKind::Warped), 1);
	TestEqual(TEXT("and it is on the far side"), G->Edges[AtGate.EdgeIndex].Id, FName(TEXT("pad-core")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFLaneWalkerFrameRateTest, "DF.Unit.LaneWalker.SameWalkAtAnyFrameRate", DFLaneWalkerTest::Flags)
bool FDFLaneWalkerFrameRateTest::RunTest(const FString& Parameters)
{
	using namespace DFLaneWalkerTest;
	// A host at 30 fps and one at 120 must not disagree about where a wave is, and one long hitch
	// must not walk an enemy through a junction it should have routed at.
	UDFLaneGraphAsset* G = Straight();
	const FDFLaneItinerary& It = *G->FindItinerary(TEXT("scenic"));
	FRouting R(*G);

	auto WalkFor = [&](float Dt, int32 Frames)
	{
		FDFLaneWalkerState S;
		FDFLaneWalker::Begin(*G, It, 0.f, S);
		TArray<FDFWalkEvent> Events;
		for (int32 I = 0; I < Frames; ++I)
		{
			FDFLaneWalker::Advance(*G, It, Params(10.f), Dt, R, S, Events);
		}
		return TPair<FDFLaneWalkerState, TArray<FDFWalkEvent>>(S, Events);
	};

	const auto Slow = WalkFor(1.f / 30.f, 30);      // 1 s
	const auto Fast = WalkFor(1.f / 120.f, 120);    // 1 s
	TestTrue(TEXT("a second of walking is a second, at either rate"), FMath::IsNearlyEqual(Slow.Key.TotalTraveledCm, Fast.Key.TotalTraveledCm, 1.f));
	TestEqual(TEXT("on the same edge"), Slow.Key.EdgeIndex, Fast.Key.EdgeIndex);

	// One hitch long enough to cover the whole route — spawn-mid-side-core is ~341 m, so 60 s at
	// 10 m/s clears it with room to spare. It must still route at both junctions, not through them.
	const auto Hitch = WalkFor(60.f, 1);
	TestEqual(TEXT("a single huge frame still reaches the core"), CountOf(Hitch.Value, EDFWalkEventKind::ReachedCore), 1);
	TestEqual(TEXT("through both junctions"), CountOf(Hitch.Value, EDFWalkEventKind::EnteredEdge), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFLaneWalkerScatterTest, "DF.Unit.LaneWalker.ScatterIsLateralOnly", DFLaneWalkerTest::Flags)
bool FDFLaneWalkerScatterTest::RunTest(const FString& Parameters)
{
	using namespace DFLaneWalkerTest;
	// The plan's lateral offset spreads a pack across the corridor; it must never lift one off the
	// lane's height, or a mote swarm would climb as it fans out.
	UDFLaneGraphAsset* G = Straight();
	const FDFLaneItinerary& It = *G->FindItinerary(TEXT("ground"));
	FRouting R(*G);

	FDFLaneWalkerState Centre, Offset;
	FDFLaneWalker::Begin(*G, It, 0.f, Centre);
	FDFLaneWalker::Begin(*G, It, 1.7f * M, Offset);   // half the 3.4 m corridor
	TArray<FDFWalkEvent> E;
	for (int32 I = 0; I < 60; ++I)
	{
		FDFLaneWalker::Advance(*G, It, Params(10.f), 1.f / 30.f, R, Centre, E);
		FDFLaneWalker::Advance(*G, It, Params(10.f), 1.f / 30.f, R, Offset, E);
	}
	const FVector A = FDFLaneWalker::LocationOf(*G, Centre);
	const FVector B = FDFLaneWalker::LocationOf(*G, Offset);
	TestTrue(TEXT("same distance along the lane"), FMath::IsNearlyEqual(Centre.TotalTraveledCm, Offset.TotalTraveledCm, 0.1f));
	TestTrue(TEXT("offset sideways by the scatter"), FMath::IsNearlyEqual(FVector::Dist2D(A, B), 1.7f * M, 1.f));
	TestTrue(TEXT("and at the same height"), FMath::IsNearlyEqual(A.Z, B.Z, 0.01f));
	TestTrue(TEXT("facing along the lane"), FDFLaneWalker::FacingOf(*G, Centre).Equals(FVector::ForwardVector, 1e-3f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
