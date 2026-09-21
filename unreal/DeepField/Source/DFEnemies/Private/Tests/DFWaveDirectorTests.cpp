#include "DFGameplayTags.h"
#include "Misc/AutomationTest.h"
#include "Testing/DFTestUtils.h"
#include "Waves/DFDetRng.h"
#include "Waves/DFWaveDirector.h"
#include "Waves/DFWavePlan.h"
#include "Waves/DFWaveSchedule.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.Unit.WaveSchedule.*, DF.Unit.WavePlan.InjectionStreams, DF.Unit.WaveDirector.* — playback of a
// plan against time, the B§1.11 stream hook, and the host-side director. The director is ticked by
// hand: a test world has no game mode, so it never begins play and would not tick a spawned actor.

namespace DFWaveDirectorTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Hz = 30.f;
	constexpr float Frame = 1.f / 60.f;

	/** Two waves on one lane: grunts (no scatter) and a stealth pair; wave 1 is a night wave. */
	FDFWavePlanTables Tables()
	{
		FDFWavePlanTables T;
		T.MapId = TEXT("directorFixture");
		T.TickHz = Hz;
		T.Dials.CountScalePerExtraPlayer = 0.6f;
		T.Dials.HpScalePerExtraPlayer = 0.15f;
		T.Dials.EndlessCountGrowthPerLap = 1.15f;
		T.Dials.HpGrowth = 1.22f;
		T.Dials.EndlessHpGrowth = 1.15f;
		T.Dials.BountyScale = 1.f;
		T.Dials.BountyGrowth = 1.15f;
		T.Dials.ScrapGrowth = 1.15f;
		T.Enemies.Add(TEXT("grunt"));
		T.Enemies.Add(TEXT("lurker")).bStealth = true;
		T.Waves.AddDefaulted(2);
		T.Waves[0].Add({ TEXT("grunt"), 4, 20, 0, TEXT("ground") });
		T.Waves[1].Add({ TEXT("grunt"), 3, 15, 0, TEXT("ground") });
		T.Waves[1].Add({ TEXT("lurker"), 2, 30, 10, TEXT("ground") });
		T.StealthWeightFactorByWave.Add(1, 1.5f);
		T.ConditionByWave.Add(1, TEXT("night"));
		return T;
	}

	FDFSpawnEntry At(int32 Tick, const TCHAR* Id = TEXT("grunt"))
	{
		FDFSpawnEntry E;
		E.DefId = Id;
		E.TickOffset = Tick;
		E.RouteId = TEXT("ground");
		return E;
	}

	/** Adds one marked body per stream, at a tick drawn from the stream's own generator. */
	class FOneExtra : public IDFWaveInjectionStream
	{
	public:
		FOneExtra(const TCHAR* InName, FName InElite) : Name(InName), Elite(InElite) {}
		virtual const TCHAR* StreamName() const override { return Name; }
		virtual void Inject(const FDFWaveInjectionContext& Context, FDFDetRng& Rng, TArray<FDFSpawnEntry>& OutAppended) const override
		{
			FDFSpawnEntry Extra = Context.Planned[Rng.NextInt(0, Context.Planned.Num())];
			Extra.Elite = Elite;
			Extra.HpFactor = Context.HpScale;
			OutAppended.Add(Extra);
		}
	private:
		const TCHAR* Name;
		FName Elite;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWaveScheduleReleaseTest, "DF.Unit.WaveSchedule.ReleasesOnTick", DFWaveDirectorTest::Flags)
