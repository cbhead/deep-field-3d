#pragma once

#include "CoreMinimal.h"
#include "DFWavePlan.generated.h"

class UDFContentSubsystem;

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

/** Balance.cs dials the plan reads, by their balance.json names. No defaults: a plan with a missing dial is an error. */
struct FDFWavePlanDials
{
	float CountScalePerExtraPlayer = 0.f;
	float HpScalePerExtraPlayer = 0.f;
	float EndlessCountGrowthPerLap = 0.f;
	float HpGrowth = 0.f;
	float EndlessHpGrowth = 0.f;
	float BountyScale = 0.f;
	float BountyGrowth = 0.f;
	float ScrapGrowth = 0.f;
};

/** Everything PlanWave reads, as plain data: built from content at match start, or by hand in a test. */
struct DFENEMIES_API FDFWavePlanTables
{
	FName MapId;
	TArray<TArray<FDFWavePlanGroup>> Waves;          // the authored arc; index = wave index, inner order = authored order
	TMap<FName, FDFWavePlanEnemy> Enemies;           // every enemy a group names
	TMap<int32, float> StealthWeightFactorByWave;    // scheduled conditions only (ConditionRow.StealthWeightFactor)
	FDFWavePlanDials Dials;

	/** Non-empty arc, no empty wave, every group's enemy known and its numbers sane, nothing scheduled past the arc, every dial set. */
	bool Validate(FString& OutError) const;

	/** From the imported DataTables. False with the offending id in OutError; never a silent default. */
	static bool FromContent(const UDFContentSubsystem& Content, FName MapId, FDFWavePlanTables& Out, FString& OutError);
};

struct DFENEMIES_API FDFWavePlan
{
	/** Sorted by (TickOffset, DefId ordinal); entries that tie keep plan order (authored groups, then condition spawns). */
	static TArray<FDFSpawnEntry> PlanWave(uint32 Seed, const FDFWavePlanTables& Tables, int32 WaveIndex, int32 PlayerCount);

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
