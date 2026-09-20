#include "Waves/DFWavePlan.h"

#include "Content/DFContentRows.h"
#include "Waves/DFDetMath.h"
#include "Waves/DFDetRng.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DFWavePlan)

DF_DET_FP_PUSH

namespace
{
	/** 1 + PerExtraPlayer x (players - 1), in the sim's operation order and without a fused multiply-add. */
	float PlayerFactor(float PerExtraPlayer, int32 PlayerCount)
	{
		DF_DET_FP_SCOPE
		const float Extra = PerExtraPlayer * static_cast<float>(PlayerCount - 1);
		const float Factor = 1.f + Extra;
		return Factor;
	}

	float Lateral(FDFDetRng& Rng, float ScatterWidth)
	{
		DF_DET_FP_SCOPE
		if (ScatterWidth <= 0.f)
		{
			return 0.f;   // and no draw: an unscattered enemy must not advance the stream
		}
		const float Centered = Rng.NextFloat() - 0.5f;
		return Centered * ScatterWidth;
	}

	/** Weather's contribution, appended after the authored groups are fully planned and drawn from
	 *  the condition stream — the authored loop must be done with the wave stream first, or a
	 *  condition would reroll scatter and spacing for enemies it never touched. */
	void AppendConditionSpawns(uint32 Seed, const FDFWavePlanTables& Tables, int32 WaveIndex, float HpScale, TArray<FDFSpawnEntry>& Entries)
	{
		const float* Factor = Tables.StealthWeightFactorByWave.Find(WaveIndex);
		if (!Factor || *Factor <= 1.f || !Tables.Waves.IsValidIndex(WaveIndex))
		{
			return;
		}

		// Only the stealth already authored into this wave is amplified: night makes a stealth wave
		// darker, it does not conjure Shades into a wave that never asked for them. The extra count
		// is off the authored count, not the player-scaled one — that is the sim's rule.
		const float Over = *Factor - 1.f;
		FDFDetRng Rng = FDFRngStreams::StreamFor(Seed, FDFRngStreams::Condition, static_cast<uint32>(WaveIndex));
		for (const FDFWavePlanGroup& Group : Tables.Waves[WaveIndex])
		{
			const FDFWavePlanEnemy& Enemy = Tables.Enemies.FindChecked(Group.EnemyId);
			if (!Enemy.bStealth)
			{
				continue;
			}
			const float Scaled = static_cast<float>(Group.Count) * Over;
			const int32 Extra = static_cast<int32>(FDFDetMath::RoundHalfToEven(Scaled));

			// Slotted between the authored ones rather than trailing behind, so the wave reads as
			// denser and not as a second wave stapled on.
			int32 Tick = Group.StartDelayTicks + Group.SpacingTicks / 2;
			for (int32 I = 0; I < Extra; ++I)
			{
				FDFSpawnEntry& Entry = Entries.AddDefaulted_GetRef();
				Entry.DefId = Group.EnemyId;
				Entry.TickOffset = Tick;
				Entry.HpFactor = HpScale;
				Entry.RouteId = Group.RouteId;
				Entry.LateralOffset = Lateral(Rng, Enemy.ScatterWidth);
				Tick += Group.SpacingTicks + Rng.NextInt(0, Group.SpacingTicks / 5 + 1);
			}
		}
	}

	/** The sim sorts by (tick, ordinal def id) with List.Sort, which is unstable: where two entries
	 *  tie, their order there is an accident of the .NET runtime. Here ties keep plan order and ids
	 *  compare by OrdinalKey, so the result is one specific sequence on every platform and target. */
	void SortPlan(TArray<FDFSpawnEntry>& Entries)
	{
		TMap<FName, FString> Ordinal;
		for (const FDFSpawnEntry& Entry : Entries)
		{
			if (!Ordinal.Contains(Entry.DefId))
			{
				Ordinal.Add(Entry.DefId, FDFWavePlan::OrdinalKey(Entry.DefId));
			}
		}
		Entries.StableSort([&Ordinal](const FDFSpawnEntry& A, const FDFSpawnEntry& B)
		{
			if (A.TickOffset != B.TickOffset)
			{
				return A.TickOffset < B.TickOffset;
			}
			return A.DefId != B.DefId && FCString::Strcmp(*Ordinal[A.DefId], *Ordinal[B.DefId]) < 0;
		});
	}
}

