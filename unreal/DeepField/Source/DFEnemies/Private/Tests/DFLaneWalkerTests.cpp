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
	if (!TestTrue(TEXT("begins on the first edge"), FDFLaneWalker::Begin(*G, It, Params(10.f), R, 0.f, S)))
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
		FDFLaneWalker::Begin(*G, G->Itineraries[0], Params(10.f), R, 0.f, S);
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
	FDFLaneWalker::Begin(*G, It, Params(10.f), R, 0.f, S);
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
	FDFLaneWalker::Begin(*G, It, Drifter, R, 0.f, Walker);
	Run(*G, It, Drifter, R, Walker, 400);
	TestTrue(TEXT("a drifter strands at the shut gate"), Walker.bStranded);
	TestEqual(TEXT("and sorts last"), FDFLaneWalker::RemainingToCoreMeters(*G, It, Walker, R.EdgeOpen), TNumericLimits<float>::Max());

	// The ram does not: it takes the blocked edge and starts breaching.
	FDFLaneWalkerState Sieger;
	FDFLaneWalker::Begin(*G, It, Ram, R, 0.f, Sieger);
	const TArray<FDFWalkEvent> Events = Run(*G, It, Ram, R, Sieger, 400);
	TestFalse(TEXT("a ram is never stranded at a wall"), Sieger.bStranded);
	TestEqual(TEXT("it announced the breach once"), CountOf(Events, EDFWalkEventKind::BreachStarted), 1);
	TestTrue(TEXT("on the edge the barricade shuts"), G->Edges[Sieger.EdgeIndex].Id == FName(TEXT("mid-core")) || G->Edges[Sieger.EdgeIndex].Id == FName(TEXT("mid-side")));

	// The whole point: it reports a real, small distance, so targeting picks it first.
	const float Remaining = FDFLaneWalker::RemainingToCoreMeters(*G, It, Sieger, R.EdgeOpen);
	TestTrue(FString::Printf(TEXT("the ram reports a real distance (%.1f m), not the cut-off sentinel"), Remaining), Remaining < 1000.f);
	TestTrue(TEXT("and is nearer the core than the stranded drifter"), Remaining < FDFLaneWalker::RemainingToCoreMeters(*G, It, Walker, R.EdgeOpen));

	// The bias and the speed are real terms, not decoration. Pinning only "a cheap wall is taken and
	// a dear one is not" leaves both factors deletable from the formula with the suite still green —
	// the accidental-pass shape one level in, proving the branch is taken rather than the arithmetic
	// right. So: hold the wall fixed and vary each factor alone, either side of the decision.
	//
	// Breaching is priced hp/dps x speed x bias against a ~141 m detour (mid-side 100 + side-core 141
	// versus mid-core 100, so going round costs ~141 m more than straight through).
	auto ChoiceWith = [&](float BarricadeHp, float Bias, float RowSpeed)
	{
		FRouting Wall(*G, BarricadeHp);
		Wall.EdgeOpen[G->EdgeIndexOf(TEXT("mid-core"))] = false;
		Wall.Rebuild(*G);
		FDFLaneWalkerParams P = Params(10.f);
		P.StructureDps = 14.f;
		P.SiegeBreachBias = Bias;
		P.RowSpeedMetersPerSec = RowSpeed;
		int32 Cur = 1;
		return G->Edges[FDFLaneWalker::ChooseNextEdge(*G, It, P, TEXT("mid"), Wall, Cur)].Id;
	};
	// One wall, one speed, two biases: 0.6 makes 300 hp look like ~129 m of walking (cheaper than
	// the detour), 2.0 makes the same wall ~429 m (dearer). Only the bias moved.
	TestEqual(TEXT("at bias 0.6 the wall is worth breaking"), ChoiceWith(300.f, 0.6f, 10.f), FName(TEXT("mid-core")));
	TestEqual(TEXT("at bias 2.0 the same wall is worth walking round"), ChoiceWith(300.f, 2.0f, 10.f), FName(TEXT("mid-side")));
	// One wall, one bias, two row speeds: a faster enemy pays more walking-distance per second of
	// breaching, so the same wall it would break at 4 m/s it walks round at 20. Only the speed moved.
	TestEqual(TEXT("a slow ram breaks it"), ChoiceWith(300.f, 0.6f, 4.f), FName(TEXT("mid-core")));
	TestEqual(TEXT("a fast one goes round"), ChoiceWith(300.f, 0.6f, 20.f), FName(TEXT("mid-side")));

	// And the row speed is the one that counts: a chilled Ram must value the wall as a Ram does.
	{
		FRouting Wall(*G, 300.f);
		Wall.EdgeOpen[G->EdgeIndexOf(TEXT("mid-core"))] = false;
		Wall.Rebuild(*G);
		FDFLaneWalkerParams Chilled = Params(3.5f);   // 10 m/s row speed, chilled to 0.35x
		Chilled.StructureDps = 14.f;
		Chilled.RowSpeedMetersPerSec = 10.f;
		FDFLaneWalkerParams Unchilled = Params(10.f);
		Unchilled.StructureDps = 14.f;
		Unchilled.RowSpeedMetersPerSec = 10.f;
		int32 A = 1, B = 1;
		TestEqual(TEXT("a chilled ram values the wall exactly as an unchilled one does"),
			FDFLaneWalker::ChooseNextEdge(*G, It, Chilled, TEXT("mid"), Wall, A),
			FDFLaneWalker::ChooseNextEdge(*G, It, Unchilled, TEXT("mid"), Wall, B));
	}

	// The original pair, kept: the wall's own hp still decides at a fixed bias and speed.
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
	FDFLaneWalker::Begin(*G, G->Itineraries[0], Params(10.f), R, 0.f, S);
	TArray<FDFWalkEvent> Events = Run(*G, G->Itineraries[0], Params(10.f), R, S, 600);
	TestEqual(TEXT("warped once"), CountOf(Events, EDFWalkEventKind::Warped), 1);
	TestEqual(TEXT("and reached the core"), CountOf(Events, EDFWalkEventKind::ReachedCore), 1);
	TestTrue(TEXT("the warp's 450 m cost no walking"), FMath::IsNearlyEqual(S.TotalTraveledCm, 100.f * M, 2.f * M));

	// A frozen enemy standing on a departure pad still goes: a zero-length span is the one place
	// this model cannot park anyone (Step.cs). Walk it to the gate, then freeze it.
	FDFLaneWalkerState Frozen;
	FDFLaneWalker::Begin(*G, G->Itineraries[0], Params(10.f), R, 0.f, Frozen);
	TArray<FDFWalkEvent> Before;
	for (int32 I = 0; I < 200 && CountOf(Before, EDFWalkEventKind::Warped) == 0; ++I)
	{
		FDFLaneWalker::Advance(*G, G->Itineraries[0], Params(10.f), 1.f / 30.f, R, Frozen, Before);
	}
	TestEqual(TEXT("it warped on the way"), CountOf(Before, EDFWalkEventKind::Warped), 1);

	FDFLaneWalkerState AtGate;
	FDFLaneWalker::Begin(*G, G->Itineraries[0], Params(10.f), R, 0.f, AtGate);
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
		FDFLaneWalker::Begin(*G, It, Params(10.f), R, 0.f, S);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFLaneWalkerWarpParityTest, "DF.Unit.LaneWalker.WarpArrivalBehavesLikeAnyOtherNode", DFLaneWalkerTest::Flags)
