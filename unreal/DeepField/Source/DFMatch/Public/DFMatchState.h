#pragma once

#include "CoreMinimal.h"
#include "DFMatchPhaseMachine.h"
#include "GameFramework/GameStateBase.h"
#include "Match/DFMatchTypes.h"
#include "Messages/DFMessageBus.h"
#include "Messages/DFMessages.h"
#include "DFMatchState.generated.h"

class ADFEventRelay;
class ADFPlayerState;
class ADFWaveDirector;
class ADFEnemy;
class UDFEconomyStateComponent;
class UDFLaneGraphAsset;
struct FDFLaneRouting;
struct FDFSpawnEntry;

DECLARE_MULTICAST_DELEGATE_OneParam(FDFOnMatchStateChanged, class ADFMatchState* /*MatchState*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FDFOnWaveBoundary, int32 /*WaveIndex about to begin*/);

DECLARE_MULTICAST_DELEGATE_OneParam(FDFOnEarlyCalled, int32 /*Seat*/);
/** bOutUnderWay: a listener that has started the new match (the game mode's travel) sets it; left false,
 *  no new match is coming and the match state stops its countdown. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FDFOnRestartRequested, class ADFMatchState* /*MatchState*/, bool& /*bOutUnderWay*/);

/** How a match starts. The game mode fills it from the travel URL and hands it over in InitGameState. */
struct DFMATCH_API FDFMatchSettings
{
	/** The map whose waves this match plays; None = take it from the level's lane graph. */
	FName MapId;
	/** The plan seed (the save records it; a resume replans from it). 0 = pick one at BeginPlay. */
	uint32 Seed = 0;
	bool bLobby = false;
	bool bEndless = false;
	bool bWaitForPlayers = false;
	/** < 0: the balance dial intermissionSeconds (8 in the sim). */
	float IntermissionSeconds = -1.f;
	/** Seconds from Victory / Defeat to a new match (?playagain=). < 0: the match state's RestartDelaySeconds; 0 = never. */
	float RestartSeconds = -1.f;
	/** How long the host's travel to the new match takes once asked for; the request goes out this much
	 *  before the countdown ends. Not from the URL: ADFGameMode::RestartLeadFor (the server-travel pause on
	 *  a listen or dedicated host, 0 standalone). */
	float RestartLeadSeconds = 0.f;
};

/**
 * The match (WS-28, ADR-0024). On the host it owns the phase machine (FDFMatchPhaseMachine, a port of
 * Step.cs), drives ADFWaveDirector (WS-05), and sends the Wave* / MatchLaunched / Victory / Defeat
 * messages through ADFEventRelay; everywhere it replicates what the HUD shows.
 *
 * WS-28's own replicated fields: phase, wave index, total waves, lobby, endless, threat, lap, enemies
 * remaining and when the intermission clock runs out. Every other match-wide field is a component its
 * domain writes and attaches here (ADR-0024): money, lives, team scrap and bounty (WS-06's
 * UDFEconomyStateComponent, attached as "Economy"), lane and mutable edge states (WS-09). Lives are
 * read through IDFMatchLivesSource (DFCore) and never written.
 *
 * The host keeps each seat's record (ADFPlayerState): a DF.Message.EnemyKilled with a KillerPlayerId
 * is that seat's kill (and match XP, Step.cs:2090), a DF.Message.EnemyDamaged with a SourcePlayerId
 * its damage (Step.cs:2071). ADFEnemy names the seat through IDFSeatHolder (DFCore).
 *
 * Play-again: after Victory or Defeat the host runs a restart clock (RestartDelaySeconds, or the URL's
 * ?playagain=) and broadcasts OnRestartRequested once, the travel's lead before it runs out (a listen
 * host's switch waits out the engine's server-travel pause). ADFGameMode binds it and reloads the map
 * with the same options, so the economy, the seats' records and everything else start fresh; a test
 * binds it instead and nothing travels. The clock's end replicates as PhaseEndsAtServerTime, the same
 * field the intermission uses, so every HUD counts down "New match in 12s" under the banner and reads
 * "Starting new match…" once it is due, until the map goes. If nobody starts one, the clock stops.
 */
