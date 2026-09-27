#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Towers/DFTowerMath.h"
#include "DFTargetingComponent.generated.h"

/**
 * A tower's eyes (WS-04). On the host it turns the world into DFTowerMath::FDFTargetCandidate rows
 * and asks DFTowerMath::PickTarget what the sim would shoot. The rules live in DFTowerMath and the
 * facts live here:
 * - bodies come from UDFTargetRegistry (DFCore);
 * - layer, position, burrowed, stealth and RemainingToCore come from IDFTargetable (WS-05's enemy);
 * - detection and mark come from the body's UDFStatusComponent (WS-02);
 * - sight is a DF_Sight trace from the muzzle to the aim point (terrain and registered blockers,
 *   §3.2), plus DFTowerMath::IsSightBlockedByBody for every sight-blocking body (the Monolith).
 *   Traces only happen for candidates that could win.
 *
 * It also keeps the one piece of per-tower targeting memory the sim has: the last target, for the
 * night acquisition delay ("a beat before a tower settles on something new").
 */
UCLASS(ClassGroup = (DF), meta = (BlueprintSpawnableComponent))
class DFTOWERS_API UDFTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDFTargetingComponent();

	/** Height above the owner's origin that sight traces start from (the validator's 1.6 m "muzzle height"). */
	UPROPERTY(EditAnywhere, Category = "DF|Targeting") float SightHeightCm = 160.f;

	/** Every registered body as a candidate, in registration (spawn) order. OutActors is parallel to the result. */
	TArray<DFTowerMath::FDFTargetCandidate> GatherCandidates(TArray<AActor*>& OutActors) const;

	/**
	 * What this tower shoots now: the sim's PickTarget over the registry. Null when nothing qualifies.
	 * RangeMeters is DFTowerMath::RangeMeters for this tower this wave (the caller owns path levels and weather).
	 */
	AActor* PickTarget(const FDFTowerRow& Row, float RangeMeters);

	/** The body's Detection channel is active (a stealthy body is then fair game for towers). */
	static bool IsDetected(const AActor* Body);
	/** The body's Vulnerability channel is active (marked: exempt from night's acquisition delay). */
	static bool IsMarked(const AActor* Body);

	/**
	 * Step.cs FireTowers' acquisition bookkeeping: when Target is not the last target, remember it and
	 * return the delay to wait before firing (0 when there is none); for the same target, 0. A null
	 * target clears the memory (the sim's `tower.LastTargetId = -1`).
	 */
	float NoteTarget(AActor* Target, const FDFConditionRow* Condition);

	/** Sight from this tower's muzzle to the body's aim point: terrain, registered blockers and sight-blocking bodies. */
	bool IsSightBlocked(AActor* Body, TConstArrayView<AActor*> Bodies) const;

private:
	FVector MuzzleLocation() const;

	int32 LastTargetId = INDEX_NONE;
};
