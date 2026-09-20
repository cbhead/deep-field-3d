#include "LaneGraph/DFLaneGraphAsset.h"

#include <limits>

namespace DFLaneGraphPrivate
{
	// "Cannot get there": the sim uses float.PositiveInfinity and so does every comparison here.
	static constexpr float Unreachable = std::numeric_limits<float>::infinity();

	// "Cut off from every core" as a distance something SORTS by: World.cs RemainingToCore
	// returns float.MaxValue, so a stranded enemy is the last target, never the first (infinity
	// would poison the comparison). Distinct from Unreachable, which the tables hold and the
	// relaxations test with IsFinite.
	static constexpr float CutOff = TNumericLimits<float>::Max();
}

DEFINE_LOG_CATEGORY_STATIC(LogDFLaneGraph, Log, All);

void UDFLaneGraphAsset::PostLoad()
{
	Super::PostLoad();
	RebuildIndex();
}

void UDFLaneGraphAsset::RebuildIndex()
{
	NodeIndexMap.Reset();
	EdgeIndexMap.Reset();
	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		NodeIndexMap.Add(Nodes[i].Id, i);
	}
	for (int32 i = 0; i < Edges.Num(); ++i)
	{
		EdgeIndexMap.Add(Edges[i].Id, i);
	}
	bIndexBuilt = true;
}

int32 UDFLaneGraphAsset::NodeIndexOf(FName NodeId) const
{
	if (!bIndexBuilt)
	{
		const_cast<UDFLaneGraphAsset*>(this)->RebuildIndex();
	}
	const int32* Found = NodeIndexMap.Find(NodeId);
	return Found ? *Found : INDEX_NONE;
}

int32 UDFLaneGraphAsset::EdgeIndexOf(FName EdgeId) const
{
	if (!bIndexBuilt)
	{
		const_cast<UDFLaneGraphAsset*>(this)->RebuildIndex();
	}
	const int32* Found = EdgeIndexMap.Find(EdgeId);
	return Found ? *Found : INDEX_NONE;
}

const FDFLaneNode* UDFLaneGraphAsset::FindNode(FName NodeId) const
{
	const int32 Index = NodeIndexOf(NodeId);
	return Index != INDEX_NONE ? &Nodes[Index] : nullptr;
}

const FDFLaneEdge* UDFLaneGraphAsset::FindEdge(FName EdgeId) const
{
	const int32 Index = EdgeIndexOf(EdgeId);
	return Index != INDEX_NONE ? &Edges[Index] : nullptr;
}

const FDFLaneItinerary* UDFLaneGraphAsset::FindItinerary(FName ItineraryId) const
{
	return Itineraries.FindByPredicate([ItineraryId](const FDFLaneItinerary& I) { return I.Id == ItineraryId; });
}

