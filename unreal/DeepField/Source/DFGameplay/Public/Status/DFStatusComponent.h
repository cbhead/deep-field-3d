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
class UDFDamageContext;
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
// Two Step.cs rules live here rather than in the resolver because they need the world:
//   * Ember's passive — a burn applied by an Ember hero (DF.Faction.Ember on the source's ASC,
//     or its owner's / instigator's — or DF.Ability.Passive.BurnDuration, the tag UDFGE_Passive_Ember
//     grants) lasts Balance("emberBurnDurationFactor", 1.3) times longer.
//     The factor goes into the resolver BEFORE its refresh / strongest-wins branch, so an Ember
//     refresh also keeps the longer duration (DF.Unit.Status.EmberDurationOnRefresh).
//   * a same-id refresh keeps the running channel effect (period and phase untouched, no extra
//     on-application tick) and moves its effect-context instigator to the refresher, because
//     Step.cs UpdateStatuses damages with slot.Source: the DoT is credited to whoever refreshed
//     it (DF.Unit.Status.RefreshRetargetsDot);
//   * the reaction burst — bound to the resolver's OnReaction: the consumed status's effect is
//     removed first, then BurstFraction x MaxHealth goes through UDFGE_Damage with no source
//     location (no arc; mark, flat armor and shield apply), and the resolver only writes the
//     EmitStatus if Health is still above 0 (DF.Unit.Status.BurstIsArmoredAndShielded, NoEmitOnKill).
// Thermal / Toxin ticks run at a fixed Balance("tickHz", 30): see the resolver header.
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

	/** Same by content id; DurationFactor multiplies the row duration on top of the condition and Ember factors the component adds itself. */
	UFUNCTION(BlueprintCallable, Category = "DF|Status")
	EDFStatusApplyResult ApplyById(FName StatusId, AActor* Source, float MagnitudeOverride = -1.f, float DurationFactor = 1.f);

	/** True when Source (or its owner / instigator) carries DF.Faction.Ember on an ASC — the applier whose burns last longer. */
	static bool IsEmberApplier(const AActor* Source);

	/** Balance "emberBurnDurationFactor" (1.3) and "tickHz" (30) as read at BeginPlay (defaults until content loads). */
	float GetEmberBurnDurationFactor() const { return EmberBurnDurationFactor; }
	float GetStatusTickHz() const { return StatusTickHz; }

	/** Every detail of the last Apply (reaction id, burst fraction, emitted status, ...). */
	const FDFStatusApplyOutcome& GetLastOutcome() const { return LastOutcome; }

	UFUNCTION(BlueprintCallable, Category = "DF|Status") void ClearChannel(EDFStatusChannel Channel);
	UFUNCTION(BlueprintCallable, Category = "DF|Status") void ClearAll();

	UFUNCTION(BlueprintPure, Category = "DF|Status") bool IsChannelActive(EDFStatusChannel Channel) const;
	UFUNCTION(BlueprintPure, Category = "DF|Status") FGameplayTag ActiveStatus(EDFStatusChannel Channel) const;
	UFUNCTION(BlueprintPure, Category = "DF|Status") float ActiveMagnitude(EDFStatusChannel Channel) const;
	/** Seconds left on the channel, correct on the host AND on a joining client: the replicated
	 *  EndTimeServer is a server timestamp, so it is subtracted from the shared server clock
	 *  (AGameStateBase::GetServerWorldTimeSeconds), never from the local world time. */
	UFUNCTION(BlueprintPure, Category = "DF|Status") float TimeRemaining(EDFStatusChannel Channel) const;
	/** TimeRemaining when this machine knows the server clock; false when it does not yet — a client
	 *  in the moment before its AGameStateBase replicates, where the local clock is unrelated to the
	 *  server's (C5 append, ruling R13). A view that gets false shows the status icon WITHOUT its
	 *  countdown: never a zero and never a full ring. An inactive channel is known (true, 0). */
	UFUNCTION(BlueprintPure, Category = "DF|Status") bool TryGetTimeRemaining(EDFStatusChannel Channel, float& OutSeconds) const;
	/** The rule TryGetTimeRemaining applies: only a client without a game state lacks the server clock. */
	static bool IsServerClockKnown(bool bHasGameState, ENetMode NetMode) { return bHasGameState || NetMode != NM_Client; }
	UFUNCTION(BlueprintPure, Category = "DF|Status") bool IsControlled() const;
	UFUNCTION(BlueprintPure, Category = "DF|Status") float GetCcResist() const { return Resolver.CcResist; }

	/** The replicated slots (valid on every machine). */
	const TArray<FDFStatusSlotRep>& GetSlots() const { return Slots; }
	/** The server-side resolver (empty on clients). */
	const FDFStatusResolver& GetResolver() const { return Resolver; }

	/** The channel's running UDFGE_Status_<Channel>, or an invalid handle when the slot is empty.
	 *  A same-id refresh keeps this handle (the effect is retargeted, never re-applied), which is
	 *  how a caller tells a refresh from a re-application. */
	FActiveGameplayEffectHandle GetChannelEffectHandle(EDFStatusChannel Channel) const { return ChannelEffects[static_cast<int32>(Channel)]; }

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

	/** C5 / B§1.6: the owner is a hero — only Movement statuses and stagger (no cc-resist) land; Thermal / Toxin and
	 *  the rest are RejectedHero before the reaction scan. WS-03 sets it on ADFHeroCharacter; InitFromEnemyRow clears it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DF|Status")
	bool bHero = false;

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
	/** The one clock every slot timestamp is in: the server's world time, as this machine knows it. */
	float Now() const;
	/** Health > 0 on the owner's UDFHealthSet (true when it has none). */
	bool IsAlive() const;
	UAbilitySystemComponent* GetASC() const;
	UDFTintComponent* GetTint() const;
	void LoadReactions();
	void ReadRates();
	FDFStatusTargetState ReadTargetState() const;

	void OnSlotWritten(EDFStatusChannel Channel, AActor* Source, bool bFromReaction);
	void OnSlotLeft(EDFStatusChannel Channel, FName StatusId, bool bExpired);
	void ApplyChannelEffect(EDFStatusChannel Channel, const FDFStatusSlot& Slot, const FDFStatusRow& Row, AActor* Source);
	void RemoveChannelEffect(EDFStatusChannel Channel);
	/** Same-id refresh: the running channel effect keeps its period, but its context now names the refresher. */
	void RetargetChannelEffect(EDFStatusChannel Channel, AActor* Source);
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

	/** Keeps each channel's damage context alive while its periodic effect runs (the effect context only weak-references it). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDFDamageContext>> ChannelContexts;

	TMap<FName, FDFStatusRow> StatusRowOverrides;
	TMap<FName, FDFReactionRow> ReactionRowOverrides;
	bool bReactionsLoaded = false;

	/** Balance.cs EmberBurnDurationFactor until the balance table overrides it. */
	float EmberBurnDurationFactor = 1.3f;
	/** Balance.cs TickHz: the Thermal / Toxin tick rate (period = 1 / this). */
	float StatusTickHz = 30.f;
};
