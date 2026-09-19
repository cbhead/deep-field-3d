#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "Content/DFContentRows.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Status/DFStatusResolver.h"
#include "Status/DFStatusTypes.h"
#include "DFStatusComponent.generated.h"

class UAbilitySystemComponent;
class UDFTintComponent;
struct FDFEnemyRow;

/** Server: a slot was written (Applied / emitted by a reaction). */
DECLARE_MULTICAST_DELEGATE_OneParam(FDFStatusSlotEvent, const FDFStatusSlot& /*Slot*/);
/** Server: a reaction fired (Burst = the damage applied, 0 when the row has no burst). */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FDFReactionEvent, FName /*ReactionId*/, float /*Burst*/, const FDFStatusApplyOutcome& /*Outcome*/);
/** Client and server: the replicated slots changed. */
DECLARE_MULTICAST_DELEGATE(FDFSlotsChanged);

// C5 — the world half of the status system. Wraps FDFStatusResolver (the semantics) and, on the
// server, mirrors every decision into GAS and the rest of the game:
//   * the channel's C++ effect (UDFGE_Status_<Channel>) with the slot's magnitude as
//     DF.SetByCaller.Magnitude and DF.Status.<Id> as a dynamic granted tag — Movement ->
//     SpeedFactor, Thermal/Toxin -> a periodic damage tick, Defense -> FlatArmor, Vulnerability
//     -> DamageTakenFactor, Control/Tether/Detection -> tags;
//   * reaction bursts through UDFGE_Damage (BurstFraction x MaxHealth, source Reaction);
//   * cues GameplayCue.DF.Status.<Id>.Applied/Removed and GameplayCue.DF.Reaction.<Id>;
//   * messages DF.Message.StatusApplied / StatusExpired / ReactionTriggered (FDFMsg_Status);
//   * the sibling UDFTintComponent (on the server directly, on clients from OnRep_Slots).
// Slots replicate as a compact 8-entry array {StatusTag, EndTimeServer, Magnitude}; the
// resolver itself is server-only.
//
// Rows come from UDFContentSubsystem (Status / Reaction / Ids("reactions")); AddStatusRowOverride
// and AddReactionRowOverride let a test (or a dev map) supply rows without the tables.
UCLASS(ClassGroup = (DF), meta = (BlueprintSpawnableComponent))
class DFGAMEPLAY_API UDFStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDFStatusComponent();

	// ---- C5 API ----

	/** Apply DF.Status.<Id>; MagnitudeOverride < 0 uses the row (Swift elite halves Movement magnitudes through it). Server only. */
	UFUNCTION(BlueprintCallable, Category = "DF|Status")
	EDFStatusApplyResult Apply(FGameplayTag StatusTag, AActor* Source, float MagnitudeOverride = -1.f);

	/** Same by content id; DurationFactor multiplies the row duration (Ember's burn passive x1.3, conditions). */
	UFUNCTION(BlueprintCallable, Category = "DF|Status")
	EDFStatusApplyResult ApplyById(FName StatusId, AActor* Source, float MagnitudeOverride = -1.f, float DurationFactor = 1.f);

	/** Every detail of the last Apply (reaction id, burst fraction, emitted status, ...). */
	const FDFStatusApplyOutcome& GetLastOutcome() const { return LastOutcome; }

	UFUNCTION(BlueprintCallable, Category = "DF|Status") void ClearChannel(EDFStatusChannel Channel);
	UFUNCTION(BlueprintCallable, Category = "DF|Status") void ClearAll();

	UFUNCTION(BlueprintPure, Category = "DF|Status") bool IsChannelActive(EDFStatusChannel Channel) const;
	UFUNCTION(BlueprintPure, Category = "DF|Status") FGameplayTag ActiveStatus(EDFStatusChannel Channel) const;
	UFUNCTION(BlueprintPure, Category = "DF|Status") float ActiveMagnitude(EDFStatusChannel Channel) const;
	UFUNCTION(BlueprintPure, Category = "DF|Status") float TimeRemaining(EDFStatusChannel Channel) const;
	UFUNCTION(BlueprintPure, Category = "DF|Status") bool IsControlled() const;
	UFUNCTION(BlueprintPure, Category = "DF|Status") float GetCcResist() const { return Resolver.CcResist; }

	/** The replicated slots (valid on every machine). */
	const TArray<FDFStatusSlotRep>& GetSlots() const { return Slots; }
	/** The server-side resolver (empty on clients). */
	const FDFStatusResolver& GetResolver() const { return Resolver; }

	// ---- target facts the gates read ----

	/** Enemy row Mass; Tether is refused at >= the resolver's TetherImmuneMass (6). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "DF|Status")
	float Mass = 1.f;

	/** Owner tags that make Tether fail (default: DF.Enemy.State.Phased, DF.Enemy.BossFrame01). Checked on the owner's ASC. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Status")
	FGameplayTagContainer ImmunityTags;

	/** Explicit immunity (skater, boss) independent of tags. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Status")
	bool bTetherImmune = false;

	/** C15 target id carried in FDFMsg_Status (the owner assigns it; 0 = unknown). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Status")
	int32 TargetId = 0;

	/** Conditions multiply channel durations (C5); the condition subsystem writes this. Missing = 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Status")
	TMap<EDFStatusChannel, float> ChannelDurationFactors;

	/** Mass and the boss immunity from the enemy row. */
	void InitFromEnemyRow(const FDFEnemyRow& Row);

	// ---- row fixtures (tests, dev maps) ----
	void AddStatusRowOverride(FName StatusId, const FDFStatusRow& Row);
	void AddReactionRowOverride(FName ReactionId, const FDFReactionRow& Row);
	const FDFStatusRow* FindStatusRow(FName StatusId) const;

	// ---- events for the owning actor ----
	FDFStatusSlotEvent OnStatusApplied;
	FDFStatusSlotEvent OnStatusRemoved;
	FDFReactionEvent OnReactionTriggered;
	FDFSlotsChanged OnSlotsChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION() void OnRep_Slots();

