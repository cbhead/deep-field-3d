#pragma once

#include "Combat/DFTargetable.h"
#include "Economy/DFEconomySeams.h"
#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DFTowerTestDummy.generated.h"

/** A stand-in for WS-05's ADFEnemy in DF.Unit.Tower tests: every IDFTargetable answer is a field the test sets. */
UCLASS(NotBlueprintable, NotPlaceable, HideDropdown)
class ADFTowerTestDummy : public AActor, public IDFTargetable
{
	GENERATED_BODY()

public:
	ADFTowerTestDummy()
	{
		RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	}

	int32 Id = 0;
	EDFEnemyLayer Layer = EDFEnemyLayer::Ground;
	bool bDead = false;
	bool bBurrowed = false;
	bool bStealth = false;
	bool bBlocksSight = false;
	float Remaining = 0.f;

	virtual int32 GetTargetId() const override { return Id; }
	virtual EDFEnemyLayer GetTargetLayer() const override { return Layer; }
	virtual FVector GetTargetPosition() const override { return GetActorLocation(); }
	virtual FVector GetAimPoint() const override { return GetActorLocation() + FVector(0.f, 0.f, 80.f); }
	virtual bool IsTargetDead() const override { return bDead; }
	virtual bool IsTargetBurrowed() const override { return bBurrowed; }
	virtual bool IsTargetStealthy() const override { return bStealth; }
	virtual bool BlocksTowerSight() const override { return bBlocksSight; }
	virtual float GetRemainingToCore() const override { return Remaining; }
};

/** A stand-in for WS-06's economy component in DF.Unit.Tower.Build* tests: a purse the test fills. */
UCLASS(NotBlueprintable, HideDropdown)
class UDFTowerTestWallet : public UObject, public IDFTeamWallet
{
	GENERATED_BODY()

public:
	int32 Money = 0;
	TMap<EDFScrapType, int32> Scrap;

	virtual int32 GetMoney() const override { return Money; }
	virtual const TMap<EDFScrapType, int32>& GetTeamScrap() const override { return Scrap; }
	virtual bool TrySpend(int32 InMoney, const FDFScrapBundle* Bundle) override
	{
		if (Money < InMoney)
		{
			return false;
		}
		if (Bundle)
		{
			for (const TPair<EDFScrapType, int32>& Part : Bundle->Amounts)
			{
				if (Scrap.FindRef(Part.Key) < Part.Value)
				{
					return false;
				}
			}
			for (const TPair<EDFScrapType, int32>& Part : Bundle->Amounts)
			{
				Scrap.FindOrAdd(Part.Key) -= Part.Value;
			}
		}
		Money -= InMoney;
		return true;
	}
	virtual void AddMoney(int32 InMoney) override { Money += InMoney; }
};
