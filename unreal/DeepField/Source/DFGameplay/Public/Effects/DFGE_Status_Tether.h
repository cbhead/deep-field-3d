#pragma once

#include "Effects/DFGE_StatusBase.h"
#include "DFGE_Status_Tether.generated.h"

// C5 — the Tether channel's effect. Magnetize: tags only — the pull itself is a physics event the tethering actor drives from the slot's PullSpeed.
UCLASS()
class DFGAMEPLAY_API UDFGE_Status_Tether : public UDFGE_StatusBase
{
	GENERATED_BODY()

public:
	UDFGE_Status_Tether();
};
