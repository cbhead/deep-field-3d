#pragma once

#include "CoreMinimal.h"
#include "World/DFWorldActor.h"
#include "DFCore.generated.h"

// The core: where every ground route ends and what a leak damages. One per Core node.
UCLASS()
class DFWORLD_API ADFCore : public ADFWorldActor
{
	GENERATED_BODY()

public:
	/** The Core node id in the lane graph (usually "core"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	FName Id;

	virtual FName GetStableId() const override { return Id; }

protected:
	virtual void AssignStableId(FName NewId) override { Id = NewId; }
};