FString FDFWavePlan::OrdinalKey(FName Id)
{
	return Id.ToString().ToLower();
}

int32 FDFWavePlan::Lap(const FDFWavePlanTables& Tables, int32 WaveIndex)
{
	return Tables.Waves.Num() > 0 ? WaveIndex / Tables.Waves.Num() : 0;
}

float FDFWavePlan::HpScale(const FDFWavePlanTables& Tables, int32 WaveIndex, int32 PlayerCount)
{
	DF_DET_FP_SCOPE
	const float Player = PlayerFactor(Tables.Dials.HpScalePerExtraPlayer, PlayerCount);
	const int32 LastAuthored = Tables.Waves.Num() - 1;
	if (WaveIndex <= LastAuthored)
	{
		return Player * FDFDetMath::PowInt(Tables.Dials.HpGrowth, WaveIndex);
	}
	// Continuous at the join: the last authored wave keeps its campaign value and endless grows from there.
	const float Campaign = Player * FDFDetMath::PowInt(Tables.Dials.HpGrowth, LastAuthored);
	return Campaign * FDFDetMath::PowInt(Tables.Dials.EndlessHpGrowth, WaveIndex - LastAuthored);
}

float FDFWavePlan::BountyScale(const FDFWavePlanDials& Dials, int32 WaveIndex)
{
	DF_DET_FP_SCOPE
	return Dials.BountyScale * FDFDetMath::PowInt(Dials.BountyGrowth, WaveIndex);
}

float FDFWavePlan::ScrapScale(const FDFWavePlanDials& Dials, int32 WaveIndex)
{
	return FDFDetMath::PowInt(Dials.ScrapGrowth, WaveIndex);
}

TArray<FDFSpawnEntry> FDFWavePlan::PlanWave(uint32 Seed, const FDFWavePlanTables& Tables, int32 WaveIndex, int32 PlayerCount)
{
	DF_DET_FP_SCOPE
	TArray<FDFSpawnEntry> Entries;
	if (!ensureMsgf(Tables.Waves.Num() > 0 && WaveIndex >= 0 && PlayerCount >= 1,
		TEXT("PlanWave(%s, wave %d, %d players): nothing to plan"), *Tables.MapId.ToString(), WaveIndex, PlayerCount))
	{
		return Entries;
	}

	FDFDetRng Rng = FDFRngStreams::StreamFor(Seed, FDFRngStreams::Wave, static_cast<uint32>(WaveIndex));

	// Past the authored arc (endless) the tables cycle. The hp curve keeps compounding on the raw
	// wave index and each lap adds bodies on top, so a wave 27 is an authored wave 7 with the numbers of a 27.
	const int32 LapIndex = Lap(Tables, WaveIndex);
	const TArray<FDFWavePlanGroup>& Groups = Tables.Waves[WaveIndex % Tables.Waves.Num()];

	const float CountScale = PlayerFactor(Tables.Dials.CountScalePerExtraPlayer, PlayerCount)
		* FDFDetMath::PowInt(Tables.Dials.EndlessCountGrowthPerLap, LapIndex);
	const float Hp = HpScale(Tables, WaveIndex, PlayerCount);

	for (const FDFWavePlanGroup& Group : Groups)
	{
		const FDFWavePlanEnemy& Enemy = Tables.Enemies.FindChecked(Group.EnemyId);
		const float Scaled = static_cast<float>(Group.Count) * CountScale;
		const int32 Count = FMath::Max(1, static_cast<int32>(FDFDetMath::RoundHalfToEven(Scaled)));

		int32 Tick = Group.StartDelayTicks;
		for (int32 I = 0; I < Count; ++I)
		{
			FDFSpawnEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.DefId = Group.EnemyId;
			Entry.TickOffset = Tick;
			Entry.HpFactor = Hp;
			Entry.RouteId = Group.RouteId;
			Entry.LateralOffset = Lateral(Rng, Enemy.ScatterWidth);
			Entry.Elite = Group.Elite;
			Entry.bBoss = Group.bBoss;
			// Jitter keeps packs from metronoming; up to +20 % of base spacing.
			const int32 Jitter = Group.SpacingTicks > 0 ? Rng.NextInt(0, Group.SpacingTicks / 5 + 1) : 0;
			Tick += Group.SpacingTicks + Jitter;
		}
	}

	AppendConditionSpawns(Seed, Tables, WaveIndex, Hp, Entries);

	SortPlan(Entries);
	return Entries;
}

