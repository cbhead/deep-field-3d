#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LaneGraph/DFLaneGraphTypes.h"
#include "DFLaneGraphAsset.generated.h"

// C6 — the map's routes as a graph (CONTRACTS/lanegraph.md). Written by the DFLevelImport
// commandlet from unreal/content/levels/<map>.level.json (DA_LaneGraph_<Map>), read by enemies,
// towers, the build system and the validator. The derivation is UDFLaneGraphBuilder, a port of
// sim/Sim.Core/Content/LaneGraph.cs; the runtime helpers below are the same file's routing
// functions, because the runtime and the validator must agree on what "sealed" means, and two
// implementations of one predicate is how they come to disagree.
UCLASS(BlueprintType)
class DFWORLD_API UDFLaneGraphAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lane Graph") FName MapId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lane Graph") TArray<FDFLaneNode> Nodes;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lane Graph") TArray<FDFLaneEdge> Edges;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lane Graph") TArray<FDFLaneItinerary> Itineraries;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lane Graph") TArray<FDFLaneGateDef> Gates;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lane Graph") TArray<FDFOperatedGateDef> OperatedGates;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lane Graph") TArray<FDFMutableDef> Mutables;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lane Graph") TArray<FDFBossRouteDef> BossRoutes;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World") TArray<FDFSocketDef> Sockets;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World") TArray<FDFStationDef> Stations;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World") TArray<FDFTraversalDef> Traversal;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World") TArray<FDFVehicleSpawnDef> VehicleSpawns;

	/** Pairs of distinct waypoints under 0.5 m apart: probably one place typed twice. Reported, never merged. */
	UPROPERTY(VisibleAnywhere, Category = "Lane Graph") TArray<FString> NearMisses;

	// --- lookup -----------------------------------------------------------------------------

	int32 NodeIndexOf(FName NodeId) const;
	int32 EdgeIndexOf(FName EdgeId) const;
	const FDFLaneNode* FindNode(FName NodeId) const;
	const FDFLaneEdge* FindEdge(FName EdgeId) const;
	const FDFLaneItinerary* FindItinerary(FName ItineraryId) const;

	/** The edge from one via to the next on an itinerary's layer, or INDEX_NONE. */
	int32 EdgeBetween(FName From, FName To, EDFEnemyLayer Layer) const;

	/** The edges an itinerary walks, in order (Via[i] -> Via[i+1]). */
	TArray<int32> EdgesOf(const FDFLaneItinerary& Itinerary) const;

	/** Rebuild the index maps after editing the arrays (the importer and the tests do; PostLoad does too). */
	void RebuildIndex();

	// --- routing (LaneGraph.cs, ported) ---------------------------------------------------------

	/** Metres from every node to the nearest core over open edges (INFINITY when unreachable).
	 *  EdgeOpen: one bool per edge; null = everything open. Relaxed to a fixed point, no heap, so
	 *  two machines cannot order equal distances differently. */
	TArray<float> DistanceToCore(const TArray<bool>* EdgeOpen = nullptr) const;

	/** The same relaxation seeded at one node: what "head for the next via" costs from everywhere. */
	TArray<float> DistanceToNode(FName NodeId, const TArray<bool>* EdgeOpen = nullptr) const;

	/** The rule the mutable layer rests on: every spawn (all of them, not the ones a wave uses) reaches a core. */
	bool EverySpawnReachesCore(const TArray<bool>* EdgeOpen = nullptr) const;

	/** Whether closing exactly these edges (on top of nothing else) would seal a spawn from the core. */
	bool WouldSeal(const TSet<FName>& ClosedEdgeIds) const;
	bool WouldSeal(const TArray<int32>& ClosedEdgeIndices) const;

	/** Every edge something can close: lane gates, operated gates, mutables with an edge. Distinct, in that order. */
	TArray<FName> ClosableEdges() const;

	/** Metres still to walk from a point T (0..1) along an edge to the core.
	 *  Along the itinerary when one is given and it uses the edge (the walker's own plan, so
	 *  targeting compares like with like across routes and warps); otherwise the shortest open way.
	 *  Warps contribute nothing. INFINITY if the core is unreachable. */
	float RemainingToCore(int32 EdgeIndex, float T, const FDFLaneItinerary* Itinerary = nullptr, const TArray<bool>* EdgeOpen = nullptr) const;

	/** The routing rule (LaneGraph.ChooseEdge): the cheapest open edge out of AtNode towards the
	 *  next via the walker can still reach, falling through to the core. ViaCursor advances past
	 *  vias reached or cut off. Returns an edge index or INDEX_NONE. */
	int32 ChooseEdge(const FDFLaneItinerary& Itinerary, int32& ViaCursor, FName AtNode, const TArray<bool>& EdgeOpen,
		const TArray<TArray<float>>& DistToNode, const TArray<float>& DistToCore) const;

	/** The edges an itinerary's walkers take end to end in a configuration (author-time only). */
	TArray<int32> PathFor(const FDFLaneItinerary& Itinerary, const TArray<bool>& EdgeOpen) const;

	/** Walk an itinerary back into the polyline it came from: the derivation is lossless exactly
	 *  when this returns the route's original waypoints. */
	TArray<FVector> Rebuild(FName ItineraryId) const;

	/** Metres of walked length of one edge (0 for a warp), recomputed from the waypoints. */
	static float WalkedLengthMeters(const FDFLaneEdge& Edge);

	virtual void PostLoad() override;

private:
	TArray<float> DistanceTo(TFunctionRef<bool(const FDFLaneNode&)> IsTarget, const TArray<bool>* EdgeOpen) const;

	TMap<FName, int32> NodeIndex;
	TMap<FName, int32> EdgeIndex;
	bool bIndexBuilt = false;
};
