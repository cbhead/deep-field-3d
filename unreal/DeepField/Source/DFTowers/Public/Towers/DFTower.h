#pragma once

#include "Combat/DFStructure.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Towers/DFTowerMath.h"
#include "DFTower.generated.h"

class UDFTargetingComponent;
class UDFTowerRigComponent;
struct FDFConditionRow;
struct FDFTowerRow;

DECLARE_MULTICAST_DELEGATE_TwoParams(FDFOnTowerShot, class ADFTower* /*Tower*/, AActor* /*Target*/);

/**
 * A built tower (WS-04). Replicated state is what every machine needs to draw it: the def, the
 * socket, path levels, hp, the current target (turret aim) and the weather it is fighting in (the
 * range ring). The weapon runs on the host only. StepTower is Step.cs FireTowers + StepTowerProjectiles
 * for this one tower, per kind:
 * - **Bolt / Mortar / Flak** keep the cooldown, pick a target, wait out night's acquisition delay
 *   on a new target, then fire a homing round (DFTowerMath::FDFTowerShot) that lands on the target
 *   (or splashes around it) or vanishes if the target dies first.
 * - **Tesla** strikes at once and chains to the nearest un-struck bodies, with damage falling off per hop.
 * - **Beam** deals damage every frame, ramping while it holds one target; switching costs the ramp.
 * - **Aura** applies its statuses to everything in range on its target layers, every frame.
 * - **Barricade / Support** have no weapon (a barricade works by existing; Support is WS-16's).
 *
 * Damage goes through WS-02's UDFGE_Damage on the target's ability system, with a UDFDamageContext
 * (DF.Damage.Source.Tower, the tower as instigator and source location), then the row's Applies
 * statuses through the target's UDFStatusComponent, in Step.cs Damage()'s order: damage first,
 * statuses after.
 *
 * A tower is also a structure (IDFStructure): a Ram's siege takes its hp, a hero's melee mends it,
 * and at 0 hp it is broken: its weapon stops at once and UDFBuildSubsystem removes it at the end of
 * the frame (DF.Message.TowerDestroyed), so every hit that frame still lands, as Step.cs removes only
 * after every enemy has swung. A row with StructureHp 0 is indestructible.
 *
 * The C9 rig (UDFTowerRigComponent) is cosmetic and runs on every machine that draws: it turns the
 * turret toward the replicated CurrentTarget and shows the stage modules for the replicated path levels.
 */
UCLASS()
class DFTOWERS_API ADFTower : public AActor, public IDFStructure
{
	GENERATED_BODY()

public:
	ADFTower();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	// ---- IDFStructure ------------------------------------------------------------------------------
	virtual int32 GetStructureId() const override { return StructureId; }
	virtual FVector GetStructurePosition() const override { return GetActorLocation(); }
	virtual float GetStructureHp() const override { return Hp; }
	virtual float GetStructureMaxHp() const override;
	virtual void ApplySiegeDamage(float Amount, int32 AttackerId) override;
	virtual void ApplyRepair(float Amount, int32 PlayerId) override;
	/** Destructible and at 0 hp: its weapon is silent and it is removed at the end of the frame. */
	bool IsBroken() const;

	/** Host: the next structure id. Towers and traps share the sequence (the sim's w.NextId). */
	static int32 AllocateStructureId();

	// ---- host setup ------------------------------------------------------------------------------
	/** Host: become DefId (a towers.json id) on SocketId, built by OwnerSeat for Spent money. False if the def is unknown. */
	bool InitializeTower(FName InDefId, FName InSocketId, int32 InOwnerSeat, int32 InSpent);
	/** Host: the condition scheduled for this wave (NAME_None for clear weather). Set at each wave start. */
	void SetActiveCondition(FName ConditionId);
	/** Host: Overdrive / Overclock. Factor multiplies the fire rate for Seconds (Step.cs BuffFactor/BuffTimer). */
	void ApplyBuff(float Factor, float Seconds);
	/** Host: one purchase on PathIndex, after DFTowerMath::QuoteUpgrade allowed it and the money and scrap were taken. */
	void ApplyUpgrade(int32 PathIndex, int32 MoneyCost);

	/** Host: one step of the weapon and the rounds in flight. Tick calls it; tests call it by hand. */
	void StepTower(float DeltaSeconds);

