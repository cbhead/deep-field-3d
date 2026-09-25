#pragma once

#include "Content/DFContentRows.h"
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/Interface.h"
#include "DFTargetable.generated.h"

// What a tower may shoot, as towers see it (contract-append, WS-04, 2026-09-25). DFTowers and
// DFEnemies are sibling modules (layer 3 of modules.json) and cannot name each other's classes, so
// an enemy answers the targeting rules through this interface, declared in the layer both can see,
// and joins UDFTargetRegistry so a tower never iterates every actor in the world. WS-05's ADFEnemy
// implements it and registers in BeginPlay; anything else a tower may shoot (a test dummy) does the same.
//
// Detection (stealth revealed) and mark are NOT here: they are status channels, read from the
// target's UDFStatusComponent (DFGameplay), one source of truth for every reader.

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UDFTargetable : public UInterface
{
	GENERATED_BODY()
};

class DFCORE_API IDFTargetable
{
	GENERATED_BODY()

public:
	/** Stable id for this body within the match (messages and target bookkeeping use it). */
	virtual int32 GetTargetId() const = 0;
	/** Ground or Air (the enemy row's layer). */
	virtual EDFEnemyLayer GetTargetLayer() const = 0;
	/** The body's position on the lane (cm), what range is measured to (the sim's enemy.Pos). */
	virtual FVector GetTargetPosition() const = 0;
	/** Where shots aim and sight lines end (cm). The sim aims 0.8 m above the position. */
	virtual FVector GetAimPoint() const = 0;
	virtual bool IsTargetDead() const = 0;
	/** Underground (a Mole's cycle): untargetable. */
	virtual bool IsTargetBurrowed() const = 0;
	/** The row is stealthy; towers need the Detection channel active to see it. */
	virtual bool IsTargetStealthy() const = 0;
	/** A sight-blocking body (the Monolith): it shields whatever walks behind it from towers. */
	virtual bool BlocksTowerSight() const = 0;
	/** UDFLaneGraphAsset::RemainingToCore along its itinerary: an OPAQUE sort key, compare only
	 *  (TNumericLimits<float>::Max() when stranded; small when sieging). INT 2026-09-21, ws-04. */
	virtual float GetRemainingToCore() const = 0;
};

/**
 * Every live targetable body in this world. Bodies register in BeginPlay and unregister in EndPlay;
 * a tower's targeting component walks this list instead of every actor. Order is registration order,
 * which is spawn order: with equal RemainingToCore the earlier-spawned body wins, as the sim's
 * `w.Enemies` order decides ties.
 */
UCLASS()
class DFCORE_API UDFTargetRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDFTargetRegistry* Get(const UObject* WorldContext);

	/** Actor must implement IDFTargetable; a second registration is ignored. */
	void Register(AActor* Actor);
	void Unregister(AActor* Actor);

	/** Live registered bodies, in registration order (stale entries are skipped and pruned). */
	TArray<AActor*> GetTargets();

	int32 Num() const { return Targets.Num(); }

private:
	TArray<TWeakObjectPtr<AActor>> Targets;
};
