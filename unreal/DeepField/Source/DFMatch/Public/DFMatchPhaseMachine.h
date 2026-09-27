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
	Restart,       // the restart clock after Victory / Defeat is down to its lead: the host starts a new match (sent once)
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
 *
 * Play-again (WS-28, not the sim's: Godot's end screen waited for the player): a finished match starts
 * the restart clock, RestartSeconds long, which counts down to the moment the new match starts. Tick
 * returns Restart once, RestartLeadSeconds before that moment: a networked host's travel takes that long
 * to happen (the engine's server-travel pause), so asking early makes the reload land as the countdown
 * reaches 0. The clock then stays at 0 (the new match is due) until the map goes, or until the host says
 * no new match is coming (AbandonRestart). A match finishes only by Victory or Defeat, so a running one
 * never restarts, an endless run included (it never reaches Victory; only its core falling ends it), and
 * neither does a party still in the lobby.
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
	/** Seconds from Victory or Defeat to a new match; 0 = the match stays over. */
	float RestartSeconds = 0.f;
	/** How long before the new match the host asks for it: its travel's delay (ADFGameMode::RestartLeadFor,
	 *  the server-travel pause on a listen or dedicated host, 0 standalone). The clock never starts shorter. */
	float RestartLeadSeconds = 0.f;
	/** Seconds to the new match: < 0 when none is coming (the match runs, the restart is off or was
	 *  abandoned); after Victory / Defeat it counts down and stays at 0 once the new match is due. */
	float RestartTimer = -1.f;
	/** The Restart step has been returned: it never is again (the host is already travelling). */
	bool bRestartSent = false;

	/** A fresh match (World.cs defaults + ApplyLaunch's clock). */
	void Reset(float InIntermissionSeconds, int32 InTotalWaves, bool bInLobby, bool bInEndless, bool bInWaitForPlayers,
		float InRestartSeconds = 0.f, float InRestartLeadSeconds = 0.f);

	/**
	 * The lead learned after Reset (a PIE host listens only after InitGame: ADFGameMode::StartPlay). Only
	 * the lead changes: the phase, the wave and the intermission clock go on where they are. A restart
	 * clock that has not asked yet is raised to the lead if shorter, as Finish would have started it.
	 */
	void SetRestartLead(float InRestartLeadSeconds);

	bool IsOver() const { return Phase == EDFMatchPhase::Victory || Phase == EDFMatchPhase::Defeat; }

	/** Whether the intermission clock is counting down right now (the HUD shows a countdown only then). */
	bool IsClockRunning(int32 ConnectedPlayers) const;

	/** Whether a new match follows this finished one: RestartTimer is the seconds left, 0 once it is due. */
	bool IsRestartClockRunning() const { return IsOver() && RestartTimer >= 0.f; }

	/** The host asked for the new match and none is coming (nobody took the request up, or the travel was
	 *  refused): the clock stops and the match stays over on its banner. Restart is not sent again. */
	void AbandonRestart();

	/** Command.Launch from the launch seat: lobby -> intermission with a full clock. False if not in the lobby. */
	bool Launch();

	/** Command.StartWave (early call): the intermission clock goes to zero. False in the lobby or outside intermission. */
	bool CallEarly();

	/**
	 * One host frame. Returns Defeat if the lives source says the core is gone (any phase, before the
	 * clock: a dead core never starts a wave), BeginWave when the intermission clock runs out, else None.
	 * Once the match is over it runs the restart clock instead, and returns Restart (once) when that is
	 * down to RestartLeadSeconds.
	 */
	EDFMatchStep Tick(float DeltaSeconds, int32 ConnectedPlayers, TOptional<int32> Lives);

	/**
	 * The director cleared ClearedWave. Lives first (Step.cs CheckEndState): Defeat if the core is
	 * gone, else Victory after the last authored wave (never in endless), else Intermission with the
	 * clock reset. None if the report is stale (not the running wave) or the match is over.
	 */
	EDFMatchStep WaveCleared(int32 ClearedWave, TOptional<int32> Lives);

private:
	/** Victory or Defeat, and the restart clock starts (not in the lobby, not with RestartSeconds 0), never
	 *  shorter than the lead. */
	void Finish(EDFMatchPhase Verdict);
};
