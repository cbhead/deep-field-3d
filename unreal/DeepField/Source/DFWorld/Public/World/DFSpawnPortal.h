#pragma once

#include "CoreMinimal.h"
#include "Content/DFContentRows.h"
#include "World/DFWorldActor.h"
#include "DFSpawnPortal.generated.h"

class UDFLaneGraphAsset;
class UStaticMeshComponent;

// Where a route begins: one per Spawn node that starts an itinerary (warp arrival pads are Spawn
// nodes too, but those are ADFWarpGates). The 8 m in front of it is spawn apron (rule 12): nothing
// built there, and no socket has to cover it.
//
// Until WS-33's art it wears a placeholder: a crimson gate (two posts and a lintel, 5.4 m wide and
// 4 m tall, the opening 4.2 m so the widest scatter walks through it) that faces the lane leaving
// this node, so the wave is seen to come out of something. Set dressing only (World/DFWorldLook.h).
UCLASS()
class DFWORLD_API ADFSpawnPortal : public ADFWorldActor
{
	GENERATED_BODY()

public:
	ADFSpawnPortal();

	/** The Spawn node id. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	EDFEnemyLayer Layer = EDFEnemyLayer::Ground;

	virtual FName GetStableId() const override { return Id; }

	/**
	 * Lays the placeholder gate out again (construction and BeginPlay do; call it after changing Id
	 * or Layer at run time). The opening faces the first walk edge out of this node in the level's
	 * lane graph, so the wave steps through it; with no graph, or no edge, it faces the actor's own
	 * forward. An air portal's opening is centred on the actor (the flyers leave at its height); a
	 * ground portal stands on it.
	 */
	void RefreshLook();

	/** The yaw (degrees) of the first walk edge leaving NodeId, if the graph has one with length. */
	static bool LaneYawOutOf(const UDFLaneGraphAsset& Graph, FName NodeId, float& OutYawDegrees);

	USceneComponent* GetLookRoot() const { return LookRoot; }

protected:
	virtual void AssignStableId(FName NewId) override { Id = NewId; }
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	/** Turns to face the lane at play time, so it and the parts under it are Movable. */
	UPROPERTY(VisibleAnywhere, Category = "DF|Look")
	TObjectPtr<USceneComponent> LookRoot;

	UPROPERTY(VisibleAnywhere, Category = "DF|Look")
	TObjectPtr<UStaticMeshComponent> PostLeft;

	UPROPERTY(VisibleAnywhere, Category = "DF|Look")
	TObjectPtr<UStaticMeshComponent> PostRight;

	UPROPERTY(VisibleAnywhere, Category = "DF|Look")
	TObjectPtr<UStaticMeshComponent> Lintel;

private:
	/** The level's lane graph: the ADFLaneGraphInfo in this actor's own level (so the editor can
	 *  lay out the gate too), else, in a game world, the world subsystem's. */
	const UDFLaneGraphAsset* FindLaneGraph() const;
};
