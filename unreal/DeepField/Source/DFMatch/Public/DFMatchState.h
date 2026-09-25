#pragma once

#include "CoreMinimal.h"
#include "DFMatchPhaseMachine.h"
#include "GameFramework/GameStateBase.h"
#include "Match/DFMatchTypes.h"
#include "Messages/DFMessages.h"
#include "DFMatchState.generated.h"

class ADFEventRelay;
class ADFPlayerState;
class ADFWaveDirector;

DECLARE_MULTICAST_DELEGATE_OneParam(FDFOnMatchStateChanged, class ADFMatchState* /*MatchState*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FDFOnWaveBoundary, int32 /*WaveIndex about to begin*/);

DECLARE_MULTICAST_DELEGATE_OneParam(FDFOnEarlyCalled, int32 /*Seat*/);

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
};

/**
 * The match (WS-28, ADR-0023). On the host it owns the phase machine (FDFMatchPhaseMachine, a port of
 * Step.cs), drives ADFWaveDirector (WS-05), and sends the Wave* / MatchLaunched / Victory / Defeat
 * messages through ADFEventRelay; everywhere it replicates what the HUD shows.
 *
 * WS-28's own replicated fields: phase, wave index, total waves, lobby, endless, threat, lap, enemies
 * remaining and when the intermission clock runs out. Every other match-wide field is a component its
 * domain writes and attaches here (ADR-0023): money, lives, team scrap and bounty (WS-06), lane and
 * mutable edge states (WS-09). Lives are read through IDFMatchLivesSource (DFCore) and never written.
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
	/** Whether the intermission clock is counting down (the HUD shows a countdown only then). */
	bool IsPhaseClockRunning() const { return PhaseEndsAtServerTime >= 0.f; }
	/** Seconds until the next wave, from the replicated end time and the shared server clock; 0 when the clock is not running. */
	float GetPhaseSecondsLeft() const;

	/** Fired when a replicated field changes: on the host when it is written, on clients on receipt. */
	FDFOnMatchStateChanged OnMatchStateChanged;

	// ---- host -----------------------------------------------------------------------------------
	/** From the game mode's InitGameState, before BeginPlay. Resets the phase machine. */
	void ConfigureMatch(const FDFMatchSettings& InSettings);

	/** Use this director instead of spawning one at BeginPlay (tests, a resume that built its own). */
	void UseWaveDirector(ADFWaveDirector* InDirector);
	ADFWaveDirector* GetWaveDirector() const { return Director; }
	ADFEventRelay* GetEventRelay() const { return Relay; }

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

	/** Host: just before a wave begins (WS-03 respawns players whose bleedout ran out). */
	FDFOnWaveBoundary OnWaveBoundary;
	/** Host: a seat called the next wave early (WS-06 pays the early-call bonus, B§1.8). */
	FDFOnEarlyCalled OnEarlyCalled;
	const FDFMatchSettings& GetSettings() const { return Settings; }

	/** The phase machine itself, host only (tests read it). */
	const FDFMatchPhaseMachine& GetPhaseMachine() const { return Machine; }

private:
	UFUNCTION()
	void OnRep_Match();

	void EnsureDirector();
	void BindDirector();
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
	UPROPERTY(Transient) TObjectPtr<ADFEventRelay> Relay;

	UPROPERTY(ReplicatedUsing = OnRep_Match) EDFMatchPhase Phase = EDFMatchPhase::Intermission;
	UPROPERTY(ReplicatedUsing = OnRep_Match) int32 WaveIndex = -1;
	UPROPERTY(ReplicatedUsing = OnRep_Match) int32 TotalWaves = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Match) bool bLobby = false;
	UPROPERTY(ReplicatedUsing = OnRep_Match) bool bEndless = false;
	UPROPERTY(ReplicatedUsing = OnRep_Match) float Threat = 1.f;
	UPROPERTY(ReplicatedUsing = OnRep_Match) int32 Lap = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Match) int32 EnemiesRemaining = 0;
	/** Server-clock time the intermission clock reaches zero; -1 when it is not running. */
	UPROPERTY(ReplicatedUsing = OnRep_Match) float PhaseEndsAtServerTime = -1.f;
};
