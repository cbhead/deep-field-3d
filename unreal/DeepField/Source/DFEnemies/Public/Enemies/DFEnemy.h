#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Waves/DFWavePlan.h"
#include "DFEnemy.generated.h"

class ADFWaveDirector;
class UDFEnemyMovement;
class UDFLaneGraphAsset;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
struct FDFLaneRouting;
struct FDFWalkEvent;

/**
 * An enemy on the lane: one released FDFSpawnEntry made into a body (WS-05). The host moves it with
 * UDFEnemyMovement and replicates the transform; it reports itself to the director that released it
 * exactly once, when it leaves play for any reason (a leak now; a kill once towers can kill), so a
 * wave clears when its last body is gone.
 *
 * The body is a placeholder until WS-34's enemy art: an engine cylinder the size of the sim's enemy,
 * tinted per enemy id, so a wave reads as a line of shapes walking the lane.
 */
UCLASS(NotBlueprintable)
class DFENEMIES_API ADFEnemy : public AActor
{
	GENERATED_BODY()

public:
	ADFEnemy();

	/** Spawn the body for a released entry and put it on its lane, at the lane's start. Null, with the
	 *  reason, if the content has no such enemy or the lane cannot start it (the director is then told
	 *  at once, so the wave still clears). */
	static ADFEnemy* SpawnFromEntry(UWorld* World, const FDFSpawnEntry& Entry, const UDFLaneGraphAsset* Graph,
		TSharedPtr<const FDFLaneRouting> Routing, ADFWaveDirector* Director, FString& OutError);

	const FDFSpawnEntry& GetEntry() const { return Entry; }
	UDFEnemyMovement* GetMovement() const { return Movement; }

	/** Host: fires once when this body walks into the core, just before it is removed. The match turns
	 *  it into leak damage (lives are WS-06's; until they exist, only the log sees it). */
	DECLARE_MULTICAST_DELEGATE_OneParam(FDFOnEnemyLeaked, ADFEnemy* /*Enemy*/);
	FDFOnEnemyLeaked OnLeaked;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void HandleWalkEvent(const FDFWalkEvent& Event);
	void ApplyPlaceholderLook();

	UPROPERTY(VisibleAnywhere, Category = "DF|Enemy")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "DF|Enemy")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "DF|Enemy")
	TObjectPtr<UDFEnemyMovement> Movement;

	/** Replicated so a client can tint its copy of the body; the host is the only one that walks it. */
	UPROPERTY(ReplicatedUsing = OnRep_Entry)
	FDFSpawnEntry Entry;

	UFUNCTION()
	void OnRep_Entry() { ApplyPlaceholderLook(); }

	TWeakObjectPtr<ADFWaveDirector> Director;
	bool bReportedGone = false;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
