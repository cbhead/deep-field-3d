#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DFArmorProfile.generated.h"

// The target-side half of the damage execution's inputs (C4): the enemy row's directional
// armor numbers and the facing to test them against. An actor that has a front arc (ADFEnemy,
// the boss) implements this so an applier only has to say where the shot came from; the
// execution asks the target (IDFArmorProfileSource) for the rest. Absent the interface — and absent explicit values on
// the UDFDamageContext — the hit has no aspect and takes no arc factor.

USTRUCT(BlueprintType)
struct DFGAMEPLAY_API FDFArmorProfile
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FrontArmorArcDegrees = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FrontArmorFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RearWeakFactor = 1.f;
	/** World-space facing the arc is measured from (unit length). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Forward = FVector::ForwardVector;
};

UINTERFACE(MinimalAPI, BlueprintType)
class UDFArmorProfileSource : public UInterface
{
	GENERATED_BODY()
};

/** Implemented by any actor whose hits have an aspect (ADFEnemy, the boss). */
class DFGAMEPLAY_API IDFArmorProfileSource
{
	GENERATED_BODY()

public:
	virtual FDFArmorProfile GetArmorProfile() const = 0;
};
