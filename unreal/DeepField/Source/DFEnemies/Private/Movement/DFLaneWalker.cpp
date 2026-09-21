#include "Movement/DFLaneWalker.h"

#include "Waves/DFDetMath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DFLaneWalker)

DEFINE_LOG_CATEGORY_STATIC(LogDFWalk, Log, All);

namespace
{
	constexpr float CmPerMeter = 100.f;

	void Add(TArray<FDFWalkEvent>& Events, EDFWalkEventKind Kind, FName Detail, const FVector& Location)
	{
		FDFWalkEvent& E = Events.AddDefaulted_GetRef();
		E.Kind = Kind;
		E.Detail = Detail;
		E.Location = Location;
	}

	/** Sideways along the ground plane: the scatter must not lift a walker off its lane. */
	FVector LateralAxis(const FVector& Forward)
	{
		const FVector Flat(Forward.X, Forward.Y, 0.f);
		return Flat.IsNearlyZero() ? FVector::RightVector : FVector::CrossProduct(FVector::UpVector, Flat.GetSafeNormal());
	}
}

TArray<float> FDFLaneWalker::SegmentLengthsCm(const FDFLaneEdge& Edge)
{
	TArray<float> Lengths;
	for (int32 I = 0; I + 1 < Edge.Waypoints.Num(); ++I)
	{
		Lengths.Add(FVector::Dist(Edge.Waypoints[I], Edge.Waypoints[I + 1]));
	}
	return Lengths;
}

float FDFLaneWalker::SegmentGrade(const FDFLaneEdge& Edge, int32 Segment)
{
	if (!Edge.Waypoints.IsValidIndex(Segment) || !Edge.Waypoints.IsValidIndex(Segment + 1))
	{
		return 0.f;
	}
	const FVector Delta = Edge.Waypoints[Segment + 1] - Edge.Waypoints[Segment];
	const float Horizontal = Delta.Size2D();
	// A vertical or zero-length segment has no grade a walker can act on: a ladder is traversal,
	// not a lane, and the validator refuses a lane steep enough for this to matter (rule 1).
	return Horizontal > KINDA_SMALL_NUMBER ? Delta.Z / Horizontal : 0.f;
}

float FDFLaneWalker::SlopeFactor(float Grade, const FDFLaneWalkerParams& Params)
{
	const float Threshold = FMath::Max(0.f, Params.SlopeGradeThreshold);
	if (Grade > Threshold)
	{
		return Params.SlopeSpeedUp;     // a climb is a kill zone: the lever level designers get free
	}
	if (Grade < -Threshold)
	{
		return Params.SlopeSpeedDown;
	}
	return 1.f;
}

bool FDFLaneWalker::Begin(const UDFLaneGraphAsset& Graph, const FDFLaneItinerary& Itinerary, float LateralOffsetCm, FDFLaneWalkerState& Out)
{
	Out = FDFLaneWalkerState();
	Out.LateralOffsetCm = LateralOffsetCm;

	const TArray<int32> Edges = Graph.EdgesOf(Itinerary);
	if (Edges.Num() == 0 || Edges[0] == INDEX_NONE)
	{
		UE_LOG(LogDFWalk, Error, TEXT("itinerary '%s' has no first edge; nothing can walk it"), *Itinerary.Id.ToString());
		return false;
	}
	Out.EdgeIndex = Edges[0];
	Out.ViaCursor = 1;   // Via[0] is where it starts; routing asks about the next one
	return true;
}

