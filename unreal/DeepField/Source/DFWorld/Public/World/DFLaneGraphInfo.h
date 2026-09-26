#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "DFLaneGraphInfo.generated.h"

class UDFLaneGraphAsset;

// The one actor in L_<Map>_Gameplay that says which lane graph this level is. A hard reference so
// the asset loads with the level; UDFWorldSubsystem finds this actor and hands the asset out.
UCLASS(NotPlaceable)
class DFWORLD_API ADFLaneGraphInfo : public AInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	FName MapId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	TObjectPtr<UDFLaneGraphAsset> LaneGraph;
};
