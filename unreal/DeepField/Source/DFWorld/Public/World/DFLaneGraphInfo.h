#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "DFLaneGraphInfo.generated.h"

class UDFLaneGraphAsset;
class UInstancedStaticMeshComponent;
class UStaticMesh;

// The one actor in L_<Map>_Gameplay that says which lane graph this level is. A hard reference so
// the asset loads with the level; UDFWorldSubsystem finds this actor and hands the asset out.
//
// In a game it also draws the ground lanes: a faint flat strip along every ground walk edge, so the
// player can see the path from the portal to the core. Built at BeginPlay, on every machine that
// renders (each client draws its own; nothing replicates), never in the editor, so no saved level
// carries it; no collision (World/DFWorldLook.h).
UCLASS(NotPlaceable)
class DFWORLD_API ADFLaneGraphInfo : public AInfo
{
	GENERATED_BODY()

public:
	ADFLaneGraphInfo();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	FName MapId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	TObjectPtr<UDFLaneGraphAsset> LaneGraph;

	/** Draws the strip again from LaneGraph (BeginPlay does). Nothing outside a game world or on a
	 *  dedicated server. */
	void RebuildLaneStrip();

	/** The strip, once a game has drawn it; null in the editor. */
	UInstancedStaticMeshComponent* GetLaneStrip() const { return LaneStrip; }

protected:
	virtual void BeginPlay() override;

private:
	/** The engine cube the strip is made of (a property, so the reference is visible to the cook). */
	UPROPERTY()
	TObjectPtr<UStaticMesh> StripMesh;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> LaneStrip;
};