void FDFLaneWalker::Advance(const UDFLaneGraphAsset& Graph, const FDFLaneItinerary& Itinerary, const FDFLaneWalkerParams& Params,
	float DeltaSeconds, const TArray<bool>& EdgeOpen, const TArray<TArray<float>>& DistToNode, const TArray<float>& DistToCore,
	FDFLaneWalkerState& State, TArray<FDFWalkEvent>& OutEvents)
{
	if (!State.IsWalking())
	{
		return;
	}

	// Distance this step, before the per-segment slope factor: that one is applied as each segment
	// is walked, because the grade is the segment's, not the edge's.
	float RemainingCm = FMath::Max(0.f, Params.SpeedMetersPerSec) * CmPerMeter * FMath::Max(0.f, DeltaSeconds);

	// A step can cross several edges; bound the walk so a corrupt graph (a cycle of warps, a
	// zero-length walk edge) cannot spin here forever. 64 edges is far past any authored route.
	int32 EdgeBudget = 64;

	while (State.IsWalking() && EdgeBudget-- > 0)
	{
		const FDFLaneEdge* Edge = Graph.Edges.IsValidIndex(State.EdgeIndex) ? &Graph.Edges[State.EdgeIndex] : nullptr;
		if (!Edge)
		{
			UE_LOG(LogDFWalk, Error, TEXT("walker on edge index %d, which the graph does not have"), State.EdgeIndex);
			State.EdgeIndex = INDEX_NONE;
			return;
		}

		// A warp is crossed the instant it is reached, whatever the speed (Step.cs): a zero-length
		// span is the one position this model cannot represent, so nobody may be parked on one.
		if (Edge->IsWarp())
		{
			const FVector Pad = Edge->Waypoints.Num() > 0 ? Edge->Waypoints.Last() : FVector::ZeroVector;
			Add(OutEvents, EDFWalkEventKind::Warped, Edge->Id, Pad);
			const FName ArrivedAt = Edge->To;
			State.Segment = 0;
			State.SegmentProgressCm = 0.f;
			const int32 Next = Graph.ChooseEdge(Itinerary, State.ViaCursor, ArrivedAt, EdgeOpen, DistToNode, DistToCore);
			if (Next == INDEX_NONE)
			{
				// Stranded on arrival: it keeps the pad as its position rather than the warp edge,
				// which has no length to stand on.
				if (!State.bStranded)
				{
					State.bStranded = true;
					Add(OutEvents, EDFWalkEventKind::Stranded, ArrivedAt, Pad);
				}
				return;
			}
			State.EdgeIndex = Next;
			Add(OutEvents, EDFWalkEventKind::EnteredEdge, Graph.Edges[Next].Id, Pad);
			continue;
		}

		if (RemainingCm <= 0.f)
		{
			return;
		}

		const TArray<float> Lengths = SegmentLengthsCm(*Edge);
		if (Lengths.Num() == 0)
		{
			UE_LOG(LogDFWalk, Error, TEXT("walk edge '%s' has fewer than two waypoints"), *Edge->Id.ToString());
			State.EdgeIndex = INDEX_NONE;
			return;
		}
		State.Segment = FMath::Clamp(State.Segment, 0, Lengths.Num() - 1);

		// Slope is the segment's own: a climb slows this stretch and not the whole edge.
		const float Factor = SlopeFactor(SegmentGrade(*Edge, State.Segment), Params);
		const float SegmentLeft = Lengths[State.Segment] - State.SegmentProgressCm;
		const float StepCm = RemainingCm * Factor;

		if (StepCm < SegmentLeft)
		{
			State.SegmentProgressCm += StepCm;
			State.TotalTraveledCm += StepCm;
			return;
		}

		// Finished this segment; the distance it cost is charged at this segment's factor.
		State.TotalTraveledCm += SegmentLeft;
		RemainingCm -= SegmentLeft / FMath::Max(Factor, KINDA_SMALL_NUMBER);
		++State.Segment;
		State.SegmentProgressCm = 0.f;

		if (State.Segment < Lengths.Num())
		{
			continue;   // still on this edge, next segment (possibly a different grade)
		}

		// At a node: the one place routing is decided.
		const FVector NodeLocation = Edge->Waypoints.Last();
		if (const FDFLaneNode* To = Graph.FindNode(Edge->To); To && To->Kind == EDFLaneNodeKind::Core)
		{
			State.EdgeIndex = INDEX_NONE;
			Add(OutEvents, EDFWalkEventKind::ReachedCore, Edge->To, NodeLocation);
			return;
		}

		const int32 Next = Graph.ChooseEdge(Itinerary, State.ViaCursor, Edge->To, EdgeOpen, DistToNode, DistToCore);
		if (Next == INDEX_NONE)
		{
			// Nowhere open to go. The no-sealing rule is meant to make this unreachable, so it stops
			// at the node and says so once rather than leaking or vanishing — a silent fallback here
			// is how a deadlock ships. It stays on the edge it just finished, so it still has a
			// position to be shot at.
			State.Segment = Lengths.Num() - 1;
			State.SegmentProgressCm = Lengths[State.Segment];
			if (!State.bStranded)
			{
				State.bStranded = true;
				Add(OutEvents, EDFWalkEventKind::Stranded, Edge->To, NodeLocation);
			}
			return;
		}

		if (State.bStranded)
		{
			State.bStranded = false;
			Add(OutEvents, EDFWalkEventKind::Unstranded, Edge->To, NodeLocation);
		}
		State.EdgeIndex = Next;
		State.Segment = 0;
		Add(OutEvents, EDFWalkEventKind::EnteredEdge, Graph.Edges[Next].Id, NodeLocation);
	}

	if (EdgeBudget <= 0)
	{
		UE_LOG(LogDFWalk, Error, TEXT("walker crossed 64 edges in one step on itinerary '%s'; stopping it rather than spinning"), *Itinerary.Id.ToString());
	}
}

