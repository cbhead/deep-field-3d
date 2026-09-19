#pragma once

#include "CoreMinimal.h"
#include "World/DFWorldActor.h"
#include "DFWarpGate.generated.h"

// One end of a warp edge (a route's teleport leg): the departure pad an enemy steps onto and the
// arrival pad it is standing on the same tick. Keyed by the node id at that end; the arrival pad is
// a Spawn node, so the apron rule applies after it exactly as after a gate.
UCLASS()
class DFWORLD_API ADFWarpGate : public ADFWorldActor
{
	GENERATED_BODY()

public:
	/** The node id at this end of the warp. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	FName Id;

	/** The zero-length Warp edge this pad belongs to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	FName EdgeId;

	/** True at the far end (the arrival pad). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	bool bArrival = false;

	virtual FName GetStableId() const override { return Id; }

protected:
	virtual void AssignStableId(FName NewId) override { Id = NewId; }
};
