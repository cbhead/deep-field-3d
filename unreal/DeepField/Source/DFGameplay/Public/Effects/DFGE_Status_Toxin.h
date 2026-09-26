#pragma once

#include "Effects/DFGE_StatusBase.h"
#include "DFGE_Status_Toxin.generated.h"

// C5 — the Toxin channel's effect. Poison: the same periodic tick as Thermal; the UDFDamageContext the component attaches carries IgnoresArmor / IgnoresShield from the row.
UCLASS()
class DFGAMEPLAY_API UDFGE_Status_Toxin : public UDFGE_StatusBase
{
	GENERATED_BODY()

public:
	UDFGE_Status_Toxin();
};
