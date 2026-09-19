#pragma once

#include "Effects/DFGE_StatusBase.h"
#include "DFGE_Status_Movement.generated.h"

// C5 — the Movement channel's effect. Multiplies UDFMovementSet.SpeedFactor by the row's SpeedFactor (chill 0.65, tar, rubble 0.8); compound, so enrage and slope factors still stack on top.
UCLASS()
class DFGAMEPLAY_API UDFGE_Status_Movement : public UDFGE_StatusBase
{
	GENERATED_BODY()

public:
	UDFGE_Status_Movement();
};