int32 UDFLaneGraphAsset::EdgeBetween(FName From, FName To, EDFEnemyLayer Layer) const
{
	for (int32 i = 0; i < Edges.Num(); ++i)
	{
		if (Edges[i].From == From && Edges[i].To == To && Edges[i].Layer == Layer)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

TArray<int32> UDFLaneGraphAsset::EdgesOf(const FDFLaneItinerary& Itinerary) const
{
	TArray<int32> Out;
	for (int32 i = 0; i + 1 < Itinerary.Via.Num(); ++i)
	{
		const int32 E = EdgeBetween(Itinerary.Via[i], Itinerary.Via[i + 1], Itinerary.Layer);
		if (E == INDEX_NONE)
		{
			UE_LOG(LogDFLaneGraph, Error, TEXT("%s: itinerary '%s' names %s -> %s but no such edge exists"),
				*GetName(), *Itinerary.Id.ToString(), *Itinerary.Via[i].ToString(), *Itinerary.Via[i + 1].ToString());
			break;
		}
		Out.Add(E);
	}
	return Out;
}

float UDFLaneGraphAsset::WalkedLengthMeters(const FDFLaneEdge& Edge)
{
	if (Edge.IsWarp())
	{
		return 0.f;
	}
	double Total = 0.0;
	for (int32 i = 0; i + 1 < Edge.Waypoints.Num(); ++i)
	{
		Total += FVector::Dist(Edge.Waypoints[i], Edge.Waypoints[i + 1]);
	}
	return static_cast<float>(Total / 100.0);
}

TArray<float> UDFLaneGraphAsset::DistanceTo(TFunctionRef<bool(const FDFLaneNode&)> IsTarget, const TArray<bool>* EdgeOpen) const
{
	if (!bIndexBuilt)
	{
		const_cast<UDFLaneGraphAsset*>(this)->RebuildIndex();
	}
	TArray<float> Dist;
	Dist.SetNum(Nodes.Num());
	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		Dist[i] = IsTarget(Nodes[i]) ? 0.f : DFLaneGraphPrivate::Unreachable;
	}

	// Bellman-Ford to a fixed point over reversed edges: dist[from] = min(dist[to] + length).
	// Nodes.Num() passes is the bound; it converges in a handful on graphs this size.
	for (int32 Pass = 0; Pass < Nodes.Num(); ++Pass)
	{
		bool bChanged = false;
		for (int32 E = 0; E < Edges.Num(); ++E)
		{
			if (EdgeOpen && EdgeOpen->IsValidIndex(E) && !(*EdgeOpen)[E])
			{
				continue;
			}
			const FDFLaneEdge& Edge = Edges[E];
			const int32 ToIndex = NodeIndexOf(Edge.To);
			const int32 FromIndex = NodeIndexOf(Edge.From);
			if (ToIndex == INDEX_NONE || FromIndex == INDEX_NONE)
			{
				continue;
			}
			const float Ahead = Dist[ToIndex];
			if (!FMath::IsFinite(Ahead))
			{
				continue;
			}
			const float Through = Ahead + Edge.LengthMeters * Edge.CostFactor;
			if (Through < Dist[FromIndex])
			{
				Dist[FromIndex] = Through;
				bChanged = true;
			}
		}
		if (!bChanged)
		{
			break;
		}
	}
	return Dist;
}

TArray<float> UDFLaneGraphAsset::DistanceToCore(const TArray<bool>* EdgeOpen) const
{
	return DistanceTo([](const FDFLaneNode& N) { return N.Kind == EDFLaneNodeKind::Core; }, EdgeOpen);
}

TArray<float> UDFLaneGraphAsset::DistanceToNode(FName NodeId, const TArray<bool>* EdgeOpen) const
{
	return DistanceTo([NodeId](const FDFLaneNode& N) { return N.Id == NodeId; }, EdgeOpen);
}

bool UDFLaneGraphAsset::EverySpawnReachesCore(const TArray<bool>* EdgeOpen) const
{
	const TArray<float> Dist = DistanceToCore(EdgeOpen);
	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		if (Nodes[i].Kind == EDFLaneNodeKind::Spawn && !FMath::IsFinite(Dist[i]))
		{
			return false;
		}
	}
	return true;
}

bool UDFLaneGraphAsset::WouldSeal(const TArray<int32>& ClosedEdgeIndices) const
{
	TArray<bool> Open;
	Open.Init(true, Edges.Num());
	for (const int32 E : ClosedEdgeIndices)
	{
		if (Open.IsValidIndex(E))
		{
			Open[E] = false;
		}
	}
	return !EverySpawnReachesCore(&Open);
}

bool UDFLaneGraphAsset::WouldSeal(const TSet<FName>& ClosedEdgeIds) const
{
	TArray<int32> Indices;
	for (const FName& Id : ClosedEdgeIds)
	{
		const int32 E = EdgeIndexOf(Id);
		if (E == INDEX_NONE)
		{
			UE_LOG(LogDFLaneGraph, Warning, TEXT("%s: WouldSeal asked about edge '%s', which does not exist"), *GetName(), *Id.ToString());
			continue;
		}
		Indices.Add(E);
	}
	return WouldSeal(Indices);
}