private:
	bool HasAuthority() const;
	float Now() const;
	UAbilitySystemComponent* GetASC() const;
	UDFTintComponent* GetTint() const;
	void LoadReactions();
	void ReadRates();
	FDFStatusTargetState ReadTargetState() const;

	void OnSlotWritten(EDFStatusChannel Channel, AActor* Source, bool bFromReaction);
	void OnSlotLeft(EDFStatusChannel Channel, FName StatusId, bool bExpired);
	void ApplyChannelEffect(EDFStatusChannel Channel, const FDFStatusSlot& Slot, const FDFStatusRow& Row, AActor* Source);
	void RemoveChannelEffect(EDFStatusChannel Channel);
	void ApplyBurst(const FDFStatusApplyOutcome& Outcome, AActor* Source);
	void FireCue(FName StatusId, const TCHAR* Verb, float Magnitude, AActor* Source);
	void BroadcastStatus(const FGameplayTag& MessageTag, const FGameplayTag& StatusTag, EDFStatusChannel Channel, float Magnitude, float Duration);
	void UpdateTintFromResolver(EDFStatusChannel Channel);
	void UpdateTintFromSlots();
	void SyncSlots();

	FDFStatusResolver Resolver;
	FDFStatusApplyOutcome LastOutcome;
	FActiveGameplayEffectHandle ChannelEffects[FDFStatusResolver::NumChannels];
	TWeakObjectPtr<AActor> ChannelSources[FDFStatusResolver::NumChannels];

	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<FDFStatusSlotRep> Slots;

	TMap<FName, FDFStatusRow> StatusRowOverrides;
	TMap<FName, FDFReactionRow> ReactionRowOverrides;
	bool bReactionsLoaded = false;
};