FVector FDFLaneWalker::LocationOf(const UDFLaneGraphAsset& Graph, const FDFLaneWalkerState& State)
{
	if (!State.IsWalking() || !Graph.Edges.IsValidIndex(State.EdgeIndex))
	{
		return FVector::ZeroVector;
	}
	const FDFLaneEdge& Edge = Graph.Edges[State.EdgeIndex];
	if (!Edge.Waypoints.IsValidIndex(State.Segment + 1))
	{
		return Edge.Waypoints.Num() > 0 ? Edge.Waypoints.Last() : FVector::ZeroVector;
	}

	const FVector A = Edge.Waypoints[State.Segment];
	const FVector B = Edge.Waypoints[State.Segment + 1];
	const float Length = FVector::Dist(A, B);
	const FVector Spine = Length > KINDA_SMALL_NUMBER ? FMath::Lerp(A, B, FMath::Clamp(State.SegmentProgressCm / Length, 0.f, 1.f)) : A;
	return Spine + LateralAxis(B - A) * State.LateralOffsetCm;
}

FVector FDFLaneWalker::FacingOf(const UDFLaneGraphAsset& Graph, const FDFLaneWalkerState& State)
{
	if (!State.IsWalking() || !Graph.Edges.IsValidIndex(State.EdgeIndex))
	{
		return FVector::ForwardVector;
	}
	const FDFLaneEdge& Edge = Graph.Edges[State.EdgeIndex];
	if (!Edge.Waypoints.IsValidIndex(State.Segment + 1))
	{
		return FVector::ForwardVector;
	}
	const FVector Delta = Edge.Waypoints[State.Segment + 1] - Edge.Waypoints[State.Segment];
	return Delta.IsNearlyZero() ? FVector::ForwardVector : Delta.GetSafeNormal();
}

float FDFLaneWalker::RemainingToCoreMeters(const UDFLaneGraphAsset& Graph, const FDFLaneItinerary& Itinerary, const FDFLaneWalkerState& State, const TArray<bool>& EdgeOpen)
{
	if (!State.IsWalking())
	{
		return 0.f;   // leaked: on no edge, nothing left to walk (World.cs)
	}
	if (State.bStranded)
	{
		// Cut off, and only the walker knows it. Given an itinerary, RemainingToCore prices the
		// plan's remaining edges without asking whether they are open — deliberately, because "a
		// closed edge later on the plan is the tick's problem, not this metric's" (C6). For a
		// stranded walker the tick has already asked and been told there is nowhere to go, so the
		// honest answer is the contract's cut-off sentinel: it sorts LAST in a targeting order,
		// never first. Otherwise a wave held at a shut gate would read as the closest thing to the
		// core and pull every tower off the enemies actually walking in.
		return TNumericLimits<float>::Max();
	}
	const FDFLaneEdge& Edge = Graph.Edges[State.EdgeIndex];
	const TArray<float> Lengths = SegmentLengthsCm(Edge);

	// RemainingToCore takes T over the whole edge; the walker's position is per segment.
	float Walked = 0.f;
	float Total = 0.f;
	for (int32 I = 0; I < Lengths.Num(); ++I)
	{
		Total += Lengths[I];
		if (I < State.Segment)
		{
			Walked += Lengths[I];
		}
		else if (I == State.Segment)
		{
			Walked += State.SegmentProgressCm;
		}
	}
	const float T = Total > KINDA_SMALL_NUMBER ? FMath::Clamp(Walked / Total, 0.f, 1.f) : 0.f;
	return Graph.RemainingToCore(State.EdgeIndex, T, &Itinerary, &EdgeOpen);
}
