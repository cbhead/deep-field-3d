#include "DFMatchPhaseMachine.h"

void FDFMatchPhaseMachine::Reset(float InIntermissionSeconds, int32 InTotalWaves, bool bInLobby, bool bInEndless, bool bInWaitForPlayers, float InRestartSeconds)
{
	IntermissionSeconds = FMath::Max(0.f, InIntermissionSeconds);
	TotalWaves = FMath::Max(0, InTotalWaves);
	bLobby = bInLobby;
	bEndless = bInEndless;
	bWaitForPlayers = bInWaitForPlayers;
	RestartSeconds = FMath::Max(0.f, InRestartSeconds);
	Phase = EDFMatchPhase::Intermission;
	WaveIndex = -1;
	PhaseTimer = IntermissionSeconds;
	RestartTimer = -1.f;
	bRestartSent = false;
}

bool FDFMatchPhaseMachine::IsClockRunning(int32 ConnectedPlayers) const
{
	return Phase == EDFMatchPhase::Intermission
		&& !bLobby
		&& TotalWaves > 0
		&& !(bWaitForPlayers && ConnectedPlayers == 0);
}

bool FDFMatchPhaseMachine::Launch()
{
	// Step.cs ApplyLaunch. Who may launch (the lowest connected seat) is the caller's check: this
	// machine does not know about seats.
	if (!bLobby)
	{
		return false;
	}
	bLobby = false;
	Phase = EDFMatchPhase::Intermission;
	PhaseTimer = IntermissionSeconds;
	return true;
}

bool FDFMatchPhaseMachine::CallEarly()
{
	// Step.cs Command.StartWave: `if (w.Phase == MatchPhase.Intermission && !w.Lobby) w.PhaseTimer = 0f;`
	if (Phase != EDFMatchPhase::Intermission || bLobby)
	{
		return false;
	}
	PhaseTimer = 0.f;
	return true;
}

void FDFMatchPhaseMachine::Finish(EDFMatchPhase Verdict)
{
	Phase = Verdict;
	// A party that never launched has played no match to follow with another; RestartSeconds 0 keeps
	// the banner up for good (the match state's knob, or ?restart=0).
	RestartTimer = (RestartSeconds > 0.f && !bLobby && !bRestartSent) ? RestartSeconds : -1.f;
}

EDFMatchStep FDFMatchPhaseMachine::Tick(float DeltaSeconds, int32 ConnectedPlayers, TOptional<int32> Lives)
{
	if (IsOver())
	{
		// The restart clock runs whoever is connected: a host left alone still gets a new match.
		if (!IsRestartClockRunning())
		{
			return EDFMatchStep::None;
		}
		RestartTimer -= DeltaSeconds;
		if (RestartTimer > 0.f)
		{
			return EDFMatchStep::None;
		}
		RestartTimer = -1.f;
		bRestartSent = true;
		return EDFMatchStep::Restart;
	}
	// Step.cs runs CheckEndState after UpdateWaves in the same step. Here leaks arrive between frames,
	// so checking first is the same verdict one frame sooner, and it means a dead core never starts a wave.
	if (Lives.IsSet() && Lives.GetValue() <= 0)
	{
		Finish(EDFMatchPhase::Defeat);
		return EDFMatchStep::Defeat;
	}
	if (!IsClockRunning(ConnectedPlayers))
	{
		return EDFMatchStep::None;
	}
	PhaseTimer -= DeltaSeconds;
	if (PhaseTimer > 0.f)
	{
		return EDFMatchStep::None;
	}
	PhaseTimer = 0.f;
	++WaveIndex;
	Phase = EDFMatchPhase::Wave;
	return EDFMatchStep::BeginWave;
}

EDFMatchStep FDFMatchPhaseMachine::WaveCleared(int32 ClearedWave, TOptional<int32> Lives)
{
	if (IsOver() || Phase != EDFMatchPhase::Wave || ClearedWave != WaveIndex)
	{
		return EDFMatchStep::None;
	}
	if (Lives.IsSet() && Lives.GetValue() <= 0)
	{
		Finish(EDFMatchPhase::Defeat);
		return EDFMatchStep::Defeat;
	}
	// Endless has no last wave; the arc rolls over and the core is the only thing that can end the run.
	if (!bEndless && WaveIndex + 1 >= TotalWaves)
	{
		Finish(EDFMatchPhase::Victory);
		return EDFMatchStep::Victory;
	}
	Phase = EDFMatchPhase::Intermission;
	PhaseTimer = IntermissionSeconds;
	return EDFMatchStep::Intermission;
}
