#include "Waves/DFWavePlan.h"

#include "Content/DFContentSubsystem.h"

// Content -> plan tables. The plan itself never sees a DataTable, so it stays a pure function a
// test can feed by hand; this is the one place the two meet.

bool FDFWavePlanTables::FromContent(const UDFContentSubsystem& Content, FName MapId, FDFWavePlanTables& Out, FString& OutError)
{
	Out = FDFWavePlanTables();
	Out.MapId = MapId;

	auto Fail = [&OutError, MapId](const FString& Why)
	{
		OutError = FString::Printf(TEXT("wave plan tables for '%s': %s"), *MapId.ToString(), *Why);
		return false;
	};

	const FDFSectorRow* Sector = Content.Sector(MapId);
	if (!Sector)
	{
		return Fail(TEXT("no row in the maps table"));
	}

	// Groups arrive in table order, which the importer writes in JSON order: wave index ascending,
	// and within a wave the authored order — the order the wave stream is drawn in. A table that
	// goes backwards was not written by the importer.
	int32 LastWave = 0;
	for (const FDFWaveGroupRow* Row : Content.Waves(MapId))
	{
		if (Row->WaveIndex < LastWave)
		{
			return Fail(FString::Printf(TEXT("wave table is out of order at wave %d ('%s')"), Row->WaveIndex, *Row->EnemyId.ToString()));
		}
		LastWave = Row->WaveIndex;
		if (!Out.Waves.IsValidIndex(Row->WaveIndex))
		{
			Out.Waves.SetNum(Row->WaveIndex + 1);
		}

		FDFWavePlanGroup& Group = Out.Waves[Row->WaveIndex].AddDefaulted_GetRef();
		Group.EnemyId = Row->EnemyId;
		Group.Count = Row->Count;
		Group.SpacingTicks = Row->SpacingTicks;
		Group.StartDelayTicks = Row->StartDelayTicks;
		Group.RouteId = Row->RouteId;
		Group.Elite = Row->Elite;
		Group.bBoss = Row->bBoss;

		if (!Out.Enemies.Contains(Row->EnemyId))
		{
			const FDFEnemyRow* Enemy = Content.Enemy(Row->EnemyId);
			if (!Enemy)
			{
				return Fail(FString::Printf(TEXT("wave %d names enemy '%s', which has no row"), Row->WaveIndex, *Row->EnemyId.ToString()));
			}
			FDFWavePlanEnemy& PlanEnemy = Out.Enemies.Add(Row->EnemyId);
			PlanEnemy.ScatterWidth = Enemy->ScatterWidth;
			PlanEnemy.bStealth = Enemy->bStealth;
			PlanEnemy.SplitCount = Enemy->SplitCount;
		}
	}
	if (Out.Waves.Num() != Sector->TotalWaves)
	{
		return Fail(FString::Printf(TEXT("the maps table says %d waves, the wave table has %d"), Sector->TotalWaves, Out.Waves.Num()));
	}

	for (const TPair<int32, FName>& Scheduled : Sector->ConditionSchedule)
	{
		const FDFConditionRow* Condition = Content.Condition(Scheduled.Value);
		if (!Condition)
		{
			return Fail(FString::Printf(TEXT("wave %d schedules condition '%s', which has no row"), Scheduled.Key, *Scheduled.Value.ToString()));
		}
		Out.StealthWeightFactorByWave.Add(Scheduled.Key, Condition->StealthWeightFactor);
	}

	// A missing dial reads as 0 (and the subsystem logs its name); Validate refuses it.
	Out.Dials.CountScalePerExtraPlayer = Content.Balance(TEXT("countScalePerExtraPlayer"));
	Out.Dials.HpScalePerExtraPlayer = Content.Balance(TEXT("hpScalePerExtraPlayer"));
	Out.Dials.EndlessCountGrowthPerLap = Content.Balance(TEXT("endlessCountGrowthPerLap"));
	Out.Dials.HpGrowth = Content.Balance(TEXT("hpGrowth"));
	Out.Dials.EndlessHpGrowth = Content.Balance(TEXT("endlessHpGrowth"));
	Out.Dials.BountyScale = Content.Balance(TEXT("bountyScale"));
	Out.Dials.BountyGrowth = Content.Balance(TEXT("bountyGrowth"));
	Out.Dials.ScrapGrowth = Content.Balance(TEXT("scrapGrowth"));

	return Out.Validate(OutError);
}
