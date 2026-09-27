#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/Interface.h"
#include "DFStructure.generated.h"

// What an enemy can break and a hero can mend (contract-append, WS-04, 2026-09-25). Towers and
// barricades are DFTowers'; the Ram that sieges them is DFEnemies' and the melee that repairs them is
// DFPlayer's, three sibling modules. So the structure answers through this interface, declared in
// the layer all three can see, and joins UDFStructureRegistry. The registry also holds the two rules
// the callers share, ported once: Step.cs SiegeStructures' target pick and ApplyPlayerMelee's repair
// pick. WS-05 calls Siege from a sieging enemy's step; WS-03 calls RepairNearby from a melee swing.

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UDFStructure : public UInterface
{
	GENERATED_BODY()
};

class DFCORE_API IDFStructure
{
	GENERATED_BODY()

public:
	/** The structure id messages carry (ADFTower::GetStructureId). */
	virtual int32 GetStructureId() const = 0;
	/** Where it stands (cm): what siege reach and repair reach are measured to (the sim's tower.Pos). */
	virtual FVector GetStructurePosition() const = 0;
	virtual float GetStructureHp() const = 0;
	/** The row's StructureHp. 0 = indestructible: never sieged, never repaired (the sim's rule). */
	virtual float GetStructureMaxHp() const = 0;
	/** Host: a siege hit (Step.cs `target.Hp -= StructureDps * Dt`). At 0 hp the structure is broken;
	 *  its owner removes it at the end of the frame, so every hit this frame still lands, as in the sim. */
	virtual void ApplySiegeDamage(float Amount, int32 AttackerId) = 0;
	/** Host: a melee repair (Step.cs ApplyPlayerMelee), capped at the max. */
	virtual void ApplyRepair(float Amount, int32 PlayerId) = 0;
};

/**
 * Every structure in this world, in build order (structures register in BeginPlay, unregister in
 * EndPlay). Host-side rules only; clients read a structure's replicated hp.
 */
UCLASS()
class DFCORE_API UDFStructureRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDFStructureRegistry* Get(const UObject* WorldContext);

	/** Actor must implement IDFStructure; a second registration is ignored. */
	void Register(AActor* Actor);
	void Unregister(AActor* Actor);
	/** Live registered structures, in build order. */
	TArray<AActor*> GetStructures();

	/**
	 * Step.cs SiegeStructures' pick for one enemy at PositionCm: the nearest destructible structure
	 * within ReachMeters (d <= reach; a strictly nearer one wins, so ties keep the earlier-built).
	 * A structure broken earlier this frame is still in the list until its owner removes it, as the
	 * sim removes only after every enemy has swung.
	 */
	AActor* FindSiegeTarget(const FVector& PositionCm, float ReachMeters);

	/**
	 * One sieging enemy's step: find the target and hit it for Dps x DeltaSeconds. Returns true when
	 * the enemy is sieging (it stops to swing: the sim's `enemy.Sieging`, which its movement reads),
	 * false when nothing is in reach. Dps <= 0 never sieges.
	 */
	bool Siege(const FVector& PositionCm, float ReachMeters, float Dps, float DeltaSeconds, int32 AttackerId);

	/** ApplyPlayerMelee's repair pick: the first structure in build order that is destructible, hurt,
	 *  not broken, and within ReachMeters of PositionCm. */
	AActor* FindRepairTarget(const FVector& PositionCm, float ReachMeters);

	/** A melee swing's repair: mend the pick by Amount. True when it did (the swing is spent on the
	 *  repair and deals no damage: "a swing does one thing"). */
	bool RepairNearby(const FVector& PositionCm, float ReachMeters, float Amount, int32 PlayerId);

	int32 Num() const { return Structures.Num(); }

private:
	TArray<TWeakObjectPtr<AActor>> Structures;
};