UCLASS()
class DFMATCH_API ADFMatchState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ADFMatchState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	// ---- read (every machine) -------------------------------------------------------------------
	EDFMatchPhase GetPhase() const { return Phase; }
	/** The current (or just-cleared) wave, 0-based; -1 before the first. */
	int32 GetWaveIndex() const { return WaveIndex; }
	int32 GetTotalWaves() const { return TotalWaves; }
	bool IsLobby() const { return bLobby; }
	bool IsEndless() const { return bEndless; }
	bool IsOver() const { return Phase == EDFMatchPhase::Victory || Phase == EDFMatchPhase::Defeat; }
	/** The hp scale of the current wave — the HUD's endless readout (DescribeWave's number, not a copy). */
	float GetThreat() const { return Threat; }
	int32 GetLap() const { return Lap; }
	/** Bodies alive plus bodies the director has yet to release this wave. */
	int32 GetEnemiesRemaining() const { return EnemiesRemaining; }
	/** Whether the phase's clock is counting down: the intermission's, or once the match is over the
	 *  restart's (the HUD shows a countdown only then). */
	bool IsPhaseClockRunning() const { return PhaseEndsAtServerTime >= 0.f; }
	/** Seconds until the next wave, or until the new match once this one is over, from the replicated
	 *  end time and the shared server clock; 0 when the clock is not running, and once it has run out. */
	float GetPhaseSecondsLeft() const;
	/** The match is over and a new one follows: its countdown runs, or has run out and the host is
	 *  loading it (the end time stays in the past until the map goes). */
	bool IsRestartPending() const { return IsOver() && IsPhaseClockRunning(); }

	/** Fired when a replicated field changes: on the host when it is written, on clients on receipt. */
	FDFOnMatchStateChanged OnMatchStateChanged;

	// ---- host -----------------------------------------------------------------------------------
	/** From the game mode's InitGameState, before BeginPlay. Resets the phase machine. */
	void ConfigureMatch(const FDFMatchSettings& InSettings);

	/** Use this director instead of spawning one at BeginPlay (tests, a resume that built its own). */
	void UseWaveDirector(ADFWaveDirector* InDirector);
	ADFWaveDirector* GetWaveDirector() const { return Director; }
	ADFEventRelay* GetEventRelay() const { return Relay; }
	/** WS-06's money and lives (every machine; the host writes it). */
	UDFEconomyStateComponent* GetEconomy() const { return Economy; }

	/** Command.Launch from Seat: only the lowest connected seat may leave the lobby (World.cs LaunchSeat). */
	bool ServerLaunch(int32 Seat);
	/** Command.StartWave from Seat: the intermission clock goes to zero (not in the lobby). */
	bool ServerCallEarly(int32 Seat);

	/** One host frame of the match. Tick calls it on the host; tests call it by hand (a test world does not tick actors). */
	void AdvanceMatch(float DeltaSeconds);

	/** Seated, connected players (the player array holds only connected ones; inactive states are the game mode's). */
	int32 GetConnectedPlayerCount() const;
	/** World.cs LaunchSeat: the lowest connected seat, 1 if nobody is seated. */
	int32 GetLaunchSeat() const;

	/**
	 * Host: respawn every seated hero whose bleedout ran out (UDFHeroStateComponent::ShouldRespawnAtWaveBoundary).
	 * Run at the wave boundary before OnWaveBoundary, as Step.cs UpdateWaves does; a hero still bleeding stays
	 * down into the next wave.
	 */
	void HostRespawnBledOutHeroes();

	/** Host: just before a wave begins, after bled-out heroes have respawned. */
	FDFOnWaveBoundary OnWaveBoundary;
	/** Host: a seat called the next wave early (WS-06 pays the early-call bonus, B§1.8). */
	FDFOnEarlyCalled OnEarlyCalled;
	/**
	 * Host: the restart clock of a finished match is down to its lead; start a new match. Broadcast once
	 * per match (FDFMatchPhaseMachine sends Restart once). ADFGameMode binds it, reloads the map and sets
	 * bOutUnderWay. Unbound (a test world, a game mode of its own) or refused, nobody sets it: the clock
	 * stops and the match stays on its banner.
	 */
	FDFOnRestartRequested OnRestartRequested;
	const FDFMatchSettings& GetSettings() const { return Settings; }

	/** The phase machine itself, host only (tests read it). */
	const FDFMatchPhaseMachine& GetPhaseMachine() const { return Machine; }

	/** Seconds from Victory / Defeat to a new match when the URL does not say (?playagain=<seconds>); 0 = never. */
	UPROPERTY(EditDefaultsOnly, Category = "DF|Match", meta = (ClampMin = "0"))
	float RestartDelaySeconds = 15.f;

