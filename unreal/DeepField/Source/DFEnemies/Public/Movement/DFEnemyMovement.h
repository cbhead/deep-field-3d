#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Movement/DFLaneWalker.h"
#include "DFEnemyMovement.generated.h"

class UDFLaneGraphAsset;
class UDFStatusComponent;
struct FDFEnemyRow;

// The component that turns walker state into a body. Host-only: it ticks on authority and nowhere
// else, and it does not replicate — `ADFEnemy` owns the replicated properties (§3.3 puts movement
// at 10–15 Hz with dormancy, and air enemies as {EdgeIndex, T}), because what a client needs is a
// position to interpolate, not a walker to re-run.
//
// The rules that decide how fast a body moves stay in a pure function: `Step.cs MoveEnemies` opens
// with six of them, in an order that matters (a frozen Ram is still frozen however enraged it is),
// and an order is the kind of thing that should be provable without spawning an actor.

/** Everything `ResolveSpeed` reads, gathered by the caller so the rule itself needs no world. */
struct DFENEMIES_API FDFEnemySpeedInputs
{
	/** The row's own metres per second, before anything modifies it. */
	float RowSpeedMetersPerSec = 1.f;

	/** A Shade nobody has revealed moves faster — ignoring it costs you, which is what stops stealth
	 *  from being a pure tempo loss. `bRevealed` is the Detection channel being active. */
	bool bStealth = false;
	bool bRevealed = false;
	float StealthSpeedBonus = 0.f;

	/** What the Movement channel wrote to `UDFMovementSet::SpeedFactor` (C4). 1 = unmodified. */
	float StatusSpeedFactor = 1.f;

	/** Enrage: a Ram that is losing hurries. Read from the row so the dial is content. */
	float HpFraction = 1.f;
	float EnrageBelowHpFraction = 0.f;
	float EnrageSpeedFactor = 1.f;

	/** Busy demolishing something: not advancing. */
	bool bSieging = false;

	/** Hard control (shock, freeze) — a dead stop, and it beats enrage. */
	bool bHardControlled = false;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FDFOnWalkEvent, const FDFWalkEvent& /*Event*/);

UCLASS(ClassGroup = (DF), NotBlueprintable, meta = (BlueprintSpawnableComponent))
class DFENEMIES_API UDFEnemyMovement : public UActorComponent
{
	GENERATED_BODY()

public:
	UDFEnemyMovement();

	/** `Step.cs MoveEnemies`' opening, as a pure rule. The order is the sim's and each step of it is
	 *  load-bearing: stealth, then the Movement channel, then enrage — **then** the two that zero it,
	 *  so a frozen Ram is still frozen however badly it is losing. Never returns less than zero. */
	static float ResolveSpeed(const FDFEnemySpeedInputs& In);

	/** The burrow cycle is distance-based, not time-based, so it is the same cycle at any frame rate
	 *  and survives a save: under for the first `BurrowedMeters` of every `CycleMeters`. */
	static bool IsBurrowedAt(float TotalTraveledMeters, float CycleMeters, float BurrowedMeters);

	/** Put this body on a lane. False (with the reason) if the itinerary is unknown or nothing is
	 *  open out of its first node — the caller should not spawn a body that cannot start.
	 *  `LateralOffsetCm` is the plan's scatter for this body, in centimetres. It takes the number
	 *  rather than the whole `FDFSpawnEntry` on purpose: the entry's elite, boss and hp fields belong
	 *  to `ADFEnemy`, and a parameter list is the cheapest place to say what a class actually reads. */
	bool Configure(const UDFLaneGraphAsset* InGraph, FName ItineraryId, float LateralOffsetCm,
		const FDFEnemyRow& Row, TSharedPtr<const FDFLaneRouting> InRouting, FString& OutError);

	/** The routing tables, shared by every walker on the map and rebuilt when an edge changes state
	 *  — not per enemy per frame. */
	void SetRouting(TSharedPtr<const FDFLaneRouting> InRouting) { Routing = MoveTemp(InRouting); }

	/** Displace along the lane (B§1.5): a Maul, a launcher, a Nova crater. Returns metres given up. */
	float KnockBack(float Meters);

	/** Sieging stops the body without being a status — it is not control and fills no cc-resist. */
	void SetSieging(bool bInSieging) { bSieging = bInSieging; }
	bool IsSieging() const { return bSieging; }

	/** Untargetable while under (a Mole). Derived from distance, so it is never out of step with position. */
	bool IsBurrowed() const { return bBurrowed; }

	/** Metres still to walk, for targeting. Cut off sorts last; leaked has nothing left. */
	float RemainingToCoreMeters() const;

	const FDFLaneWalkerState& GetWalkerState() const { return Walker; }
	const FDFLaneWalkerParams& GetWalkerParams() const { return Params; }
	bool IsOnLane() const { return Walker.IsWalking(); }

	/** Every walker event, relayed in order: the caller turns them into messages, cues and leak damage. */
	FDFOnWalkEvent OnWalkEvent;

	virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	/** Gather what ResolveSpeed needs from the row, the attribute set and the status component. */
	FDFEnemySpeedInputs GatherSpeedInputs() const;

	UPROPERTY(Transient) TObjectPtr<const UDFLaneGraphAsset> Graph;
	UPROPERTY(Transient) TObjectPtr<UDFStatusComponent> Status;

	TSharedPtr<const FDFLaneRouting> Routing;
	FDFLaneWalkerState Walker;
	FDFLaneWalkerParams Params;
	FName ItineraryId;
	const FDFLaneItinerary* Itinerary = nullptr;

	float BurrowCycleMeters = 0.f;
	float BurrowedMeters = 0.f;
	bool bBurrower = false;
	bool bBurrowed = false;
	bool bSieging = false;
	bool bStealth = false;
	float StealthSpeedBonus = 0.f;
	float RowSpeedMetersPerSec = 1.f;
	float EnrageBelowHpFraction = 0.f;
	float EnrageSpeedFactor = 1.f;
};
