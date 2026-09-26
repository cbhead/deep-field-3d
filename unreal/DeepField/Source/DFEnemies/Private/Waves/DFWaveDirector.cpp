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

	// Bump first: an injection stream is foreign code too, and anything it does that touches this
	// director must land against the new wave rather than the one being replaced.
	++WaveGeneration;
	TArray<FDFSpawnEntry> Plan = FDFWavePlan::PlanWave(Seed, Tables, WaveIndex, PlayerCount, InjectionStreams);
	if (!IsValid(this))
	{
		return false;   // a stream destroyed us while planning
	}
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

	// Every broadcast below hands control to foreign code that may destroy this actor, abort the
	// wave, or abort and begin another. Capture which wave this frame is releasing and re-check
	// after each one: Destroy() only marks the actor pending-kill, so none of these are crashes —
	// they are a director quietly working for a match that is over, or for the wrong wave.
	const uint64 Generation = WaveGeneration;

	// A listener may kill the body on the spot; if that was the last one the wave would clear — and
	// the next could begin — inside the release loop. Hold the verdict until the loop is done.
	bReleasing = true;
	Schedule.Advance(DeltaSeconds, [this, Generation](const FDFSpawnEntry& Entry)
	{
		++Alive;   // before the broadcast, for the same reason
		OnSpawnRequested.Broadcast(Entry);
		return IsStillReleasing(Generation);
	});
	if (!IsValid(this))
	{
		return;   // destroyed mid-release: nothing here may touch a member again
	}
	bReleasing = false;

	if (!IsStillReleasing(Generation))
	{
		return;   // aborted, or a replacement wave began inside a broadcast — it owns the state now
	}

	if (Schedule.IsExhausted() && !bExhaustedAnnounced)
	{
		bExhaustedAnnounced = true;
		OnWaveSpawnsExhausted.Broadcast(ActiveWave);
		if (!IsStillReleasing(Generation))
		{
			return;
		}
	}
	CheckCleared();
}

void ADFWaveDirector::NotifyEnemyAdded(int32 Count)
{
	if (IsLiveWave())
	{
		Alive += FMath::Max(0, Count);
	}
}

void ADFWaveDirector::NotifyEnemyRemoved(int32 Count)
{
	// A straggler reporting in after the director died is the caller doing nothing wrong: WS-19's
	// brood vents and WS-24's mutables are told to report every body, and a torn-down match is not
	// their business. It is this class's business not to announce a cleared wave out of one.
	if (!IsLiveWave())
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

void ADFWaveDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	// Not AbortWave: that resets the schedule and touches the tick function, which is more than a
	// dying actor should do. Teardown is not a wave clearing either, so nothing is broadcast.
	// This is the tidy path, not the guard: EndPlay does not run for an actor that never began
	// play, so IsLiveWave's IsValid term is what actually closes the hole.
	bWaveActive = false;
	ActiveWave = INDEX_NONE;
	bReleasing = false;
	Super::EndPlay(Reason);
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
	if (!IsLiveWave() || bReleasing || !Schedule.IsExhausted() || Alive > 0)
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
