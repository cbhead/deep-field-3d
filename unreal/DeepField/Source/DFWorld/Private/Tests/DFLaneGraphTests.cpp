// DF.Unit.LaneGraph.* — the derivation and the routing helpers, pinned against the legacy level
// files (unreal/content/levels/legacy) and the sim's expectations (sim/Sim.Core.Tests/LaneGraphTests.cs,
// Sim.Harness gate 22c). Run: unreal/Build/test.sh DF.Unit.LaneGraph

#include "Misc/AutomationTest.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

#include "LaneGraph/DFLaneGraphAsset.h"
#include "LaneGraph/DFLaneGraphBuilder.h"
#include "LaneGraph/DFLevelFile.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "World/DFWarpGate.h"

namespace DFLaneGraphTest
{
	UDFLaneGraphAsset* BuildLegacy(FAutomationTestBase& Test, const TCHAR* MapId)
	{
		const FString Path = FDFLevelFile::ResolvePath(MapId, /*bPreferLegacy*/ true);
		if (!Test.TestTrue(FString::Printf(TEXT("legacy level file for %s exists (%s)"), MapId, *FDFLevelFile::ContentLevelsDir()), !Path.IsEmpty()))
		{
			return nullptr;
		}
		FDFLevelFile Level;
		FString Error;
		if (!Test.TestTrue(FString::Printf(TEXT("%s loads: %s"), MapId, *Error), FDFLevelFile::Load(Path, Level, Error)))
		{
			return nullptr;
		}
		UDFLaneGraphAsset* Asset = NewObject<UDFLaneGraphAsset>(GetTransientPackage());
		if (!Test.TestTrue(FString::Printf(TEXT("%s builds: %s"), MapId, *Error), UDFLaneGraphBuilder::BuildFromLevel(Level, *Asset, Error)))
		{
			return nullptr;
		}
		return Asset;
	}