TArray<FDFSpawnEntry> FDFWavePlan::PlanWave(uint32 Seed, const FDFWavePlanTables& Tables, int32 WaveIndex, int32 PlayerCount,
	TConstArrayView<TSharedRef<const IDFWaveInjectionStream>> Streams)
{
	TArray<FDFSpawnEntry> Entries = PlanWave(Seed, Tables, WaveIndex, PlayerCount);
	if (Streams.Num() == 0 || Entries.Num() == 0)
	{
		return Entries;
	}

	// Every stream sees the same pure plan — not what an earlier stream added — so the order streams
	// are registered in changes nothing but the order of ties.
	const TArray<FDFSpawnEntry> Planned = Entries;
	const FDFWaveInjectionContext Context{ Tables, WaveIndex, PlayerCount, HpScale(Tables, WaveIndex, PlayerCount), Planned };
	for (const TSharedRef<const IDFWaveInjectionStream>& Stream : Streams)
	{
		FDFDetRng Rng = FDFRngStreams::StreamFor(Seed, Stream->StreamName(), static_cast<uint32>(WaveIndex));
		TArray<FDFSpawnEntry> Appended;
		Stream->Inject(Context, Rng, Appended);
		Entries.Append(MoveTemp(Appended));
	}
	SortPlan(Entries);
	return Entries;
}

bool FDFWavePlanTables::SetWavesFromRows(TConstArrayView<const FDFWaveGroupRow*> Rows, int32 TotalWaves, FString& OutError)
{
	auto Fail = [&OutError, this](const FString& Why)
	{
		Waves.Reset();
		OutError = FString::Printf(TEXT("wave plan tables for '%s': %s"), *MapId.ToString(), *Why);
		return false;
	};

	// Bound first, size second: nothing below may allocate by a number that came out of a JSON file.
	if (TotalWaves < 1 || TotalWaves > MaxAuthoredWaves)
	{
		return Fail(FString::Printf(TEXT("the maps table says %d waves; a map authors 1 to %d"), TotalWaves, MaxAuthoredWaves));
	}
	Waves.Reset();
	Waves.SetNum(TotalWaves);

	// Groups arrive in table order, which the importer writes in JSON order: wave index ascending,
	// and within a wave the authored order — the order the wave stream is drawn in. A table that
	// goes backwards was not written by the importer.
	int32 LastWave = 0;
	for (const FDFWaveGroupRow* Row : Rows)
	{
		if (Row->WaveIndex < 0 || Row->WaveIndex >= TotalWaves)
		{
			return Fail(FString::Printf(TEXT("group '%s' has waveIndex %d, outside the map's %d waves"), *Row->EnemyId.ToString(), Row->WaveIndex, TotalWaves));
		}
		if (Row->WaveIndex < LastWave)
		{
			return Fail(FString::Printf(TEXT("wave table is out of order at wave %d ('%s')"), Row->WaveIndex, *Row->EnemyId.ToString()));
		}
		LastWave = Row->WaveIndex;

		FDFWavePlanGroup& Group = Waves[Row->WaveIndex].AddDefaulted_GetRef();
		Group.EnemyId = Row->EnemyId;
		Group.Count = Row->Count;
		Group.SpacingTicks = Row->SpacingTicks;
		Group.StartDelayTicks = Row->StartDelayTicks;
		Group.RouteId = Row->RouteId;
		Group.Elite = Row->Elite;
		Group.bBoss = Row->bBoss;
	}
	return true;   // a wave left with no groups is Validate's to refuse
}