private:
	UFUNCTION()
	void OnRep_Match();

	/** Host: follow the bus for the seats' records (EnemyKilled, EnemyDamaged); EndPlay stops. */
	void SubscribeCredit();
	void UnsubscribeCredit();
	void HandleEnemyKilled(const FDFMsg_Kill& Kill);
	void HandleEnemyDamaged(const FDFMsg_Damage& Damage);
	/** The seated player state on Seat, or null (the seat has left, or it is 0: a tower's). */
	ADFPlayerState* FindSeat(int32 Seat) const;

	void EnsureDirector();
	void BindDirector();
	/** Host, own director only: answer OnSpawnRequested with ADFEnemy bodies on the map's lane graph. */
	void BindEnemySpawner();
	void HandleSpawnRequested(const FDFSpawnEntry& Entry);
	void HandleEnemyLeaked(ADFEnemy* Enemy);
	void HandleWaveCleared(int32 ClearedWave);
	void ApplyStep(EDFMatchStep Step);
	TOptional<int32> ReadLives() const;
	FDFMsg_Wave Describe(int32 Wave) const;
	int32 PlayersForPlan() const { return FMath::Max(1, GetConnectedPlayerCount()); }
	/** Copies the machine and the director into the replicated fields; notifies if anything changed. */
	void Publish();

	FDFMatchSettings Settings;
	FDFMatchPhaseMachine Machine;
	bool bConfigured = false;
	bool bOwnsDirector = false;
	FDelegateHandle ClearedHandle;

	UPROPERTY(Transient) TObjectPtr<ADFWaveDirector> Director;
	TWeakObjectPtr<const UDFLaneGraphAsset> LaneGraph;
	TSharedPtr<FDFLaneRouting> Routing;
	FDelegateHandle SpawnHandle;
	UPROPERTY(Transient) TObjectPtr<ADFEventRelay> Relay;
	UPROPERTY(VisibleAnywhere, Category = "DF|Match") TObjectPtr<UDFEconomyStateComponent> Economy;
	FDFMessageHandle KilledHandle;
	FDFMessageHandle DamagedHandle;

	UPROPERTY(ReplicatedUsing = OnRep_Match) EDFMatchPhase Phase = EDFMatchPhase::Intermission;
	UPROPERTY(ReplicatedUsing = OnRep_Match) int32 WaveIndex = -1;
	UPROPERTY(ReplicatedUsing = OnRep_Match) int32 TotalWaves = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Match) bool bLobby = false;
	UPROPERTY(ReplicatedUsing = OnRep_Match) bool bEndless = false;
	UPROPERTY(ReplicatedUsing = OnRep_Match) float Threat = 1.f;
	UPROPERTY(ReplicatedUsing = OnRep_Match) int32 Lap = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Match) int32 EnemiesRemaining = 0;
	/** Server-clock time the phase clock reaches zero (the intermission's, or after Victory / Defeat the
	 *  restart's, which stays once passed while the new match loads); -1 when it is not running. */
	UPROPERTY(ReplicatedUsing = OnRep_Match) float PhaseEndsAtServerTime = -1.f;
};
