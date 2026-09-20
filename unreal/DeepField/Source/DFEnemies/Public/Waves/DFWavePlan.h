#pragma once

#include "CoreMinimal.h"
#include <limits>
#include "DFWavePlan.generated.h"

class UDFContentSubsystem;
struct FDFDetRng;
struct FDFWaveGroupRow;
struct FDFWavePlanTables;

// The wave plan, ported from sim/Sim.Core/WavePlan.cs (the written spec, ADR-0005; B§1.11).
// A wave's content is a pure function of (seed, tables, waveIndex, playerCount): no sequential
// draws, so wave N is identical whatever happened in waves 1..N-1. Authored groups give the
// shape; the wave stream jitters spacing and scatter; player count multiplies bodies first and
// hp second; conditions add their spawns from their own stream so they cannot reroll an authored
// one. The elite, boss and PCG streams of B§1.11 append after this, each from its own stream.

/** One body the director will spawn. */
USTRUCT()
struct DFENEMIES_API FDFSpawnEntry
{
	GENERATED_BODY()

	UPROPERTY() FName DefId;
	UPROPERTY() int32 TickOffset = 0;        // sim ticks (balance dial tickHz) from wave start
	UPROPERTY() float HpFactor = 1.f;        // multiplies FDFEnemyRow::Hp
	/** The sim carries an index into MapDef.Routes; routes belong to the lane graph here (C6), so
	 *  the director resolves the id against the map's itineraries. */
	UPROPERTY() FName RouteId;
	UPROPERTY() float LateralOffset = 0.f;   // metres across the lane (scatter)
	/** Authored on the group (FDFWaveGroupRow): an elite modifier id, or the boss. Passed through for
	 *  the director; the sim has neither, and a condition's extra bodies never inherit them. */
	UPROPERTY() FName Elite;
	UPROPERTY() bool bBoss = false;
};

struct FDFWavePlanGroup
{
	FName EnemyId;
	int32 Count = 0;
	int32 SpacingTicks = 0;
	int32 StartDelayTicks = 0;
	FName RouteId;
	FName Elite;
	bool bBoss = false;
};

/** What the plan needs to know about an enemy, and nothing else. */
struct FDFWavePlanEnemy
{
	float ScatterWidth = 0.f;
	bool bStealth = false;
	int32 SplitCount = 0;   // not read by the plan; carried for spawn totals (a Cluster's children)
};

/** Balance.cs dials the plan reads, by their balance.json names. Unset is NaN, not 0: a per-player
 *  dial of 0 is a legitimate balance choice ("co-op adds no bodies"), a missing dial is an error. */
struct FDFWavePlanDials
{
	static constexpr float Unset = std::numeric_limits<float>::quiet_NaN();

	float CountScalePerExtraPlayer = Unset;   // >= 0
	float HpScalePerExtraPlayer = Unset;      // >= 0
	float EndlessCountGrowthPerLap = Unset;   // > 0
	float HpGrowth = Unset;                   // > 0
	float EndlessHpGrowth = Unset;            // > 0
	float BountyScale = Unset;                // >= 0
	float BountyGrowth = Unset;               // > 0
	float ScrapGrowth = Unset;                // > 0
};

/** Everything PlanWave reads, as plain data: built from content at match start, or by hand in a test. */
struct DFENEMIES_API FDFWavePlanTables
{
	FName MapId;
	TArray<TArray<FDFWavePlanGroup>> Waves;          // the authored arc; index = wave index, inner order = authored order
	TMap<FName, FDFWavePlanEnemy> Enemies;           // every enemy a group names
	TMap<int32, float> StealthWeightFactorByWave;    // scheduled conditions only (ConditionRow.StealthWeightFactor)
	TMap<int32, FName> ConditionByWave;              // the same schedule's condition ids, for the wave message
	FDFWavePlanDials Dials;
	float TickHz = FDFWavePlanDials::Unset;          // balance dial tickHz: what a TickOffset is counted in

