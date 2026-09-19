// DF.Unit.LaneGraph.* — the derivation and the routing helpers, pinned against the legacy level
// files (unreal/content/levels/legacy) and the sim's expectations (sim/Sim.Core.Tests/LaneGraphTests.cs,
// Sim.Harness gate 22c). Run: unreal/Build/test.sh DF.Unit.LaneGraph

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "LaneGraph/DFLaneGraphAsset.h"
#include "LaneGraph/DFLaneGraphBuilder.h"
#include "LaneGraph/DFLevelFile.h"

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
	// never both"): either door alone is fine; both together leave the gate nowhere to walk.
	TestFalse(TEXT("closing the cut alone does not seal"), Asset->WouldSeal(TSet<FName>({ TEXT("westGate-cutMouth") })));
	TestFalse(TEXT("closing the switchback alone does not seal"), Asset->WouldSeal(TSet<FName>({ TEXT("westGate-switchbackNorth") })));
	TestTrue(TEXT("closing both seals the west gate from the core"), Asset->WouldSeal(TSet<FName>({ TEXT("westGate-cutMouth"), TEXT("westGate-switchbackNorth") })));
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
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