	TSet<FName> NodeIds(const UDFLaneGraphAsset& Asset)
	{
		TSet<FName> Out;
		for (const FDFLaneNode& N : Asset.Nodes) { Out.Add(N.Id); }
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFLaneGraphFoundryDerivationTest, "DF.Unit.LaneGraph.FoundryDerivation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFLaneGraphFoundryDerivationTest::RunTest(const FString& Parameters)
{
	UDFLaneGraphAsset* Asset = DFLaneGraphTest::BuildLegacy(*this, TEXT("foundry"));
	if (!Asset) { return false; }

	// Two routes, no overlap, no warps: four nodes (a gate and a core per layer) and nothing else.
	const TSet<FName> Expected = { TEXT("westGate"), TEXT("core"), TEXT("airWest"), TEXT("airCore") };
	const TSet<FName> Ids = DFLaneGraphTest::NodeIds(*Asset);
	TestEqual(TEXT("node count"), Asset->Nodes.Num(), 4);
	for (const FName& Id : Expected)
	{
		TestTrue(FString::Printf(TEXT("node %s"), *Id.ToString()), Ids.Contains(Id));
	}
	for (const FDFLaneNode& N : Asset->Nodes)
	{
		TestNotEqual(FString::Printf(TEXT("%s is not a junction"), *N.Id.ToString()), N.Kind, EDFLaneNodeKind::Junction);
	}

	TestEqual(TEXT("edge count"), Asset->Edges.Num(), 2);
	const FDFLaneEdge* Ground = Asset->FindEdge(TEXT("westGate-core"));
	const FDFLaneEdge* Air = Asset->FindEdge(TEXT("airWest-airCore@air"));
	if (TestNotNull(TEXT("westGate-core"), Ground))
	{
		TestEqual(TEXT("ground edge kind"), Ground->Kind, EDFLaneEdgeKind::Walk);
		TestEqual(TEXT("ground edge waypoints"), Ground->Waypoints.Num(), 8);
		TestEqual(TEXT("ground edge length (m)"), Ground->LengthMeters, 126.f, 0.01f);   // 20+14+20+22+18+14+18
		TestEqual(TEXT("ground edge start is the gate in Unreal cm (X=-Z*100, Y=X*100)"), Ground->Waypoints[0], FVector(0.0, -4000.0, 0.0));
	}
	if (TestNotNull(TEXT("airWest-airCore@air"), Air))
	{
		TestEqual(TEXT("air edge layer"), Air->Layer, EDFEnemyLayer::Air);
		TestEqual(TEXT("air edge start height (cm)"), Air->Waypoints[0].Z, 900.0, 0.01);
	}

	TestEqual(TEXT("itinerary count"), Asset->Itineraries.Num(), 2);
	if (const FDFLaneItinerary* GroundRoute = Asset->FindItinerary(TEXT("ground")))
	{
		TestEqual(TEXT("ground via"), GroundRoute->Via, TArray<FName>({ TEXT("westGate"), TEXT("core") }));
	}
	else
	{
		AddError(TEXT("no 'ground' itinerary"));
	}

	// Lossless: the itinerary rebuilds the authored polyline exactly.
	FDFLevelFile Level;
	FString Error;
	FDFLevelFile::Load(FDFLevelFile::ResolvePath(TEXT("foundry"), true), Level, Error);
	for (const FDFLevelRoute& Route : Level.Routes)
	{
		const TArray<FVector> Rebuilt = Asset->Rebuild(Route.Id);
		TestEqual(FString::Printf(TEXT("%s rebuilds to its waypoint count"), *Route.Id.ToString()), Rebuilt.Num(), Route.Waypoints.Num());
		for (int32 i = 0; i < FMath::Min(Rebuilt.Num(), Route.Waypoints.Num()); ++i)
		{
			TestTrue(FString::Printf(TEXT("%s waypoint %d rebuilt exactly"), *Route.Id.ToString(), i), Rebuilt[i] == Route.Waypoints[i]);
		}
	}
	TestEqual(TEXT("no near misses"), Asset->NearMisses.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFLaneGraphSwitchyardJunctionsTest, "DF.Unit.LaneGraph.SwitchyardJunctions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFLaneGraphSwitchyardJunctionsTest::RunTest(const FString& Parameters)
{
	UDFLaneGraphAsset* Asset = DFLaneGraphTest::BuildLegacy(*this, TEXT("switchyard"));
	if (!Asset) { return false; }

	// The shortcut shares gate and core with the long way and nothing between: step 2b promotes a
	// mid-point on each so the itineraries differ by where they go (LaneGraphTests.cs).
	const TSet<FName> Ids = DFLaneGraphTest::NodeIds(*Asset);
	TestTrue(TEXT("cutMouth"), Ids.Contains(TEXT("cutMouth")));
	TestTrue(TEXT("switchbackNorth"), Ids.Contains(TEXT("switchbackNorth")));
	TestEqual(TEXT("node count"), Asset->Nodes.Num(), 6);
	TestEqual(TEXT("edge count"), Asset->Edges.Num(), 5);
	for (const TCHAR* Id : { TEXT("westGate-cutMouth"), TEXT("cutMouth-core"), TEXT("westGate-switchbackNorth"), TEXT("switchbackNorth-core"), TEXT("airWest-airCore@air") })
	{
		TestNotNull(Id, Asset->FindEdge(Id));
	}

	const FDFLaneItinerary* Long = Asset->FindItinerary(TEXT("ground"));
	const FDFLaneItinerary* Short = Asset->FindItinerary(TEXT("groundShort"));
	if (TestNotNull(TEXT("ground"), Long) && TestNotNull(TEXT("groundShort"), Short))
	{
		TestEqual(TEXT("ground via"), Long->Via, TArray<FName>({ TEXT("westGate"), TEXT("switchbackNorth"), TEXT("core") }));
		TestEqual(TEXT("groundShort via"), Short->Via, TArray<FName>({ TEXT("westGate"), TEXT("cutMouth"), TEXT("core") }));
	}

	// The two lane gates, on the two edges out of the gate.
	TestEqual(TEXT("gate count"), Asset->Gates.Num(), 2);
	const FDFLaneGateDef* B1 = Asset->Gates.FindByPredicate([](const FDFLaneGateDef& G) { return G.SocketId == TEXT("b1"); });
	const FDFLaneGateDef* B2 = Asset->Gates.FindByPredicate([](const FDFLaneGateDef& G) { return G.SocketId == TEXT("b2"); });
	if (TestNotNull(TEXT("b1"), B1)) { TestEqual(TEXT("b1 edge"), B1->EdgeId, FName(TEXT("westGate-cutMouth"))); }
	if (TestNotNull(TEXT("b2"), B2)) { TestEqual(TEXT("b2 edge"), B2->EdgeId, FName(TEXT("westGate-switchbackNorth"))); }
	if (const FDFLaneEdge* Cut = Asset->FindEdge(TEXT("westGate-cutMouth")))
	{
		TestEqual(TEXT("cut edge closable by its socket"), Cut->ClosableBy, EDFLaneClosableBy::Socket);
		TestEqual(TEXT("cut edge closable by b1"), Cut->ClosableById, FName(TEXT("b1")));
	}
	TestEqual(TEXT("closable edges (gates and levers share the two edges)"), Asset->ClosableEdges().Num(), 2);
	TestEqual(TEXT("operated gates"), Asset->OperatedGates.Num(), 2);

	// Sealing, as the sim proves it (Sim.Harness gate 22c "you may shut either way through, and
	// never both") and as rule 11 reads under RFC-0001: either door alone is fine (a single-edge
	// seal would FAIL validation); both together seal, which is a legal combination the validator
	// REPORTS and the runtime refuses at the second closure — WouldSeal is the predicate both use.
	TestFalse(TEXT("closing the cut alone does not seal (a single-edge seal would fail rule 11)"), Asset->WouldSeal(TSet<FName>({ TEXT("westGate-cutMouth") })));
	TestFalse(TEXT("closing the switchback alone does not seal (a single-edge seal would fail rule 11)"), Asset->WouldSeal(TSet<FName>({ TEXT("westGate-switchbackNorth") })));
	TestTrue(TEXT("closing both seals the west gate from the core: a sealing combination — reported by the validator, refused by the runtime at the last closure (RFC-0001), not a validation failure"), Asset->WouldSeal(TSet<FName>({ TEXT("westGate-cutMouth"), TEXT("westGate-switchbackNorth") })));
	TestTrue(TEXT("everything open: every spawn reaches a core"), Asset->EverySpawnReachesCore());

	// Each gate moves the wave (Sim.Harness gate 57 rule 3): shutting the cut sends the shortcut
	// itinerary the long way round.
	TArray<bool> AllOpen;
	AllOpen.Init(true, Asset->Edges.Num());
	TArray<bool> CutShut = AllOpen;
	CutShut[Asset->EdgeIndexOf(TEXT("westGate-cutMouth"))] = false;
	if (Short)
	{
		const TArray<int32> Before = Asset->PathFor(*Short, AllOpen);
		const TArray<int32> After = Asset->PathFor(*Short, CutShut);
		TestEqual(TEXT("open: groundShort walks the cut"), Before, TArray<int32>({ Asset->EdgeIndexOf(TEXT("westGate-cutMouth")), Asset->EdgeIndexOf(TEXT("cutMouth-core")) }));
		TestEqual(TEXT("cut shut: groundShort walks the switchback"), After, TArray<int32>({ Asset->EdgeIndexOf(TEXT("westGate-switchbackNorth")), Asset->EdgeIndexOf(TEXT("switchbackNorth-core")) }));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFLaneGraphToasterWarpEdgesTest, "DF.Unit.LaneGraph.ToasterWarpEdges", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFLaneGraphToasterWarpEdgesTest::RunTest(const FString& Parameters)
{
	UDFLaneGraphAsset* Asset = DFLaneGraphTest::BuildLegacy(*this, TEXT("toaster"));
	if (!Asset) { return false; }

	// Two teleport legs (long's 11 and 20; west shares the first) -> two warp edges, zero length.
	int32 Warps = 0;
	for (const FDFLaneEdge& E : Asset->Edges)
	{
		if (E.Kind == EDFLaneEdgeKind::Warp)
		{
			++Warps;
			TestEqual(FString::Printf(TEXT("%s is zero length"), *E.Id.ToString()), E.LengthMeters, 0.f);
			TestEqual(FString::Printf(TEXT("%s has two waypoints"), *E.Id.ToString()), E.Waypoints.Num(), 2);
		}
	}
	TestEqual(TEXT("warp edge count"), Warps, 2);
	TestNotNull(TEXT("southPad-westPad"), Asset->FindEdge(TEXT("southPad-westPad")));
	TestNotNull(TEXT("northPad-housePad"), Asset->FindEdge(TEXT("northPad-housePad")));

	// Arrival pads are entrances: Spawn nodes.
	for (const TCHAR* Pad : { TEXT("westPad"), TEXT("housePad") })
	{
		const FDFLaneNode* Node = Asset->FindNode(Pad);
		if (TestNotNull(Pad, Node)) { TestEqual(FString::Printf(TEXT("%s is a spawn"), Pad), Node->Kind, EDFLaneNodeKind::Spawn); }
	}

	// Warps are never first, last or adjacent on any itinerary (structural, not a rule).
	for (const FDFLaneItinerary& It : Asset->Itineraries)
	{
		const TArray<int32> Path = Asset->EdgesOf(It);
		for (int32 i = 0; i < Path.Num(); ++i)
		{
			if (!Asset->Edges[Path[i]].IsWarp()) { continue; }
			TestTrue(FString::Printf(TEXT("%s: warp not first"), *It.Id.ToString()), i > 0);
			TestTrue(FString::Printf(TEXT("%s: warp not last"), *It.Id.ToString()), i < Path.Num() - 1);
			if (i > 0) { TestFalse(FString::Printf(TEXT("%s: warp not after a warp"), *It.Id.ToString()), Asset->Edges[Path[i - 1]].IsWarp()); }
			if (i < Path.Num() - 1) { TestFalse(FString::Printf(TEXT("%s: warp not before a warp"), *It.Id.ToString()), Asset->Edges[Path[i + 1]].IsWarp()); }
		}
	}

	// 'drive' is where the direct route rejoins the long way: two edges in, one out.
	const FDFLaneNode* Drive = Asset->FindNode(TEXT("drive"));
	if (TestNotNull(TEXT("drive"), Drive))
	{
		TestEqual(TEXT("drive is a junction"), Drive->Kind, EDFLaneNodeKind::Junction);
		int32 In = 0, Out = 0;
		for (const FDFLaneEdge& E : Asset->Edges)
		{
			if (E.To == TEXT("drive")) { ++In; }
			if (E.From == TEXT("drive")) { ++Out; }
		}
		TestEqual(TEXT("edges into drive"), In, 2);
		TestEqual(TEXT("edges out of drive"), Out, 1);
	}

	// LaneGraphTests.cs: 'direct' adds one edge the long way did not have, and no new places.
	const FDFLaneItinerary* Direct = Asset->FindItinerary(TEXT("direct"));
	const FDFLaneItinerary* Long = Asset->FindItinerary(TEXT("long"));
	if (TestNotNull(TEXT("direct"), Direct) && TestNotNull(TEXT("long"), Long))
	{
		for (const FName& Via : Direct->Via)
		{
			TestTrue(FString::Printf(TEXT("long visits %s"), *Via.ToString()), Long->Via.Contains(Via));
		}
		const TArray<int32> DirectEdges = Asset->EdgesOf(*Direct);
		const TArray<int32> LongEdges = Asset->EdgesOf(*Long);
		int32 OnlyDirect = 0;
		for (const int32 E : DirectEdges) { if (!LongEdges.Contains(E)) { ++OnlyDirect; } }
		TestEqual(TEXT("direct contributes exactly one edge"), OnlyDirect, 1);
	}
	TestEqual(TEXT("edge count"), Asset->Edges.Num(), 9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFLaneGraphRemainingToCoreMonotonicTest, "DF.Unit.LaneGraph.RemainingToCoreMonotonic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFLaneGraphRemainingToCoreMonotonicTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* MapId : { TEXT("foundry"), TEXT("switchyard"), TEXT("spire"), TEXT("toaster"), TEXT("testlane") })
	{
		UDFLaneGraphAsset* Asset = DFLaneGraphTest::BuildLegacy(*this, MapId);
		if (!Asset) { continue; }

		for (const FDFLaneItinerary& It : Asset->Itineraries)
		{
			const TArray<int32> Path = Asset->EdgesOf(It);
			float Previous = TNumericLimits<float>::Max();
			float Expected = 0.f;
			for (const int32 E : Path) { Expected += Asset->Edges[E].LengthMeters; }
			for (int32 p = 0; p < Path.Num(); ++p)
			{
				const int32 E = Path[p];
				for (int32 Step = 0; Step <= 10; ++Step)
				{
					const float T = Step / 10.f;
					const float Remaining = Asset->RemainingToCore(E, T, &It);
					TestTrue(FString::Printf(TEXT("%s/%s: %s t=%.1f finite"), MapId, *It.Id.ToString(), *Asset->Edges[E].Id.ToString(), T), FMath::IsFinite(Remaining));
					TestTrue(FString::Printf(TEXT("%s/%s: %s t=%.1f never increases (%.3f -> %.3f)"), MapId, *It.Id.ToString(), *Asset->Edges[E].Id.ToString(), T, Previous, Remaining), Remaining <= Previous + KINDA_SMALL_NUMBER);
					if (p == 0 && Step == 0)
					{
						TestEqual(FString::Printf(TEXT("%s/%s: start = whole itinerary"), MapId, *It.Id.ToString()), Remaining, Expected, 0.01f);
					}
					Previous = Remaining;
				}
				// Continuous across the node: the end of this edge is the start of the next.
				if (p + 1 < Path.Num())
				{
					TestEqual(FString::Printf(TEXT("%s/%s: continuous at %s"), MapId, *It.Id.ToString(), *Asset->Edges[E].To.ToString()),
						Asset->RemainingToCore(E, 1.f, &It), Asset->RemainingToCore(Path[p + 1], 0.f, &It), 0.01f);
				}
			}
			TestEqual(FString::Printf(TEXT("%s/%s: zero at the core"), MapId, *It.Id.ToString()), Previous, 0.f, 0.01f);

			// Warps contribute nothing: remaining is the same on both pads.
			for (const int32 E : Path)
			{
				if (Asset->Edges[E].IsWarp())
				{
					TestEqual(FString::Printf(TEXT("%s: warp %s adds no distance"), MapId, *Asset->Edges[E].Id.ToString()),
						Asset->RemainingToCore(E, 0.f, &It), Asset->RemainingToCore(E, 1.f, &It), 0.001f);
				}
			}
		}

		// Without an itinerary the metric is the shortest open way, which is never longer than any plan.
		for (const FDFLaneItinerary& It : Asset->Itineraries)
		{
			const TArray<int32> Path = Asset->EdgesOf(It);
			if (Path.Num() > 0)
			{
				TestTrue(FString::Printf(TEXT("%s/%s: shortest <= planned"), MapId, *It.Id.ToString()),
					Asset->RemainingToCore(Path[0], 0.f) <= Asset->RemainingToCore(Path[0], 0.f, &It) + KINDA_SMALL_NUMBER);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFLaneGraphWouldSealTest, "DF.Unit.LaneGraph.WouldSeal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFLaneGraphWouldSealTest::RunTest(const FString& Parameters)
{
	// A hand-built graph: two spawns, a junction, a core.
	//   S1 -> J -> C          (S1-J 10 m, J-C 10 m)
	//   S1 -> C               (S1-C 30 m, the long way)
	//   S2 -> J               (S2-J 5 m)
	UDFLaneGraphAsset* Asset = NewObject<UDFLaneGraphAsset>(GetTransientPackage());
	const auto AddNode = [Asset](const TCHAR* Id, EDFLaneNodeKind Kind)
	{
		FDFLaneNode N;
		N.Id = Id;
		N.Kind = Kind;
		Asset->Nodes.Add(N);
	};
	const auto AddEdge = [Asset](const TCHAR* From, const TCHAR* To, float Meters)
	{
		FDFLaneEdge E;
		E.Id = FName(*(FString(From) + TEXT("-") + To));
		E.From = From;
		E.To = To;
		E.Waypoints = { FVector::ZeroVector, FVector(Meters * 100.f, 0.f, 0.f) };
		E.LengthMeters = Meters;
		Asset->Edges.Add(E);
	};
	AddNode(TEXT("S1"), EDFLaneNodeKind::Spawn);
	AddNode(TEXT("S2"), EDFLaneNodeKind::Spawn);
	AddNode(TEXT("J"), EDFLaneNodeKind::Junction);
	AddNode(TEXT("C"), EDFLaneNodeKind::Core);
	AddEdge(TEXT("S1"), TEXT("J"), 10.f);
	AddEdge(TEXT("J"), TEXT("C"), 10.f);
	AddEdge(TEXT("S1"), TEXT("C"), 30.f);
	AddEdge(TEXT("S2"), TEXT("J"), 5.f);
	Asset->RebuildIndex();

	TestTrue(TEXT("open: every spawn reaches the core"), Asset->EverySpawnReachesCore());
	TestFalse(TEXT("S1-J alone: S1 still has the long way, S2 still has J-C"), Asset->WouldSeal(TSet<FName>({ TEXT("S1-J") })));
	TestFalse(TEXT("S1-C alone: S1 goes via J"), Asset->WouldSeal(TSet<FName>({ TEXT("S1-C") })));
	TestTrue(TEXT("J-C: seals S2 (its only way out)"), Asset->WouldSeal(TSet<FName>({ TEXT("J-C") })));
	TestTrue(TEXT("S1-J and S1-C: seals S1"), Asset->WouldSeal(TSet<FName>({ TEXT("S1-J"), TEXT("S1-C") })));
	TestFalse(TEXT("nothing closed never seals"), Asset->WouldSeal(TSet<FName>()));
	AddExpectedMessage(TEXT("WouldSeal asked about edge 'nope', which does not exist"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	TestFalse(TEXT("an unknown edge id is ignored, not a seal"), Asset->WouldSeal(TSet<FName>({ TEXT("nope") })));

	// Monotonic (harness gate 57 rule 1): anything legal with more shut is legal with less shut.
	const TArray<FName> All = { TEXT("S1-J"), TEXT("J-C"), TEXT("S1-C"), TEXT("S2-J") };
	for (int32 Mask = 0; Mask < (1 << All.Num()); ++Mask)
	{
		TSet<FName> Closed;
		for (int32 g = 0; g < All.Num(); ++g) { if (Mask & (1 << g)) { Closed.Add(All[g]); } }
		if (Asset->WouldSeal(Closed)) { continue; }
		for (int32 g = 0; g < All.Num(); ++g)
		{
			if (!(Mask & (1 << g))) { continue; }
			TSet<FName> Fewer = Closed;
			Fewer.Remove(All[g]);
			TestFalse(FString::Printf(TEXT("opening %s from a legal set stays legal"), *All[g].ToString()), Asset->WouldSeal(Fewer));
		}
	}

	// Distances: relaxation to a fixed point from the core outwards.
	const TArray<float> Dist = Asset->DistanceToCore();
	TestEqual(TEXT("S1 -> core is 20 m (via J, not the 30 m long way)"), Dist[Asset->NodeIndexOf(TEXT("S1"))], 20.f, 0.001f);
	TestEqual(TEXT("S2 -> core is 15 m"), Dist[Asset->NodeIndexOf(TEXT("S2"))], 15.f, 0.001f);
	TestEqual(TEXT("core is 0"), Dist[Asset->NodeIndexOf(TEXT("C"))], 0.f);
	TArray<bool> Open;
	Open.Init(true, Asset->Edges.Num());
	Open[Asset->EdgeIndexOf(TEXT("J-C"))] = false;
	const TArray<float> Cut = Asset->DistanceToCore(&Open);
	TestEqual(TEXT("J-C shut: S1 takes the long way"), Cut[Asset->NodeIndexOf(TEXT("S1"))], 30.f, 0.001f);
	TestFalse(TEXT("J-C shut: S2 is cut off"), FMath::IsFinite(Cut[Asset->NodeIndexOf(TEXT("S2"))]));

	// RemainingToCore for the cut-off walker (World.cs): float max, not infinity, so a targeting
	// sort puts it LAST — after anything that can still get there — and the comparison stays sane.
	const int32 S2J = Asset->EdgeIndexOf(TEXT("S2-J"));
	const int32 S1C = Asset->EdgeIndexOf(TEXT("S1-C"));
	const float Stranded = Asset->RemainingToCore(S2J, 0.5f, nullptr, &Open);
	const float Walking = Asset->RemainingToCore(S1C, 0.5f, nullptr, &Open);
	TestEqual(TEXT("J-C shut: S2-J remaining is TNumericLimits<float>::Max()"), Stranded, TNumericLimits<float>::Max());
	TestTrue(TEXT("J-C shut: S2-J remaining is finite (infinity would poison the sort)"), FMath::IsFinite(Stranded));
	TestEqual(TEXT("J-C shut: halfway along S1-C is 15 m"), Walking, 15.f, 0.001f);
	TestTrue(TEXT("the stranded enemy sorts after the walking one"), Walking < Stranded);
	TestEqual(TEXT("INDEX_NONE is leaked: nothing left to walk"), Asset->RemainingToCore(INDEX_NONE, 0.f), 0.f);
	TestEqual(TEXT("an index past the table reads as cut off"), Asset->RemainingToCore(Asset->Edges.Num() + 3, 0.f), TNumericLimits<float>::Max());

	// What is left of THIS edge is raw metres (World.cs:109-111 sums the remaining segment lengths
	// unweighted); CostFactor only prices the edges ahead, through DistToCore (LaneGraph.cs:510).
	Asset->Edges[S1C].CostFactor = 2.f;
	Asset->RebuildIndex();
	TestEqual(TEXT("S1-C at cost 2: halfway is still 15 m (this edge is not cost-weighted)"), Asset->RemainingToCore(S1C, 0.5f, nullptr, &Open), 15.f, 0.001f);
	Asset->Edges[S1C].CostFactor = 1.f;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFLaneGraphSiegeBreachPricingTest, "DF.Unit.LaneGraph.SiegeBreachPricing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFLaneGraphSiegeBreachPricingTest::RunTest(const FString& Parameters)
{
	// Step.cs NextEdge: a siege enemy is not turned away by a barricade, the wall is PRICED —
	// chewing through costs the metres it could have walked instead. It breaches when breaking is
	// cheaper than walking round, so the player's own gate is what sends the Ram at the wall.
	// Switchyard: the cut (short) and the switchback (long) both lead from the west gate to the
	// core; shut the cut and price it.
	UDFLaneGraphAsset* Asset = DFLaneGraphTest::BuildLegacy(*this, TEXT("switchyard"));
	if (!Asset) { return false; }
	const FDFLaneItinerary* Short = Asset->FindItinerary(TEXT("groundShort"));
	if (!TestNotNull(TEXT("groundShort"), Short)) { return false; }

	const int32 Cut = Asset->EdgeIndexOf(TEXT("westGate-cutMouth"));
	const int32 CutOn = Asset->EdgeIndexOf(TEXT("cutMouth-core"));
	const int32 Switchback = Asset->EdgeIndexOf(TEXT("westGate-switchbackNorth"));
	const int32 SwitchbackOn = Asset->EdgeIndexOf(TEXT("switchbackNorth-core"));
	if (Cut == INDEX_NONE || CutOn == INDEX_NONE || Switchback == INDEX_NONE || SwitchbackOn == INDEX_NONE)
	{
		AddError(TEXT("Switchyard's four ground edges are missing"));
		return false;
	}
	const auto Metres = [Asset](int32 E) { return Asset->Edges[E].LengthMeters * Asset->Edges[E].CostFactor; };
	const float Detour = (Metres(Switchback) + Metres(SwitchbackOn)) - (Metres(Cut) + Metres(CutOn));
	TestTrue(TEXT("the switchback is the long way round"), Detour > 1.f);

	TArray<bool> AllOpen;
	AllOpen.Init(true, Asset->Edges.Num());
	TArray<bool> CutShut = AllOpen;
	CutShut[Cut] = false;
	const auto Tables = [Asset](const TArray<bool>& Open, TArray<TArray<float>>& OutToNode, TArray<float>& OutToCore)
	{
		OutToCore = Asset->DistanceToCore(&Open);
		OutToNode.SetNum(Asset->Nodes.Num());
		for (int32 i = 0; i < Asset->Nodes.Num(); ++i) { OutToNode[i] = Asset->DistanceToNode(Asset->Nodes[i].Id, &Open); }
	};
	TArray<TArray<float>> ToNodeOpen, ToNodeShut;
	TArray<float> ToCoreOpen, ToCoreShut;
	Tables(AllOpen, ToNodeOpen, ToCoreOpen);
	Tables(CutShut, ToNodeShut, ToCoreShut);

	// Everyone else: the shut edge is skipped and the plan falls through to the core the long way.
	int32 Cursor = 1;
	TestEqual(TEXT("no siege: the cut is shut, walk the switchback"), Asset->ChooseEdge(*Short, Cursor, TEXT("westGate"), CutShut, ToNodeShut, ToCoreShut), Switchback);
	TestEqual(TEXT("no siege: the via behind the wall was dropped"), Cursor, 2);

	// A Ram, costed against the open tables (siege routing sees through gates) with the vias asked
	// of the map as it is: a thin wall is cheaper than the detour, so it breaches the cut ...
	// (Step.cs: an open edge costs nothing extra; a shut one costs the wall on it — here only the
	// cut has a wall, any other shut edge has nothing to chew through.)
	const auto Wall = [Cut](float WallMetres, const TArray<bool>& Open)
	{
		const TArray<bool>* OpenPtr = &Open;
		return [WallMetres, Cut, OpenPtr](int32 E) { return (*OpenPtr)[E] ? 0.f : (E == Cut ? WallMetres : std::numeric_limits<float>::infinity()); };
	};
	Cursor = 1;
	TestEqual(TEXT("siege, thin wall: breach the cut"), Asset->ChooseEdge(*Short, Cursor, TEXT("westGate"), CutShut, ToNodeOpen, ToCoreOpen, Wall(Detour * 0.5f, CutShut), &ToNodeShut), Cut);
	TestEqual(TEXT("siege: the via is still asked of the shut map"), Cursor, 2);
	// ... a thick one is not, so it walks round like everyone else ...
	Cursor = 1;
	TestEqual(TEXT("siege, thick wall: walk the switchback"), Asset->ChooseEdge(*Short, Cursor, TEXT("westGate"), CutShut, ToNodeOpen, ToCoreOpen, Wall(Detour * 2.f, CutShut), &ToNodeShut), Switchback);
	// ... and a wall with nothing to chew through (INFINITY) is a shut edge like any other.
	Cursor = 1;
	TestEqual(TEXT("siege, no wall to break: skip the shut edge"), Asset->ChooseEdge(*Short, Cursor, TEXT("westGate"), CutShut, ToNodeOpen, ToCoreOpen, Wall(std::numeric_limits<float>::infinity(), CutShut), &ToNodeShut), Switchback);

	// With every edge open the pricing is inert: same edge as the plain rule, same cursor.
	Cursor = 1;
	int32 PlainCursor = 1;
	TestEqual(TEXT("open map: siege and plain agree"),
		Asset->ChooseEdge(*Short, Cursor, TEXT("westGate"), AllOpen, ToNodeOpen, ToCoreOpen, Wall(1.f, AllOpen), &ToNodeOpen),
		Asset->ChooseEdge(*Short, PlainCursor, TEXT("westGate"), AllOpen, ToNodeOpen, ToCoreOpen));
	TestEqual(TEXT("open map: cursors agree"), Cursor, PlainCursor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFLaneGraphWarpGateIdsTest, "DF.Unit.LaneGraph.WarpGateIds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFLaneGraphWarpGateIdsTest::RunTest(const FString& Parameters)
{
	// The importer places one ADFWarpGate per warp END, keyed "<node>@<edge>": a pad that is the
	// arrival of one warp and the departure of the next must get two actors, one per role, and
	// a node-only key would give it one. StableIdFor/SplitStableId are the key and its inverse.
	const FName Shared(TEXT("midPad"));
	const FName In(TEXT("southPad-midPad"));
	const FName Out(TEXT("midPad-northPad"));
	const FName Arrive = ADFWarpGate::StableIdFor(Shared, In);
	const FName Depart = ADFWarpGate::StableIdFor(Shared, Out);
	TestEqual(TEXT("arrival id"), Arrive, FName(TEXT("midPad@southPad-midPad")));
	TestEqual(TEXT("departure id"), Depart, FName(TEXT("midPad@midPad-northPad")));
	TestNotEqual(TEXT("one pad, two warps, two actors"), Arrive, Depart);

	FName Node, Edge;
	TestTrue(TEXT("arrival id splits"), ADFWarpGate::SplitStableId(Arrive, Node, Edge));
	TestEqual(TEXT("arrival node"), Node, Shared);
	TestEqual(TEXT("arrival edge"), Edge, In);
	TestTrue(TEXT("an air warp's '@air' stays with the edge"), ADFWarpGate::SplitStableId(ADFWarpGate::StableIdFor(TEXT("a"), TEXT("a-b@air")), Node, Edge));
	TestEqual(TEXT("air warp node"), Node, FName(TEXT("a")));
	TestEqual(TEXT("air warp edge"), Edge, FName(TEXT("a-b@air")));
	TestFalse(TEXT("a bare node id is not a warp gate id"), ADFWarpGate::SplitStableId(TEXT("midPad"), Node, Edge));
	TestFalse(TEXT("an empty edge is not a warp gate id"), ADFWarpGate::SplitStableId(TEXT("midPad@"), Node, Edge));

	// Toaster, the one legacy map with warps: two legs, four ends, four distinct ids, and every
	// id round-trips to the end it names.
	UDFLaneGraphAsset* Asset = DFLaneGraphTest::BuildLegacy(*this, TEXT("toaster"));
	if (!Asset) { return false; }
	TSet<FName> Ids;
	int32 Ends = 0;
	for (const FDFLaneEdge& E : Asset->Edges)
	{
		if (!E.IsWarp()) { continue; }
		for (const FName& End : { E.From, E.To })
		{
			++Ends;
			const FName Id = ADFWarpGate::StableIdFor(End, E.Id);
			Ids.Add(Id);
			TestTrue(FString::Printf(TEXT("%s splits"), *Id.ToString()), ADFWarpGate::SplitStableId(Id, Node, Edge));
			TestEqual(FString::Printf(TEXT("%s names its node"), *Id.ToString()), Node, End);
			TestEqual(FString::Printf(TEXT("%s names its edge"), *Id.ToString()), Edge, E.Id);
		}
	}
	TestEqual(TEXT("toaster: four warp ends"), Ends, 4);
	TestEqual(TEXT("toaster: four distinct warp gate ids"), Ids.Num(), Ends);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFLaneGraphLevelFileVectorsTest, "DF.Unit.LaneGraph.LevelFileVectors", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFLaneGraphLevelFileVectorsTest::RunTest(const FString& Parameters)
{
	// Every vector in a level file is read or the load fails, naming the record: a missing or
	// malformed [x,y,z] must never land silently at the origin.
	const FString Base = TEXT(R"({"id":"t","heroSpawn":[0,0,0],"armory":[1,0,0],)")
		TEXT(R"("routes":[{"id":"r","layer":"ground","waypoints":[[0,0,0],[10,0,0]]}],)")
		TEXT(R"("sockets":[{"id":"g1","tag":"ground","pos":[5,0,3]}],)")
		TEXT(R"("stations":[{"id":"s1","pos":[2,0,2]}],)")
		TEXT(R"("vehicles":[{"id":"v1","defId":"buggy","pos":[3,0,3],"yawDegrees":0}],)")
		TEXT(R"("laneNodeNames":[{"id":"start","at":[0,0,0]}],)")
		TEXT(R"("operatedGates":[{"id":"l1","edgeId":"start-end","at":[5,0,1],"label":"L"}]})");
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Tests"), TEXT("DFLevelFileVectors.level.json"));

	const auto Load = [&Path](const FString& Json, FDFLevelFile& Out, FString& Error)
	{
		FFileHelper::SaveStringToFile(Json, *Path);
		return FDFLevelFile::Load(Path, Out, Error);
	};

	FDFLevelFile Level;
	FString Error;
	if (TestTrue(FString::Printf(TEXT("the base file loads: %s"), *Error), Load(Base, Level, Error)))
	{
		TestEqual(TEXT("armory read (sim x=1 -> Unreal Y=100)"), Level.Armory, FVector(0.0, 100.0, 0.0));
		TestEqual(TEXT("one socket"), Level.Sockets.Num(), 1);
		TestEqual(TEXT("one station"), Level.Stations.Num(), 1);
		TestEqual(TEXT("one vehicle"), Level.Vehicles.Num(), 1);
		TestEqual(TEXT("one node name"), Level.LaneNodeNames.Num(), 1);
		TestEqual(TEXT("one lever"), Level.OperatedGates.Num(), 1);
	}

	struct FCase { const TCHAR* What; const TCHAR* From; const TCHAR* To; const TCHAR* Expect; };
	const FCase Cases[] = {
		{ TEXT("socket pos too short"),        TEXT(R"("pos":[5,0,3])"),          TEXT(R"("pos":[5,0])"),      TEXT("socket 'g1'") },
		{ TEXT("socket pos missing"),          TEXT(R"(,"pos":[5,0,3])"),         TEXT(""),                    TEXT("socket 'g1'") },
		{ TEXT("station pos missing"),         TEXT(R"(,"pos":[2,0,2])"),         TEXT(""),                    TEXT("station 's1'") },
		{ TEXT("vehicle pos null"),            TEXT(R"("pos":[3,0,3])"),          TEXT(R"("pos":null)"),       TEXT("vehicle 'v1'") },
		{ TEXT("node name at missing"),        TEXT(R"(,"at":[0,0,0])"),          TEXT(""),                    TEXT("laneNodeNames 'start'") },
		{ TEXT("lever at is a string"),        TEXT(R"("at":[5,0,1])"),           TEXT(R"("at":"here")"),      TEXT("operatedGate 'l1'") },
		{ TEXT("armory missing"),              TEXT(R"("armory":[1,0,0],)"),      TEXT(""),                    TEXT("armory") },
		{ TEXT("heroSpawn is a number"),       TEXT(R"("heroSpawn":[0,0,0])"),    TEXT(R"("heroSpawn":5)"),    TEXT("heroSpawn") },
		{ TEXT("route waypoint too short"),    TEXT(R"([10,0,0])"),               TEXT(R"([10,0])"),           TEXT("route 'r'") },
	};
	for (const FCase& Case : Cases)
	{
		const FString Json = Base.Replace(Case.From, Case.To);
		if (!TestNotEqual(FString::Printf(TEXT("%s: the case changes the file"), Case.What), Json, Base)) { continue; }
		FDFLevelFile Bad;
		FString BadError;
		TestFalse(FString::Printf(TEXT("%s: the load fails"), Case.What), Load(Json, Bad, BadError));
		TestTrue(FString::Printf(TEXT("%s: the error names the record (\"%s\" in \"%s\")"), Case.What, Case.Expect, *BadError), BadError.Contains(Case.Expect));
	}
	IFileManager::Get().Delete(*Path);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