bool FDFWaveScheduleReleaseTest::RunTest(const FString& Parameters)
{
	using namespace DFWaveDirectorTest;
	FDFWaveSchedule Schedule;
	Schedule.Reset({ At(0), At(15), At(15, TEXT("lurker")), At(30) }, Hz);

	TArray<FName> Released;
	auto Emit = [&Released](const FDFSpawnEntry& E) { Released.Add(E.DefId); return true; };

	TestEqual(TEXT("tick 0 is due before any time passes"), Schedule.Advance(0.f, Emit), 1);
	TestEqual(TEXT("nothing at 0.49 s (tick 14)"), Schedule.Advance(0.49f, Emit), 0);
	TestTrue(TEXT("the next is 0.01 s away"), FMath::IsNearlyEqual(Schedule.SecondsUntilNext(), 0.01f, 1e-4f));
	TestEqual(TEXT("both tick-15 entries at 0.51 s"), Schedule.Advance(0.02f, Emit), 2);
	TestEqual(TEXT("in plan order"), Released.Last(), FName(TEXT("lurker")));
	TestFalse(TEXT("one left"), Schedule.IsExhausted());
	TestEqual(TEXT("a long frame releases everything overdue"), Schedule.Advance(5.f, Emit), 1);
	TestTrue(TEXT("exhausted"), Schedule.IsExhausted());
	TestEqual(TEXT("an exhausted schedule is inert"), Schedule.Advance(1.f, Emit), 0);
	TestEqual(TEXT("four released"), Schedule.NumReleased(), 4);

	// Thirty frames of exactly one tick are thirty ticks, not twenty-nine and a rounding error.
	FDFWaveSchedule Steady;
	Steady.Reset({ At(30) }, Hz);
	int32 Count = 0;
	for (int32 I = 0; I < 30; ++I)
	{
		Count += Steady.Advance(1.f / Hz, [](const FDFSpawnEntry&) { return true; });
	}
	TestEqual(TEXT("30 frames at the tick rate reach tick 30"), Steady.CurrentTick(), static_cast<int64>(30));
	TestEqual(TEXT("and release the tick-30 entry"), Count, 1);

	// Resume: the plan is recomputed, what was already released is skipped, not re-emitted.
	FDFWaveSchedule Resumed;
	Resumed.Reset({ At(0), At(0), At(10) }, Hz);
	Resumed.SkipReleased(2);
	TestEqual(TEXT("only the unreleased entry is left"), Resumed.NumRemaining(), 1);
	TestEqual(TEXT("and nothing at tick 0 fires again"), Resumed.Advance(0.f, [](const FDFSpawnEntry&) { return true; }), 0);

	// A release that says stop: the entry it stopped on counts as released and keeps its place, and
	// the ones behind it are due immediately next time — the clock moved even though they did not.
	FDFWaveSchedule Stopping;
	Stopping.Reset({ At(0), At(0), At(0), At(0) }, Hz);
	int32 Seen = 0;
	TestEqual(TEXT("stops after the entry that said so"), Stopping.Advance(0.f, [&Seen](const FDFSpawnEntry&) { return ++Seen < 2; }), 2);
	TestEqual(TEXT("two released"), Stopping.NumReleased(), 2);
	TestEqual(TEXT("the rest come on the next advance, with no time passing"), Stopping.Advance(0.f, [](const FDFSpawnEntry&) { return true; }), 2);
	TestTrue(TEXT("exhausted"), Stopping.IsExhausted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWavePlanInjectionTest, "DF.Unit.WavePlan.InjectionStreams", DFWaveDirectorTest::Flags)
bool FDFWavePlanInjectionTest::RunTest(const FString& Parameters)
{
	using namespace DFWaveDirectorTest;
	const FDFWavePlanTables T = Tables();
	const uint32 Seed = 77u;

	const TArray<FDFSpawnEntry> Pure = FDFWavePlan::PlanWave(Seed, T, 1, 2);
	TestEqual(TEXT("no streams is the pure plan"), FDFWavePlan::PlanWave(Seed, T, 1, 2, {}).Num(), Pure.Num());

	const TSharedRef<const IDFWaveInjectionStream> Elite = MakeShared<FOneExtra>(TEXT("elite"), FName(TEXT("gilded")));
	const TSharedRef<const IDFWaveInjectionStream> Boss = MakeShared<FOneExtra>(TEXT("boss"), FName(TEXT("frame01")));
	const TArray<FDFSpawnEntry> Both = FDFWavePlan::PlanWave(Seed, T, 1, 2, { Elite, Boss });
	const TArray<FDFSpawnEntry> Swapped = FDFWavePlan::PlanWave(Seed, T, 1, 2, { Boss, Elite });
	TestEqual(TEXT("each stream added one body"), Both.Num(), Pure.Num() + 2);

	// The authored plan is untouched: strip the marked bodies and the pure plan is what is left, in order.
	TArray<FDFSpawnEntry> Stripped = Both.FilterByPredicate([](const FDFSpawnEntry& E) { return E.Elite.IsNone(); });
	bool bSame = Stripped.Num() == Pure.Num();
	for (int32 I = 0; bSame && I < Pure.Num(); ++I)
	{
		bSame = Stripped[I].DefId == Pure[I].DefId && Stripped[I].TickOffset == Pure[I].TickOffset && Stripped[I].LateralOffset == Pure[I].LateralOffset;
	}
	TestTrue(TEXT("streams never shift an authored spawn"), bSame);

	for (int32 I = 1; I < Both.Num(); ++I)
	{
		TestTrue(FString::Printf(TEXT("sorted at %d"), I), Both[I - 1].TickOffset <= Both[I].TickOffset);
	}

	// Registration order cannot change what a stream adds: each sees the pure plan and its own generator.
	auto Marked = [](const TArray<FDFSpawnEntry>& Plan, FName Elite)
	{
		const FDFSpawnEntry* Found = Plan.FindByPredicate([Elite](const FDFSpawnEntry& E) { return E.Elite == Elite; });
		return Found ? Found->TickOffset : -1;
	};
	TestEqual(TEXT("the elite lands on the same tick either way"), Marked(Both, TEXT("gilded")), Marked(Swapped, TEXT("gilded")));
	TestEqual(TEXT("and so does the boss"), Marked(Both, TEXT("frame01")), Marked(Swapped, TEXT("frame01")));

	// A stream's generator is not the wave stream.
	FDFDetRng WaveRng = FDFRngStreams::StreamFor(Seed, FDFRngStreams::Wave, 1u);
	FDFDetRng EliteRng = FDFRngStreams::StreamFor(Seed, TEXT("elite"), 1u);
	TestNotEqual(TEXT("own stream, own draws"), WaveRng.NextUInt(), EliteRng.NextUInt());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWaveDirectorReleaseTest, "DF.Unit.WaveDirector.ReleasesAndClears", DFWaveDirectorTest::Flags)
bool FDFWaveDirectorReleaseTest::RunTest(const FString& Parameters)
{
	using namespace DFWaveDirectorTest;
	FDFTestWorld World;
	ADFWaveDirector* Director = World.SpawnActor<ADFWaveDirector>();
	FString Error;
	if (!TestTrue(TEXT("configures from hand-built tables"), Director->ConfigureWithTables(9u, Tables(), Error)))
	{
		AddError(Error);
		return false;
	}

	const TArray<FDFSpawnEntry> Plan = FDFWavePlan::PlanWave(9u, Director->GetTables(), 0, 1);
	double Elapsed = 0.0;
	TArray<double> ReleasedAt;
	int32 Exhausted = 0;
	TArray<int32> Cleared;
	Director->OnSpawnRequested.AddLambda([&ReleasedAt, &Elapsed](const FDFSpawnEntry&) { ReleasedAt.Add(Elapsed); });
	Director->OnWaveSpawnsExhausted.AddLambda([&](int32) { ++Exhausted; });
	Director->OnWaveCleared.AddLambda([&](int32 Wave) { Cleared.Add(Wave); });

	TestTrue(TEXT("wave 0 begins"), Director->BeginWave(0, 1));
	AddExpectedMessage(TEXT("BeginWave"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	TestFalse(TEXT("a second BeginWave while one runs is refused"), Director->BeginWave(1, 1));

	for (int32 I = 0; I < 60 * 5 && !Director->GetSchedule().IsExhausted(); ++I)
	{
		Elapsed += Frame;
		Director->Tick(Frame);
	}
	if (!TestEqual(TEXT("every planned body was requested"), ReleasedAt.Num(), Plan.Num()))
	{
		return false;
	}
	for (int32 I = 0; I < Plan.Num(); ++I)
	{
		const double Due = Plan[I].TickOffset / static_cast<double>(Hz);
		TestTrue(FString::Printf(TEXT("body %d (tick %d) released within a frame of %.3f s, at %.3f s"), I, Plan[I].TickOffset, Due, ReleasedAt[I]),
			ReleasedAt[I] + 1e-6 >= Due && ReleasedAt[I] < Due + Frame + 1e-6);
	}
	TestEqual(TEXT("exhaustion announced once"), Exhausted, 1);
	TestEqual(TEXT("everything released is alive"), Director->GetAliveCount(), Plan.Num());
	TestTrue(TEXT("not cleared while bodies live"), Cleared.IsEmpty() && Director->IsWaveActive());

	// A Cluster dies and splits: one out, two in. The wave must outlive the parent.
	Director->NotifyEnemyAdded(2);
	Director->NotifyEnemyRemoved(Plan.Num());
	TestTrue(TEXT("children keep the wave open"), Cleared.IsEmpty());
	Director->NotifyEnemyRemoved(2);
	TestTrue(TEXT("cleared once, wave 0"), Cleared.Num() == 1 && Cleared[0] == 0);
	TestFalse(TEXT("no wave is active"), Director->IsWaveActive());
	Director->NotifyEnemyRemoved();   // a straggler's late report between waves is ignored, not an underflow
	TestEqual(TEXT("still cleared once"), Cleared.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWaveDirectorReentrancyTest, "DF.Unit.WaveDirector.InstantKillsAndChainedWaves", DFWaveDirectorTest::Flags)
bool FDFWaveDirectorReentrancyTest::RunTest(const FString& Parameters)
{
	using namespace DFWaveDirectorTest;
	// The worst listener: kills every body the moment it is requested, and starts the next wave from
	// inside the cleared broadcast. Wave 0 must still release all of its bodies before it clears.
	FDFTestWorld World;
	ADFWaveDirector* Director = World.SpawnActor<ADFWaveDirector>();
	FString Error;
	Director->ConfigureWithTables(9u, Tables(), Error);

	const int32 Wave0 = FDFWavePlan::PlanWave(9u, Director->GetTables(), 0, 1).Num();
	const int32 Wave1 = FDFWavePlan::PlanWave(9u, Director->GetTables(), 1, 1).Num();
	TMap<int32, int32> RequestedByWave;
	TArray<int32> Cleared;
	Director->OnSpawnRequested.AddLambda([&](const FDFSpawnEntry&)
	{
		RequestedByWave.FindOrAdd(Director->GetWaveIndex())++;
		Director->NotifyEnemyRemoved();
	});
	Director->OnWaveCleared.AddLambda([&](int32 Wave)
	{
		Cleared.Add(Wave);
		if (Wave == 0)
		{
			Director->BeginWave(1, 1);
		}
	});

	Director->BeginWave(0, 1);
	for (int32 I = 0; I < 60 * 10 && Cleared.Num() < 2; ++I)
	{
		Director->Tick(Frame);
	}
	TestEqual(TEXT("wave 0 released everything before clearing"), RequestedByWave.FindRef(0), Wave0);
	TestEqual(TEXT("wave 1 (night: +1 lurker) released everything"), RequestedByWave.FindRef(1), Wave1);
	TestTrue(TEXT("cleared 0 then 1"), Cleared.Num() == 2 && Cleared[0] == 0 && Cleared[1] == 1);
	TestTrue(TEXT("night added a body to wave 1"), Wave1 == 3 + 2 + 1);

	// Abort from inside a release must not announce exhaustion for a wave that no longer exists.
	ADFWaveDirector* Aborting = World.SpawnActor<ADFWaveDirector>();
	Aborting->ConfigureWithTables(9u, Tables(), Error);
	int32 Announced = 0;
	Aborting->OnSpawnRequested.AddLambda([Aborting](const FDFSpawnEntry&) { Aborting->AbortWave(); });
	Aborting->OnWaveSpawnsExhausted.AddLambda([&Announced](int32) { ++Announced; });
	Aborting->BeginWave(0, 1);
	Aborting->Tick(Frame);
	TestEqual(TEXT("an aborted wave announces nothing"), Announced, 0);
	TestFalse(TEXT("and is over"), Aborting->IsWaveActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWaveDirectorDestroyedMidReleaseTest, "DF.Unit.WaveDirector.DestroyedInsideItsOwnBroadcast", DFWaveDirectorTest::Flags)
bool FDFWaveDirectorDestroyedMidReleaseTest::RunTest(const FString& Parameters)
{
	using namespace DFWaveDirectorTest;
	// The worst thing a listener can do: end the match from inside the release broadcast, destroying
	// the director while its own loop is running. Destroy() only marks the actor pending-kill, so
	// the memory survives to the next GC and nothing crashes — which is what makes this quiet. What
	// must not happen is the rest of the frame's bodies being requested for a match that is over.
	FDFTestWorld World;
	ADFWaveDirector* Director = World.SpawnActor<ADFWaveDirector>();
	FString Error;
	Director->ConfigureWithTables(9u, Tables(), Error);

	// Wave 0 is four grunts 20 ticks apart; a 2 s frame makes every one of them due at once.
	int32 Requested = 0;
	int32 Cleared = 0;
	Director->OnSpawnRequested.AddLambda([&Requested, Director](const FDFSpawnEntry&)
	{
		if (++Requested == 1)
		{
			Director->Destroy();   // "the host lost, tear the match down"
		}
	});
	Director->OnWaveCleared.AddLambda([&Cleared](int32) { ++Cleared; });

	Director->BeginWave(0, 1);
	TestTrue(TEXT("more than one body is due this frame"), Director->GetSchedule().Num() > 1);
	Director->Tick(2.f);

	TestEqual(TEXT("exactly one body was requested before the director died"), Requested, 1);
	TestFalse(TEXT("the director is no longer valid"), IsValid(Director));
	TestEqual(TEXT("and no wave cleared out of a destroyed director"), Cleared, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWaveDirectorGenerationTest, "DF.Unit.WaveDirector.AbortAndBeginInsideAReleaseDoesNotBleed", DFWaveDirectorTest::Flags)
bool FDFWaveDirectorGenerationTest::RunTest(const FString& Parameters)
{
	using namespace DFWaveDirectorTest;
	// bWaveActive is true for two different situations — "still wave A" and "A was aborted and B
	// began inside a broadcast" — and a release loop that trusted it would carry on over B's
	// entries against A's clock, releasing B's early bodies at once. The generation token is what
	// makes those two answers different.
	FDFTestWorld World;
	ADFWaveDirector* Director = World.SpawnActor<ADFWaveDirector>();
	FString Error;
	Director->ConfigureWithTables(9u, Tables(), Error);

	TMap<int32, int32> RequestedByWave;
	bool bSwapped = false;
	Director->OnSpawnRequested.AddLambda([&](const FDFSpawnEntry&)
	{
		RequestedByWave.FindOrAdd(Director->GetWaveIndex())++;
		if (!bSwapped)
		{
			// The listener replaces the wave mid-release: the loop must stop, not continue over
			// the new schedule.
			bSwapped = true;
			Director->AbortWave();
			Director->BeginWave(1, 1);
		}
	});

	Director->BeginWave(0, 1);
	Director->Tick(2.f);   // long enough that every one of wave 0's four grunts is due at once

	TestEqual(TEXT("wave 0 released exactly the one body before it was replaced"), RequestedByWave.FindRef(0), 1);
	TestEqual(TEXT("and none of wave 1's bodies came out on wave 0's clock"), RequestedByWave.FindRef(1), 0);
	TestTrue(TEXT("wave 1 is the active one"), Director->IsWaveActive() && Director->GetWaveIndex() == 1);
	TestEqual(TEXT("nothing has been released from it yet"), Director->GetSchedule().NumReleased(), 0);

	// And it runs normally from the next frame, on its own clock.
	Director->Tick(1.f / 60.f);
	TestTrue(TEXT("wave 1 starts releasing on its own time"), RequestedByWave.FindRef(1) > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWaveDirectorDestroyedInExhaustedTest, "DF.Unit.WaveDirector.DestroyedInsideTheExhaustedBroadcast", DFWaveDirectorTest::Flags)
bool FDFWaveDirectorDestroyedInExhaustedTest::RunTest(const FString& Parameters)
{
	using namespace DFWaveDirectorTest;
	// The release loop was guarded; the broadcast right after it was not. A listener that ends the
	// match from OnWaveSpawnsExhausted left CheckCleared writing members on a pending-kill actor
	// and broadcasting OnWaveCleared into a torn-down match — silent, like every one of these.
	FDFTestWorld World;
	ADFWaveDirector* Director = World.SpawnActor<ADFWaveDirector>();
	FString Error;
	Director->ConfigureWithTables(9u, Tables(), Error);

	int32 Cleared = 0;
	int32 Exhausted = 0;
	Director->OnSpawnRequested.AddLambda([Director](const FDFSpawnEntry&) { Director->NotifyEnemyRemoved(); });
	Director->OnWaveSpawnsExhausted.AddLambda([Director, &Exhausted](int32) { ++Exhausted; Director->Destroy(); });
	Director->OnWaveCleared.AddLambda([&Cleared](int32) { ++Cleared; });

	Director->BeginWave(0, 1);
	// Long enough to exhaust the plan, not merely to make every entry due: wave 0's four grunts are
	// 20 ticks apart plus up to 20 % jitter, so the last is due around 2.4 s. A 2 s frame releases
	// three of them, leaves the schedule unexhausted, and never reaches the broadcast under test —
	// which is exactly what the first version of this fixture did.
	Director->Tick(5.f);

	TestEqual(TEXT("the exhausted broadcast fired, which is the one being guarded"), Exhausted, 1);
	TestFalse(TEXT("the director is gone"), IsValid(Director));
	TestEqual(TEXT("and no wave cleared out of a destroyed director"), Cleared, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWaveDirectorLateReportTest, "DF.Unit.WaveDirector.LateReportsIntoADestroyedDirector", DFWaveDirectorTest::Flags)
bool FDFWaveDirectorLateReportTest::RunTest(const FString& Parameters)
{
	using namespace DFWaveDirectorTest;
	// NotifyEnemyRemoved is a public entry point that reaches CheckCleared, and it guarded on
	// bWaveActive alone — which destruction did not clear. So: destroy the director from the
	// exhausted broadcast with bodies still alive, then let the stragglers report in, as WS-19's
	// brood vents and WS-24's mutables will. Alive falls to zero and a cleared wave is announced
	// out of a torn-down match. No caller does this today; the invariant sending them here does.
	FDFTestWorld World;
	ADFWaveDirector* Director = World.SpawnActor<ADFWaveDirector>();
	FString Error;
	Director->ConfigureWithTables(9u, Tables(), Error);

	int32 Cleared = 0;
	int32 Alive = 0;
	Director->OnSpawnRequested.AddLambda([&Alive](const FDFSpawnEntry&) { ++Alive; });
	Director->OnWaveSpawnsExhausted.AddLambda([Director](int32) { Director->Destroy(); });
	Director->OnWaveCleared.AddLambda([&Cleared](int32) { ++Cleared; });

	Director->BeginWave(0, 1);
	Director->Tick(5.f);   // long enough to exhaust the plan, so the exhausted broadcast fires

	TestFalse(TEXT("the director is gone"), IsValid(Director));
	TestTrue(TEXT("with bodies still alive"), Alive > 0);

	// The stragglers report in afterwards, one at a time, exactly as a killed enemy would.
	for (int32 I = 0; I < Alive; ++I)
	{
		Director->NotifyEnemyRemoved();
	}
	Director->NotifyEnemyAdded(2);
	Director->NotifyEnemyRemoved(2);

	TestEqual(TEXT("no wave cleared out of a destroyed director"), Cleared, 0);
	TestFalse(TEXT("and it does not think a wave is running"), Director->IsWaveActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWaveDirectorDescribeTest, "DF.Unit.WaveDirector.DescribeWaveAndContent", DFWaveDirectorTest::Flags)
bool FDFWaveDirectorDescribeTest::RunTest(const FString& Parameters)
{
	using namespace DFWaveDirectorTest;
	FDFTestWorld World;
	ADFWaveDirector* Director = World.SpawnActor<ADFWaveDirector>();
	FString Error;
	Director->ConfigureWithTables(9u, Tables(), Error);

	const FDFMsg_Wave Night = Director->DescribeWave(1, 2);
	TestEqual(TEXT("total waves"), Night.TotalWaves, 2);
	TestEqual(TEXT("lap 0"), Night.Lap, 0);
	TestEqual(TEXT("threat is the hp scale the wave spawns with"), Night.Threat, FDFWavePlan::HpScale(Director->GetTables(), 1, 2));
	TestTrue(TEXT("condition tag"), Night.Condition == DFTags::Condition_Night);
	TestFalse(TEXT("a clear wave has no condition"), Director->DescribeWave(0, 1).Condition.IsValid());
	TestEqual(TEXT("wave 3 is lap 1"), Director->DescribeWave(3, 1).Lap, 1);

	// From imported content: the director plans exactly what FromContent + PlanWave plan.
	ADFWaveDirector* Live = World.SpawnActor<ADFWaveDirector>();
	if (TestTrue(TEXT("configures from the foundry tables"), Live->Configure(20260906u, TEXT("foundry"), Error)))
	{
		Live->BeginWave(2, 1);
		TestEqual(TEXT("schedule holds the plan"), Live->GetSchedule().Num(), FDFWavePlan::PlanWave(20260906u, Live->GetTables(), 2, 1).Num());
		TestEqual(TEXT("at the content's tick rate"), Live->GetTables().TickHz, 30.f);
	}
	else
	{
		AddError(Error);
	}

	ADFWaveDirector* Lost = World.SpawnActor<ADFWaveDirector>();
	AddExpectedMessage(TEXT("missing content"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 0);
	TestFalse(TEXT("an unknown map does not configure"), Lost->Configure(1u, TEXT("noSuchMap"), Error));
	TestTrue(TEXT("and says which"), Error.Contains(TEXT("noSuchMap")));
	TestFalse(TEXT("nor begin a wave"), Lost->IsConfigured());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
