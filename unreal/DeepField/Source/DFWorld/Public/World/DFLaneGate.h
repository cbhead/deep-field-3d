#pragma once

#include "CoreMinimal.h"
#include "World/DFWorldActor.h"
#include "DFLaneGate.generated.h"

// C6 Gates[]{EdgeId, SocketId}: a barricade slot that closes a lane edge while a barricade stands
// on it. The gate IS its socket in every sense that matters — the id is the socket id, the actor
// stands on the socket — so a Ram has a wall to price and a lever cannot be confused with a door.
UCLASS()
class DFWORLD_API ADFLaneGate : public ADFWorldActor
{
	GENERATED_BODY()

public:
	/** The barricade socket that shuts this edge (the gate's stable id). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	FName SocketId;

	/** The lane edge this gate closes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	FName EdgeId;

	virtual FName GetStableId() const override { return SocketId; }

protected:
	virtual void AssignStableId(FName NewId) override { SocketId = NewId; }
};
