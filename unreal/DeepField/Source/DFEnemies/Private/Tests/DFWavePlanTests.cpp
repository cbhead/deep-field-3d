#include "Content/DFContentRows.h"
#include "Content/DFContentSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Testing/DFTestUtils.h"
#include "Waves/DFDetMath.h"
#include "Waves/DFDetRng.h"
#include "Waves/DFWavePlan.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.Unit.WavePlan.* — the port against golden vectors dumped from the frozen C# sim by
// tools/waveplan-golden. The golden file carries its own copy of the sim's wave tables, so these
// tests are about the algorithm and survive a rebalance of unreal/content/json/waves_*.json.
// DF.Unit.WavePlanBaseline, at the end of this file, is the one that reads live content.

namespace
{
	struct FGoldenBalance { uint32 CountScalePerExtraPlayer, HpScalePerExtraPlayer, EndlessCountGrowthPerLap, HpGrowth, EndlessHpGrowth, BountyScale, BountyGrowth, ScrapGrowth; };
	struct FGoldenGroup { const char* Map; int32 Wave; const char* Enemy; int32 Count; int32 SpacingTicks; int32 StartDelayTicks; const char* Route; };
	struct FGoldenEnemy { const char* Id; uint32 ScatterBits; bool bStealth; int32 SplitCount; };
	struct FGoldenCondition { const char* Map; int32 Wave; uint32 FactorBits; };
	struct FGoldenRng { uint32 Seed; const char* Stream; uint32 Index; uint32 Draws[8]; };
	struct FGoldenPow { uint32 XBits; int32 N; uint32 ResultBits; };
	struct FGoldenRound { uint32 XBits; uint32 ResultBits; };
	struct FGoldenScale { const char* Map; int32 Wave; int32 Players; uint32 HpBits; uint32 BountyBits; uint32 ScrapBits; };
	struct FGoldenPlan { const char* Map; int32 Wave; int32 Players; int32 Count; uint32 Hash; };
	struct FGoldenEntry { const char* DefId; int32 Tick; uint32 HpBits; const char* Route; uint32 LateralBits; };

#include "DFWavePlanGolden.inl"

	constexpr EAutomationTestFlags WavePlanTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	uint32 Bits(float F)
	{
		uint32 U;
		FMemory::Memcpy(&U, &F, sizeof(U));
		return U;
	}

	float FromBits(uint32 U)
	{
		float F;
		FMemory::Memcpy(&F, &U, sizeof(F));
		return F;
	}

	FDFWavePlanDials GoldenDials()
	{
		FDFWavePlanDials Dials;
		Dials.CountScalePerExtraPlayer = FromBits(GGoldenBalance.CountScalePerExtraPlayer);
		Dials.HpScalePerExtraPlayer = FromBits(GGoldenBalance.HpScalePerExtraPlayer);
		Dials.EndlessCountGrowthPerLap = FromBits(GGoldenBalance.EndlessCountGrowthPerLap);
		Dials.HpGrowth = FromBits(GGoldenBalance.HpGrowth);
		Dials.EndlessHpGrowth = FromBits(GGoldenBalance.EndlessHpGrowth);
		Dials.BountyScale = FromBits(GGoldenBalance.BountyScale);
		Dials.BountyGrowth = FromBits(GGoldenBalance.BountyGrowth);
		Dials.ScrapGrowth = FromBits(GGoldenBalance.ScrapGrowth);
		return Dials;
	}

	/** The sim's tables for one map, exactly as the golden file froze them. */
	FDFWavePlanTables GoldenTables(const FString& Map)
	{
		FDFWavePlanTables Tables;
		Tables.MapId = FName(*Map);
		Tables.Dials = GoldenDials();
		Tables.TickHz = 30.f;   // Balance.TickHz; the plan itself never reads it
		for (const FGoldenGroup& G : GGoldenGroups)
		{
			if (Map != FString(G.Map))
			{
				continue;
			}
			if (!Tables.Waves.IsValidIndex(G.Wave))
			{
				Tables.Waves.SetNum(G.Wave + 1);
			}
			FDFWavePlanGroup& Group = Tables.Waves[G.Wave].AddDefaulted_GetRef();
			Group.EnemyId = FName(G.Enemy);
			Group.Count = G.Count;
			Group.SpacingTicks = G.SpacingTicks;
			Group.StartDelayTicks = G.StartDelayTicks;
			Group.RouteId = FName(G.Route);
		}
		for (const FGoldenEnemy& E : GGoldenEnemies)
		{
			FDFWavePlanEnemy& Enemy = Tables.Enemies.Add(FName(E.Id));
			Enemy.ScatterWidth = FromBits(E.ScatterBits);
			Enemy.bStealth = E.bStealth;
			Enemy.SplitCount = E.SplitCount;
		}
		for (const FGoldenCondition& C : GGoldenConditions)
		{
			if (Map == FString(C.Map))
			{
				Tables.StealthWeightFactorByWave.Add(C.Wave, FromBits(C.FactorBits));
			}
		}
		return Tables;
	}

