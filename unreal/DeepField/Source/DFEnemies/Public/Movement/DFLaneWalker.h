#pragma once

#include "CoreMinimal.h"
#include "LaneGraph/DFLaneGraphAsset.h"
#include "DFLaneWalker.generated.h"

// Walking an itinerary, ported from sim/Sim.Core/Step.cs MoveEnemies (the written spec, ADR-0005)
// with ADR-0018's terrain rules on top. Pure state over a lane graph: no actor, no world, no
// navmesh — so it is testable without either, and ADFEnemy/UDFEnemyMovement own only the body.
//
// The sim's model, kept exactly: position is (edge, segment, metres into that segment) rather than
// a parameter along the edge, because segments are the unit the waypoints give and a lerp inside
// one is the only place a position is interpolated. A warp is crossed the instant it is reached
// whatever the speed — a frozen or sieging enemy on a departure pad still goes, because a
// zero-length edge is the one position this model cannot represent. Routing happens at a node and
// nowhere else. An enemy with nowhere open to go is *stranded*, not leaked and not deleted: it
// stays where it stopped, still targetable, and says so once.

class UDFLaneGraphAsset;

/** What a step did, in the order it happened. The caller turns these into messages and cues. */
UENUM()
enum class EDFWalkEventKind : uint8
{
	EnteredEdge,   // routing chose a new edge (Detail = its id)
	Warped,        // crossed a warp edge; the body should be moved, not interpolated
	Stranded,      // nowhere open to go from this node (Detail = the node id); fires once
	Unstranded,    // an edge opened and it is moving again
	BreachStarted, // a siege enemy took a blocked edge (Detail = its id): the player gets the
	               // enemy, the wall and a countdown, long before the lane opens somewhere they
	               // were not looking (Step.cs AnnounceBreach)
	ReachedCore,   // the caller applies the leak damage and removes the body
};

USTRUCT()
struct DFENEMIES_API FDFWalkEvent
{
	GENERATED_BODY()

	UPROPERTY() EDFWalkEventKind Kind = EDFWalkEventKind::EnteredEdge;
	UPROPERTY() FName Detail;                       // edge or node id, by kind
	UPROPERTY() FVector Location = FVector::ZeroVector;   // cm, where it happened
};

/** The knobs a step reads, from the enemy row and the balance dials. Owned by the caller so the
 *  walker stays pure and a test can state them outright. */
USTRUCT()
struct DFENEMIES_API FDFLaneWalkerParams
{
	GENERATED_BODY()

	/** Metres per second as it walks *now*: the row's speed times status, enrage and stealth, which
	 *  the caller has already resolved (they are GAS's, not the walker's). */
	UPROPERTY() float SpeedMetersPerSec = 1.f;

	/** The row's own speed, untouched by any modifier. Siege pricing uses this and not the effective
	 *  speed: `Step.cs:1222` prices the wall with `def.SpeedMetersPerSec`, and every modifier in
	 *  MoveEnemies writes a local rather than the def. Otherwise a chilled or enraged Ram values the
	 *  same wall differently from one that is neither, and the breach-versus-detour decision flips
	 *  at walls the sim would not flip at. Left at 0 it falls back to SpeedMetersPerSec. */
	UPROPERTY() float RowSpeedMetersPerSec = 0.f;

	/** ADR-0018 / B§1.5: climbing costs, descending pays. Applied per waypoint segment from that
	 *  segment's own grade, never from the edge's steepest sample. */
	UPROPERTY() float SlopeSpeedUp = 0.85f;
	UPROPERTY() float SlopeSpeedDown = 1.1f;

	/** The grade either factor starts at, as a fraction (0.15 = 15 %). Applied symmetrically —
	 *  uphill above +15 %, downhill below -15 % — so micro-undulation is walked flat and the rule
	 *  stays a lever for a climb or a drop that reads as one (PROGRAMME.md §3.2, B§1.5). */
	UPROPERTY() float SlopeGradeThreshold = 0.15f;

	/** > 0 means this enemy sieges (FDFEnemyRow::StructureDps), and **routing changes shape**: it is
	 *  priced through shut edges rather than stopped by them, so a Ram walks at a barricade when
	 *  breaking it is cheaper than walking round. A sieging walker is therefore never stranded at a
	 *  wall — which matters more than it looks, because a stranded walker reports cut off and sorts
	 *  LAST, so getting this wrong makes every tower in range ignore the enemy breaking the
	 *  player's wall. Step.cs NextEdge. */
	UPROPERTY() float StructureDps = 0.f;

	/** Balance.SiegeBreachBias: below 1 it makes breaching look cheaper than it is, so a wall in
	 *  front of a short detour is worth going round and one in front of a long detour is not. */
	UPROPERTY() float SiegeBreachBias = 0.6f;

	// Air lanes need nothing here: the importer already places an air edge's waypoints at the
	// authored height above ground (FDFLaneEdge::AglMeters), so a flyer following the spine is
	// holding AGL by construction. A flyer that must re-acquire AGL after being displaced is
	// UDFEnemyMovement's problem, not the walker's.
};

/** Where a walker is. Small and plain: this is what replicates (10-15 Hz) and what a save writes. */
USTRUCT()
struct DFENEMIES_API FDFLaneWalkerState
{
	GENERATED_BODY()