bool FDFWavePlanTables::Validate(FString& OutError) const
{
	auto Fail = [&OutError, this](const FString& Why)
	{
		OutError = FString::Printf(TEXT("wave plan tables for '%s': %s"), *MapId.ToString(), *Why);
		return false;
	};

	if (Waves.Num() == 0)
	{
		return Fail(TEXT("no authored waves"));
	}
	for (int32 W = 0; W < Waves.Num(); ++W)
	{
		if (Waves[W].Num() == 0)
		{
			return Fail(FString::Printf(TEXT("wave %d has no groups"), W));
		}
		for (const FDFWavePlanGroup& Group : Waves[W])
		{
			if (!Enemies.Contains(Group.EnemyId))
			{
				return Fail(FString::Printf(TEXT("wave %d names enemy '%s', which has no row"), W, *Group.EnemyId.ToString()));
			}
			if (Group.RouteId.IsNone())
			{
				return Fail(FString::Printf(TEXT("wave %d group '%s' has no route"), W, *Group.EnemyId.ToString()));
			}
			if (Group.Count < 1 || Group.SpacingTicks < 0 || Group.StartDelayTicks < 0)
			{
				return Fail(FString::Printf(TEXT("wave %d group '%s': count %d, spacing %d, delay %d"), W, *Group.EnemyId.ToString(), Group.Count, Group.SpacingTicks, Group.StartDelayTicks));
			}
		}
	}
	for (const TPair<int32, float>& Scheduled : StealthWeightFactorByWave)
	{
		if (!Waves.IsValidIndex(Scheduled.Key))
		{
			return Fail(FString::Printf(TEXT("a condition is scheduled on wave %d, outside the authored arc of %d"), Scheduled.Key, Waves.Num()));
		}
	}

	// Every dial must be set (unset is NaN). Growth factors compound, so they must be positive; the
	// per-player and bounty scales may be 0 — the sim computes 1 + x * (players - 1) and takes any x.
	struct FDial { const TCHAR* Name; float Value; bool bMustBePositive; };
	const FDial Required[] = {
		{ TEXT("countScalePerExtraPlayer"), Dials.CountScalePerExtraPlayer, false },
		{ TEXT("hpScalePerExtraPlayer"), Dials.HpScalePerExtraPlayer, false },
		{ TEXT("endlessCountGrowthPerLap"), Dials.EndlessCountGrowthPerLap, true },
		{ TEXT("hpGrowth"), Dials.HpGrowth, true },
		{ TEXT("endlessHpGrowth"), Dials.EndlessHpGrowth, true },
		{ TEXT("bountyScale"), Dials.BountyScale, false },
		{ TEXT("bountyGrowth"), Dials.BountyGrowth, true },
		{ TEXT("scrapGrowth"), Dials.ScrapGrowth, true },
		{ TEXT("tickHz"), TickHz, true },
	};
	for (const FDial& Dial : Required)
	{
		if (!FMath::IsFinite(Dial.Value))
		{
			return Fail(FString::Printf(TEXT("balance dial '%s' is missing"), Dial.Name));
		}
		if (Dial.bMustBePositive ? Dial.Value <= 0.f : Dial.Value < 0.f)
		{
			return Fail(FString::Printf(TEXT("balance dial '%s' is %g; it must be %s"), Dial.Name, Dial.Value, Dial.bMustBePositive ? TEXT("positive") : TEXT("zero or more")));
		}
	}
	return true;
}

DF_DET_FP_POP
