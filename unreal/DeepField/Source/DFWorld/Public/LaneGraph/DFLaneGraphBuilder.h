#pragma once

#include "CoreMinimal.h"
#include "LaneGraph/DFLevelFile.h"
#include "UObject/Object.h"
#include "DFLaneGraphBuilder.generated.h"

class UDFLaneGraphAsset;

// The derivation: routes (polylines) -> nodes, edges, itineraries, exactly as
// sim/Sim.Core/Content/LaneGraph.cs FromRoutes does it. A waypoint becomes a node when it is a
// route's first or last point, either end of a warp leg, or the routes through it disagree about
// where they came from and go; parallel spans between the same pair get a mid-point promoted so an
// itinerary can tell them apart; identical spans become one edge. Coalescing is EXACT equality —
// a derivation must never move a lane, so near-duplicates are reported (NearMisses) for a human.
UCLASS()
class DFWORLD_API UDFLaneGraphBuilder : public UObject
{
	GENERATED_BODY()

public:
	/** Nodes, Edges, Itineraries and NearMisses from the routes; everything else on the asset is left alone. */
	static bool Derive(const TArray<FDFLevelRoute>& Routes, const TArray<FDFLevelNodeName>& NodeNames, UDFLaneGraphAsset& Out, FString& OutError);

	/** Derive, then carry the rest of the level file over: gates, levers, sockets, stations,
	 *  vehicles, the armory as a traversal entry, ClosableBy on the gated edges. The one code path
	 *  the importer and the tests share. */
	static bool BuildFromLevel(const FDFLevelFile& Level, UDFLaneGraphAsset& Out, FString& OutError);

	/** Half a metre: under this and not equal, two waypoints are probably one place. */
	static constexpr float NearMissMeters = 0.5f;
};