	/** The order the golden hashes were taken in: total, so an unstable sort on the C# side cannot
	 *  matter, and by OrdinalKey, so neither can the casing FName::ToString() happens to return. */
	void SortCanonical(TArray<FDFSpawnEntry>& Entries)
	{
		Entries.Sort([](const FDFSpawnEntry& A, const FDFSpawnEntry& B)
		{
			if (A.TickOffset != B.TickOffset)
			{
				return A.TickOffset < B.TickOffset;
			}
			if (const int32 ById = FCString::Strcmp(*FDFWavePlan::OrdinalKey(A.DefId), *FDFWavePlan::OrdinalKey(B.DefId)))
			{
				return ById < 0;
			}
			if (const int32 ByRoute = FCString::Strcmp(*FDFWavePlan::OrdinalKey(A.RouteId), *FDFWavePlan::OrdinalKey(B.RouteId)))
			{
				return ByRoute < 0;
			}
			if (Bits(A.LateralOffset) != Bits(B.LateralOffset))
			{
				return Bits(A.LateralOffset) < Bits(B.LateralOffset);
			}
			return Bits(A.HpFactor) < Bits(B.HpFactor);
		});
	}

	/** FNV-1a over (defId, 0, tick, hp bits, routeId, 0, lateral bits), little-endian, ids lower-cased — the dumper's recipe. */
	uint32 HashPlan(const TArray<FDFSpawnEntry>& Entries)
	{
		uint32 H = 2166136261u;
		auto Byte = [&H](uint32 B) { H ^= B & 0xFFu; H *= 16777619u; };
		auto U32 = [&Byte](uint32 V) { for (int32 I = 0; I < 4; ++I) { Byte(V >> (I * 8)); } };
		auto Str = [&Byte](const FName& Name)
		{
			for (const TCHAR C : FDFWavePlan::OrdinalKey(Name))
			{
				Byte(static_cast<uint32>(C));
			}
			Byte(0);
		};
		for (const FDFSpawnEntry& E : Entries)
		{
			Str(E.DefId);
			U32(static_cast<uint32>(E.TickOffset));
			U32(Bits(E.HpFactor));
			Str(E.RouteId);
			U32(Bits(E.LateralOffset));
		}
		return H;
	}

	bool SameEntry(const FDFSpawnEntry& A, const FDFSpawnEntry& B)
	{
		return A.DefId == B.DefId && A.TickOffset == B.TickOffset && A.RouteId == B.RouteId
			&& Bits(A.HpFactor) == Bits(B.HpFactor) && Bits(A.LateralOffset) == Bits(B.LateralOffset);
	}

