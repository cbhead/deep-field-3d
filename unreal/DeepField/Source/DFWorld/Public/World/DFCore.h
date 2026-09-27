#pragma once

#include "CoreMinimal.h"
#include "World/DFWorldActor.h"
#include "DFCore.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

// The core: where every ground route ends and what a leak damages. One per Core node.
//
// Until WS-34's art it wears a placeholder: a cyan pillar on a dark plinth with a bright cap and a
// cyan light at the top (PROGRAMME C§1's "core = cyan pool"), 5.85 m tall so it reads from across
// the map. Set dressing only (World/DFWorldLook.h): a leak walks into it and is gone.
UCLASS()
class DFWORLD_API ADFCore : public ADFWorldActor
{
	GENERATED_BODY()

public:
	ADFCore();

	/** The Core node id in the lane graph (usually "core"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	FName Id;

	virtual FName GetStableId() const override { return Id; }

	/** Tints the placeholder again (construction and BeginPlay do). */
	void RefreshLook();

protected:
	virtual void AssignStableId(FName NewId) override { Id = NewId; }
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "DF|Look")
	TObjectPtr<UStaticMeshComponent> Plinth;

	UPROPERTY(VisibleAnywhere, Category = "DF|Look")
	TObjectPtr<UStaticMeshComponent> Shaft;

	UPROPERTY(VisibleAnywhere, Category = "DF|Look")
	TObjectPtr<UStaticMeshComponent> Cap;

	UPROPERTY(VisibleAnywhere, Category = "DF|Look")
	TObjectPtr<UPointLightComponent> Glow;
};