TArray<FName> UDFLaneGraphAsset::ClosableEdges() const
{
	TArray<FName> Out;
	for (const FDFLaneGateDef& Gate : Gates)
	{
		Out.AddUnique(Gate.EdgeId);
	}
	for (const FDFOperatedGateDef& Lever : OperatedGates)
	{
		Out.AddUnique(Lever.EdgeId);
	}
	for (const FDFMutableDef& Mutable : Mutables)
	{
		if (!Mutable.EdgeId.IsNone())
		{
			Out.AddUnique(Mutable.EdgeId);
		}
	}
	return Out;
}

float UDFLaneGraphAsset::RemainingToCore(int32 EdgeIndex, float T, const FDFLaneItinerary* Itinerary, const TArray<bool>* EdgeOpen) const
{
	// World.cs RemainingToCore: on no edge at all (INDEX_NONE) the enemy has leaked — nothing left
	// to walk, so 0; an index past the table is a broken caller and reads as cut off.
	if (EdgeIndex < 0)
	{
		return 0.f;
	}
	if (!Edges.IsValidIndex(EdgeIndex))
	{
		return DFLaneGraphPrivate::CutOff;
	}
	const FDFLaneEdge& Edge = Edges[EdgeIndex];
	// What is left of this edge, priced the way the edges ahead are (length * CostFactor); 0 on a warp.
	const float OnThisEdge = (1.f - FMath::Clamp(T, 0.f, 1.f)) * Edge.LengthMeters * Edge.CostFactor;

	if (Itinerary)
	{
		const TArray<int32> Path = EdgesOf(*Itinerary);
		const int32 At = Path.IndexOfByKey(EdgeIndex);
		if (At != INDEX_NONE)
		{
			// The walker's own plan: the sum of what is still ahead of it on the itinerary. Not the
			// shortest way — "the long way round" is the point of naming a route, and a closed
			// edge later on the plan is the tick's problem (it re-plans at the junction), not this
			// metric's.
			float Ahead = 0.f;
			for (int32 i = At + 1; i < Path.Num(); ++i)
			{
				Ahead += Edges[Path[i]].LengthMeters * Edges[Path[i]].CostFactor;
			}
			return OnThisEdge + Ahead;
		}
	}

	const TArray<float> Dist = DistanceToCore(EdgeOpen);
	const int32 ToIndex = NodeIndexOf(Edge.To);
	const float Ahead = ToIndex != INDEX_NONE ? Dist[ToIndex] : DFLaneGraphPrivate::Unreachable;
	// Cut off from every core: as far away as it is possible to be, so it sorts LAST (World.cs).
	return FMath::IsFinite(Ahead) ? OnThisEdge + Ahead : DFLaneGraphPrivate::CutOff;
}

