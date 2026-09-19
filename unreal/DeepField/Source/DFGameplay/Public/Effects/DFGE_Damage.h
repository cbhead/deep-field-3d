#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DFGE_Damage.generated.h"

// The instant damage effect every DF hit goes through: one execution (UDFDamageExecution),
// base damage as DF.SetByCaller.Damage, everything else on the UDFDamageContext attached to
// the effect context. Weapons, towers, traps, contact damage and reaction bursts all apply
// this class; a Blueprint child may only change cues.
UCLASS()
class DFGAMEPLAY_API UDFGE_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UDFGE_Damage();
};
