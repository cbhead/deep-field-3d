#include "LaneGraph/DFLaneGraphBuilder.h"

#include "LaneGraph/DFLaneGraphAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFLaneGraphBuilder, Log, All);

namespace
{
	// TMap<FVector> hashes the bytes, and -0.0 == 0.0 but hashes differently; every Z=0 waypoint
	// arrives as -0.0 (X = -Z*100), so fold signed zeros before a position becomes a key.
	FVector Canon(const FVector& V)
	{
		return FVector(V.X + 0.0, V.Y + 0.0, V.Z + 0.0);
	}

	// Full-precision text of a position: the dedupe key for spans, as the C# "{X:R},{Y:R},{Z:R}".
	FString PointKey(const FVector& V)
	{
		return FString::Printf(TEXT("%.17g,%.17g,%.17g"), V.X, V.Y, V.Z);
	}

	// Node ordering follows the sim frame (X, then Y, then Z in metres Y-up) so a derived id "n7"
	// means the same junction here as it does in the C# graph and its fixtures.
	bool SimLess(const FVector& A, const FVector& B)
	{
		const FVector SA = FDFSimFrame::ToSim(A);
		const FVector SB = FDFSimFrame::ToSim(B);
		if (SA.X != SB.X) { return SA.X < SB.X; }
		if (SA.Y != SB.Y) { return SA.Y < SB.Y; }
		return SA.Z < SB.Z;
	}

	struct FOccurrence
	{
		int32 Route;
		int32 Index;
	};

	struct FSpan
	{
		int32 Route;
		int32 Start;
		int32 End;
	};

	float GradePercent(const TArray<FVector>& Waypoints)
	{
		float Max = 0.f;
		for (int32 i = 0; i + 1 < Waypoints.Num(); ++i)
		{
			const FVector D = Waypoints[i + 1] - Waypoints[i];
			const double Run = FVector2D(D.X, D.Y).Size();
			if (Run > KINDA_SMALL_NUMBER)
			{
				Max = FMath::Max(Max, static_cast<float>(FMath::Abs(D.Z) / Run * 100.0));
			}
		}
		return Max;
	}
}