	UPROPERTY() int32 EdgeIndex = INDEX_NONE;   // INDEX_NONE once the core is reached
	UPROPERTY() int32 Segment = 0;              // which waypoint pair
	UPROPERTY() float SegmentProgressCm = 0.f;  // distance into that pair
	UPROPERTY() int32 ViaCursor = 0;            // how far along the itinerary's vias routing has got
	UPROPERTY() float TotalTraveledCm = 0.f;    // for the burrow cycle, which is distance-based
	UPROPERTY() float LateralOffsetCm = 0.f;    // the plan's scatter, held across the whole walk
	UPROPERTY() bool bStranded = false;

	bool IsWalking() const { return EdgeIndex != INDEX_NONE; }
};

/** The routing tables a step needs. Rebuilt when an edge changes state, not per enemy per frame.
 *  A sieging walker needs both sets: it is costed against tables that see through gates (or the far
 *  side of a shut edge reads as unreachable and the wall in front of it is invisible), while whether
 *  a via is still worth heading for is asked of the map as it actually is. */
struct DFENEMIES_API FDFLaneRouting
{
	TArray<bool> EdgeOpen;
	TArray<TArray<float>> DistToNode;       // the map as it is
	TArray<float> DistToCore;
	TArray<TArray<float>> DistToNodeOpen;   // as if every edge were open; siege only
	TArray<float> DistToCoreOpen;

	/** Structure hp standing in a closed edge (a barricade, a wall), for siege pricing. Unbound is
	 *  treated as "nothing there", which prices a shut edge as free to breach — so bind it whenever
	 *  anything can be sieged. */
	TFunction<float(int32)> BlockingHpOf;

	/** Fill every table from a graph and an open-state array. */
	void Rebuild(const UDFLaneGraphAsset& Graph);

	bool IsOpen(int32 EdgeIndex) const { return EdgeOpen.IsValidIndex(EdgeIndex) && EdgeOpen[EdgeIndex]; }
};

/** What arriving at a node did. */
enum class EDFArrival : uint8
{
	Moving,     // routed onto a new edge
	Stranded,   // nowhere open to go from here
	Leaked,     // the node was a core
};

struct DFENEMIES_API FDFLaneWalker
{
	/** Arrive at a node and route from it — the core/strand/enter decision, in **one** place.
	 *  Both the warp path and the end-of-edge path call it: they used to be two copies and the copies
	 *  drifted, which is how the warp path ended up never clearing `bStranded`. Where a stranded
	 *  walker *stands* differs between the two, so that stays the caller's to apply. */
	static EDFArrival ArriveAtNode(const UDFLaneGraphAsset& Graph, const FDFLaneItinerary& Itinerary, const FDFLaneWalkerParams& Params,
		const FDFLaneRouting& Routing, FName AtNode, const FVector& ArrivalLocation, FDFLaneWalkerState& State, TArray<FDFWalkEvent>& OutEvents);

	/** Put a walker at the start of an itinerary, **routing its first edge the same way every later
	 *  one is chosen** (Step.cs:1011). Taking `EdgesOf(Itinerary)[0]` unconditionally would start a
	 *  walker on a shut edge and walk it through — the spawn node is a routing decision like any
	 *  other. False if nothing is open out of the first via. */
	static bool Begin(const UDFLaneGraphAsset& Graph, const FDFLaneItinerary& Itinerary, const FDFLaneWalkerParams& Params,
		const FDFLaneRouting& Routing, float LateralOffsetCm, FDFLaneWalkerState& Out);

	/** One step. Speed 0 (frozen, sieging) still crosses a warp it is standing on, as the sim does.
	 *  EdgeOpen/DistToNode/DistToCore are the caller's routing tables, rebuilt when an edge changes
	 *  state — not per enemy per frame. Events append in the order they happened. */
	static void Advance(const UDFLaneGraphAsset& Graph, const FDFLaneItinerary& Itinerary, const FDFLaneWalkerParams& Params,
		float DeltaSeconds, const FDFLaneRouting& Routing, FDFLaneWalkerState& State, TArray<FDFWalkEvent>& OutEvents);

	/** The edge routing would take out of AtNode, sieging or not. Exposed because it is the whole
	 *  difference between a Ram that walks at your wall and one that stands at it sorting last. */
	static int32 ChooseNextEdge(const UDFLaneGraphAsset& Graph, const FDFLaneItinerary& Itinerary, const FDFLaneWalkerParams& Params,
		FName AtNode, const FDFLaneRouting& Routing, int32& ViaCursor);

	/** Where the body is: the spine at the current segment, pushed sideways by the scatter. */
	static FVector LocationOf(const UDFLaneGraphAsset& Graph, const FDFLaneWalkerState& State);

	/** Unit vector along the current segment, or +X if there is none. */
	static FVector FacingOf(const UDFLaneGraphAsset& Graph, const FDFLaneWalkerState& State);

	/** Metres still to walk to the core, for targeting (lowest wins). The contract's conventions:
	 *  a cut-off enemy sorts last, a leaked one has nothing left. A stranded walker reports cut off
	 *  even though the graph's own metric cannot see it — see the note in the .cpp. */
	static float RemainingToCoreMeters(const UDFLaneGraphAsset& Graph, const FDFLaneItinerary& Itinerary, const FDFLaneWalkerState& State, const TArray<bool>& EdgeOpen);

	/** The signed grade of one waypoint segment as a fraction (+ uphill), 0 where it is vertical or degenerate. */
	static float SegmentGrade(const FDFLaneEdge& Edge, int32 Segment);

	/** The speed factor that grade earns under these params. */
	static float SlopeFactor(float Grade, const FDFLaneWalkerParams& Params);

	/** Lengths of each waypoint pair of an edge, in cm. */
	static TArray<float> SegmentLengthsCm(const FDFLaneEdge& Edge);
};
