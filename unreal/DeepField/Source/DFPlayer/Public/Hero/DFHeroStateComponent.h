#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Hero/DFHeroLife.h"
#include "DFHeroStateComponent.generated.h"

class UDFHeroStateComponent;

/** Host-side life events, for WS-28 to publish as C15 messages and to credit the reviver. */
enum class EDFHeroLifeEvent : uint8
{
	/** Health reached 0 with teammates present: down and bleeding out. DF.Message.PlayerDowned. */
	Downed,
	/** Health reached 0 alone: the solo respawn timer started. Also DF.Message.PlayerDowned (Step.cs emits it for both). */
	SoloDowned,
	/** A reviver's hold completed: up at RevivedHpFraction health. DF.Message.PlayerRevived; the reviver gets +5 match XP and a revive (Step.cs). */
	Revived,
	/** The solo timer ran out, or WS-28 respawned a bled-out hero at the wave boundary. DF.Message.PlayerRespawned; full health, at the hero spawn. */
	Respawned,
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FDFOnHeroLifeEvent, EDFHeroLifeEvent /*Event*/, UDFHeroStateComponent* /*Reviver, for Revived*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FDFOnHeroStateChanged, UDFHeroStateComponent* /*State*/);

/**
 * What clients need to show a hero's state, written by the host on every transition only. Timers are
 * server-clock deadlines (AGameStateBase::GetServerWorldTimeSeconds), so nothing replicates per frame;
 * revive progress is replicated with the time it was stamped and whether a reviver is holding, and a
 * client extrapolates it at the same fixed rates the host uses.
 */
USTRUCT()
struct DFPLAYER_API FDFHeroStateRep
{
	GENERATED_BODY()

	/** EDFHeroLife. */
	UPROPERTY() uint8 Life = 0;
	UPROPERTY() float BleedoutEndsAt = 0.f;
	UPROPERTY() float RespawnEndsAt = 0.f;
	UPROPERTY() float ReviveProgress = 0.f;
	UPROPERTY() float ReviveStampedAt = 0.f;
	UPROPERTY() bool bReviveHeld = false;
	/** The owner (player state) of the hero reviving this one, if any. */
	UPROPERTY() TWeakObjectPtr<AActor> Reviver;
	/** C12 Seat: the seat index, INDEX_NONE (−1) on foot. */
	UPROPERTY() int32 SeatIndex = INDEX_NONE;
	UPROPERTY() TWeakObjectPtr<AActor> SeatVehicle;
};

/**
 * A player's downed, bleedout, revive and vehicle-seat state (WS-03, ADR-0024, B§1.14). WS-28 hosts it
 * on ADFPlayerState, so it survives the pawn being destroyed and respawned. The rules are DFHeroLife;
 * this component holds the host's FDFHeroLifeState, steps it, replicates it, and raises the events.
 *
 * What the host's other code does with it (WS-28's attach PR and the hero's health, later):
 * - When a hero's Health reaches 0, call HostDeplete with the number of connected players.
 * - A reviver holding the revive calls HostBeginRevive on the downed player's component and
 *   HostEndRevive on release. Only one reviver counts at a time; range is checked every tick from
 *   the two pawns' locations.
 * - When the intermission ends, call HostRespawn on every component whose
 *   ShouldRespawnAtWaveBoundary() is true, and move that pawn to the hero spawn.
 * - Subscribe OnHeroLifeEvent to publish PlayerDowned / PlayerRevived / PlayerRespawned through
 *   ADFEventRelay (with the seat as PlayerId), to restore health, and to credit the reviver.
 *
 * A plain UActorComponent rather than ModularGameplay's UPlayerStateComponent: that base would need
 * ModularGameplay public in DFPlayer's Build.cs and in modules.json (INT), and nothing uses the
 * extension system yet.
 */
