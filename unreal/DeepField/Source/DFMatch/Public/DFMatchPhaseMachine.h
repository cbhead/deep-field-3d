#pragma once

#include "CoreMinimal.h"
#include "Match/DFMatchTypes.h"
#include "Misc/Optional.h"

/** What a host frame (or a cleared wave) asks the match state to do next. */
enum class EDFMatchStep : uint8
{
	None,
	BeginWave,     // WaveIndex has advanced; begin it on the director and announce it
	Intermission,  // the wave cleared and the arc goes on; the intermission clock is reset
	Victory,
	Defeat,
};

/**
 * The match phase, as a pure function of commands, time and lives — a port of Step.cs `UpdateWaves`
 * (Step.cs:940) + `CheckEndState` (Step.cs:1977) + `ApplyLaunch` / `StartWave` (WS-28, ADR-0024).
 * No UObject, no world: ADFMatchState owns one on the host and applies the steps it returns, and the
 * DF.Unit.Match tests drive it directly.
 *
 * The wave itself belongs to ADFWaveDirector (WS-05): this machine starts a wave and learns that it
 * cleared; it never counts bodies.
 *
 * Lives are optional: until WS-06's economy component exists there is no lives source, and an
 * unknown lives count never defeats anyone.
 */
struct DFMATCH_API FDFMatchPhaseMachine
{
	EDFMatchPhase Phase = EDFMatchPhase::Intermission;
	/** The current (or just-cleared) wave; -1 before the first (World.cs WaveIndex). */
	int32 WaveIndex = -1;
	/** Seconds left in intermission (World.cs PhaseTimer). */
	float PhaseTimer = 8.f;
	/** Balance dial intermissionSeconds. */
	float IntermissionSeconds = 8.f;
	/** The authored arc. 0 = this map has no waves (a dev map): the machine never begins one. */
	int32 TotalWaves = 0;
	/** The party is assembling: the clock does not run and early calls are ignored until Launch. */
	bool bLobby = false;
	/** A dedicated server with nobody connected does not burn its intermission. */
	bool bWaitForPlayers = false;
	/** Endless: the arc never ends; past the last authored wave the director laps the tables. */
	bool bEndless = false;

	/** A fresh match (World.cs defaults + ApplyLaunch's clock). */
	void Reset(float InIntermissionSeconds, int32 InTotalWaves, bool bInLobby, bool bInEndless, bool bInWaitForPlayers);

	bool IsOver() const { return Phase == EDFMatchPhase::Victory || Phase == EDFMatchPhase::Defeat; }

	/** Whether the intermission clock is counting down right now (the HUD shows a countdown only then). */
	bool IsClockRunning(int32 ConnectedPlayers) const;

	/** Command.Launch from the launch seat: lobby -> intermission with a full clock. False if not in the lobby. */
	bool Launch();

	/** Command.StartWave (early call): the intermission clock goes to zero. False in the lobby or outside intermission. */
	bool CallEarly();

	/**
	 * One host frame. Returns Defeat if the lives source says the core is gone (any phase, before the
	 * clock: a dead core never starts a wave), BeginWave when the intermission clock runs out, else None.
	 */
	EDFMatchStep Tick(float DeltaSeconds, int32 ConnectedPlayers, TOptional<int32> Lives);

	/**
	 * The director cleared ClearedWave. Lives first (Step.cs CheckEndState): Defeat if the core is
	 * gone, else Victory after the last authored wave (never in endless), else Intermission with the
	 * clock reset. None if the report is stale (not the running wave) or the match is over.
	 */
	EDFMatchStep WaveCleared(int32 ClearedWave, TOptional<int32> Lives);
};
