#pragma once

#include "Effects/DFGE_StatusBase.h"
#include "DFGE_Status_Control.generated.h"

// C5 — the Control channel's effect. Shock / freeze / stagger: tags only — DF.Status.Channel.Control is what the movement code reads to hard-stop; cc-resist is the component's gauge.
UCLASS()
class DFGAMEPLAY_API UDFGE_Status_Control : public UDFGE_StatusBase
{
	GENERATED_BODY()

public:
	UDFGE_Status_Control();
};