UCLASS(ClassGroup = (DF), meta = (BlueprintSpawnableComponent))
class DFPLAYER_API UDFHeroStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDFHeroStateComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The component on a player state, or on a pawn's player state. */
	static UDFHeroStateComponent* Find(const AActor* PlayerStateOrPawn);

	// ---- any machine ----------------------------------------------------------------------------

	EDFHeroLife GetLife() const;
	bool IsUp() const { return GetLife() == EDFHeroLife::Up; }
	bool IsDowned() const { return GetLife() == EDFHeroLife::Downed; }
	bool IsRespawning() const { return GetLife() == EDFHeroLife::Respawning; }
	/** Downed with bleedout still running (DF.Player.State.Bleeding). */
	bool IsBleeding() const;

	/** Seconds of revive progress (C12 ReviveProgress is GetReviveFraction). */
	float GetReviveProgress() const;
	float GetReviveFraction() const;

	/** Seconds of bleedout left, 0 when not bleeding. False on a client that does not know the server clock yet (R13: show no countdown). */
	bool TryGetBleedoutSecondsLeft(float& OutSeconds) const;
	/** Seconds until the solo respawn, 0 when not respawning. False as above. */
	bool TryGetRespawnSecondsLeft(float& OutSeconds) const;

	int32 GetSeatIndex() const { return Rep.SeatIndex; }
	AActor* GetSeatVehicle() const { return Rep.SeatVehicle.Get(); }
	/** The player state of whoever is reviving this hero, if anyone. */
	AActor* GetReviver() const { return Rep.Reviver.Get(); }

	/** DF.Player.State.Downed, .Bleeding and .Seated as they currently apply. */
	void GetStateTags(FGameplayTagContainer& OutTags) const;

	/** DFHeroLife's numbers, from DT_Balance (reviveSeconds, bleedoutSeconds, soloRespawnSeconds, reviveRangeMeters) when content is loaded. */
	const FDFHeroLifeRules& GetRules() const;

	// ---- host only (ignored elsewhere) ----------------------------------------------------------

	/** Health reached 0 (see DFHeroLife::Deplete). Leaves any seat. */
	EDFHeroDown HostDeplete(int32 ConnectedPlayers);

	/** Reviver starts holding the revive on this hero. False if this hero is not down, the reviver is not up, or someone else is already reviving. */
	bool HostBeginRevive(UDFHeroStateComponent* Reviver);
	void HostEndRevive(UDFHeroStateComponent* Reviver);

	/** One host step (DFHeroLife::Tick). TickComponent calls it with whether the reviver is up and in range; tests call it directly. */
	void HostAdvance(float DeltaSeconds, bool bReviveHeld);

	/** Bled out and still down: WS-28 calls HostRespawn when the next wave begins. */
	bool ShouldRespawnAtWaveBoundary() const;
	void HostRespawn();

	/** Take seat SeatIndex of Vehicle. Refused unless standing (Step.cs refuses "downed"). */
	bool HostTakeSeat(AActor* Vehicle, int32 SeatIndex);
	void HostLeaveSeat();

	/** Host only: the life transitions, once each, with the reviver for Revived. */
	FDFOnHeroLifeEvent OnHeroLifeEvent;

	/** Any change to what the readers return: on the host when written, on clients on receipt. */
	FDFOnHeroStateChanged OnHeroStateChanged;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_State();

	bool HasHostAuthority() const;
	float Now() const;
	bool IsServerClockKnown() const;
	bool IsWithinReviveRange(const UDFHeroStateComponent& Reviver) const;
	void ClearSeat();
	/** Copies the host's state into Rep, stamped now, and tells listeners and the net driver. */
	void WriteRep();

	UPROPERTY(ReplicatedUsing = OnRep_State)
	FDFHeroStateRep Rep;

	/** The host's truth; clients read Rep. */
	FDFHeroLifeState LifeState;
	TWeakObjectPtr<UDFHeroStateComponent> ActiveReviver;
	bool bReviveHeldLastStep = false;
	mutable TOptional<FDFHeroLifeRules> CachedRules;
};