	bool SamePlan(const TArray<FDFSpawnEntry>& A, const TArray<FDFSpawnEntry>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 I = 0; I < A.Num(); ++I)
		{
			if (!SameEntry(A[I], B[I]))
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWavePlanRngTest, "DF.Unit.WavePlan.RngMatchesSim", WavePlanTestFlags)
bool FDFWavePlanRngTest::RunTest(const FString& Parameters)
{
	for (const FGoldenRng& G : GGoldenRng)
	{
		const FString Stream(G.Stream);
		FDFDetRng Rng = FDFRngStreams::StreamFor(G.Seed, *Stream, G.Index);
		for (int32 I = 0; I < UE_ARRAY_COUNT(G.Draws); ++I)
		{
			const uint32 Draw = Rng.NextUInt();
			if (Draw != G.Draws[I])
			{
				AddError(FString::Printf(TEXT("StreamFor(%u, %s, %u) draw %d: 0x%08X, sim 0x%08X"), G.Seed, *Stream, G.Index, I, Draw, G.Draws[I]));
				break;
			}
		}
	}

	// NextFloat narrows a double; NextInt is a modulo. Both ride on NextUInt but are pinned separately.
	FDFDetRng Rng = FDFRngStreams::StreamFor(GGoldenSeed, FDFRngStreams::Wave, 3u);
	for (int32 I = 0; I + 1 < UE_ARRAY_COUNT(GGoldenFloatInt); I += 2)
	{
		TestEqual(FString::Printf(TEXT("NextFloat bits, pair %d"), I / 2), Bits(Rng.NextFloat()), GGoldenFloatInt[I]);
		TestEqual(FString::Printf(TEXT("NextInt(0, 7), pair %d"), I / 2), static_cast<uint32>(Rng.NextInt(0, 7)), GGoldenFloatInt[I + 1]);
	}

	// The named streams are the sim's strings; a typo here would be a silent reseed of every wave.
	TestEqual(TEXT("wave stream name"), FString(FDFRngStreams::Wave), FString(TEXT("wave")));
	TestEqual(TEXT("condition stream name"), FString(FDFRngStreams::Condition), FString(TEXT("condition")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWavePlanDetMathTest, "DF.Unit.WavePlan.DetMathMatchesSim", WavePlanTestFlags)
bool FDFWavePlanDetMathTest::RunTest(const FString& Parameters)
{
	for (const FGoldenPow& G : GGoldenPow)
	{
		const uint32 Got = Bits(FDFDetMath::PowInt(FromBits(G.XBits), G.N));
		if (Got != G.ResultBits)
		{
			AddError(FString::Printf(TEXT("PowInt(%g, %d): 0x%08X, sim 0x%08X"), FromBits(G.XBits), G.N, Got, G.ResultBits));
		}
	}

	// .NET's own MathF.Round — the call WavePlan.cs makes — for ties, the floats either side of a
	// tie, night-wave extras and lap counts. No wave in the sim's tables lands exactly on .5, so
	// without these the plans never reach the ties-to-even branch.
	for (const FGoldenRound& G : GGoldenRound)
	{
		const uint32 Got = Bits(FDFDetMath::RoundHalfToEven(FromBits(G.XBits)));
		if (Got != G.ResultBits)
		{
			AddError(FString::Printf(TEXT("RoundHalfToEven(%.9g): %.9g, MathF.Round %.9g"), FromBits(G.XBits), FromBits(Got), FromBits(G.ResultBits)));
		}
	}

	// And by hand: 5 shades x 0.5 night weight is 2 extra, not 3.
	const TPair<float, float> Rounds[] = { { 0.5f, 0.f }, { 1.5f, 2.f }, { 2.5f, 2.f }, { 3.5f, 4.f }, { 2.4f, 2.f }, { 2.6f, 3.f }, { 7.f, 7.f }, { -2.5f, -2.f }, { -3.5f, -4.f } };
	for (const TPair<float, float>& R : Rounds)
	{
		TestEqual(FString::Printf(TEXT("RoundHalfToEven(%g)"), R.Key), FDFDetMath::RoundHalfToEven(R.Key), R.Value);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWavePlanScalesTest, "DF.Unit.WavePlan.ScalesMatchSim", WavePlanTestFlags)
bool FDFWavePlanScalesTest::RunTest(const FString& Parameters)
{
	TMap<FString, FDFWavePlanTables> ByMap;
	for (const FGoldenScale& G : GGoldenScales)
	{
		const FString Map(G.Map);
		const FDFWavePlanTables& Tables = ByMap.Contains(Map) ? ByMap[Map] : ByMap.Add(Map, GoldenTables(Map));
		const FString Where = FString::Printf(TEXT("%s wave %d, %dp"), *Map, G.Wave, G.Players);
		TestEqual(Where + TEXT(": HpScale bits"), Bits(FDFWavePlan::HpScale(Tables, G.Wave, G.Players)), G.HpBits);
		TestEqual(Where + TEXT(": BountyScale bits"), Bits(FDFWavePlan::BountyScale(Tables.Dials, G.Wave)), G.BountyBits);
		TestEqual(Where + TEXT(": ScrapScale bits"), Bits(FDFWavePlan::ScrapScale(Tables.Dials, G.Wave)), G.ScrapBits);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWavePlanPlansTest, "DF.Unit.WavePlan.PlansMatchSim", WavePlanTestFlags)
bool FDFWavePlanPlansTest::RunTest(const FString& Parameters)
{
	// Every authored wave of every map plus six endless waves, at 1, 2 and 4 players.
	TMap<FString, FDFWavePlanTables> ByMap;
	int32 Mismatches = 0;
	for (const FGoldenPlan& G : GGoldenPlans)
	{
		const FString Map(G.Map);
		if (!ByMap.Contains(Map))
		{
			FString Error;
			FDFWavePlanTables Tables = GoldenTables(Map);
			if (!Tables.Validate(Error))
			{
				AddError(Error);
				return false;
			}
			ByMap.Add(Map, MoveTemp(Tables));
		}

		TArray<FDFSpawnEntry> Plan = FDFWavePlan::PlanWave(GGoldenSeed, ByMap[Map], G.Wave, G.Players);
		SortCanonical(Plan);
		const uint32 Hash = HashPlan(Plan);
		if ((Plan.Num() != G.Count || Hash != G.Hash) && ++Mismatches <= 8)
		{
			AddError(FString::Printf(TEXT("%s wave %d, %dp: %d entries hash 0x%08X; sim %d entries hash 0x%08X"), *Map, G.Wave, G.Players, Plan.Num(), Hash, G.Count, G.Hash));
		}
	}
	TestEqual(TEXT("plans that differ from the sim"), Mismatches, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWavePlanSpelledTest, "DF.Unit.WavePlan.SpelledPlansMatchSim", WavePlanTestFlags)
bool FDFWavePlanSpelledTest::RunTest(const FString& Parameters)
{
	// Two whole plans, entry by entry, so a hash mismatch above has something readable beside it:
	// a scatter wave (laterals off the wave stream) and a night wave with authored stealth (the condition stream).
	auto Compare = [this](const TCHAR* Map, int32 Wave, int32 Players, TConstArrayView<FGoldenEntry> Golden)
	{
		TArray<FDFSpawnEntry> Plan = FDFWavePlan::PlanWave(GGoldenSeed, GoldenTables(Map), Wave, Players);
		SortCanonical(Plan);
		const FString Where = FString::Printf(TEXT("%s wave %d, %dp"), Map, Wave, Players);
		if (!TestEqual(Where + TEXT(": entry count"), Plan.Num(), Golden.Num()))
		{
			return;
		}
		for (int32 I = 0; I < Plan.Num(); ++I)
		{
			const FDFSpawnEntry& E = Plan[I];
			const FGoldenEntry& G = Golden[I];
			if (E.DefId != FName(G.DefId) || E.TickOffset != G.Tick || Bits(E.HpFactor) != G.HpBits || E.RouteId != FName(G.Route) || Bits(E.LateralOffset) != G.LateralBits)
			{
				AddError(FString::Printf(TEXT("%s entry %d: %s @%d hp 0x%08X %s lat 0x%08X; sim %hs @%d hp 0x%08X %hs lat 0x%08X"), *Where, I,
					*E.DefId.ToString(), E.TickOffset, Bits(E.HpFactor), *E.RouteId.ToString(), Bits(E.LateralOffset),
					G.DefId, G.Tick, G.HpBits, G.Route, G.LateralBits));
				return;
			}
		}
	};
	Compare(GGoldenScatterMap, GGoldenScatterWave, GGoldenScatterPlayers, GGoldenScatterPlan);
	Compare(GGoldenNightMap, GGoldenNightWave, GGoldenNightPlayers, GGoldenNightPlan);

	// The scatter plan really does scatter, and the night plan really does add bodies.
	bool bAnyLateral = false;
	for (const FGoldenEntry& G : GGoldenScatterPlan)
	{
		bAnyLateral |= G.LateralBits != 0u;
	}
	TestTrue(TEXT("the scatter fixture has lateral offsets"), bAnyLateral);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWavePlanPureTest, "DF.Unit.WavePlan.IsPureFunction", WavePlanTestFlags)
bool FDFWavePlanPureTest::RunTest(const FString& Parameters)
{
	const FDFWavePlanTables Tables = GoldenTables(GGoldenScatterMap);
	const int32 Wave = GGoldenScatterWave;

	// Wave N does not depend on what was planned before it, or on how often.
	const TArray<FDFSpawnEntry> Alone = FDFWavePlan::PlanWave(GGoldenSeed, Tables, Wave, 2);
	for (int32 W = Tables.Waves.Num() + 3; W >= 0; --W)
	{
		FDFWavePlan::PlanWave(GGoldenSeed, Tables, W, 4);
	}
	TestTrue(TEXT("same plan after planning every other wave"), SamePlan(Alone, FDFWavePlan::PlanWave(GGoldenSeed, Tables, Wave, 2)));

	// The seed and the wave index each reseed the stream.
	TestFalse(TEXT("another seed scatters differently"), SamePlan(Alone, FDFWavePlan::PlanWave(GGoldenSeed + 1u, Tables, Wave, 2)));

	// Player count adds bodies first, hp second.
	const TArray<FDFSpawnEntry> Solo = FDFWavePlan::PlanWave(GGoldenSeed, Tables, Wave, 1);
	const TArray<FDFSpawnEntry> Four = FDFWavePlan::PlanWave(GGoldenSeed, Tables, Wave, 4);
	TestTrue(TEXT("four players face more bodies than one"), Four.Num() > Solo.Num());
	TestTrue(TEXT("and tougher ones"), Four[0].HpFactor > Solo[0].HpFactor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWavePlanConditionStreamTest, "DF.Unit.WavePlan.ConditionNeverShiftsAuthoredSpawns", WavePlanTestFlags)
bool FDFWavePlanConditionStreamTest::RunTest(const FString& Parameters)
{
	// The same wave with and without its night: every authored spawn is untouched, and what night
	// adds is stealth only. If a condition drew from the wave stream the first half would fail.
	const FDFWavePlanTables Night = GoldenTables(GGoldenNightMap);
	FDFWavePlanTables Clear = Night;
	Clear.StealthWeightFactorByWave.Remove(GGoldenNightWave);

	TArray<FDFSpawnEntry> WithNight = FDFWavePlan::PlanWave(GGoldenSeed, Night, GGoldenNightWave, 1);
	const TArray<FDFSpawnEntry> Without = FDFWavePlan::PlanWave(GGoldenSeed, Clear, GGoldenNightWave, 1);
	TestTrue(TEXT("night adds bodies"), WithNight.Num() > Without.Num());

	for (const FDFSpawnEntry& Authored : Without)
	{
		const int32 Found = WithNight.IndexOfByPredicate([&Authored](const FDFSpawnEntry& E) { return SameEntry(E, Authored); });
		if (!TestTrue(FString::Printf(TEXT("authored %s @%d survives the condition"), *Authored.DefId.ToString(), Authored.TickOffset), Found != INDEX_NONE))
		{
			return false;
		}
		WithNight.RemoveAt(Found);
	}
	for (const FDFSpawnEntry& Extra : WithNight)
	{
		TestTrue(FString::Printf(TEXT("night's extra %s is a stealth enemy"), *Extra.DefId.ToString()), Night.Enemies.FindChecked(Extra.DefId).bStealth);
	}

	// A condition with no stealth weight (fog) adds nothing.
	FDFWavePlanTables Fog = Clear;
	Fog.StealthWeightFactorByWave.Add(GGoldenNightWave, 1.f);
	TestTrue(TEXT("a weight of 1 adds nothing"), SamePlan(Without, FDFWavePlan::PlanWave(GGoldenSeed, Fog, GGoldenNightWave, 1)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWavePlanOrderTest, "DF.Unit.WavePlan.SortedAndTiesKeepPlanOrder", WavePlanTestFlags)
bool FDFWavePlanOrderTest::RunTest(const FString& Parameters)
{
	// Every golden plan is ordered by (tick, ordinal def id).
	for (const TCHAR* Map : { TEXT("foundry"), TEXT("toaster") })
	{
		const FDFWavePlanTables Tables = GoldenTables(Map);
		for (int32 W = 0; W < Tables.Waves.Num(); ++W)
		{
			const TArray<FDFSpawnEntry> Plan = FDFWavePlan::PlanWave(GGoldenSeed, Tables, W, 4);
			for (int32 I = 1; I < Plan.Num(); ++I)
			{
				const bool bOrdered = Plan[I - 1].TickOffset < Plan[I].TickOffset
					|| (Plan[I - 1].TickOffset == Plan[I].TickOffset && FCString::Strcmp(*FDFWavePlan::OrdinalKey(Plan[I - 1].DefId), *FDFWavePlan::OrdinalKey(Plan[I].DefId)) <= 0);
				if (!bOrdered)
				{
					AddError(FString::Printf(TEXT("%s wave %d: entry %d is out of order"), Map, W, I));
					break;
				}
			}
		}
	}

	// Ties keep plan order: two groups of one enemy, all on tick 0, come out first group first.
	FDFWavePlanTables Tied;
	Tied.MapId = TEXT("tied");
	Tied.Dials = GoldenDials();
	Tied.Enemies.Add(TEXT("zeta"));
	Tied.Enemies.Add(TEXT("alpha"));
	TArray<FDFWavePlanGroup>& Wave = Tied.Waves.AddDefaulted_GetRef();
	Wave.Add({ TEXT("zeta"), 2, 0, 0, TEXT("left") });
	Wave.Add({ TEXT("alpha"), 2, 0, 0, TEXT("left") });
	Wave.Add({ TEXT("alpha"), 2, 0, 0, TEXT("right") });
	const TArray<FDFSpawnEntry> Plan = FDFWavePlan::PlanWave(1u, Tied, 0, 1);
	if (TestEqual(TEXT("six entries"), Plan.Num(), 6))
	{
		const TCHAR* ExpectedId[] = { TEXT("alpha"), TEXT("alpha"), TEXT("alpha"), TEXT("alpha"), TEXT("zeta"), TEXT("zeta") };
		const TCHAR* ExpectedRoute[] = { TEXT("left"), TEXT("left"), TEXT("right"), TEXT("right"), TEXT("left"), TEXT("left") };
		for (int32 I = 0; I < 6; ++I)
		{
			TestEqual(FString::Printf(TEXT("entry %d id"), I), Plan[I].DefId, FName(ExpectedId[I]));
			TestEqual(FString::Printf(TEXT("entry %d route"), I), Plan[I].RouteId, FName(ExpectedRoute[I]));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWavePlanEndlessTest, "DF.Unit.WavePlan.EndlessLaps", WavePlanTestFlags)
bool FDFWavePlanEndlessTest::RunTest(const FString& Parameters)
{
	const FDFWavePlanTables Tables = GoldenTables(TEXT("foundry"));
	const int32 Authored = Tables.Waves.Num();
	const FDFWavePlanDials& Dials = Tables.Dials;

	TestEqual(TEXT("the last authored wave is lap 0"), FDFWavePlan::Lap(Tables, Authored - 1), 0);
	TestEqual(TEXT("the wave after it is lap 1"), FDFWavePlan::Lap(Tables, Authored), 1);

	// A lap-1 wave is its authored wave with EndlessCountGrowthPerLap more bodies per group.
	const int32 Wave = 3;
	int32 Expected = 0;
	for (const FDFWavePlanGroup& Group : Tables.Waves[Wave])
	{
		Expected += FMath::Max(1, static_cast<int32>(FDFDetMath::RoundHalfToEven(static_cast<float>(Group.Count) * Dials.EndlessCountGrowthPerLap)));
	}
	TestEqual(TEXT("lap 1 body count"), FDFWavePlan::PlanWave(GGoldenSeed, Tables, Authored + Wave, 1).Num(), Expected);
	TestTrue(TEXT("lap 1 has more bodies than lap 0"), Expected > FDFWavePlan::PlanWave(GGoldenSeed, Tables, Wave, 1).Num());

	// Hp is continuous at the join and grows at the endless rate after it.
	const float Last = FDFWavePlan::HpScale(Tables, Authored - 1, 1);
	TestEqual(TEXT("the campaign curve through the arc"), Bits(Last), Bits(FDFDetMath::PowInt(Dials.HpGrowth, Authored - 1)));
	TestEqual(TEXT("one endless step from the last authored value"), Bits(FDFWavePlan::HpScale(Tables, Authored, 1)), Bits(Last * Dials.EndlessHpGrowth));
	TestTrue(TEXT("endless grows slower than the campaign would have"), FDFWavePlan::HpScale(Tables, Authored + 10, 1) < FDFDetMath::PowInt(Dials.HpGrowth, Authored + 10));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWavePlanValidateTest, "DF.Unit.WavePlan.ValidateRejectsBadTables", WavePlanTestFlags)
bool FDFWavePlanValidateTest::RunTest(const FString& Parameters)
{
	FString Error;
	const FDFWavePlanTables Good = GoldenTables(TEXT("switchyard"));
	TestTrue(TEXT("the sim's own tables validate"), Good.Validate(Error));

	FDFWavePlanTables UnknownEnemy = Good;
	UnknownEnemy.Waves[0][0].EnemyId = TEXT("notAnEnemy");
	TestFalse(TEXT("an enemy with no row"), UnknownEnemy.Validate(Error));
	TestTrue(TEXT("names the id"), Error.Contains(TEXT("notAnEnemy")));

	FDFWavePlanTables MissingDial = Good;
	MissingDial.Dials.HpGrowth = FDFWavePlanDials::Unset;
	TestFalse(TEXT("a missing dial"), MissingDial.Validate(Error));
	TestTrue(TEXT("names the dial and calls it missing"), Error.Contains(TEXT("hpGrowth")) && Error.Contains(TEXT("missing")));

	FDFWavePlanTables NeverSet = Good;
	NeverSet.Dials = FDFWavePlanDials();
	TestFalse(TEXT("dials nobody set are missing, not zero"), NeverSet.Validate(Error));

	// 0 is a balance choice for the per-player dials — the sim computes 1 + x * (players - 1) for any x.
	FDFWavePlanTables FlatCoop = Good;
	FlatCoop.Dials.CountScalePerExtraPlayer = 0.f;
	FlatCoop.Dials.HpScalePerExtraPlayer = 0.f;
	if (TestTrue(TEXT("zero co-op scaling is valid"), FlatCoop.Validate(Error)))
	{
		TestEqual(TEXT("and four players then face the solo wave"), FDFWavePlan::PlanWave(GGoldenSeed, FlatCoop, 0, 4).Num(), FDFWavePlan::PlanWave(GGoldenSeed, Good, 0, 1).Num());
		TestEqual(TEXT("at solo hp"), Bits(FDFWavePlan::HpScale(FlatCoop, 3, 4)), Bits(FDFWavePlan::HpScale(Good, 3, 1)));
	}

	FDFWavePlanTables ZeroGrowth = Good;
	ZeroGrowth.Dials.HpGrowth = 0.f;
	TestFalse(TEXT("a growth factor of 0 is not"), ZeroGrowth.Validate(Error));
	TestTrue(TEXT("and the message says why"), Error.Contains(TEXT("hpGrowth")) && Error.Contains(TEXT("positive")));

	FDFWavePlanTables NegativeCoop = Good;
	NegativeCoop.Dials.CountScalePerExtraPlayer = -0.1f;
	TestFalse(TEXT("nor a negative per-player dial"), NegativeCoop.Validate(Error));

	FDFWavePlanTables LateCondition = Good;
	LateCondition.StealthWeightFactorByWave.Add(Good.Waves.Num(), 1.5f);
	TestFalse(TEXT("a condition scheduled past the arc"), LateCondition.Validate(Error));

	FDFWavePlanTables EmptyWave = Good;
	EmptyWave.Waves[1].Reset();
	TestFalse(TEXT("a wave with no groups"), EmptyWave.Validate(Error));

	FDFWavePlanTables NoRoute = Good;
	NoRoute.Waves[0][0].RouteId = NAME_None;
	TestFalse(TEXT("a group with no route"), NoRoute.Validate(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWavePlanNameCaseTest, "DF.Unit.WavePlan.OrderIgnoresNameCase", WavePlanTestFlags)
bool FDFWavePlanNameCaseTest::RunTest(const FString& Parameters)
{
	// FName is case-insensitive, and outside the editor ToString() hands back whichever casing was
	// registered first ("escape" reads "Escape": InputCore's key got there first). Order and hashes
	// must be a function of the id, so they go through OrdinalKey. The editor preserves case per
	// FName, which lets this test stand in for a Game build: spell the ids the way another module might have.
	TestEqual(TEXT("one key whatever the casing"), FDFWavePlan::OrdinalKey(FName(TEXT("Escape"))), FDFWavePlan::OrdinalKey(FName(TEXT("escape"))));
	TestEqual(TEXT("and it is the lower-case one"), FDFWavePlan::OrdinalKey(FName(TEXT("GroundShort"))), FString(TEXT("groundshort")));

	auto Tied = [](const TCHAR* First, const TCHAR* Second)
	{
		FDFWavePlanTables T;
		T.MapId = TEXT("case");
		T.Dials = GoldenDials();
		T.Enemies.Add(First);
		T.Enemies.Add(Second);
		TArray<FDFWavePlanGroup>& Wave = T.Waves.AddDefaulted_GetRef();
		Wave.Add({ First, 1, 0, 0, TEXT("ground") });
		Wave.Add({ Second, 1, 0, 0, TEXT("ground") });
		return FDFWavePlan::PlanWave(1u, T, 0, 1);
	};
	// Ordinal on the raw strings would put 'Z' (0x5A) before 'a' (0x61).
	const TArray<FDFSpawnEntry> Mixed = Tied(TEXT("Zeta"), TEXT("alpha"));
	const TArray<FDFSpawnEntry> Lower = Tied(TEXT("zeta"), TEXT("alpha"));
	if (TestTrue(TEXT("two entries each"), Mixed.Num() == 2 && Lower.Num() == 2))
	{
		TestEqual(TEXT("alpha first, however zeta is spelled"), Mixed[0].DefId, FName(TEXT("alpha")));
		TestEqual(TEXT("same order as the lower-case spelling"), Mixed[0].DefId, Lower[0].DefId);
	}

	// The golden hash of a route the engine has its own casing for.
	TArray<FDFSpawnEntry> One;
	One.AddDefaulted_GetRef().DefId = TEXT("drifter");
	One[0].RouteId = TEXT("escape");
	TArray<FDFSpawnEntry> Other = One;
	Other[0].RouteId = TEXT("Escape");
	Other[0].DefId = TEXT("Drifter");
	TestEqual(TEXT("hash does not see casing"), HashPlan(One), HashPlan(Other));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWavePlanRowsTest, "DF.Unit.WavePlan.RowsAreBoundedBeforeSizing", WavePlanTestFlags)
bool FDFWavePlanRowsTest::RunTest(const FString& Parameters)
{
	// waves.schema.json only says "integer": a wave index is content, and content is hostile until
	// checked. None of these may size an array by the number they carry.
	auto Row = [](int32 WaveIndex)
	{
		FDFWaveGroupRow R;
		R.WaveIndex = WaveIndex;
		R.EnemyId = TEXT("drifter");
		R.Count = 3;
		R.RouteId = TEXT("ground");
		return R;
	};
	auto Build = [](TConstArrayView<FDFWaveGroupRow> Rows, int32 TotalWaves, FString& Error, FDFWavePlanTables& Out)
	{
		TArray<const FDFWaveGroupRow*> Pointers;
		for (const FDFWaveGroupRow& R : Rows)
		{
			Pointers.Add(&R);
		}
		Out.MapId = TEXT("rows");
		return Out.SetWavesFromRows(Pointers, TotalWaves, Error);
	};

	FString Error;
	FDFWavePlanTables T;
	const FDFWaveGroupRow Good[] = { Row(0), Row(0), Row(1), Row(2) };
	if (TestTrue(TEXT("rows in range build"), Build(Good, 3, Error, T)))
	{
		TestTrue(TEXT("three waves, two groups in the first"), T.Waves.Num() == 3 && T.Waves[0].Num() == 2 && T.Waves[2].Num() == 1);
	}

	const FDFWaveGroupRow Wraps[] = { Row(0), Row(MAX_int32) };
	TestFalse(TEXT("INT_MAX does not wrap into a SetNum"), Build(Wraps, 3, Error, T));
	TestTrue(TEXT("and names the index"), Error.Contains(TEXT("2147483647")));
	TestEqual(TEXT("nothing half-built is left behind"), T.Waves.Num(), 0);

	const FDFWaveGroupRow Huge[] = { Row(0), Row(200000000) };
	TestFalse(TEXT("200 million does not allocate"), Build(Huge, 3, Error, T));

	const FDFWaveGroupRow Negative[] = { Row(-1) };
	TestFalse(TEXT("a negative index"), Build(Negative, 3, Error, T));

	const FDFWaveGroupRow OnePast[] = { Row(0), Row(3) };
	TestFalse(TEXT("one past the map's last wave"), Build(OnePast, 3, Error, T));

	const FDFWaveGroupRow Backwards[] = { Row(1), Row(0) };
	TestFalse(TEXT("a table that goes backwards"), Build(Backwards, 3, Error, T));
	TestTrue(TEXT("says out of order"), Error.Contains(TEXT("out of order")));

	TestFalse(TEXT("a map with two billion waves"), Build(Good, 2000000000, Error, T));
	TestFalse(TEXT("a map with none"), Build(Good, 0, Error, T));
	TestTrue(TEXT("the cap is the structural one"), Build(Good, FDFWavePlanTables::MaxAuthoredWaves, Error, T) && T.Waves.Num() == FDFWavePlanTables::MaxAuthoredWaves);

	// Fewer authored waves than the map claims: the rows build, Validate refuses the empty wave.
	FDFWavePlanTables Short = GoldenTables(TEXT("foundry"));
	const FDFWaveGroupRow TwoOfThree[] = { Row(0), Row(1) };
	TestTrue(TEXT("rows for 2 of 3 waves build"), Build(TwoOfThree, 3, Error, Short));
	Short.MapId = TEXT("short");
	TestFalse(TEXT("but do not validate"), Short.Validate(Error));
	TestTrue(TEXT("because wave 2 is empty"), Error.Contains(TEXT("wave 2 has no groups")));
	return true;
}

// ---------------------------------------------------------------------------------------------
// DF.Unit.WavePlanBaseline — live content (the imported DataTables) against the sim's recorded
// matches in docs/gate-baseline.tsv (G3: "wave counts/hp scales reproduced").
//
// The file's `spawned` column counts EnemySpawned events over a whole match: every body the plan
// spawned, plus the children of every Cluster that was killed (a leaked Cluster never splits).
// So `planned <= spawned <= planned + every possible child` always, and where no Cluster leaked —
// the mid-band rows, and any map without splitters — `spawned` is exactly the upper bound.
// Two rows are not solo: foundry/4p seats two players (its party is ember/forge/ember/forge, and
// the second ember and forge are refused their faction), and the distinct-factions row seats four.
// There is no hp column; hp scales are compared with the sim's own values in the golden file.
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFWavePlanBaselineTest, "DF.Unit.WavePlanBaseline", WavePlanTestFlags)
bool FDFWavePlanBaselineTest::RunTest(const FString& Parameters)
{
	const FString BaselinePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../../docs/gate-baseline.tsv"));
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *BaselinePath))
	{
		AddError(FString::Printf(TEXT("cannot read %s"), *BaselinePath));
		return false;
	}

	FDFTestWorld World;
	const UDFContentSubsystem* Content = World.GetSubsystem<UDFContentSubsystem>();
	if (!Content || !Content->IsReady())
	{
		AddError(TEXT("content tables are not imported: run the DFContentPipeline import commandlet (WS-01) before this test"));
		return false;
	}

	const TMap<FString, int32> SeatedPlayers = {
		{ TEXT("foundry/4p"), 2 },
		{ TEXT("foundry/4p-distinct-factions~not-a-gate"), 4 },
	};

	uint32 Seed = 0;
	int32 SpawnedColumn = INDEX_NONE;
	int32 ResultColumn = INDEX_NONE;
	int32 RowsChecked = 0;
	TMap<FName, FDFWavePlanTables> TablesByMap;

	for (const FString& Line : Lines)
	{
		if (Line.StartsWith(TEXT("# seed ")))
		{
			Seed = static_cast<uint32>(FCString::Strtoui64(*Line.Mid(7), nullptr, 10));
			continue;
		}
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
		{
			continue;
		}
		TArray<FString> Cells;
		Line.ParseIntoArray(Cells, TEXT("\t"), /*bCullEmpty*/ false);
		if (Cells[0] == TEXT("scenario"))
		{
			SpawnedColumn = Cells.IndexOfByKey(FString(TEXT("spawned")));
			ResultColumn = Cells.IndexOfByKey(FString(TEXT("result")));
			continue;
		}
		if (SpawnedColumn == INDEX_NONE || ResultColumn == INDEX_NONE || !Cells.IsValidIndex(SpawnedColumn) || Seed == 0)
		{
			AddError(FString::Printf(TEXT("%s: a data row before the seed line and the header, or a short row: %s"), *BaselinePath, *Line));
			return false;
		}

		const FString& Scenario = Cells[0];
		FString MapName, Variant;
		if (!Scenario.Split(TEXT("/"), &MapName, &Variant))
		{
			AddError(FString::Printf(TEXT("scenario '%s' is not <map>/<variant>"), *Scenario));
			continue;
		}
		const FName MapId(*MapName);
		if (!TablesByMap.Contains(MapId))
		{
			FString Error;
			FDFWavePlanTables Tables;
			if (!FDFWavePlanTables::FromContent(*Content, MapId, Tables, Error))
			{
				AddError(Error);
				return false;
			}
			TablesByMap.Add(MapId, MoveTemp(Tables));
		}
		const FDFWavePlanTables& Tables = TablesByMap[MapId];
		const int32 Players = SeatedPlayers.Contains(Scenario) ? SeatedPlayers[Scenario] : 1;

		int32 Planned = 0;
		int32 Children = 0;
		for (int32 W = 0; W < Tables.Waves.Num(); ++W)
		{
			for (const FDFSpawnEntry& Entry : FDFWavePlan::PlanWave(Seed, Tables, W, Players))
			{
				++Planned;
				Children += Tables.Enemies.FindChecked(Entry.DefId).SplitCount;
			}
		}

		const int32 Spawned = FCString::Atoi(*Cells[SpawnedColumn]);
		const bool bEveryClusterDied = Children == 0 || (Variant == TEXT("mid-band") && Cells[ResultColumn] == TEXT("win"));
		if (bEveryClusterDied)
		{
			TestEqual(FString::Printf(TEXT("%s (%dp): spawned = %d planned + %d split children"), *Scenario, Players, Planned, Children), Planned + Children, Spawned);
		}
		else
		{
			TestTrue(FString::Printf(TEXT("%s (%dp): spawned %d lies in [%d planned, +%d split children]"), *Scenario, Players, Spawned, Planned, Children), Spawned >= Planned && Spawned <= Planned + Children);
		}
		++RowsChecked;
	}
	TestTrue(TEXT("the baseline has rows for all four maps"), RowsChecked >= 4 && TablesByMap.Num() == 4);

	// Hp, bounty and scrap scales from live dials, against the sim's own numbers.
	for (const FGoldenScale& G : GGoldenScales)
	{
		const FDFWavePlanTables* Tables = TablesByMap.Find(FName(G.Map));
		if (!Tables)
		{
			continue;   // testlane is a sim fixture, not a baseline scenario
		}
		const FString Where = FString::Printf(TEXT("%hs wave %d, %dp"), G.Map, G.Wave, G.Players);
		TestEqual(Where + TEXT(": HpScale bits"), Bits(FDFWavePlan::HpScale(*Tables, G.Wave, G.Players)), G.HpBits);
		TestEqual(Where + TEXT(": BountyScale bits"), Bits(FDFWavePlan::BountyScale(Tables->Dials, G.Wave)), G.BountyBits);
		TestEqual(Where + TEXT(": ScrapScale bits"), Bits(FDFWavePlan::ScrapScale(Tables->Dials, G.Wave)), G.ScrapBits);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