	/** Non-empty arc, no empty wave, every group's enemy known and its numbers sane, nothing scheduled past the arc, every dial set. */
	bool Validate(FString& OutError) const;

	/** The most waves a map may author. A structural bound, not a balance number: it is what keeps a
	 *  typo in maps.json or waves_*.json from sizing an array by it. */
	static constexpr int32 MaxAuthoredWaves = 512;

	/** Wave-table rows -> Waves, in table order. Every WaveIndex must lie in [0, TotalWaves) and never
	 *  decrease; checked before anything is sized by it. Does not look enemies up (FromContent does). */
	bool SetWavesFromRows(TConstArrayView<const FDFWaveGroupRow*> Rows, int32 TotalWaves, FString& OutError);

	/** From the imported DataTables. False with the offending id in OutError; never a silent default. */
	static bool FromContent(const UDFContentSubsystem& Content, FName MapId, FDFWavePlanTables& Out, FString& OutError);
};

/** What an injection stream is handed. The plan so far is read-only: a stream appends, it never edits. */
struct FDFWaveInjectionContext
{
	const FDFWavePlanTables& Tables;
	int32 WaveIndex = 0;
	int32 PlayerCount = 1;
	float HpScale = 1.f;
	TConstArrayView<FDFSpawnEntry> Planned;   // authored + condition spawns, sorted
};

/** B§1.11: elite injection, boss waves and PCG variants append to the pure plan, each from its own
 *  RNG stream, so adding one cannot shift a single authored spawn. The plan seeds the generator from
 *  (seed, StreamName, waveIndex) and hands it over; a stream never makes its own. */
class IDFWaveInjectionStream
{
public:
	virtual ~IDFWaveInjectionStream() = default;
	/** Names the RNG stream. Distinct per stream, and none of FDFRngStreams' names. */
	virtual const TCHAR* StreamName() const = 0;
	virtual void Inject(const FDFWaveInjectionContext& Context, FDFDetRng& Rng, TArray<FDFSpawnEntry>& OutAppended) const = 0;
};

struct DFENEMIES_API FDFWavePlan
{
	/** The string an id sorts and hashes by: its lower-cased spelling. An FName is case-insensitive
	 *  and, outside the editor, ToString() returns whichever casing reached the name table first in
	 *  the process ("escape" reads back "Escape": InputCore registered the key) — so only a
	 *  case-folded key is a function of the id alone. Same order as the sim's ordinal compare for
	 *  the all-lowercase enemy ids the sim has. */
	static FString OrdinalKey(FName Id);

	/** Sorted by (TickOffset, OrdinalKey(DefId)); entries that tie keep plan order (authored groups, then condition spawns). */
	static TArray<FDFSpawnEntry> PlanWave(uint32 Seed, const FDFWavePlanTables& Tables, int32 WaveIndex, int32 PlayerCount);

	/** PlanWave, then each stream's additions in the order given, then the same sort. With no streams it is PlanWave. */
	static TArray<FDFSpawnEntry> PlanWave(uint32 Seed, const FDFWavePlanTables& Tables, int32 WaveIndex, int32 PlayerCount,
		TConstArrayView<TSharedRef<const IDFWaveInjectionStream>> Streams);

	/** The hp multiplier a wave spawns with, player factor included: HpGrowth compounded through
	 *  the authored arc, then EndlessHpGrowth from the last authored wave's value (continuous at the join). */
	static float HpScale(const FDFWavePlanTables& Tables, int32 WaveIndex, int32 PlayerCount);

	/** What a kill on this wave pays, as a multiple of the row's bounty. */
	static float BountyScale(const FDFWavePlanDials& Dials, int32 WaveIndex);

	/** What a kill on this wave yields in scrap, as a multiple of the row's yield. */
	static float ScrapScale(const FDFWavePlanDials& Dials, int32 WaveIndex);

	/** Past the authored arc the tables cycle; this is how many times they have. */
	static int32 Lap(const FDFWavePlanTables& Tables, int32 WaveIndex);
};
