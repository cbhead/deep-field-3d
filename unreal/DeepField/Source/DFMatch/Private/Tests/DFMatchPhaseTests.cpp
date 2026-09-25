#include "DFMatchPhaseMachine.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.Unit.Match.* (phase) — FDFMatchPhaseMachine against Step.cs UpdateWaves / CheckEndState /
// ApplyLaunch / StartWave. Pure: no world, no director. The world half is DFMatchStateTests.cpp.

namespace DFMatchPhaseTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	const TOptional<int32> Alive(20);
	const TOptional<int32> Unknown;

	FDFMatchPhaseMachine Fresh(int32 TotalWaves, bool bLobby = false, bool bEndless = false, bool bWait = false)
	{
		FDFMatchPhaseMachine M;
		M.Reset(8.f, TotalWaves, bLobby, bEndless, bWait);
		return M;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchPhaseOrderTest, "DF.Unit.Match.PhaseOrderMatchesSim", DFMatchPhaseTest::Flags)
bool FDFMatchPhaseOrderTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchPhaseTest;
	FDFMatchPhaseMachine M = Fresh(2);
	TestEqual(TEXT("a match opens in intermission"), M.Phase, EDFMatchPhase::Intermission);
	TestEqual(TEXT("before the first wave the index is -1 (World.cs)"), M.WaveIndex, -1);

	TestEqual(TEXT("7.9 s into an 8 s intermission: nothing"), M.Tick(7.9f, 1, Alive), EDFMatchStep::None);
	TestEqual(TEXT("the clock runs out: the first wave begins"), M.Tick(0.2f, 1, Alive), EDFMatchStep::BeginWave);
	TestEqual(TEXT("wave 0"), M.WaveIndex, 0);
	TestEqual(TEXT("phase Wave"), M.Phase, EDFMatchPhase::Wave);
	TestEqual(TEXT("time alone never ends a wave (the director does)"), M.Tick(100.f, 1, Alive), EDFMatchStep::None);

	TestEqual(TEXT("wave 0 cleared, one more to go: intermission"), M.WaveCleared(0, Alive), EDFMatchStep::Intermission);
	TestTrue(TEXT("with a full clock"), FMath::IsNearlyEqual(M.PhaseTimer, 8.f));
	TestEqual(TEXT("exactly 8 s later: wave 1"), M.Tick(8.f, 1, Alive), EDFMatchStep::BeginWave);
	TestEqual(TEXT("wave 1"), M.WaveIndex, 1);

	TestEqual(TEXT("the last authored wave cleared: victory"), M.WaveCleared(1, Alive), EDFMatchStep::Victory);
	TestTrue(TEXT("the match is over"), M.IsOver());
	TestEqual(TEXT("an over match ignores time"), M.Tick(100.f, 1, Alive), EDFMatchStep::None);
	TestEqual(TEXT("and a late clear"), M.WaveCleared(1, Alive), EDFMatchStep::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchLastLeakIsDefeatTest, "DF.Unit.Match.LastLeakIsDefeat", DFMatchPhaseTest::Flags)
bool FDFMatchLastLeakIsDefeatTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchPhaseTest;
	// Step.cs CheckEndState tests lives before the cleared test: a last enemy that leaks the core to
	// zero ends the arc in Defeat, never Victory.
	FDFMatchPhaseMachine M = Fresh(1);
	TestEqual(TEXT("the only wave begins"), M.Tick(8.f, 1, Alive), EDFMatchStep::BeginWave);
	TestEqual(TEXT("cleared by a leak that took the last life: defeat"), M.WaveCleared(0, TOptional<int32>(0)), EDFMatchStep::Defeat);
	TestEqual(TEXT("phase Defeat"), M.Phase, EDFMatchPhase::Defeat);

	// Mid-arc too: the leak that empties the core defeats even though waves remain.
	FDFMatchPhaseMachine Mid = Fresh(5);
	Mid.Tick(8.f, 1, Alive);
	TestEqual(TEXT("a mid-arc wave cleared on zero lives: defeat, not intermission"), Mid.WaveCleared(0, TOptional<int32>(0)), EDFMatchStep::Defeat);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchLivesEveryTickTest, "DF.Unit.Match.LivesCheckedEveryTick", DFMatchPhaseTest::Flags)
bool FDFMatchLivesEveryTickTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchPhaseTest;
	FDFMatchPhaseMachine InWave = Fresh(3);
	InWave.Tick(8.f, 1, Alive);
	TestEqual(TEXT("lives reach zero mid-wave: defeat on the next frame"), InWave.Tick(0.016f, 1, TOptional<int32>(0)), EDFMatchStep::Defeat);

	FDFMatchPhaseMachine Break = Fresh(3);
	TestEqual(TEXT("a dead core never starts a wave, even with the clock at zero"), Break.Tick(8.f, 1, TOptional<int32>(-3)), EDFMatchStep::Defeat);
	TestEqual(TEXT("the wave index did not move"), Break.WaveIndex, -1);

	FDFMatchPhaseMachine NoSource = Fresh(1);
	NoSource.Tick(8.f, 1, Unknown);
	TestEqual(TEXT("with no lives source (before WS-06's component) a clear is a victory"), NoSource.WaveCleared(0, Unknown), EDFMatchStep::Victory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchLobbyGateTest, "DF.Unit.Match.LobbyGate", DFMatchPhaseTest::Flags)
bool FDFMatchLobbyGateTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchPhaseTest;
	FDFMatchPhaseMachine M = Fresh(3, /*bLobby*/ true);
	TestFalse(TEXT("the lobby's clock does not run"), M.IsClockRunning(2));
	TestEqual(TEXT("ten minutes in the lobby: still nothing"), M.Tick(600.f, 2, Alive), EDFMatchStep::None);
	TestFalse(TEXT("an early call is ignored in the lobby (Step.cs StartWave)"), M.CallEarly());
	TestTrue(TEXT("Launch leaves the lobby"), M.Launch());
	TestFalse(TEXT("into intermission"), M.bLobby);
	TestTrue(TEXT("with a full clock (ApplyLaunch)"), FMath::IsNearlyEqual(M.PhaseTimer, 8.f));
	TestFalse(TEXT("a second Launch does nothing"), M.Launch());
	TestEqual(TEXT("the clock now runs"), M.Tick(8.f, 2, Alive), EDFMatchStep::BeginWave);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchEarlyCallTest, "DF.Unit.Match.EarlyCall", DFMatchPhaseTest::Flags)
bool FDFMatchEarlyCallTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchPhaseTest;
	FDFMatchPhaseMachine M = Fresh(3);
	M.Tick(2.f, 1, Alive);
	TestTrue(TEXT("an early call in intermission is taken"), M.CallEarly());
	TestEqual(TEXT("the wave begins on the next frame"), M.Tick(0.f, 1, Alive), EDFMatchStep::BeginWave);
	TestFalse(TEXT("an early call during a wave does nothing"), M.CallEarly());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchWaitForPlayersTest, "DF.Unit.Match.WaitForPlayers", DFMatchPhaseTest::Flags)
bool FDFMatchWaitForPlayersTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchPhaseTest;
	FDFMatchPhaseMachine M = Fresh(3, false, false, /*bWait*/ true);
	TestEqual(TEXT("a dedicated server with nobody on: the clock holds"), M.Tick(60.f, 0, Alive), EDFMatchStep::None);
	TestTrue(TEXT("still a full clock"), FMath::IsNearlyEqual(M.PhaseTimer, 8.f));
	TestEqual(TEXT("someone connects: it runs"), M.Tick(8.f, 1, Alive), EDFMatchStep::BeginWave);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchEndlessRollsOverTest, "DF.Unit.Match.EndlessRollsOver", DFMatchPhaseTest::Flags)
bool FDFMatchEndlessRollsOverTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchPhaseTest;
	FDFMatchPhaseMachine M = Fresh(2, false, /*bEndless*/ true);
	for (int32 Wave = 0; Wave < 5; ++Wave)
	{
		TestEqual(*FString::Printf(TEXT("wave %d begins"), Wave), M.Tick(8.f, 1, Alive), EDFMatchStep::BeginWave);
		TestEqual(*FString::Printf(TEXT("wave %d's index"), Wave), M.WaveIndex, Wave);
		TestEqual(*FString::Printf(TEXT("wave %d cleared: endless never ends in victory"), Wave), M.WaveCleared(Wave, Alive), EDFMatchStep::Intermission);
	}
	M.Tick(8.f, 1, Alive);
	TestEqual(TEXT("only the core ends an endless run"), M.WaveCleared(5, TOptional<int32>(0)), EDFMatchStep::Defeat);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchNoWavesIdlesTest, "DF.Unit.Match.NoWavesMapIdles", DFMatchPhaseTest::Flags)
bool FDFMatchNoWavesIdlesTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchPhaseTest;
	// A dev map (L_Dev_Empty, the smoke) has no lane graph and so no arc: the match must idle, not
	// begin wave 0 of nothing.
	FDFMatchPhaseMachine M = Fresh(0);
	TestFalse(TEXT("no clock"), M.IsClockRunning(1));
	TestEqual(TEXT("an hour later: nothing"), M.Tick(3600.f, 1, Alive), EDFMatchStep::None);
	TestEqual(TEXT("no wave"), M.WaveIndex, -1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchStaleClearTest, "DF.Unit.Match.StaleClearIgnored", DFMatchPhaseTest::Flags)
bool FDFMatchStaleClearTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchPhaseTest;
	FDFMatchPhaseMachine M = Fresh(3);
	TestEqual(TEXT("a clear before any wave is ignored"), M.WaveCleared(0, Alive), EDFMatchStep::None);
	M.Tick(8.f, 1, Alive);
	TestEqual(TEXT("a clear for another wave is ignored"), M.WaveCleared(4, Alive), EDFMatchStep::None);
	TestEqual(TEXT("the running wave is still running"), M.Phase, EDFMatchPhase::Wave);
	TestEqual(TEXT("its own clear is taken"), M.WaveCleared(0, Alive), EDFMatchStep::Intermission);
	TestEqual(TEXT("and only once"), M.WaveCleared(0, Alive), EDFMatchStep::None);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
