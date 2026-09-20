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

	const TArray<const FDFWaveGroupRow*> Rows = Content.Waves(MapId);
	if (!Out.SetWavesFromRows(Rows, Sector->TotalWaves, OutError))
	{
		return false;
	}
	for (const FDFWaveGroupRow* Row : Rows)
	{
		if (Out.Enemies.Contains(Row->EnemyId))
		{
			continue;
		}
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

	for (const TPair<int32, FName>& Scheduled : Sector->ConditionSchedule)
	{
		const FDFConditionRow* Condition = Content.Condition(Scheduled.Value);
		if (!Condition)
		{
			return Fail(FString::Printf(TEXT("wave %d schedules condition '%s', which has no row"), Scheduled.Key, *Scheduled.Value.ToString()));
		}
		Out.StealthWeightFactorByWave.Add(Scheduled.Key, Condition->StealthWeightFactor);
	}

	// A missing dial reads as NaN (and the subsystem logs its name); Validate refuses it. 0 is a value.
	constexpr float Unset = FDFWavePlanDials::Unset;
	Out.Dials.CountScalePerExtraPlayer = Content.Balance(TEXT("countScalePerExtraPlayer"), Unset);
	Out.Dials.HpScalePerExtraPlayer = Content.Balance(TEXT("hpScalePerExtraPlayer"), Unset);
	Out.Dials.EndlessCountGrowthPerLap = Content.Balance(TEXT("endlessCountGrowthPerLap"), Unset);
	Out.Dials.HpGrowth = Content.Balance(TEXT("hpGrowth"), Unset);
	Out.Dials.EndlessHpGrowth = Content.Balance(TEXT("endlessHpGrowth"), Unset);
	Out.Dials.BountyScale = Content.Balance(TEXT("bountyScale"), Unset);
	Out.Dials.BountyGrowth = Content.Balance(TEXT("bountyGrowth"), Unset);
	Out.Dials.ScrapGrowth = Content.Balance(TEXT("scrapGrowth"), Unset);

	return Out.Validate(OutError);
}
