#include "DFMatchPhaseMachine.h"

void FDFMatchPhaseMachine::Reset(float InIntermissionSeconds, int32 InTotalWaves, bool bInLobby, bool bInEndless, bool bInWaitForPlayers)
{
	IntermissionSeconds = FMath::Max(0.f, InIntermissionSeconds);
	TotalWaves = FMath::Max(0, InTotalWaves);
	bLobby = bInLobby;
	bEndless = bInEndless;
	bWaitForPlayers = bInWaitForPlayers;
	Phase = EDFMatchPhase::Intermission;
	WaveIndex = -1;
	PhaseTimer = IntermissionSeconds;
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

EDFMatchStep FDFMatchPhaseMachine::Tick(float DeltaSeconds, int32 ConnectedPlayers, TOptional<int32> Lives)
{
	if (IsOver())
	{
		return EDFMatchStep::None;
	}
	// Step.cs runs CheckEndState after UpdateWaves in the same step. Here leaks arrive between frames,
	// so checking first is the same verdict one frame sooner, and it means a dead core never starts a wave.
	if (Lives.IsSet() && Lives.GetValue() <= 0)
	{
		Phase = EDFMatchPhase::Defeat;
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
		Phase = EDFMatchPhase::Defeat;
		return EDFMatchStep::Defeat;
	}
	// Endless has no last wave; the arc rolls over and the core is the only thing that can end the run.
	if (!bEndless && WaveIndex + 1 >= TotalWaves)
	{
		Phase = EDFMatchPhase::Victory;
		return EDFMatchStep::Victory;
	}
	Phase = EDFMatchPhase::Intermission;
	PhaseTimer = IntermissionSeconds;
	return EDFMatchStep::Intermission;
}
