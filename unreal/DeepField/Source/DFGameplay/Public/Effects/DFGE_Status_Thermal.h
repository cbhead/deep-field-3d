#pragma once

#include "Effects/DFGE_StatusBase.h"
#include "DFGE_Status_Thermal.generated.h"

// C5 — the Thermal channel's effect. Burn: a periodic UDFDamageExecution tick (DF.SetByCaller.Damage = dps / tickHz, period 1 / tickHz); the shield gate is the status component's, before this ever applies.
UCLASS()
class DFGAMEPLAY_API UDFGE_Status_Thermal : public UDFGE_StatusBase
{
	GENERATED_BODY()

public:
	UDFGE_Status_Thermal();
};
