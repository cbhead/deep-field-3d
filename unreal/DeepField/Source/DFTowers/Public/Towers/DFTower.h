#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Towers/DFTowerMath.h"
#include "DFTower.generated.h"

class UDFTargetingComponent;
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
 * Still to come (the WS-04 file lists them): the build subsystem that places, upgrades and sells
 * through WS-28's player controller; structure damage from sieging enemies; the C9 rig driving the mesh.
 */
UCLASS()
class DFTOWERS_API ADFTower : public AActor
{
	GENERATED_BODY()

public:
	ADFTower();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

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
	int32 GetStructureId() const { return StructureId; }
	int32 GetOwnerSeat() const { return OwnerSeat; }
	const TArray<int32>& GetPathLevels() const { return PathLevels; }
	float GetHp() const { return Hp; }
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

private:
	void StepWeapon(const FDFTowerRow& Row, float DeltaSeconds);
	void StepShots(const FDFTowerRow& Row, float DeltaSeconds);
	void FireRound(const FDFTowerRow& Row, AActor* Target);
	void FireTesla(const FDFTowerRow& Row, AActor* First);
	/** Step.cs Damage() as a tower hit: UDFGE_Damage on the body's ASC, then Applies. */
	void DealDamage(const FDFTowerRow& Row, AActor* Body, float Amount, const FGameplayTag& DamageType);
	void Announce(const FGameplayTag& Tag, AActor* Target, const FVector& Impact) const;
	void SetCurrentTarget(AActor* Target);

	UPROPERTY(VisibleAnywhere, Category = "DF|Tower") TObjectPtr<UDFTargetingComponent> Targeting;

	UPROPERTY(Replicated) FName DefId;
	UPROPERTY(Replicated) FName SocketId;
	UPROPERTY(Replicated) int32 StructureId = 0;
	UPROPERTY(Replicated) int32 OwnerSeat = 0;
	UPROPERTY(Replicated) TArray<int32> PathLevels;
	UPROPERTY(Replicated) float Hp = 0.f;
	UPROPERTY(Replicated) FName ActiveConditionId;
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

	struct FShotInFlight
	{
		DFTowerMath::FDFTowerShot Shot;
		TWeakObjectPtr<AActor> Target;
	};
	TArray<FShotInFlight> Shots;
};
