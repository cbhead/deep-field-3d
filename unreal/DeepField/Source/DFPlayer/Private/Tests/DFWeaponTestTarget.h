#pragma once

#include "Abilities/DFAbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Attributes/DFHealthSet.h"
#include "Combat/DFTargetable.h"
#include "Components/BoxComponent.h"
#include "CoreMinimal.h"
#include "DFWorldCollision.h"
#include "GameFramework/Actor.h"
#include "DFWeaponTestTarget.generated.h"

/**
 * A stand-in for WS-05's ADFEnemy in DF.Unit.Weapon tests (DFPlayer and DFEnemies are siblings and
 * cannot see each other): what a hero's shot needs from an enemy, and nothing else. A box that blocks
 * DF_Weapon only, an ability system with UDFHealthSet, and IDFTargetable answers the test sets.
 */
UCLASS(NotBlueprintable, NotPlaceable, HideDropdown)
class ADFWeaponTestTarget : public AActor, public IDFTargetable, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ADFWeaponTestTarget()
	{
		Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
		Box->InitBoxExtent(FVector(40.f, 40.f, 80.f));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionResponseToAllChannels(ECR_Ignore);
		Box->SetCollisionResponseToChannel(DFCollision::Weapon, ECR_Block);
		SetRootComponent(Box);
		AbilitySystem = CreateDefaultSubobject<UDFAbilitySystemComponent>(TEXT("AbilitySystem"));
		HealthSet = CreateDefaultSubobject<UDFHealthSet>(TEXT("HealthSet"));
	}

	UPROPERTY()
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY()
	TObjectPtr<UDFAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UDFHealthSet> HealthSet;

	bool bDead = false;
	bool bBurrowed = false;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }

	virtual int32 GetTargetId() const override { return 1; }
	virtual EDFEnemyLayer GetTargetLayer() const override { return EDFEnemyLayer::Ground; }
	virtual FVector GetTargetPosition() const override { return GetActorLocation(); }
	virtual FVector GetAimPoint() const override { return GetActorLocation(); }
	virtual bool IsTargetDead() const override { return bDead; }
	virtual bool IsTargetBurrowed() const override { return bBurrowed; }
	virtual bool IsTargetStealthy() const override { return false; }
	virtual bool BlocksTowerSight() const override { return false; }
	virtual float GetRemainingToCore() const override { return 0.f; }

protected:
	virtual void BeginPlay() override
	{
		Super::BeginPlay();
		AbilitySystem->InitAbilityActorInfo(this, this);
	}
};