	// ---- read (every machine) ---------------------------------------------------------------------
	FName GetDefId() const { return DefId; }
	FName GetSocketId() const { return SocketId; }
	int32 GetOwnerSeat() const { return OwnerSeat; }
	const TArray<int32>& GetPathLevels() const { return PathLevels; }
	float GetHp() const { return Hp; }
	/** Beam towers: the ramp as 0..255 of the way from 1x to its cap (rig.md's replicated Heat), for the
	 *  beam's look on every machine. 0 for everything else. */
	uint8 GetHeat() const { return Heat; }
	AActor* GetCurrentTarget() const { return CurrentTarget; }
	/** The row, from the content tables (null before InitializeTower on the host / before replication on a client). */
	const FDFTowerRow* GetRow() const;
	const FDFConditionRow* GetActiveCondition() const;
	/** Reach this wave in metres: what the range ring draws and exactly what the weapon uses. */
	float GetRangeMeters() const;

	// ---- host only --------------------------------------------------------------------------------
	int32 GetSpent() const { return Spent; }
	/** Rounds in flight (tests). */
	int32 GetShotsInFlight() const { return Shots.Num(); }
	float GetDamageDealt() const { return DamageDealt; }
	int32 GetKills() const { return Kills; }

	/** Host: a weapon discharged (a round launched, a tesla strike, a beam acquiring a new target). */
	FDFOnTowerShot OnFired;
	/** Host: a round arrived (after its damage and statuses were applied). */
	FDFOnTowerShot OnShotLanded;

	UDFTargetingComponent* GetTargeting() const { return Targeting; }
	UDFTowerRigComponent* GetRig() const { return Rig; }

private:
	void StepWeapon(const FDFTowerRow& Row, float DeltaSeconds);
	void StepShots(const FDFTowerRow& Row, float DeltaSeconds);
	void FireRound(const FDFTowerRow& Row, AActor* Target);
	void FireTesla(const FDFTowerRow& Row, AActor* First);
	/** Step.cs Damage() as a tower hit: UDFGE_Damage on the body's ASC, then Applies. */
	void DealDamage(const FDFTowerRow& Row, AActor* Body, float Amount, const FGameplayTag& DamageType);
	void Announce(const FGameplayTag& Tag, AActor* Target, const FVector& Impact) const;
	void SetCurrentTarget(AActor* Target);
	/** A barricade's intact / damaged / broken, announced when it changes (DF.Message.BarricadeState). */
	void AnnounceBarricadeState();
	/** Clients: the replicated hp moved; re-broadcast it locally as DF.Message.StructureDamaged, so a
	 *  health bar on every machine hears it without a reliable RPC per siege tick. */
	UFUNCTION() void OnRep_Hp(float OldHp);
	UFUNCTION() void OnRep_DefId();
	UFUNCTION() void OnRep_PathLevels();
	/** Build the rig for DefId (its DA_Tower_<id> when one has been imported) and show the path levels. */
	void ConfigureRig();
	/** Every machine that draws: turn toward the replicated target, or idle. */
	void TickRig(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, Category = "DF|Tower") TObjectPtr<UDFTargetingComponent> Targeting;
	UPROPERTY(VisibleAnywhere, Category = "DF|Tower") TObjectPtr<UDFTowerRigComponent> Rig;

	UPROPERTY(ReplicatedUsing = OnRep_DefId) FName DefId;
	UPROPERTY(Replicated) FName SocketId;
	UPROPERTY(Replicated) int32 StructureId = 0;
	UPROPERTY(Replicated) int32 OwnerSeat = 0;
	UPROPERTY(ReplicatedUsing = OnRep_PathLevels) TArray<int32> PathLevels;
	UPROPERTY(ReplicatedUsing = OnRep_Hp) float Hp = 0.f;
	UPROPERTY(Replicated) FName ActiveConditionId;
	UPROPERTY(Replicated) uint8 Heat = 0;
	UPROPERTY(Replicated) TObjectPtr<AActor> CurrentTarget;

	// Host-only weapon state (Tower.cs fields).
	int32 Spent = 0;
	float Cooldown = 0.f;
	float BuffFactor = 1.f;
	float BuffTimer = 0.f;
	float RampSeconds = 0.f;
	TWeakObjectPtr<AActor> RampTarget;
	float DamageDealt = 0.f;
	int32 Kills = 0;
	FName LastBarricadeState;
	TWeakObjectPtr<AActor> LastRigTarget;

	struct FShotInFlight
	{
		DFTowerMath::FDFTowerShot Shot;
		TWeakObjectPtr<AActor> Target;
	};
	TArray<FShotInFlight> Shots;
};