bool UDFLaneGraphBuilder::Derive(const TArray<FDFLevelRoute>& Routes, const TArray<FDFLevelNodeName>& NodeNames, UDFLaneGraphAsset& Out, FString& OutError)
{
	Out.Nodes.Reset();
	Out.Edges.Reset();
	Out.Itineraries.Reset();
	Out.NearMisses.Reset();

	if (Routes.Num() == 0)
	{
		OutError = TEXT("no routes");
		return false;
	}

	// --- 1. Every occurrence of every waypoint, by exact position.
	TMap<FVector, TArray<FOccurrence>> Occurrences;
	TArray<FVector> OccurrenceOrder;   // keys in first-seen order (TMap iteration is fine but this keeps it explicit)
	for (int32 R = 0; R < Routes.Num(); ++R)
	{
		for (int32 i = 0; i < Routes[R].Waypoints.Num(); ++i)
		{
			const FVector At = Canon(Routes[R].Waypoints[i]);
			TArray<FOccurrence>* List = Occurrences.Find(At);
			if (!List)
			{
				List = &Occurrences.Add(At);
				OccurrenceOrder.Add(At);
			}
			List->Add({ R, i });
		}
	}

	// --- 2. Which of them are nodes.
	TSet<FVector> IsNode;
	for (const FVector& At : OccurrenceOrder)
	{
		bool bNode = false;
		TSet<FString> Context;
		for (const FOccurrence& Use : Occurrences[At])
		{
			const FDFLevelRoute& Route = Routes[Use.Route];
			const int32 i = Use.Index;
			// Ends of a route are always nodes: a spawn gate and a core.
			if (i == 0 || i == Route.Waypoints.Num() - 1) { bNode = true; }
			// Both ends of a warp are nodes, so the warp is its own edge.
			if (i > 0 && Route.IsTeleportLeg(i - 1)) { bNode = true; }
			if (Route.IsTeleportLeg(i)) { bNode = true; }

			const FString Prev = i > 0 ? PointKey(Canon(Route.Waypoints[i - 1])) : TEXT("null");
			const FString Next = i < Route.Waypoints.Num() - 1 ? PointKey(Canon(Route.Waypoints[i + 1])) : TEXT("null");
			Context.Add(Prev + TEXT("|") + Next);
		}
		// Routes disagree about how they pass through here: a fork, a join, or both.
		if (Context.Num() > 1) { bNode = true; }
		if (bNode) { IsNode.Add(At); }
	}

	// --- 2b. Parallel spans between the same pair of nodes need somewhere to differ, or an
	// itinerary written as a node list cannot say which it means. Promote a waypoint in the middle
	// of each; iterate, because a promotion re-splits routes and can expose a new pair.
	for (int32 Pass = 0; Pass < 8; ++Pass)
	{
		TMap<FString, TArray<FSpan>> Spans;
		TArray<FString> SpanOrder;
		for (int32 R = 0; R < Routes.Num(); ++R)
		{
			const FDFLevelRoute& Route = Routes[R];
			int32 Start = 0;
			for (int32 i = 1; i < Route.Waypoints.Num(); ++i)
			{
				if (!IsNode.Contains(Canon(Route.Waypoints[i]))) { continue; }
				const FString Key = PointKey(Canon(Route.Waypoints[Start])) + TEXT(">") + PointKey(Canon(Route.Waypoints[i]))
					+ TEXT(">") + FString::FromInt(static_cast<int32>(Route.Layer));
				TArray<FSpan>* List = Spans.Find(Key);
				if (!List)
				{
					List = &Spans.Add(Key);
					SpanOrder.Add(Key);
				}
				List->Add({ R, Start, i });
				Start = i;
			}
		}

		int32 Promoted = 0;
		for (const FString& Key : SpanOrder)
		{
			const TArray<FSpan>& Group = Spans[Key];
			// Identical geometry is one edge, not a parallel pair.
			TSet<FString> Distinct;
			for (const FSpan& S : Group)
			{
				FString Shape;
				for (int32 k = S.Start; k <= S.End; ++k)
				{
					Shape += PointKey(Canon(Routes[S.Route].Waypoints[k])) + TEXT(";");
				}
				Distinct.Add(Shape);
			}
			if (Distinct.Num() < 2) { continue; }

			for (const FSpan& S : Group)
			{
				if (S.End - S.Start < 2) { continue; }   // no interior to promote
				const FVector Mid = Canon(Routes[S.Route].Waypoints[S.Start + (S.End - S.Start) / 2]);
				bool bAlready = false;
				IsNode.Add(Mid, &bAlready);
				if (!bAlready) { ++Promoted; }
			}
		}
		if (Promoted == 0) { break; }
	}

	// --- 3. Name them, in a stable order.
	TArray<FVector> Ordered = IsNode.Array();
	Ordered.Sort([](const FVector& A, const FVector& B) { return SimLess(A, B); });

	TMap<FVector, FName> Authored;
	for (const FDFLevelNodeName& Name : NodeNames)
	{
		Authored.Add(Canon(Name.At), Name.Id);
	}
	TMap<FVector, FName> NodeId;
	for (int32 i = 0; i < Ordered.Num(); ++i)
	{
		const FName* Name = Authored.Find(Ordered[i]);
		NodeId.Add(Ordered[i], Name ? *Name : FName(*FString::Printf(TEXT("n%d"), i)));
	}
	for (const FDFLevelNodeName& Name : NodeNames)
	{
		if (!IsNode.Contains(Canon(Name.At)))
		{
			UE_LOG(LogDFLaneGraphBuilder, Warning, TEXT("laneNodeNames '%s' names a point that is not a node; ignored"), *Name.Id.ToString());
		}
	}

	TMap<FVector, EDFLaneNodeKind> Kinds;
	TMap<FVector, EDFEnemyLayer> Layers;
	for (const FVector& At : Ordered)
	{
		Kinds.Add(At, EDFLaneNodeKind::Junction);
	}
	for (const FDFLevelRoute& Route : Routes)
	{
		Kinds.Add(Canon(Route.Waypoints[0]), EDFLaneNodeKind::Spawn);
		Kinds.Add(Canon(Route.Waypoints.Last()), EDFLaneNodeKind::Core);
		// An arrival pad is an entrance in every sense the apron rule means, so it is a spawn too.
		for (int32 Leg = 0; Leg < Route.LegCount(); ++Leg)
		{
			if (Route.IsTeleportLeg(Leg))
			{
				Kinds.Add(Canon(Route.Waypoints[Leg + 1]), EDFLaneNodeKind::Spawn);
			}
		}
		for (const FVector& WP : Route.Waypoints)
		{
			Layers.FindOrAdd(Canon(WP), Route.Layer);
		}
	}

	for (const FVector& At : Ordered)
	{
		FDFLaneNode Node;
		Node.Id = NodeId[At];
		Node.Position = At;
		Node.Kind = Kinds[At];
		Node.Layer = Layers[At];
		Out.Nodes.Add(Node);
	}

	// --- 4. Split each route at its nodes; dedupe identical spans.
	TMap<FString, int32> EdgeByShape;
	for (const FDFLevelRoute& Route : Routes)
	{
		FDFLaneItinerary Itinerary;
		Itinerary.Id = Route.Id;
		Itinerary.Layer = Route.Layer;
		Itinerary.Via.Add(NodeId[Canon(Route.Waypoints[0])]);

		int32 Start = 0;
		for (int32 i = 1; i < Route.Waypoints.Num(); ++i)
		{
			if (!IsNode.Contains(Canon(Route.Waypoints[i]))) { continue; }

			TArray<FVector> Span;
			for (int32 k = Start; k <= i; ++k)
			{
				Span.Add(Canon(Route.Waypoints[k]));
			}
			const bool bWarp = (i == Start + 1) && Route.IsTeleportLeg(Start);
			const FName From = NodeId[Canon(Route.Waypoints[Start])];
			const FName To = NodeId[Canon(Route.Waypoints[i])];

			// Two routes walking the same span share one edge — the duplication this decomposition exists to remove.
			FString Shape = FString::Printf(TEXT("%s|%s|%d|%s|"), *From.ToString(), *To.ToString(), static_cast<int32>(Route.Layer), bWarp ? TEXT("warp") : TEXT("walk"));
			for (const FVector& P : Span)
			{
				Shape += PointKey(P) + TEXT(";");
			}
			if (!EdgeByShape.Contains(Shape))
			{
				// Named for where it runs, not the order it was found in: "gate-drive" survives a
				// waypoint being nudged, "e6" does not. The "#n" suffix is for the case node
				// promotion could not separate — two spans between the same pair with no interior.
				FString Id = From.ToString() + TEXT("-") + To.ToString();
				if (Route.Layer == EDFEnemyLayer::Air) { Id += TEXT("@air"); }
				const auto Exists = [&Out](const FString& Candidate)
				{
					return Out.Edges.ContainsByPredicate([&Candidate](const FDFLaneEdge& E) { return E.Id.ToString() == Candidate; });
				};
				for (int32 n = 2; Exists(Id); ++n)
				{
					Id = FString::Printf(TEXT("%s-%s#%d"), *From.ToString(), *To.ToString(), n);
				}

				FDFLaneEdge Edge;
				Edge.Id = FName(*Id);
				Edge.From = From;
				Edge.To = To;
				Edge.Layer = Route.Layer;
				Edge.Kind = bWarp ? EDFLaneEdgeKind::Warp : EDFLaneEdgeKind::Walk;
				Edge.Waypoints = Span;
				Edge.LengthMeters = UDFLaneGraphAsset::WalkedLengthMeters(Edge);
				Edge.MaxGradePercent = bWarp ? 0.f : GradePercent(Span);
				if (Route.Layer == EDFEnemyLayer::Air)
				{
					double Sum = 0.0;
					for (const FVector& P : Span) { Sum += P.Z; }
					Edge.AglMeters = static_cast<float>(Sum / Span.Num() / 100.0);   // legacy air is absolute over a flat floor
				}
				EdgeByShape.Add(Shape, Out.Edges.Add(Edge));
			}

			Itinerary.Via.Add(To);
			Start = i;
		}
		Out.Itineraries.Add(MoveTemp(Itinerary));
	}

	// --- 5. Near misses: distinct points close enough to be a typo.
	TArray<FVector> All = OccurrenceOrder;
	All.Sort([](const FVector& A, const FVector& B) { return SimLess(A, B); });
	for (int32 i = 0; i < All.Num(); ++i)
	{
		for (int32 j = i + 1; j < All.Num(); ++j)
		{
			const float D = static_cast<float>(FVector::Dist(All[i], All[j]) / 100.0);
			if (D > 0.f && D < NearMissMeters)
			{
				const FVector A = FDFSimFrame::ToSim(All[i]);
				const FVector B = FDFSimFrame::ToSim(All[j]);
				Out.NearMisses.Add(FString::Printf(TEXT("(%.2f,%.2f,%.2f) and (%.2f,%.2f,%.2f) are %.3f m apart"), A.X, A.Y, A.Z, B.X, B.Y, B.Z, D));
			}
		}
	}

	Out.RebuildIndex();
	return true;
}

