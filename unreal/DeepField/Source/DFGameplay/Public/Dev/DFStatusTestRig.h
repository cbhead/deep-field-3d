#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Messages/DFMessageBus.h"
#include "DFStatusTestRig.generated.h"

class UDFAbilitySystemComponent;
class UDFHealthSet;
class UDFMovementSet;
class UDFStatusComponent;
class UDFTintComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
struct FDFMsg_GameplayCue;
struct FDFMsg_Status;

// L_Test_Status (PROGRAMME.md §5.2 WS-02 DoD: "L_Test_Status shows chill+burn -> thermalShock 12%
// via cue"; the level is /Game/DF/Dev/L_Test_Status, built by Source/DFGameplay/Dev/make-l-test-status.py).
// A stand-in enemy — DF ASC, health + movement sets, status and tint components, a sphere and a
// floating label — that chills itself ChillAtSeconds into every cycle and burns itself at
// BurnAtSeconds: the status component detonates thermalShock for 12 % of MaxHealth on UDFHealthSet
// and executes GameplayCue.DF.Reaction.ThermalShock, which the native UDFGameplayCueNotify_Reaction
// handles (debug sphere + on-screen line in the palette's Danger swatch) and forwards to
// UDFMessageBus as FDFMsg_GameplayCue. The rig hears the cue on the bus and the component's
// DF.Message.ReactionTriggered, writes what they carried into the Result properties, logs the DoD
// line, updates the label, then starts the next cycle. DF.Func.Status.ThermalShockInLevel opens the
// map in PIE and reads the Result properties. Rows: the content tables when UDFContentSubsystem is
// ready, else the Statuses.cs literals.
UCLASS(HideCategories = (Replication, Collision, Input, HLOD, Physics, Networking, LevelInstance, Cooking))
class DFGAMEPLAY_API ADFStatusTestRig : public AActor
{
	GENERATED_BODY()

public:
	ADFStatusTestRig();

	UPROPERTY(EditAnywhere, Category = "DF|Rig") float MaxHealth = 100.f;
	UPROPERTY(EditAnywhere, Category = "DF|Rig") float FlatArmor = 0.f;
	UPROPERTY(EditAnywhere, Category = "DF|Rig") float ChillAtSeconds = 1.f;
	UPROPERTY(EditAnywhere, Category = "DF|Rig") float BurnAtSeconds = 2.f;
	UPROPERTY(EditAnywhere, Category = "DF|Rig") float CycleSeconds = 6.f;

	// ---- what the last completed cycle showed ----
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "DF|Rig|Result") int32 CompletedCycles = 0;
	/** DF.Reaction row id the component reported (thermalShock). */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "DF|Rig|Result") FName LastReactionId;
	/** UDFHealthSet.Health right after the burst (88 of 100). */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "DF|Rig|Result") float LastHealthAfterBurst = 0.f;
	/** True when the reaction cue reached the bus (the notify ran on this machine) during the cycle. */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "DF|Rig|Result") bool bLastCueSeen = false;
	/** The cue as fired (GameplayCue.DF.Reaction.ThermalShock). */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "DF|Rig|Result") FGameplayTag LastCueTag;
	/** The tag the handling notify registered for: GameplayCue.DF.Reaction for the native fallback, the leaf once WS-14 ships GC_DF_Reaction_ThermalShock. */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "DF|Rig|Result") FGameplayTag LastCueHandlerTag;
	/** RawMagnitude the cue carried: the burst damage (12). */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "DF|Rig|Result") float LastCueBurst = 0.f;
	/** NormalizedMagnitude the cue carried: the row's BurstFraction (0.12). */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "DF|Rig|Result") float LastCueFraction = 0.f;
	/** True when DF.Message.ReactionTriggered reached the bus during the cycle. */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "DF|Rig|Result") bool bLastMessageSeen = false;
	/** The reaction the message named (DF.Reaction.ThermalShock). */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "DF|Rig|Result") FGameplayTag LastMessageReaction;
	/** The label text. */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "DF|Rig|Result") FString StateText;

	UDFStatusComponent* GetStatus() const { return Status; }
	UDFHealthSet* GetHealthSet() const { return HealthSet; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void StartCycle();
	void ApplyChill();
	void ApplyBurn();
	void OnReactionCue(const FGameplayTag& CueTag, const FDFMsg_GameplayCue& Msg);
	void OnReactionMessage(const FGameplayTag& MessageTag, const FDFMsg_Status& Msg);
	void SetState(const FString& Text);

	UPROPERTY(VisibleAnywhere, Category = "DF|Rig") TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, Category = "DF|Rig") TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(VisibleAnywhere, Category = "DF|Rig") TObjectPtr<UDFAbilitySystemComponent> AbilitySystem;
	UPROPERTY() TObjectPtr<UDFHealthSet> HealthSet;
	UPROPERTY() TObjectPtr<UDFMovementSet> MovementSet;
	UPROPERTY(VisibleAnywhere, Category = "DF|Rig") TObjectPtr<UDFStatusComponent> Status;
	UPROPERTY(VisibleAnywhere, Category = "DF|Rig") TObjectPtr<UDFTintComponent> Tint;

	FTimerHandle ChillTimer;
	FTimerHandle BurnTimer;
	FTimerHandle CycleTimer;
	FDFMessageHandle CueHandle;
	FDFMessageHandle MessageHandle;
};
