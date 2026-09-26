#pragma once

#include "Effects/DFGE_StatusBase.h"
#include "DFGE_Status_Detection.generated.h"

// C5 — the Detection channel's effect. Reveal: tags only — a revealed Shade is targetable because DF.Status.Channel.Detection is on it.
UCLASS()
class DFGAMEPLAY_API UDFGE_Status_Detection : public UDFGE_StatusBase
{
	GENERATED_BODY()

public:
	UDFGE_Status_Detection();
};
