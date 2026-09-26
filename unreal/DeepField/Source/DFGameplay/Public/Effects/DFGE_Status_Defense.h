#pragma once

#include "Effects/DFGE_StatusBase.h"
#include "DFGE_Status_Defense.generated.h"

// C5 — the Defense channel's effect. Shred: adds the row's ArmorDelta (negative) to UDFHealthSet.FlatArmor; the set clamps at 0.
UCLASS()
class DFGAMEPLAY_API UDFGE_Status_Defense : public UDFGE_StatusBase
{
	GENERATED_BODY()

public:
	UDFGE_Status_Defense();
};
