#pragma once

#include "CoreMinimal.h"
#include "Content/DFContentRows.h"
#include "World/DFWorldActor.h"
#include "DFSpawnPortal.generated.h"

// Where a route begins: one per Spawn node that starts an itinerary (warp arrival pads are Spawn
// nodes too, but those are ADFWarpGates). The 8 m in front of it is spawn apron (rule 12): nothing
// built there, and no socket has to cover it.
UCLASS()
class DFWORLD_API ADFSpawnPortal : public ADFWorldActor
{
	GENERATED_BODY()

public:
	/** The Spawn node id. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	EDFEnemyLayer Layer = EDFEnemyLayer::Ground;

	virtual FName GetStableId() const override { return Id; }

protected:
	virtual void AssignStableId(FName NewId) override { Id = NewId; }
};
