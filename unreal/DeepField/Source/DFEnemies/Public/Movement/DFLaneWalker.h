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

	/** Metres per second before any factor: the row's speed times status, enrage and stealth
	 *  factors, which the caller has already resolved (they are GAS's, not the walker's). */
	UPROPERTY() float SpeedMetersPerSec = 1.f;

	/** ADR-0018 / B§1.5: climbing costs, descending pays. Applied per waypoint segment from that
	 *  segment's own grade, never from the edge's steepest sample. */
	UPROPERTY() float SlopeSpeedUp = 0.85f;
	UPROPERTY() float SlopeSpeedDown = 1.1f;

	/** The grade either factor starts at, as a fraction (0.15 = 15 %). Applied symmetrically: a
	 *  segment gentler than this in either direction is walked at the flat speed. */
	UPROPERTY() float SlopeGradeThreshold = 0.15f;

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

struct DFENEMIES_API FDFLaneWalker
{
	/** Put a walker at the start of an itinerary. False if the itinerary has no first edge. */
	static bool Begin(const UDFLaneGraphAsset& Graph, const FDFLaneItinerary& Itinerary, float LateralOffsetCm, FDFLaneWalkerState& Out);

	/** One step. Speed 0 (frozen, sieging) still crosses a warp it is standing on, as the sim does.
	 *  EdgeOpen/DistToNode/DistToCore are the caller's routing tables, rebuilt when an edge changes
	 *  state — not per enemy per frame. Events append in the order they happened. */
	static void Advance(const UDFLaneGraphAsset& Graph, const FDFLaneItinerary& Itinerary, const FDFLaneWalkerParams& Params,
		float DeltaSeconds, const TArray<bool>& EdgeOpen, const TArray<TArray<float>>& DistToNode, const TArray<float>& DistToCore,
		FDFLaneWalkerState& State, TArray<FDFWalkEvent>& OutEvents);

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
