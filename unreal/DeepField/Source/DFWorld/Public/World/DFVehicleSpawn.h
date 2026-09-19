#pragma once

#include "CoreMinimal.h"
#include "World/DFWorldActor.h"
#include "DFVehicleSpawn.generated.h"

// Where a vehicle is parked at match start (C6 VehicleSpawns[]). The actor's yaw is the vehicle's:
// a vehicle nosed at the wall it is parked against is one whose first press of W is a crash.
UCLASS()
class DFWORLD_API ADFVehicleSpawn : public ADFWorldActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	FName Id;

	/** The vehicles table row (buggy, dagator, grnmchn, vehickle). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	FName VehicleId;

	virtual FName GetStableId() const override { return Id; }

protected:
	virtual void AssignStableId(FName NewId) override { Id = NewId; }
};
