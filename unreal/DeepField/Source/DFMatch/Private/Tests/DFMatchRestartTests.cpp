#include "DFGameMode.h"
#include "DFMatchPhaseMachine.h"
#include "DFMatchState.h"
#include "DFTestHostGameMode.h"
#include "Economy/DFEconomyStateComponent.h"
#include "Engine/EngineBaseTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Testing/DFTestUtils.h"
#include "Waves/DFWaveDirector.h"
#include "Waves/DFWavePlan.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.Unit.Match.* (play-again) — a finished match leads to a new one. The pure half drives
// FDFMatchPhaseMachine's restart clock; the world half drives ADFMatchState by hand, as
// DFMatchStateTests.cpp does, and listens on OnRestartRequested (the hook ADFGameMode binds to reload
// the map), so nothing travels: a test world has no game mode to bind it (RestartLeadReadAgainAtStartPlay
// spawns one, ADFTestHostGameMode, and unbinds it). The URL half runs the option through the engine's
// own FURL, as UEngine::Browse and the restart travel do.

namespace DFMatchRestartTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	const TOptional<int32> Alive(20);
	const TOptional<int32> Dead(0);

	FDFMatchPhaseMachine Fresh(int32 TotalWaves, float RestartSeconds, bool bLobby = false, bool bEndless = false, float LeadSeconds = 0.f)
	{
		FDFMatchPhaseMachine M;
		M.Reset(8.f, TotalWaves, bLobby, bEndless, /*bWaitForPlayers*/ false, RestartSeconds, LeadSeconds);
		return M;
	}

	/** The options string a map's game mode gets in InitGame: "?" + each option (UWorld::InitializeActorsForPlay). */
	FString OptionsOf(const FURL& Url)
	{
		FString Options;
		for (const FString& Op : Url.Op)
		{
			Options += TEXT("?");
			Options += Op;
		}
		return Options;
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
		/** What the hook answers: true stands in for ADFGameMode's travel, false for a refused one. */
		bool bTravels = true;

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
			// What ADFGameMode::InitGameState binds; here it only counts, and says whether it "travelled".
			Match->OnRestartRequested.AddLambda([this](ADFMatchState*, bool& bOutUnderWay)
			{
				++Requests;
				bOutUnderWay = bOutUnderWay || bTravels;
			});
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
	TestEqual(TEXT("the clock runs out (standalone: no lead): restart"), M.Tick(0.2f, 1, Alive), EDFMatchStep::Restart);
	TestTrue(TEXT("the new match is due: the clock stays, at 0, while the host loads it"), M.IsRestartClockRunning());
	TestEqual(TEXT("never below 0"), M.RestartTimer, 0.f);
	TestEqual(TEXT("and it asks once: a minute later, nothing"), M.Tick(60.f, 1, Alive), EDFMatchStep::None);
	TestTrue(TEXT("still due"), M.IsRestartClockRunning() && M.RestartTimer == 0.f);
	TestEqual(TEXT("still over while the host travels"), M.Phase, EDFMatchPhase::Victory);
	TestEqual(TEXT("a late clear changes nothing"), M.WaveCleared(0, Alive), EDFMatchStep::None);
	// Nobody started the new match (the travel was refused): the clock stops, and nothing asks again.
	M.AbandonRestart();
	TestFalse(TEXT("an abandoned restart stops the clock"), M.IsRestartClockRunning());
	TestEqual(TEXT("and is not asked for again"), M.Tick(60.f, 1, Alive), EDFMatchStep::None);
	TestFalse(TEXT("nor does the clock come back"), M.IsRestartClockRunning());

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
	M.AbandonRestart();
	TestEqual(TEXT("abandoning a restart mid-match does nothing"), M.Tick(8.f, 1, Alive), EDFMatchStep::BeginWave);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchRestartLeadTest, "DF.Unit.Match.RestartAskedTheTravelsLeadEarly", DFMatchRestartTest::Flags)
bool FDFMatchRestartLeadTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchRestartTest;
	// A listen host: the map switches ServerTravelPause (4 s) after ServerTravel, so the request goes out
	// 4 s before the countdown ends and the new match starts when the HUD reaches 0, not 4 s after it.
	FDFMatchPhaseMachine Host = Fresh(1, 15.f, false, false, /*LeadSeconds*/ 4.f);
	Host.Tick(8.f, 1, Alive);
	TestEqual(TEXT("victory"), Host.WaveCleared(0, Alive), EDFMatchStep::Victory);
	TestTrue(TEXT("the countdown is the whole delay"), FMath::IsNearlyEqual(Host.RestartTimer, 15.f));
	TestEqual(TEXT("10.9 s on (4.1 s left): not yet"), Host.Tick(10.9f, 1, Alive), EDFMatchStep::None);
	TestEqual(TEXT("down to the lead: restart"), Host.Tick(0.2f, 1, Alive), EDFMatchStep::Restart);
	TestTrue(TEXT("the countdown goes on while the travel waits out its pause"), Host.IsRestartClockRunning());
	TestTrue(TEXT("3.9 s left"), FMath::IsNearlyEqual(Host.RestartTimer, 3.9f, 1e-3f));
	TestEqual(TEXT("asked once"), Host.Tick(2.f, 1, Alive), EDFMatchStep::None);
	TestEqual(TEXT("the lead runs out: nothing more to ask"), Host.Tick(2.f, 1, Alive), EDFMatchStep::None);
	TestTrue(TEXT("due, when the host's map switches"), Host.IsRestartClockRunning() && Host.RestartTimer == 0.f);

	// A delay shorter than the lead cannot be kept: the countdown shows the real one and asks at once.
	FDFMatchPhaseMachine Quick = Fresh(1, 2.f, false, false, /*LeadSeconds*/ 4.f);
	Quick.Tick(8.f, 1, Alive);
	TestEqual(TEXT("victory"), Quick.WaveCleared(0, Alive), EDFMatchStep::Victory);
	TestTrue(TEXT("?playagain=2 on a listen host counts 4 s"), FMath::IsNearlyEqual(Quick.RestartTimer, 4.f));
	TestEqual(TEXT("and asks on the next frame"), Quick.Tick(0.016f, 1, Alive), EDFMatchStep::Restart);

	// 0 stays "never", whatever the lead.
	FDFMatchPhaseMachine Never = Fresh(1, 0.f, false, false, /*LeadSeconds*/ 4.f);
	Never.Tick(8.f, 1, Alive);
	Never.WaveCleared(0, Alive);
	TestFalse(TEXT("no restart clock with the restart off"), Never.IsRestartClockRunning());

	// The lead is the engine's: a networked host waits out ServerTravelPause, a standalone game switches at once.
	TestEqual(TEXT("standalone: no lead"), ADFGameMode::RestartLeadFor(NM_Standalone, 4.f), 0.f);
	TestEqual(TEXT("listen host: the travel pause"), ADFGameMode::RestartLeadFor(NM_ListenServer, 4.f), 4.f);
	TestEqual(TEXT("dedicated server: the travel pause"), ADFGameMode::RestartLeadFor(NM_DedicatedServer, 4.f), 4.f);
	TestEqual(TEXT("never negative"), ADFGameMode::RestartLeadFor(NM_ListenServer, -1.f), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchPlayAgainOptionTest, "DF.Unit.Match.PlayAgainOptionSurvivesTravel", DFMatchRestartTest::Flags)
bool FDFMatchPlayAgainOptionTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchRestartTest;
	// Why the key is not "restart": the engine reads such an option as its own command (UEngine::Browse
	// then loads the last URL instead of this one), and restart=30 is such an option.
	FURL Clash(nullptr, TEXT("/Game/DF/Maps/Testlane/L_Testlane?listen?restart=30"), TRAVEL_Absolute);
	TestTrue(TEXT("restart=30 is the engine's restart command"), Clash.HasOption(TEXT("restart")));

	FURL Host(nullptr, TEXT("/Game/DF/Maps/Testlane/L_Testlane?listen?playagain=30"), TRAVEL_Absolute);
	TestTrue(TEXT("the host's URL parses"), Host.Valid != 0);
	TestFalse(TEXT("playagain=30 is not: Browse loads this URL"), Host.HasOption(TEXT("restart")));
	const FDFMatchSettings First = ADFGameMode::ParseMatchSettings(OptionsOf(Host));
	TestEqual(TEXT("the first match waits 30 s"), First.RestartSeconds, 30.f);

	// ADFGameMode::RestartMatch travels to "?Restart" relative to the last URL; AGameModeBase::ProcessServerTravel
	// and UEngine::TickWorldTravel build this URL, and Browse, seeing restart, reloads the last URL itself.
	FURL Next(&Host, TEXT("?Restart"), TRAVEL_Relative);
	TestTrue(TEXT("the restart travel is the engine's restart"), Next.HasOption(TEXT("restart")));
	TestEqual(TEXT("the map is the same"), Next.Map, Host.Map);
	TestEqual(TEXT("the delay rides along"), FString(Next.GetOption(TEXT("playagain="), TEXT(""))), FString(TEXT("30")));
	TestTrue(TEXT("and so does ?listen"), Next.HasOption(TEXT("listen")));
	TestEqual(TEXT("the bare Restart does not read as a delay"), ADFGameMode::ParseMatchSettings(OptionsOf(Next)).RestartSeconds, 30.f);
	FURL Third(&Next, TEXT("?Restart"), TRAVEL_Relative);
	TestEqual(TEXT("every later match waits 30 s too"), ADFGameMode::ParseMatchSettings(OptionsOf(Third)).RestartSeconds, 30.f);

	// The option's other values, and the other options beside it.
	TestEqual(TEXT("absent: the match state's default"), ADFGameMode::ParseMatchSettings(TEXT("?listen")).RestartSeconds, -1.f);
	TestEqual(TEXT("absent with the engine's Restart: still the default"), ADFGameMode::ParseMatchSettings(TEXT("?listen?Restart")).RestartSeconds, -1.f);
	TestEqual(TEXT("?playagain=0: never"), ADFGameMode::ParseMatchSettings(TEXT("?playagain=0")).RestartSeconds, 0.f);
	TestEqual(TEXT("never negative"), ADFGameMode::ParseMatchSettings(TEXT("?playagain=-5")).RestartSeconds, 0.f);
	const FDFMatchSettings All = ADFGameMode::ParseMatchSettings(TEXT("?seed=7?lobby?endless?intermission=3?playagain=5?wavesmap=foundry"));
	TestEqual(TEXT("seed"), static_cast<int32>(All.Seed), 7);
	TestTrue(TEXT("lobby and endless"), All.bLobby && All.bEndless);
	TestEqual(TEXT("intermission"), All.IntermissionSeconds, 3.f);
	TestEqual(TEXT("play again"), All.RestartSeconds, 5.f);
	TestEqual(TEXT("waves map"), All.MapId, FName(TEXT("foundry")));
	TestEqual(TEXT("the lead is not a URL option"), All.RestartLeadSeconds, 0.f);
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

	// RestartSeconds 0 (?playagain=0): the banner stays up for good.
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
	TestEqual(TEXT("asked once when it runs out (standalone: no lead)"), F.Requests, 1);
	// The travel is under way: the new match is due, and every HUD reads "Starting new match…" until it loads.
	TestTrue(TEXT("still pending while the host travels"), F.Match->IsRestartPending());
	TestEqual(TEXT("with no seconds left"), F.Match->GetPhaseSecondsLeft(), 0.f);
	F.Match->AdvanceMatch(60.f);
	F.Match->AdvanceMatch(60.f);
	TestEqual(TEXT("never twice, however long the travel takes"), F.Requests, 1);
	TestTrue(TEXT("and still due"), F.Match->IsRestartPending() && F.Match->GetPhaseSecondsLeft() == 0.f);

	// The request was refused (the session or the engine said no): no new match is coming, so no countdown.
	{
		FFixture Refused;
		Refused.bTravels = false;
		if (!Refused.Init(*this, FDFMatchSettings()))
		{
			return false;
		}
		Refused.PlayWave();
		Refused.Match->AdvanceMatch(15.1f);
		TestEqual(TEXT("asked"), Refused.Requests, 1);
		TestFalse(TEXT("nobody started it: nothing pending"), Refused.Match->IsRestartPending());
		TestFalse(TEXT("the clock is down"), Refused.Match->IsPhaseClockRunning());
		TestEqual(TEXT("still over, on the banner"), Refused.Match->GetPhase(), EDFMatchPhase::Victory);
		Refused.Match->AdvanceMatch(60.f);
		TestEqual(TEXT("and not asked again"), Refused.Requests, 1);
	}

	// ?playagain=<seconds> wins over the class default; the defeat path runs the same clock.
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
		TestEqual(TEXT("?playagain=3: 3 s to the new match"), Short.Match->GetPhaseSecondsLeft(), 3.f, 0.01f);
		Short.Match->AdvanceMatch(3.1f);
		TestEqual(TEXT("asked after 3 s"), Short.Requests, 1);
	}

	// A listen host: asked the travel pause early, and the countdown goes on to the moment the map switches.
	{
		FFixture Hosted;
		FDFMatchSettings Settings;
		Settings.RestartLeadSeconds = 4.f;
		if (!Hosted.Init(*this, Settings))
		{
			return false;
		}
		Hosted.PlayWave();
		TestEqual(TEXT("15 s to the new match"), Hosted.Match->GetPhaseSecondsLeft(), 15.f, 0.01f);
		Hosted.Match->AdvanceMatch(10.9f);
		TestEqual(TEXT("4.1 s left: not asked yet"), Hosted.Requests, 0);
		Hosted.Match->AdvanceMatch(0.2f);
		TestEqual(TEXT("down to the travel pause: asked"), Hosted.Requests, 1);
		TestTrue(TEXT("the countdown still runs"), Hosted.Match->IsRestartPending());
		TestEqual(TEXT("3.9 s to go, not 0: the HUD keeps counting while the travel waits"), Hosted.Match->GetPhaseSecondsLeft(), 3.9f, 0.06f);
		Hosted.Match->AdvanceMatch(4.f);
		TestTrue(TEXT("then due"), Hosted.Match->IsRestartPending() && Hosted.Match->GetPhaseSecondsLeft() == 0.f);
		TestEqual(TEXT("asked once"), Hosted.Requests, 1);
	}

	// ?playagain=0: the banner stays up, nobody is asked.
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchRestartLeadLateTest, "DF.Unit.Match.RestartLeadLearnedAfterConfigure", DFMatchRestartTest::Flags)
bool FDFMatchRestartLeadLateTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchRestartTest;
	// A PIE host listens after InitGame, so its lead (0 there) arrives at StartPlay, after the match state
	// was configured. The lead moves; the match does not start over.
	FDFMatchPhaseMachine M = Fresh(2, 15.f);
	M.Tick(3.f, 1, Alive);
	M.SetRestartLead(4.f);
	TestEqual(TEXT("the lead is set"), M.RestartLeadSeconds, 4.f);
	TestEqual(TEXT("still in the first intermission"), M.Phase, EDFMatchPhase::Intermission);
	TestEqual(TEXT("before the first wave"), M.WaveIndex, -1);
	TestTrue(TEXT("with 5 s left, not a fresh 8"), FMath::IsNearlyEqual(M.PhaseTimer, 5.f));
	TestEqual(TEXT("the clock runs on: 5 s later the first wave"), M.Tick(5.f, 1, Alive), EDFMatchStep::BeginWave);
	M.WaveCleared(0, Alive);
	M.Tick(8.f, 1, Alive);
	TestEqual(TEXT("the last wave cleared: victory"), M.WaveCleared(1, Alive), EDFMatchStep::Victory);
	TestEqual(TEXT("10.9 s on: not yet"), M.Tick(10.9f, 1, Alive), EDFMatchStep::None);
	TestEqual(TEXT("asked at the lead, 4 s before the end, not at 0"), M.Tick(0.2f, 1, Alive), EDFMatchStep::Restart);
	M.SetRestartLead(-1.f);
	TestEqual(TEXT("never negative"), M.RestartLeadSeconds, 0.f);

	// Learned once the restart clock runs (not in play, where StartPlay comes first): a clock shorter than
	// the lead is raised to it, as Finish would have started it, so the countdown still ends with the switch.
	FDFMatchPhaseMachine Late = Fresh(1, 15.f);
	Late.Tick(8.f, 1, Alive);
	Late.WaveCleared(0, Alive);
	Late.Tick(13.f, 1, Alive);
	Late.SetRestartLead(4.f);
	TestTrue(TEXT("2 s left becomes 4"), FMath::IsNearlyEqual(Late.RestartTimer, 4.f));
	TestEqual(TEXT("and asks on the next frame"), Late.Tick(0.016f, 1, Alive), EDFMatchStep::Restart);
	Late.SetRestartLead(10.f);
	TestTrue(TEXT("once asked, the clock is left alone"), FMath::IsNearlyEqual(Late.RestartTimer, 4.f - 0.016f, 1e-3f));

	// The match state: two matches side by side, one told its lead after ConfigureMatch.
	FFixture Standalone;
	FFixture Hosted;
	if (!Standalone.Init(*this, FDFMatchSettings()) || !Hosted.Init(*this, FDFMatchSettings()))
	{
		return false;
	}
	Standalone.Match->AdvanceMatch(1.f);
	Hosted.Match->AdvanceMatch(1.f);
	Hosted.Match->SetRestartLead(4.f);
	TestEqual(TEXT("the settings carry the lead"), Hosted.Match->GetSettings().RestartLeadSeconds, 4.f);
	TestEqual(TEXT("so does the machine"), Hosted.Match->GetPhaseMachine().RestartLeadSeconds, 4.f);
	TestEqual(TEXT("still in the first intermission"), Hosted.Match->GetPhase(), EDFMatchPhase::Intermission);
	TestEqual(TEXT("1 s of it left, not a fresh 2"), Hosted.Match->GetPhaseSecondsLeft(), 1.f, 0.01f);
	Standalone.PlayWave();
	Hosted.PlayWave();
	TestEqual(TEXT("victory"), Hosted.Match->GetPhase(), EDFMatchPhase::Victory);
	TestEqual(TEXT("15 s to the new match"), Hosted.Match->GetPhaseSecondsLeft(), 15.f, 0.01f);
	Standalone.Match->AdvanceMatch(11.1f);
	Hosted.Match->AdvanceMatch(11.1f);
	TestEqual(TEXT("with no lead, not asked 3.9 s before the end"), Standalone.Requests, 0);
	TestEqual(TEXT("with the lead learned late, asked"), Hosted.Requests, 1);
	TestEqual(TEXT("and the countdown goes on"), Hosted.Match->GetPhaseSecondsLeft(), 3.9f, 0.06f);
	Standalone.Match->AdvanceMatch(4.f);
	TestEqual(TEXT("the standalone one asks when the clock runs out"), Standalone.Requests, 1);
	TestEqual(TEXT("the hosted one asked once"), Hosted.Requests, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchStartPlayLeadTest, "DF.Unit.Match.RestartLeadReadAgainAtStartPlay", DFMatchRestartTest::Flags)
bool FDFMatchStartPlayLeadTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchRestartTest;
	// A PIE listen host, in PIE's order: InitGame with no net driver yet, the match state and InitGameState,
	// then the listen, then StartPlay, which begins play (the match state's BeginPlay configures it again).
	// The game mode is spawned deferred so InitGame comes before its PreInitializeComponents, as in
	// InitializeActorsForPlay. What StartPlay does is ADFGameMode's; only the listen is the test host's.
	FDFTestWorld World;
	ON_SCOPE_EXIT
	{
		// StartPlay began play: end it before FDFTestWorld tears the world down (CleanupWorld warns otherwise).
		if (UWorld* W = World.GetWorld(); W && W->HasBegunPlay())
		{
			W->EndPlay(EEndPlayReason::Quit);
		}
	};
	FActorSpawnParameters Params;
	Params.bDeferConstruction = true;
	ADFTestHostGameMode* Mode = World.GetWorld()->SpawnActor<ADFTestHostGameMode>(ADFTestHostGameMode::StaticClass(), FTransform::Identity, Params);
	if (!TestNotNull(TEXT("game mode"), Mode))
	{
		return false;
	}
	FString Error;
	Mode->InitGame(TEXT("L_Testlane"), TEXT("?listen?intermission=2"), Error);
	TestEqual(TEXT("InitGame, before the listen: no lead (what a PIE host had)"), Mode->GetMatchSettings().RestartLeadSeconds, 0.f);
	Mode->FinishSpawning(FTransform::Identity);
	ADFMatchState* Match = Mode->GetGameState<ADFMatchState>();
	if (!TestNotNull(TEXT("the game mode spawned its match state"), Match))
	{
		return false;
	}
	TestEqual(TEXT("InitGameState configured it with no lead"), Match->GetPhaseMachine().RestartLeadSeconds, 0.f);
	TestFalse(TEXT("and it has not begun play"), Match->HasActorBegunPlay());

	// The director its BeginPlay would make from the level's lane graph (a test world has none): one wave.
	ADFWaveDirector* Director = World.SpawnActor<ADFWaveDirector>();
	if (!TestNotNull(TEXT("director"), Director))
	{
		return false;
	}
	if (!TestTrue(*FString::Printf(TEXT("fixture tables configure (%s)"), *Error), Director->ConfigureWithTables(7u, Tables(), Error)))
	{
		return false;
	}
	Match->UseWaveDirector(Director);
	// The game mode's own binding travels, and a test world has nowhere to go: count the requests instead.
	TestEqual(TEXT("InitGameState bound the game mode's travel"), Match->OnRestartRequested.RemoveAll(Mode), 1);
	int32 Requests = 0;
	Match->OnRestartRequested.AddLambda([&Requests](ADFMatchState*, bool& bOutUnderWay)
	{
		++Requests;
		bOutUnderWay = true;
	});

	Mode->bListening = true;
	Mode->StartPlay();
	TestTrue(TEXT("StartPlay began play: the match state's BeginPlay has configured it again"), Match->HasActorBegunPlay());
	TestEqual(TEXT("the game mode's settings carry the lead"), Mode->GetMatchSettings().RestartLeadSeconds, 4.f);
	TestEqual(TEXT("so do the match state's, through its BeginPlay"), Match->GetSettings().RestartLeadSeconds, 4.f);
	TestEqual(TEXT("and its phase machine's"), Match->GetPhaseMachine().RestartLeadSeconds, 4.f);
	TestEqual(TEXT("a one-wave match"), Match->GetTotalWaves(), 1);

	Match->AdvanceMatch(2.1f);
	Director->Tick(60.f);
	Director->NotifyEnemyRemoved(Director->GetAliveCount());
	TestEqual(TEXT("the wave cleared: victory"), Match->GetPhase(), EDFMatchPhase::Victory);
	TestEqual(TEXT("15 s to the new match"), Match->GetPhaseSecondsLeft(), 15.f, 0.01f);
	Match->AdvanceMatch(10.9f);
	TestEqual(TEXT("4.1 s left: not asked yet"), Requests, 0);
	Match->AdvanceMatch(0.2f);
	TestEqual(TEXT("down to the travel pause: asked, 4 s early, not at 0"), Requests, 1);
	TestEqual(TEXT("and the countdown goes on to the switch"), Match->GetPhaseSecondsLeft(), 3.9f, 0.06f);
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
