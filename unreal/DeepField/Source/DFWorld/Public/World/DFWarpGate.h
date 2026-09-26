#pragma once

#include "CoreMinimal.h"
#include "World/DFWorldActor.h"
#include "DFWarpGate.generated.h"

// One end of a warp edge (a route's teleport leg): the departure pad an enemy steps onto and the
// arrival pad it is standing on the same tick. Keyed by the warp leg's END — (node id, edge id),
// spelled "<node>@<edge>" — not by the node alone: a pad can be the arrival of one warp and the
// departure of another, and one actor per node would fold those two roles into one actor. The
// arrival pad is a Spawn node, so the apron rule applies after it exactly as after a gate.
UCLASS()
class DFWORLD_API ADFWarpGate : public ADFWorldActor
{
	GENERATED_BODY()

public:
	/** The node id at this end of the warp. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	FName NodeId;

	/** The zero-length Warp edge this pad belongs to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	FName EdgeId;

	/** True at the far end (the arrival pad). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	bool bArrival = false;

	/** The stable id (and editor label) of the pad at InNodeId on InEdgeId: "<node>@<edge>". */
	static FName StableIdFor(FName InNodeId, FName InEdgeId);

	/** The inverse of StableIdFor; false unless the id has the "<node>@<edge>" shape. Node ids
	 *  carry no '@' (authored names or n<k>), edge ids may ("<from>-<to>@air"): split at the first. */
	static bool SplitStableId(FName StableId, FName& OutNodeId, FName& OutEdgeId);

	virtual FName GetStableId() const override { return StableIdFor(NodeId, EdgeId); }

protected:
	virtual void AssignStableId(FName NewId) override;
};
