#include "DFMatchPhaseMachine.h"
#include "DFMatchState.h"
#include "Economy/DFEconomyStateComponent.h"
#include "Misc/AutomationTest.h"
#include "Testing/DFTestUtils.h"
#include "Waves/DFWaveDirector.h"
#include "Waves/DFWavePlan.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.Unit.Match.* (play-again) — a finished match leads to a new one. The pure half drives
// FDFMatchPhaseMachine's restart clock; the world half drives ADFMatchState by hand, as
// DFMatchStateTests.cpp does, and listens on OnRestartRequested (the hook ADFGameMode binds to reload
// the map), so nothing travels: a test world has no game mode to bind it.

namespace DFMatchRestartTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	const TOptional<int32> Alive(20);
	const TOptional<int32> Dead(0);

	FDFMatchPhaseMachine Fresh(int32 TotalWaves, float RestartSeconds, bool bLobby = false, bool bEndless = false)
	{
		FDFMatchPhaseMachine M;
		M.Reset(8.f, TotalWaves, bLobby, bEndless, /*bWaitForPlayers*/ false, RestartSeconds);
		return M;
	}

	/** One wave on one lane: two grunts. */
	FDFWavePlanTables Tables()
	{
		FDFWavePlanTables T;
		T.MapId = TEXT("restartFixture");
		T.TickHz = 30.f;
		T.Dials.CountScalePerExtraPlayer = 0.6f;
		T.Dials.HpScalePerExtraPlayer = 0.15f;
		T.Dials.EndlessCountGrowthPerLap = 1.15f;
		T.Dials.HpGrowth = 1.22f;
		T.Dials.EndlessHpGrowth = 1.15f;
		T.Dials.BountyScale = 1.f;
		T.Dials.BountyGrowth = 1.15f;
		T.Dials.ScrapGrowth = 1.15f;
		T.Enemies.Add(TEXT("grunt"));
		T.Waves.AddDefaulted(1);
		T.Waves[0].Add({ TEXT("grunt"), 2, 20, 0, TEXT("ground") });
		return T;
	}

	/** A match state + director on the one-wave tables, 2 s intermissions, counting restart requests. */
	struct FFixture
	{
		FDFTestWorld World;
		ADFWaveDirector* Director = nullptr;
		ADFMatchState* Match = nullptr;
		int32 Requests = 0;

		bool Init(FAutomationTestBase& Test, const FDFMatchSettings& InSettings)
		{
			Director = World.SpawnActor<ADFWaveDirector>();
			Match = World.SpawnActor<ADFMatchState>();
			if (!Test.TestNotNull(TEXT("director"), Director) || !Test.TestNotNull(TEXT("match state"), Match))
			{
				return false;
			}
			FString Error;
			if (!Test.TestTrue(*FString::Printf(TEXT("fixture tables configure (%s)"), *Error), Director->ConfigureWithTables(7u, Tables(), Error)))
			{
				return false;
			}
			FDFMatchSettings Settings = InSettings;
			Settings.IntermissionSeconds = 2.f;
			Match->ConfigureMatch(Settings);
			Match->UseWaveDirector(Director);
			// What ADFGameMode::InitGameState binds; here it only counts.
			Match->OnRestartRequested.AddLambda([this](ADFMatchState*) { ++Requests; });
			return true;
		}

		/** Into the wave, release it, and report every body gone (killed or leaked, the same to the wave). */
		void PlayWave() const
		{
			Match->AdvanceMatch(2.1f);
			Director->Tick(60.f);
			Director->NotifyEnemyRemoved(Director->GetAliveCount());
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchRestartClockTest, "DF.Unit.Match.RestartAfterVictoryOrDefeat", DFMatchRestartTest::Flags)
bool FDFMatchRestartClockTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchRestartTest;
	FDFMatchPhaseMachine M = Fresh(1, 15.f);
	TestFalse(TEXT("no restart clock while the match runs"), M.IsRestartClockRunning());
	M.Tick(8.f, 1, Alive);
	TestFalse(TEXT("nor during a wave"), M.IsRestartClockRunning());
	TestEqual(TEXT("the last wave cleared: victory"), M.WaveCleared(0, Alive), EDFMatchStep::Victory);
	TestTrue(TEXT("victory starts the restart clock"), M.IsRestartClockRunning());
	TestTrue(TEXT("at the full delay"), FMath::IsNearlyEqual(M.RestartTimer, 15.f));
	TestEqual(TEXT("14.9 s later: not yet"), M.Tick(14.9f, 1, Alive), EDFMatchStep::None);
	TestTrue(TEXT("0.1 s left"), FMath::IsNearlyEqual(M.RestartTimer, 0.1f, 1e-3f));
	TestEqual(TEXT("the clock runs out: restart"), M.Tick(0.2f, 1, Alive), EDFMatchStep::Restart);
	TestFalse(TEXT("the clock stops once it has asked"), M.IsRestartClockRunning());
	TestEqual(TEXT("and it asks once: a minute later, nothing"), M.Tick(60.f, 1, Alive), EDFMatchStep::None);
	TestEqual(TEXT("still over while the host travels"), M.Phase, EDFMatchPhase::Victory);
	TestEqual(TEXT("a late clear changes nothing"), M.WaveCleared(0, Alive), EDFMatchStep::None);

	// Defeat restarts too, whoever is connected: a host left alone still gets a new match.
	FDFMatchPhaseMachine Lost = Fresh(3, 15.f);
	Lost.Tick(8.f, 1, Alive);
	TestEqual(TEXT("the core falls mid-wave: defeat"), Lost.Tick(0.016f, 1, Dead), EDFMatchStep::Defeat);
	TestTrue(TEXT("defeat starts the restart clock"), Lost.IsRestartClockRunning());
	TestEqual(TEXT("with nobody connected it still runs out"), Lost.Tick(15.f, 0, Dead), EDFMatchStep::Restart);

	// A cleared wave with the core at zero is a defeat, and restarts the same way.
	FDFMatchPhaseMachine Leaked = Fresh(1, 15.f);
	Leaked.Tick(8.f, 1, Alive);
	TestEqual(TEXT("the last leak: defeat"), Leaked.WaveCleared(0, Dead), EDFMatchStep::Defeat);
	TestTrue(TEXT("the restart clock runs"), Leaked.IsRestartClockRunning());

	// A reset (the next match, or a reconfigure before play) clears it all.
	M.Reset(8.f, 1, false, false, false, 15.f);
	TestFalse(TEXT("a fresh match has no restart clock"), M.IsRestartClockRunning());
	TestFalse(TEXT("and has not asked"), M.bRestartSent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchNoRestartMidMatchTest, "DF.Unit.Match.NoRestartInLobbyOrEndlessRun", DFMatchRestartTest::Flags)
bool FDFMatchNoRestartMidMatchTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchRestartTest;
	// The lobby: the party has not launched, so there is no match to follow. Even a lobby whose lives
	// are somehow gone (Tick checks lives in any phase) does not restart.
	FDFMatchPhaseMachine Lobby = Fresh(3, 15.f, /*bLobby*/ true);
	TestEqual(TEXT("ten minutes in the lobby: nothing"), Lobby.Tick(600.f, 2, Alive), EDFMatchStep::None);
	TestFalse(TEXT("no restart clock in the lobby"), Lobby.IsRestartClockRunning());
	TestEqual(TEXT("a lobby with no lives: defeat"), Lobby.Tick(0.f, 2, Dead), EDFMatchStep::Defeat);
	TestFalse(TEXT("but no restart clock"), Lobby.IsRestartClockRunning());
	TestEqual(TEXT("and an hour later, still nothing"), Lobby.Tick(3600.f, 2, Dead), EDFMatchStep::None);

	// Endless: the arc rolls over and never reaches Victory, so the run never restarts while it lasts.
	FDFMatchPhaseMachine Endless = Fresh(2, 15.f, false, /*bEndless*/ true);
	for (int32 Wave = 0; Wave < 5; ++Wave)
	{
		Endless.Tick(8.f, 1, Alive);
		TestEqual(*FString::Printf(TEXT("endless wave %d cleared: on to the next"), Wave), Endless.WaveCleared(Wave, Alive), EDFMatchStep::Intermission);
		TestFalse(*FString::Printf(TEXT("no restart clock after wave %d"), Wave), Endless.IsRestartClockRunning());
	}
	// Only the core ends an endless run; that is a finished match like any other.
	Endless.Tick(8.f, 1, Alive);
	TestEqual(TEXT("the core falls: the endless run is over"), Endless.WaveCleared(5, Dead), EDFMatchStep::Defeat);
	TestTrue(TEXT("and a new one follows it"), Endless.IsRestartClockRunning());

	// RestartSeconds 0 (?restart=0): the banner stays up for good.
	FDFMatchPhaseMachine Never = Fresh(1, 0.f);
	Never.Tick(8.f, 1, Alive);
	TestEqual(TEXT("victory"), Never.WaveCleared(0, Alive), EDFMatchStep::Victory);
	TestFalse(TEXT("no restart clock with the restart off"), Never.IsRestartClockRunning());
	TestEqual(TEXT("an hour later: nothing"), Never.Tick(3600.f, 1, Alive), EDFMatchStep::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchStateRestartDeadlineTest, "DF.Unit.Match.StateRestartDeadline", DFMatchRestartTest::Flags)
bool FDFMatchStateRestartDeadlineTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchRestartTest;
	FFixture F;
	if (!F.Init(*this, FDFMatchSettings()))
	{
		return false;
	}
	TestEqual(TEXT("the default delay is the match state's"), F.Match->GetPhaseMachine().RestartSeconds, F.Match->RestartDelaySeconds);
	TestEqual(TEXT("15 s"), F.Match->RestartDelaySeconds, 15.f);
	TestFalse(TEXT("no restart pending while the match runs"), F.Match->IsRestartPending());

	F.PlayWave();
	TestEqual(TEXT("the only wave cleared: victory"), F.Match->GetPhase(), EDFMatchPhase::Victory);
	// The deadline replicates in the intermission's field: every HUD reads "New match in 15s".
	TestTrue(TEXT("a restart is pending"), F.Match->IsRestartPending());
	TestTrue(TEXT("the phase clock runs"), F.Match->IsPhaseClockRunning());
	TestEqual(TEXT("15 s to the new match"), F.Match->GetPhaseSecondsLeft(), 15.f, 0.01f);

	F.Match->AdvanceMatch(5.f);
	TestEqual(TEXT("5 s on: 10 s left"), F.Match->GetPhaseSecondsLeft(), 10.f, 0.06f);
	F.Match->AdvanceMatch(9.9f);
	TestEqual(TEXT("not asked before the clock runs out"), F.Requests, 0);
	F.Match->AdvanceMatch(0.2f);
	TestEqual(TEXT("asked once when it runs out"), F.Requests, 1);
	TestFalse(TEXT("the clock is down once asked"), F.Match->IsPhaseClockRunning());
	TestFalse(TEXT("nothing pending any more"), F.Match->IsRestartPending());
	F.Match->AdvanceMatch(60.f);
	F.Match->AdvanceMatch(60.f);
	TestEqual(TEXT("never twice, however long the travel takes"), F.Requests, 1);

	// ?restart=<seconds> wins over the class default; the defeat path runs the same clock.
	{
		FFixture Short;
		FDFMatchSettings Settings;
		Settings.RestartSeconds = 3.f;
		if (!Short.Init(*this, Settings))
		{
			return false;
		}
		Short.Match->AdvanceMatch(2.1f);
		UDFEconomyStateComponent* Economy = Short.Match->GetEconomy();
		if (!TestNotNull(TEXT("the economy"), Economy))
		{
			return false;
		}
		Economy->TakeLives(Economy->GetLives());
		Short.Match->AdvanceMatch(0.016f);
		TestEqual(TEXT("the core fell: defeat"), Short.Match->GetPhase(), EDFMatchPhase::Defeat);
		TestEqual(TEXT("?restart=3: 3 s to the new match"), Short.Match->GetPhaseSecondsLeft(), 3.f, 0.01f);
		Short.Match->AdvanceMatch(3.1f);
		TestEqual(TEXT("asked after 3 s"), Short.Requests, 1);
	}

	// ?restart=0: the banner stays up, nobody is asked.
	{
		FFixture Off;
		FDFMatchSettings Settings;
		Settings.RestartSeconds = 0.f;
		if (!Off.Init(*this, Settings))
		{
			return false;
		}
		Off.PlayWave();
		TestEqual(TEXT("victory"), Off.Match->GetPhase(), EDFMatchPhase::Victory);
		TestFalse(TEXT("no countdown with the restart off"), Off.Match->IsPhaseClockRunning());
		Off.Match->AdvanceMatch(3600.f);
		TestEqual(TEXT("an hour later nobody was asked"), Off.Requests, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchStateNoRestartMidMatchTest, "DF.Unit.Match.StateNoRestartInLobbyOrEndless", DFMatchRestartTest::Flags)
bool FDFMatchStateNoRestartMidMatchTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchRestartTest;
	{
		FFixture Lobby;
		FDFMatchSettings Settings;
		Settings.bLobby = true;
		if (!Lobby.Init(*this, Settings))
		{
			return false;
		}
		Lobby.Match->AdvanceMatch(600.f);
		TestTrue(TEXT("still in the lobby"), Lobby.Match->IsLobby());
		TestFalse(TEXT("no restart pending in the lobby"), Lobby.Match->IsRestartPending());
		TestEqual(TEXT("nobody asked for a new match"), Lobby.Requests, 0);
	}
	{
		FFixture Endless;
		FDFMatchSettings Settings;
		Settings.bEndless = true;
		if (!Endless.Init(*this, Settings))
		{
			return false;
		}
		for (int32 Wave = 0; Wave < 3; ++Wave)
		{
			Endless.PlayWave();
			TestEqual(*FString::Printf(TEXT("endless wave %d cleared: intermission"), Wave), Endless.Match->GetPhase(), EDFMatchPhase::Intermission);
			TestFalse(*FString::Printf(TEXT("no restart pending after wave %d"), Wave), Endless.Match->IsRestartPending());
		}
		TestEqual(TEXT("past the arc, nobody asked for a new match"), Endless.Requests, 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