bool FDFLaneWalkerWarpParityTest::RunTest(const FString& Parameters)
{
	using namespace DFLaneWalkerTest;
	// The warp path used to be a second implementation of "arrive at a node and route from it", and
	// it had drifted: it never cleared bStranded, never announced a breach, never checked for a
	// core. The blocker was the first of those — a walker freed after stranding at a warp
	// destination walked the rest of the way to the core still reporting cut off, so every tower
	// sorted it LAST and ignored it all the way in. This test is the parity the shared routine buys.
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

	// Shut the only way out of the warp destination, then walk into it.
	FRouting R(*G);
	R.EdgeOpen[G->EdgeIndexOf(TEXT("pad-core"))] = false;
	R.Rebuild(*G);

	FDFLaneWalkerState S;
	FDFLaneWalker::Begin(*G, G->Itineraries[0], Params(10.f), R, 0.f, S);
	TArray<FDFWalkEvent> Events = Run(*G, G->Itineraries[0], Params(10.f), R, S, 400);
	TestTrue(TEXT("it stranded at the warp destination"), S.bStranded);
	TestEqual(TEXT("having warped exactly once"), CountOf(Events, EDFWalkEventKind::Warped), 1);
	TestEqual(TEXT("and said so once"), CountOf(Events, EDFWalkEventKind::Stranded), 1);

	// Where it stands is the arrival pad, not the departure pad half a map away.
	TestTrue(TEXT("it is standing on the arrival pad"), FDFLaneWalker::LocationOf(*G, S).Equals(FVector(500 * M, 0, 0), 1.f));
	TestEqual(TEXT("and sorts last while cut off"), FDFLaneWalker::RemainingToCoreMeters(*G, G->Itineraries[0], S, R.EdgeOpen), TNumericLimits<float>::Max());

	// Staying stranded must not re-announce the crossing every frame.
	TArray<FDFWalkEvent> WhileStuck = Run(*G, G->Itineraries[0], Params(10.f), R, S, 120);
	TestEqual(TEXT("no second Warped while it waits"), CountOf(WhileStuck, EDFWalkEventKind::Warped), 0);
	TestEqual(TEXT("and no second Stranded"), CountOf(WhileStuck, EDFWalkEventKind::Stranded), 0);

	// **The blocker.** Open the way; it must unstrand and stop reporting cut off.
	R.EdgeOpen[G->EdgeIndexOf(TEXT("pad-core"))] = true;
	R.Rebuild(*G);
	TArray<FDFWalkEvent> Freed;
	FDFLaneWalker::Advance(*G, G->Itineraries[0], Params(10.f), 1.f / 30.f, R, S, Freed);
	TestFalse(TEXT("it is no longer stranded"), S.bStranded);
	TestEqual(TEXT("and said so"), CountOf(Freed, EDFWalkEventKind::Unstranded), 1);
	const float Remaining = FDFLaneWalker::RemainingToCoreMeters(*G, G->Itineraries[0], S, R.EdgeOpen);
	TestTrue(FString::Printf(TEXT("and reports a real distance (%.1f m), not cut off"), Remaining), Remaining < 1000.f);
	Run(*G, G->Itineraries[0], Params(10.f), R, S, 400);
	TestFalse(TEXT("and it reaches the core"), S.IsWalking());

	// A sieging walker arriving at a warp destination announces its breach, as it does at any node.
	FDFLaneWalkerParams Ram = Params(10.f);
	Ram.StructureDps = 14.f;
	Ram.RowSpeedMetersPerSec = 10.f;
	FRouting Shut(*G, 50.f);
	Shut.EdgeOpen[G->EdgeIndexOf(TEXT("pad-core"))] = false;
	Shut.Rebuild(*G);
	FDFLaneWalkerState Sieger;
	FDFLaneWalker::Begin(*G, G->Itineraries[0], Ram, Shut, 0.f, Sieger);
	TArray<FDFWalkEvent> SiegeEvents = Run(*G, G->Itineraries[0], Ram, Shut, Sieger, 400);
	TestFalse(TEXT("a ram is not stranded past a warp either"), Sieger.bStranded);
	TestEqual(TEXT("and announces the breach it starts there"), CountOf(SiegeEvents, EDFWalkEventKind::BreachStarted), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFLaneWalkerBeginRoutesTest, "DF.Unit.LaneWalker.BeginRoutesItsFirstEdge", DFLaneWalkerTest::Flags)
bool FDFLaneWalkerBeginRoutesTest::RunTest(const FString& Parameters)
{
	using namespace DFLaneWalkerTest;
	// The spawn node is a routing decision like any other (Step.cs: "Its first edge is chosen the
	// same way every later one is"). Taking the itinerary's first edge unconditionally would start a
	// walker on a shut edge and walk it straight through the thing that was shut.
	UDFLaneGraphAsset* G = Straight();
	const FDFLaneItinerary& It = *G->FindItinerary(TEXT("ground"));

	FRouting R(*G);
	R.EdgeOpen[G->EdgeIndexOf(TEXT("spawn-mid"))] = false;   // the itinerary's own first edge
	R.Rebuild(*G);

	FDFLaneWalkerState S;
	// Refusing is loud on purpose — a wave group whose route cannot start is a content fault, not a
	// thing to swallow — so the test declares the log it is asking for.
	AddExpectedMessage(TEXT("nothing open out of"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
	TestFalse(TEXT("it refuses to start on a shut first edge"), FDFLaneWalker::Begin(*G, It, Params(10.f), R, 0.f, S));
	TestFalse(TEXT("and is not left walking one"), S.IsWalking());

	// Open it again and the same call starts normally.
	R.EdgeOpen[G->EdgeIndexOf(TEXT("spawn-mid"))] = true;
	R.Rebuild(*G);
	TestTrue(TEXT("and starts when the way is open"), FDFLaneWalker::Begin(*G, It, Params(10.f), R, 0.f, S));
	TestEqual(TEXT("on the first edge"), G->Edges[S.EdgeIndex].Id, FName(TEXT("spawn-mid")));
	TestFalse(TEXT("not stranded"), S.bStranded);

	// A replicated or saved state with a stale edge index is not trusted input.
	FDFLaneWalkerState Corrupt = S;
	Corrupt.EdgeIndex = 9999;
	AddExpectedMessage(TEXT("walker state holds edge index"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
	TestEqual(TEXT("an out-of-range edge index sorts last rather than reading past the array"),
		FDFLaneWalker::RemainingToCoreMeters(*G, It, Corrupt, R.EdgeOpen), TNumericLimits<float>::Max());
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
	FDFLaneWalker::Begin(*G, It, Params(10.f), R, 0.f, Centre);
	FDFLaneWalker::Begin(*G, It, Params(10.f), R, 1.7f * M, Offset);   // half the 3.4 m corridor
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
