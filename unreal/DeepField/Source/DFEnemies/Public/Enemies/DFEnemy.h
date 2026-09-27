#pragma once

#include "AbilitySystemInterface.h"
#include "Combat/DFTargetable.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Waves/DFWavePlan.h"
#include "DFEnemy.generated.h"

class ADFWaveDirector;
class UAbilitySystemComponent;
class UDFAbilitySystemComponent;
class UDFEnemyMovement;
class UDFHealthSet;
class UDFMovementSet;
class UDFStatusComponent;
class UDFLaneGraphAsset;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
struct FDFLaneRouting;
struct FDFEnemyRow;
struct FDFWalkEvent;
struct FGameplayEffectSpec;

/**
 * An enemy on the lane: one released FDFSpawnEntry made into a body (WS-05). The host moves it with
 * UDFEnemyMovement and replicates the transform; it reports itself to the director that released it
 * exactly once, when it leaves play for any reason (a leak or a kill), so a wave clears when its last
 * body is gone.
 *
 * It is what towers shoot (IDFTargetable, joined to UDFTargetRegistry for as long as it is in play) and
 * what damage hurts: its own ability system (Minimal replication, C4: health reaches every machine, the
 * effects stay on the host) with UDFHealthSet at the row's Hp × the plan's HpFactor, its Shield and
 * FlatArmor, UDFMovementSet for the Movement channel, and UDFStatusComponent for statuses. At 0 health
 * it is killed at the end of the frame, not inside the hit: Step.cs flags death after a tower's Applies
 * loop, so the statuses of a lethal hit still land and IsTargetDead does not turn true mid-hit.
 *
 * The body is a placeholder until WS-34's enemy art: an engine cylinder the size of the sim's enemy,
 * tinted per enemy id, so a wave reads as a line of shapes walking the lane.
 */
UCLASS(NotBlueprintable)
class DFENEMIES_API ADFEnemy : public AActor, public IDFTargetable, public IAbilitySystemInterface
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
	UDFHealthSet* GetHealthSet() const { return HealthSet; }
	UDFStatusComponent* GetStatus() const { return Status; }

	/** Host: what killing it pays, fixed at spawn for the wave it spawned in (Step.cs ScaledBounty). */
	int32 GetBounty() const { return Bounty; }

	/** Health over max health, 0..1 (1 before the attributes are set). Every machine. */
	float GetHealthFraction() const;

	// ---- IAbilitySystemInterface ------------------------------------------------------------------
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ---- IDFTargetable (what a tower reads) -------------------------------------------------------
	virtual int32 GetTargetId() const override { return TargetId; }
	virtual EDFEnemyLayer GetTargetLayer() const override;
	virtual FVector GetTargetPosition() const override { return GetActorLocation(); }
	/** The sim aims 0.8 m above the position. */
	virtual FVector GetAimPoint() const override { return GetActorLocation() + FVector(0.f, 0.f, 80.f); }
	virtual bool IsTargetDead() const override { return bKilled; }
	virtual bool IsTargetBurrowed() const override;
	virtual bool IsTargetStealthy() const override;
	virtual bool BlocksTowerSight() const override;
	virtual float GetRemainingToCore() const override;
	virtual float GetKnockbackMass() const override;
	virtual void ApplyKnockback(float Meters) override;

	/** Host: fires once when this body walks into the core, just before it is removed, after
	 *  DF.Message.EnemyLeaked went to the team (WS-06's economy takes the lives on hearing it). */
	DECLARE_MULTICAST_DELEGATE_OneParam(FDFOnEnemyLeaked, ADFEnemy* /*Enemy*/);
	FDFOnEnemyLeaked OnLeaked;

	/** Host: fires once when this body's health ran out, the frame after the lethal hit landed, just
	 *  before it is removed and after DF.Message.EnemyKilled (with the bounty, which WS-06's economy
	 *  pays) went to the team. Killer is the hit's instigator (a tower, a hero), when there was one. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FDFOnEnemyKilled, ADFEnemy* /*Enemy*/, AActor* /*Killer*/);
	FDFOnEnemyKilled OnKilled;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void HandleWalkEvent(const FDFWalkEvent& Event);
	void ApplyPlaceholderLook();
	/** Host: the row's attributes on the ability system (Hp × HpFactor, Shield, FlatArmor, speed). */
	void InitAttributes(const FDFEnemyRow& Row);
	const FDFEnemyRow* FindRow() const;
	void HandleHealthDepleted(AActor* HitInstigator, AActor* Causer, const FGameplayEffectSpec* Spec, float Magnitude, float OldValue, float NewValue);
	/** Host, the frame after the lethal hit: OnKilled, then the body goes. */
	void Die();
	/** Every machine: the placeholder darkens toward red as health falls. */
	void RefreshHealthTint();

	UPROPERTY(VisibleAnywhere, Category = "DF|Enemy")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "DF|Enemy")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "DF|Enemy")
	TObjectPtr<UDFEnemyMovement> Movement;

	UPROPERTY(VisibleAnywhere, Category = "DF|Enemy")
	TObjectPtr<UDFAbilitySystemComponent> AbilitySystem;

	/** Attribute sets as subobjects: the ASC registers them in InitializeComponent. */
	UPROPERTY()
	TObjectPtr<UDFHealthSet> HealthSet;

	UPROPERTY()
	TObjectPtr<UDFMovementSet> MovementSet;

	UPROPERTY(VisibleAnywhere, Category = "DF|Enemy")
	TObjectPtr<UDFStatusComponent> Status;

	/** The id's colour is known (ApplyPlaceholderLook ran); health tints from it. */
	bool bTinted = false;

	FLinearColor BaseColour = FLinearColor::White;

	/** Replicated so a client can tint its copy of the body; the host is the only one that walks it. */
	UPROPERTY(ReplicatedUsing = OnRep_Entry)
	FDFSpawnEntry Entry;

	UFUNCTION()
	void OnRep_Entry() { ApplyPlaceholderLook(); }

	/** The C15 id messages and targeting carry: a host-wide sequence, from 1. */
	UPROPERTY(Replicated)
	int32 TargetId = 0;

	TWeakObjectPtr<ADFWaveDirector> Director;
	TWeakObjectPtr<AActor> Killer;
	int32 Bounty = 0;
	bool bReportedGone = false;
	/** Health ran out; Die runs next frame. IsTargetDead reads it. */
	bool bKilled = false;
	bool bRegistered = false;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
