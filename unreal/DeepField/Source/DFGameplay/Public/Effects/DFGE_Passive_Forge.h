#pragma once

#include "Effects/DFGE_FactionPassive.h"
#include "DFGE_Passive_Forge.generated.h"

// Forge's passive: grants DF.Ability.Passive.BuildDiscount; Magnitude() is forgeBuildDiscount (0.9), the factor WS-06 multiplies a build cost by when the buyer carries the tag (Step.cs:375). No attribute: cost is the economy's.
UCLASS()
class DFGAMEPLAY_API UDFGE_Passive_Forge : public UDFGE_FactionPassive
{
	GENERATED_BODY()

public:
	UDFGE_Passive_Forge();
};