int32 UDFLaneGraphAsset::ChooseEdge(const FDFLaneItinerary& Itinerary, int32& ViaCursor, FName AtNode, const TArray<bool>& EdgeOpen,
	const TArray<TArray<float>>& DistToNode, const TArray<float>& DistToCore,
	const TFunction<float(int32)>& ExtraCost, const TArray<TArray<float>>* DistToNodeForVias) const
{
	const int32 AtIndex = NodeIndexOf(AtNode);
	if (AtIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	// Skip vias already reached or no longer reachable as planned; the enemy re-plans at the fork.
	// Reachability of a via is asked of the map as it is, not as a siege enemy could make it: an
	// itinerary is a plan for an open map, and if the via kept insisting on a stop behind a wall
	// there would be nothing left to decide — only one edge leads there, so the Ram would break
	// through any wall of any thickness, which is the preference the pricing exists to replace.
	const TArray<TArray<float>>& ViaReach = DistToNodeForVias ? *DistToNodeForVias : DistToNode;
	while (ViaCursor < Itinerary.Via.Num())
	{
		const FName Via = Itinerary.Via[ViaCursor];
		if (Via == AtNode)
		{
			++ViaCursor;
			continue;
		}
		const int32 ViaIndex = NodeIndexOf(Via);
		if (ViaIndex == INDEX_NONE || !FMath::IsFinite(ViaReach[ViaIndex][AtIndex]))
		{
			++ViaCursor;
			continue;
		}
		break;
	}

	const int32 Target = ViaCursor < Itinerary.Via.Num() ? NodeIndexOf(Itinerary.Via[ViaCursor]) : INDEX_NONE;

	int32 Best = INDEX_NONE;
	float BestCost = TNumericLimits<float>::Max();
	for (int32 E = 0; E < Edges.Num(); ++E)
	{
		const FDFLaneEdge& Edge = Edges[E];
		if (Edge.From != AtNode || Edge.Layer != Itinerary.Layer)
		{
			continue;
		}
		// A shut edge is skipped unless the walker can pay to come through it; a wall priced at
		// INFINITY (nothing to chew through, or no siege) is skipped either way.
		const float Extra = ExtraCost ? ExtraCost(E) : 0.f;
		if (!EdgeOpen[E] && !ExtraCost)
		{
			continue;
		}
		if (Extra == DFLaneGraphPrivate::Unreachable)
		{
			continue;
		}
		const int32 ToIndex = NodeIndexOf(Edge.To);
		if (ToIndex == INDEX_NONE)
		{
			continue;
		}
		const float Ahead = Target != INDEX_NONE ? DistToNode[Target][ToIndex] : DistToCore[ToIndex];
		if (!FMath::IsFinite(Ahead))
		{
			continue;
		}
		const float Cost = Edge.LengthMeters * Edge.CostFactor + Extra + Ahead;
		if (Cost < BestCost)
		{
			BestCost = Cost;
			Best = E;
		}
	}
	return Best;
}

TArray<int32> UDFLaneGraphAsset::PathFor(const FDFLaneItinerary& Itinerary, const TArray<bool>& EdgeOpen) const
{
	const TArray<float> DistToCore = DistanceToCore(&EdgeOpen);
	TArray<TArray<float>> DistToNode;
	DistToNode.SetNum(Nodes.Num());
	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		DistToNode[i] = DistanceToNode(Nodes[i].Id, &EdgeOpen);
	}

	TArray<int32> Path;
	if (Itinerary.Via.Num() == 0)
	{
		return Path;
	}
	int32 Cursor = 1;
	FName At = Itinerary.Via[0];
	for (int32 Guard = 0; Guard < Edges.Num() + 1; ++Guard)
	{
		const int32 E = ChooseEdge(Itinerary, Cursor, At, EdgeOpen, DistToNode, DistToCore);
		if (E == INDEX_NONE)
		{
			break;
		}
		Path.Add(E);
		At = Edges[E].To;
		const FDFLaneNode* Node = FindNode(At);
		if (Node && Node->Kind == EDFLaneNodeKind::Core)
		{
			break;
		}
	}
	return Path;
}

TArray<FVector> UDFLaneGraphAsset::Rebuild(FName ItineraryId) const
{
	TArray<FVector> Points;
	const FDFLaneItinerary* Itinerary = FindItinerary(ItineraryId);
	if (!Itinerary)
	{
		return Points;
	}
	for (int32 i = 0; i + 1 < Itinerary->Via.Num(); ++i)
	{
		const int32 E = EdgeBetween(Itinerary->Via[i], Itinerary->Via[i + 1], Itinerary->Layer);
		if (E == INDEX_NONE)
		{
			break;
		}
		// Each edge repeats the node it starts on, which the previous edge already ended on.
		for (int32 k = (i == 0 ? 0 : 1); k < Edges[E].Waypoints.Num(); ++k)
		{
			Points.Add(Edges[E].Waypoints[k]);
		}
	}
	return Points;
}
