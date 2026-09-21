#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "DFGameplayCueNotify_Base.generated.h"

class AActor;

/** Which GAS cue event a forwarded cue was (EGameplayCueEvent without the GAS header for subscribers). */
UENUM(BlueprintType)
enum class EDFCueEvent : uint8
{
	Executed,      // ExecuteGameplayCue — every DF status Applied / Removed / Tick and every reaction is one of these
	Active,        // a lasting cue started (AddGameplayCue)
	WhileActive,   // a lasting cue is running: relevancy, late join
	Removed,       // a lasting cue ended (RemoveGameplayCue)
};

// What every DF cue notify posts on UDFMessageBus, keyed by the CUE TAG ITSELF (not a DF.Message.*
// tag: the C15 inventory is DFCore's and append-only; the cue tags are already the contract — C5
// "Cues: GameplayCue.DF.Status.<id>.Applied/Removed/Tick — VFX (C10) and audio (C11) subscribe").
// Subscribe to GameplayCue.DF (children included) for everything, GameplayCue.DF.Reaction for the
// reactions, one leaf for one cue. Local to the machine that ran the cue (cues already replicate
// through GAS; the bus never relays these), so it may carry the actor.
USTRUCT(BlueprintType)
struct DFGAMEPLAY_API FDFMsg_GameplayCue
{
	GENERATED_BODY()
	/** The actor the cue ran on. */
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Target = nullptr;
	/** The target's C15 id when it has a UDFStatusComponent (its TargetId), else 0. */
	UPROPERTY(BlueprintReadOnly) int32 TargetId = 0;
	/** The cue as fired: GameplayCue.DF.Reaction.ThermalShock, GameplayCue.DF.Status.Burn.Applied, ... */
	UPROPERTY(BlueprintReadOnly) FGameplayTag Cue;
	/** What the handling notify registered for: the leaf when a per-cue asset handled it, the family root
	 *  (GameplayCue.DF.Status / GameplayCue.DF.Reaction) when the native fallback did. */
	UPROPERTY(BlueprintReadOnly) FGameplayTag HandlerTag;
	UPROPERTY(BlueprintReadOnly) EDFCueEvent Event = EDFCueEvent::Executed;
	/** The applier (FGameplayCueParameters::Instigator), when the firer set one. */
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Instigator = nullptr;
	/** Status cues: the slot magnitude (Applied) or the tick damage (Tick). Reactions: the burst damage after the damage order. */
	UPROPERTY(BlueprintReadOnly) float RawMagnitude = 0.f;
	/** Reactions: the row's BurstFraction (0.12 for thermalShock). */
	UPROPERTY(BlueprintReadOnly) float NormalizedMagnitude = 0.f;
	/** The cue's location, or the target's when the firer gave none. */
	UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector;
};

// The cue base (PROGRAMME §5.2 WS-02 "cue bases"): the root of every DF GameplayCue notify. Static
// (no actor per cue), and it does one thing after the notify ran: it posts FDFMsg_GameplayCue on
// UDFMessageBus, so WS-14 (VFX) and WS-13 (audio) subscribe to cue tags without touching GAS, and
// a test map / DF.Func test can see that a cue ran. Two families derive from it —
// UDFGameplayCueNotify_Status (GameplayCue.DF.Status.<Id>.<Verb>) and UDFGameplayCueNotify_Reaction
// (GameplayCue.DF.Reaction.<Id>) — and RegisterNativeCues() enters those two classes into the
// GameplayCueManager's runtime cue set under the family ROOT tags, so every leaf the status
// component fires is handled (the cue set routes an unregistered leaf to its nearest registered
// parent) with no asset in Content. A per-cue asset (WS-14's GC_DF_Reaction_ThermalShock with a
// Niagara system, say) registers its own leaf and takes over that one cue; derive it from the
// family class so the bus still hears it. IsOverride stays true (one handler per cue).
UCLASS(Abstract)
class DFGAMEPLAY_API UDFGameplayCueNotify_Base : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	/** Enter the native family handlers into the runtime cue set (idempotent; a tag that already has a
	 *  handler — an asset — is left alone). Called from UDFAbilitySystemComponent::InitAbilityActorInfo
	 *  and UDFStatusComponent::BeginPlay: the set exists only once the asset registry finished
	 *  gathering, and the next call heals a set the manager rebuilt. */
	static void RegisterNativeCues();

	/** The C15 target id of a cue target: UDFStatusComponent::TargetId when it carries one, else 0. */
	static int32 TargetIdOf(const AActor* Target);

	/** Post the cue on the bus. HandleGameplayCue does this after the notify ran; public so a Blueprint
	 *  child that replaces HandleGameplayCue can still forward. */
	UFUNCTION(BlueprintCallable, Category = "DF|Cue")
	void ForwardToBus(AActor* MyTarget, EDFCueEvent Event, const FGameplayCueParameters& Parameters) const;

	/** Whether HandleGameplayCue forwards to the bus (a purely cosmetic child may turn it off). */
	UPROPERTY(EditDefaultsOnly, Category = "DF|Cue")
	bool bForwardToMessageBus = true;

	virtual void HandleGameplayCue(AActor* MyTarget, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) override;
};
