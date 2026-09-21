#include "Waves/DFWaveDirector.h"

#include "Content/DFContentSubsystem.h"
#include "DFGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DFWaveDirector)

DEFINE_LOG_CATEGORY_STATIC(LogDFWaves, Log, All);

ADFWaveDirector::ADFWaveDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;   // nothing to do between waves
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bReplicates = false;
}

bool ADFWaveDirector::Configure(uint32 InSeed, FName MapId, FString& OutError)
{
	const UDFContentSubsystem* Content = UDFContentSubsystem::Get(this);
	if (!Content || !Content->IsReady())
	{
		OutError = FString::Printf(TEXT("wave director for '%s': content tables are not loaded"), *MapId.ToString());
		return false;
	}
	FDFWavePlanTables Built;
	if (!FDFWavePlanTables::FromContent(*Content, MapId, Built, OutError))
	{
		return false;
	}
	return ConfigureWithTables(InSeed, MoveTemp(Built), OutError);
}

bool ADFWaveDirector::ConfigureWithTables(uint32 InSeed, FDFWavePlanTables InTables, FString& OutError)
{
	if (bWaveActive)
	{
		OutError = TEXT("wave director: cannot reconfigure while a wave is running");
		return false;
	}
	if (!InTables.Validate(OutError))
	{
		bConfigured = false;
		return false;
	}
	Seed = InSeed;
	Tables = MoveTemp(InTables);
	bConfigured = true;
	return true;
}

void ADFWaveDirector::AddInjectionStream(TSharedRef<const IDFWaveInjectionStream> Stream)
{
	InjectionStreams.Add(MoveTemp(Stream));
}

bool ADFWaveDirector::BeginWave(int32 WaveIndex, int32 PlayerCount)
{
	return ResumeWave(WaveIndex, PlayerCount, 0, 0);
}

bool ADFWaveDirector::ResumeWave(int32 WaveIndex, int32 PlayerCount, int32 AlreadyReleased, int32 AliveNow)
{
	if (!bConfigured || bWaveActive || WaveIndex < 0 || PlayerCount < 1)
	{
		UE_LOG(LogDFWaves, Warning, TEXT("BeginWave(%d, %d players) refused: configured=%d active=%d"), WaveIndex, PlayerCount, bConfigured, bWaveActive);
		return false;
	}

	TArray<FDFSpawnEntry> Plan = FDFWavePlan::PlanWave(Seed, Tables, WaveIndex, PlayerCount, InjectionStreams);
	UE_LOG(LogDFWaves, Log, TEXT("wave %d on '%s': %d bodies for %d player(s), lap %d, hp x%.3f"),
		WaveIndex, *Tables.MapId.ToString(), Plan.Num(), PlayerCount, FDFWavePlan::Lap(Tables, WaveIndex), FDFWavePlan::HpScale(Tables, WaveIndex, PlayerCount));

	Schedule.Reset(MoveTemp(Plan), Tables.TickHz);
	Schedule.SkipReleased(AlreadyReleased);
	ActiveWave = WaveIndex;
	Alive = FMath::Max(0, AliveNow);
	bWaveActive = true;
	bExhaustedAnnounced = false;
	SetActorTickEnabled(true);
	return true;
}

void ADFWaveDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bWaveActive)
	{
		return;
	}

	// A listener may kill the body on the spot; if that was the last one the wave would clear — and
	// the next could begin — inside the release loop. Hold the verdict until the loop is done.
	bReleasing = true;
	Schedule.Advance(DeltaSeconds, [this](const FDFSpawnEntry& Entry)
	{
		++Alive;   // before the broadcast, for the same reason
		OnSpawnRequested.Broadcast(Entry);
		// The listener that just ran may have ended the match and destroyed us, or aborted the wave.
		// Destroy() only marks the actor pending-kill — the memory is ours until the next GC — so
		// this is not a crash but it would be worse: bodies requested for a match that is over.
		return IsValid(this) && bWaveActive;
	});
	if (!IsValid(this))
	{
		return;
	}
	bReleasing = false;

	if (bWaveActive && Schedule.IsExhausted() && !bExhaustedAnnounced)   // a listener may have aborted the wave
	{
		bExhaustedAnnounced = true;
		OnWaveSpawnsExhausted.Broadcast(ActiveWave);
	}
	CheckCleared();
}

void ADFWaveDirector::NotifyEnemyAdded(int32 Count)
{
	if (bWaveActive)
	{
		Alive += FMath::Max(0, Count);
	}
}

void ADFWaveDirector::NotifyEnemyRemoved(int32 Count)
{
	if (!bWaveActive)
	{
		return;
	}
	Alive -= FMath::Max(0, Count);
	if (!ensureMsgf(Alive >= 0, TEXT("wave %d: more bodies removed than were ever added"), ActiveWave))
	{
		Alive = 0;
	}
	CheckCleared();
}

void ADFWaveDirector::AbortWave()
{
	bWaveActive = false;
	ActiveWave = INDEX_NONE;
	Alive = 0;
	Schedule.Reset({}, Tables.TickHz);
	SetActorTickEnabled(false);
}

void ADFWaveDirector::CheckCleared()
{
	if (!bWaveActive || bReleasing || !Schedule.IsExhausted() || Alive > 0)
	{
		return;
	}
	// State first: a listener is entitled to call BeginWave for the next wave from inside the broadcast.
	const int32 Cleared = ActiveWave;
	bWaveActive = false;
	ActiveWave = INDEX_NONE;
	SetActorTickEnabled(false);
	OnWaveCleared.Broadcast(Cleared);
}

FDFMsg_Wave ADFWaveDirector::DescribeWave(int32 WaveIndex, int32 PlayerCount) const
{
	FDFMsg_Wave Message;
	Message.WaveIndex = WaveIndex;
	Message.TotalWaves = Tables.Waves.Num();
	Message.Lap = FDFWavePlan::Lap(Tables, WaveIndex);
	Message.Threat = bConfigured ? FDFWavePlan::HpScale(Tables, WaveIndex, FMath::Max(1, PlayerCount)) : 1.f;
	if (const FName* ConditionId = Tables.ConditionByWave.Find(WaveIndex))
	{
		Message.Condition = DFTags::ForContentId(TEXT("DF.Condition"), *ConditionId);
	}
	return Message;
}
