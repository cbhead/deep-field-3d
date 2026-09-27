#pragma once

#include "Combat/DFTargetable.h"
#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DFCueTestTarget.generated.h"

/** A body for a tower to hold a beam on in DF.Unit.Vfx tests (DFTowers' own test dummy is private to its
 *  module): every IDFTargetable answer is a field the test sets, and it aims 0.8 m up, as the sim does. */
UCLASS(NotBlueprintable, NotPlaceable, HideDropdown)
class ADFCueTestTarget : public AActor, public IDFTargetable
{
	GENERATED_BODY()

public:
	ADFCueTestTarget()
	{
		RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	}

	int32 Id = 0;
	bool bDead = false;
	float Remaining = 0.f;

	virtual int32 GetTargetId() const override { return Id; }
	virtual EDFEnemyLayer GetTargetLayer() const override { return EDFEnemyLayer::Ground; }
	virtual FVector GetTargetPosition() const override { return GetActorLocation(); }
	virtual FVector GetAimPoint() const override { return GetActorLocation() + FVector(0.f, 0.f, 80.f); }
	virtual bool IsTargetDead() const override { return bDead; }
	virtual bool IsTargetBurrowed() const override { return false; }
	virtual bool IsTargetStealthy() const override { return false; }
	virtual bool BlocksTowerSight() const override { return false; }
	virtual float GetRemainingToCore() const override { return Remaining; }
};
