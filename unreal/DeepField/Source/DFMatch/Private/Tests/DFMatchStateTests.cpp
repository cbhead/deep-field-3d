#include "DFGameplayTags.h"
#include "DFMatchState.h"
#include "DFMatchTestLives.h"
#include "Misc/AutomationTest.h"
#include "Testing/DFTestUtils.h"
#include "Waves/DFWaveDirector.h"
#include "Waves/DFWavePlan.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.Unit.Match.* (world) — ADFMatchState driving a real ADFWaveDirector and sending the Wave*
// messages. Both actors are driven by hand: a test world has no game mode, so spawned actors never
// begin play and never tick (the same reason DFWaveDirectorTests ticks its director itself). That
// also keeps BeginPlay's director lookup out of the way: the test hands the state its director.

namespace DFMatchStateTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** Two waves on one lane: four grunts, then three grunts and two lurkers. */
	FDFWavePlanTables Tables()
	{
		FDFWavePlanTables T;
		T.MapId = TEXT("matchFixture");
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
		T.Enemies.Add(TEXT("lurker"));
		T.Waves.AddDefaulted(2);
		T.Waves[0].Add({ TEXT("grunt"), 4, 20, 0, TEXT("ground") });
		T.Waves[1].Add({ TEXT("grunt"), 3, 15, 0, TEXT("ground") });
		T.Waves[1].Add({ TEXT("lurker"), 2, 30, 10, TEXT("ground") });
		return T;
	}

	/** A match state + director pair on the fixture tables, 2 s intermissions. */
	struct FFixture
	{
		FDFTestWorld World;
		ADFWaveDirector* Director = nullptr;
		ADFMatchState* Match = nullptr;

		bool Init(FAutomationTestBase& Test, bool bEndless = false)
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
			FDFMatchSettings Settings;
			Settings.IntermissionSeconds = 2.f;
			Settings.bEndless = bEndless;
			Match->ConfigureMatch(Settings);
			Match->UseWaveDirector(Director);
			return true;
		}

		/** Release everything due and report every body gone (killed or leaked, the same to the wave). */
		void FinishWave() const
		{
			Director->Tick(60.f);
			Director->NotifyEnemyRemoved(Director->GetAliveCount());
		}
	};

	TArray<FGameplayTag> TagsOf(const FDFMessageCapture& Seen)
	{
		TArray<FGameplayTag> Out;
		for (const FDFMessageCapture::FRecord& Record : Seen.GetRecords())
		{
			Out.Add(Record.Tag);
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchStateDrivesDirectorTest, "DF.Unit.Match.StateDrivesDirector", DFMatchStateTest::Flags)
bool FDFMatchStateDrivesDirectorTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchStateTest;
	FFixture F;
	if (!F.Init(*this))
	{
		return false;
	}
	FDFMessageCapture Seen(F.World.MessageBus(), DFTags::Message);

	TestEqual(TEXT("the arc's length comes from the director"), F.Match->GetTotalWaves(), 2);
	TestEqual(TEXT("intermission first"), F.Match->GetPhase(), EDFMatchPhase::Intermission);
	TestTrue(TEXT("the clock runs"), F.Match->IsPhaseClockRunning());

	F.Match->AdvanceMatch(1.f);
	TestFalse(TEXT("1 s into a 2 s intermission: no wave"), F.Director->IsWaveActive());
	F.Match->AdvanceMatch(1.1f);
	TestTrue(TEXT("the clock ran out: the director is running wave 0"), F.Director->IsWaveActive());
	TestEqual(TEXT("the director's wave is the match's"), F.Director->GetWaveIndex(), 0);
	TestEqual(TEXT("phase Wave"), F.Match->GetPhase(), EDFMatchPhase::Wave);
	TestFalse(TEXT("no countdown during a wave"), F.Match->IsPhaseClockRunning());

	F.Director->Tick(60.f);
	F.Match->AdvanceMatch(0.f);
	TestEqual(TEXT("four grunts alive, none left to release"), F.Match->GetEnemiesRemaining(), 4);
	F.Director->NotifyEnemyRemoved(4);
	TestEqual(TEXT("the director's clear put the match in intermission"), F.Match->GetPhase(), EDFMatchPhase::Intermission);
	TestEqual(TEXT("nothing remaining"), F.Match->GetEnemiesRemaining(), 0);

	F.Match->AdvanceMatch(2.1f);
	TestEqual(TEXT("wave 1 on the director"), F.Director->GetWaveIndex(), 1);
	F.FinishWave();
	TestEqual(TEXT("the last authored wave cleared: victory"), F.Match->GetPhase(), EDFMatchPhase::Victory);

	const TArray<FGameplayTag> Expected = {
		DFTags::Message_WaveStarted.GetTag(), DFTags::Message_WaveCleared.GetTag(), DFTags::Message_Intermission.GetTag(),
		DFTags::Message_WaveStarted.GetTag(), DFTags::Message_WaveCleared.GetTag(), DFTags::Message_Victory.GetTag(),
	};
	const TArray<FGameplayTag> Got = TagsOf(Seen);
	TestEqual(TEXT("six messages"), Got.Num(), Expected.Num());
	for (int32 i = 0; i < FMath::Min(Got.Num(), Expected.Num()); ++i)
	{
		TestEqual(*FString::Printf(TEXT("message %d"), i), Got[i], Expected[i]);
	}
	if (Seen.GetRecords().Num() > 0)
	{
		const FDFMsg_Wave* First = Seen.GetRecords()[0].Payload.GetPtr<FDFMsg_Wave>();
		if (TestNotNull(TEXT("WaveStarted carries the director's description"), First))
		{
			TestEqual(TEXT("of wave 0"), First->WaveIndex, 0);
			TestEqual(TEXT("of a two-wave arc"), First->TotalWaves, 2);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchStateLastLeakIsDefeatTest, "DF.Unit.Match.StateLastLeakIsDefeat", DFMatchStateTest::Flags)
bool FDFMatchStateLastLeakIsDefeatTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchStateTest;
	FFixture F;
	if (!F.Init(*this))
	{
		return false;
	}
	// WS-06's economy component stands in here: lives the test sets by hand (IDFMatchLivesSource).
	UDFMatchTestLives* Lives = NewObject<UDFMatchTestLives>(F.Match);
	Lives->RegisterComponent();
	FDFMessageCapture Seen(F.World.MessageBus(), DFTags::Message);

	F.Match->AdvanceMatch(2.1f);
	F.FinishWave();
	F.Match->AdvanceMatch(2.1f);
	TestEqual(TEXT("into the last wave"), F.Director->GetWaveIndex(), 1);
	F.Director->Tick(60.f);

	// The leak takes its lives first, then the body is reported gone (the order DFMatchSeams.h asks of
	// the economy): the report clears the wave with the core already at zero.
	Lives->Lives = 0;
	F.Director->NotifyEnemyRemoved(F.Director->GetAliveCount());
	TestEqual(TEXT("a last enemy that leaks the core to zero is a defeat"), F.Match->GetPhase(), EDFMatchPhase::Defeat);

	bool bVictory = false;
	bool bDefeat = false;
	for (const FDFMessageCapture::FRecord& Record : Seen.GetRecords())
	{
		bVictory |= Record.Tag == DFTags::Message_Victory.GetTag();
		bDefeat |= Record.Tag == DFTags::Message_Defeat.GetTag();
	}
	TestTrue(TEXT("Defeat was sent"), bDefeat);
	TestFalse(TEXT("Victory was not"), bVictory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchStateDefeatStopsWaveTest, "DF.Unit.Match.DefeatStopsTheWave", DFMatchStateTest::Flags)
bool FDFMatchStateDefeatStopsWaveTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchStateTest;
	FFixture F;
	if (!F.Init(*this))
	{
		return false;
	}
	UDFMatchTestLives* Lives = NewObject<UDFMatchTestLives>(F.Match);
	Lives->RegisterComponent();
	F.Match->AdvanceMatch(2.1f);
	TestTrue(TEXT("wave 0 running"), F.Director->IsWaveActive());
	Lives->Lives = 0;
	F.Match->AdvanceMatch(0.016f);
	TestEqual(TEXT("the core fell mid-wave: defeat"), F.Match->GetPhase(), EDFMatchPhase::Defeat);
	TestFalse(TEXT("and the director stopped releasing"), F.Director->IsWaveActive());
	F.Match->AdvanceMatch(60.f);
	TestFalse(TEXT("an over match starts nothing"), F.Director->IsWaveActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchStateEndlessTest, "DF.Unit.Match.StateEndlessPastTheArc", DFMatchStateTest::Flags)
bool FDFMatchStateEndlessTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchStateTest;
	FFixture F;
	if (!F.Init(*this, /*bEndless*/ true))
	{
		return false;
	}
	for (int32 Wave = 0; Wave < 3; ++Wave)
	{
		F.Match->AdvanceMatch(2.1f);
		TestEqual(*FString::Printf(TEXT("wave %d on the director"), Wave), F.Director->GetWaveIndex(), Wave);
		F.FinishWave();
		TestEqual(*FString::Printf(TEXT("wave %d cleared: back to intermission"), Wave), F.Match->GetPhase(), EDFMatchPhase::Intermission);
	}
	TestTrue(TEXT("wave 2 (the arc's first lap) scaled its threat past wave 0's"), F.Match->GetThreat() > 1.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