bool UDFLaneGraphBuilder::BuildFromLevel(const FDFLevelFile& Level, UDFLaneGraphAsset& Out, FString& OutError)
{
	if (!Derive(Level.Routes, Level.LaneNodeNames, Out, OutError))
	{
		return false;
	}
	Out.MapId = Level.Id;
	Out.Gates.Reset();
	Out.OperatedGates.Reset();
	Out.Mutables.Reset();
	Out.BossRoutes.Reset();
	Out.Sockets.Reset();
	Out.Stations.Reset();
	Out.Traversal.Reset();
	Out.VehicleSpawns.Reset();

	TArray<FString> Problems;
	for (const FDFLevelLaneGate& Gate : Level.LaneGates)
	{
		FDFLaneGateDef Def;
		Def.EdgeId = Gate.EdgeId;
		Def.SocketId = Gate.SocketId;
		const int32 E = Out.EdgeIndexOf(Gate.EdgeId);
		if (E == INDEX_NONE)
		{
			Problems.Add(FString::Printf(TEXT("laneGates/%s gates edge '%s', which does not exist"), *Gate.SocketId.ToString(), *Gate.EdgeId.ToString()));
		}
		else if (!Level.Sockets.ContainsByPredicate([&Gate](const FDFLevelSocket& S) { return S.Id == Gate.SocketId; }))
		{
			Problems.Add(FString::Printf(TEXT("laneGates/%s names a socket that does not exist"), *Gate.SocketId.ToString()));
		}
		else
		{
			Out.Edges[E].ClosableBy = EDFLaneClosableBy::Socket;
			Out.Edges[E].ClosableById = Gate.SocketId;
		}
		Out.Gates.Add(Def);
	}
	for (const FDFLevelOperatedGate& Lever : Level.OperatedGates)
	{
		FDFOperatedGateDef Def;
		Def.Id = Lever.Id;
		Def.EdgeId = Lever.EdgeId;
		Def.At = Lever.At;
		Def.Label = Lever.Label;
		const int32 E = Out.EdgeIndexOf(Lever.EdgeId);
		if (E == INDEX_NONE)
		{
			Problems.Add(FString::Printf(TEXT("operatedGates/%s gates edge '%s', which does not exist"), *Lever.Id.ToString(), *Lever.EdgeId.ToString()));
		}
		else if (Out.Edges[E].ClosableBy == EDFLaneClosableBy::None)
		{
			// A barricade socket on the same edge keeps precedence: the wall is what a Ram prices.
			Out.Edges[E].ClosableBy = EDFLaneClosableBy::Lever;
			Out.Edges[E].ClosableById = Lever.Id;
		}
		Out.OperatedGates.Add(Def);
	}
	for (const FDFLevelSocket& Socket : Level.Sockets)
	{
		FDFSocketDef Def;
		Def.SocketId = Socket.Id;
		Def.Tag = Socket.Tag;
		Def.Position = Socket.Position;
		Out.Sockets.Add(Def);
	}
	for (const FDFLevelStation& Station : Level.Stations)
	{
		FDFStationDef Def;
		Def.Id = Station.Id;
		Def.Position = Station.Position;
		Out.Stations.Add(Def);
	}
	for (const FDFLevelVehicle& Vehicle : Level.Vehicles)
	{
		FDFVehicleSpawnDef Def;
		Def.Id = Vehicle.Id;
		Def.VehicleId = Vehicle.DefId;
		Def.Position = Vehicle.Position;
		Def.Yaw = Vehicle.Yaw;
		Out.VehicleSpawns.Add(Def);
	}
	{
		FDFTraversalDef Armory;
		Armory.Id = TEXT("armory");
		Armory.Kind = EDFTraversalKind::Armory;
		Armory.Position = Level.Armory;
		Armory.Label = TEXT("ARMORY");
		Out.Traversal.Add(Armory);
	}

	if (Problems.Num() > 0)
	{
		OutError = FString::Join(Problems, TEXT("; "));
		return false;
	}
	Out.RebuildIndex();
	return true;
}
