#pragma once

#include "CoreMinimal.h"
#include "World/DFWorldActor.h"
#include "DFHeroStation.generated.h"

// A hero station: the auto-hero traversal graph's anchor points, and the places rule 7 measures
// reachability from. One per fightable tier.
UCLASS()
class DFWORLD_API ADFHeroStation : public ADFWorldActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	FName Id;

	virtual FName GetStableId() const override { return Id; }

protected:
	virtual void AssignStableId(FName NewId) override { Id = NewId; }
};
