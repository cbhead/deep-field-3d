#include "DFMatchState.h"

#include "DFBalanceDial.h"
#include "DFEventRelay.h"
#include "DFGameplayTags.h"
#include "DFPlayerState.h"
#include "DFWorldSubsystem.h"
#include "Engine/World.h"
#include "LaneGraph/DFLaneGraphAsset.h"
#include "Match/DFMatchSeams.h"
#include "Net/UnrealNetwork.h"
#include "Waves/DFWaveDirector.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DFMatchState)

DEFINE_LOG_CATEGORY_STATIC(LogDFMatchState, Log, All);

ADFMatchState::ADFMatchState()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ADFMatchState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADFMatchState, Phase);
	DOREPLIFETIME(ADFMatchState, WaveIndex);
	DOREPLIFETIME(ADFMatchState, TotalWaves);
	DOREPLIFETIME(ADFMatchState, bLobby);
	DOREPLIFETIME(ADFMatchState, bEndless);
	DOREPLIFETIME(ADFMatchState, Threat);
	DOREPLIFETIME(ADFMatchState, Lap);
	DOREPLIFETIME(ADFMatchState, EnemiesRemaining);
	DOREPLIFETIME(ADFMatchState, PhaseEndsAtServerTime);
}

void ADFMatchState::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	if (!Relay)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Relay = GetWorld()->SpawnActor<ADFEventRelay>(ADFEventRelay::StaticClass(), FTransform::Identity, Params);
	}
	EnsureDirector();
	// Again, now that the director (and so the arc's length) is known. Nothing has started yet.
	ConfigureMatch(Settings);
}

void ADFMatchState::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Director && ClearedHandle.IsValid())
	{
		Director->OnWaveCleared.Remove(ClearedHandle);
		ClearedHandle.Reset();
	}
	if (HasAuthority())
	{
		if (bOwnsDirector && IsValid(Director))
		{
			Director->Destroy();
		}
		if (IsValid(Relay))
		{
			Relay->Destroy();
		}
	}
	Super::EndPlay(Reason);
}

void ADFMatchState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		AdvanceMatch(DeltaSeconds);
	}
}

float ADFMatchState::GetPhaseSecondsLeft() const
{
	if (PhaseEndsAtServerTime < 0.f)
	{
		return 0.f;
	}
	return FMath::Max(0.f, PhaseEndsAtServerTime - static_cast<float>(GetServerWorldTimeSeconds()));
}

void ADFMatchState::ConfigureMatch(const FDFMatchSettings& InSettings)
{
	Settings = InSettings;
	const float Intermission = Settings.IntermissionSeconds >= 0.f
		? Settings.IntermissionSeconds
		: DFBalance::Dial(this, TEXT("intermissionSeconds"), 8.f);
	const int32 Arc = (Director && Director->IsConfigured()) ? Director->GetTables().Waves.Num() : 0;
	Machine.Reset(Intermission, Arc, Settings.bLobby, Settings.bEndless, Settings.bWaitForPlayers);
	bConfigured = true;
	Publish();
}

void ADFMatchState::UseWaveDirector(ADFWaveDirector* InDirector)
{
	if (Director == InDirector)
	{
		return;
	}
	if (Director && ClearedHandle.IsValid())
	{
		Director->OnWaveCleared.Remove(ClearedHandle);
		ClearedHandle.Reset();
	}
	Director = InDirector;
	bOwnsDirector = false;
	BindDirector();
	// The arc's length comes from the director; before the first wave, pick it up.
	if (bConfigured && Machine.WaveIndex < 0)
	{
		ConfigureMatch(Settings);
	}
}

void ADFMatchState::EnsureDirector()
{
	if (Director)
	{
		return;
	}
	FName MapId = Settings.MapId;
	if (MapId.IsNone())
	{
		if (UDFWorldSubsystem* WorldSubsystem = GetWorld()->GetSubsystem<UDFWorldSubsystem>())
		{
			if (const UDFLaneGraphAsset* Graph = WorldSubsystem->GetLaneGraph())
			{
				MapId = Graph->MapId;
			}
		}
	}
	if (MapId.IsNone())
	{
		// A dev map (L_Dev_Empty, the smoke): no lane graph, no waves. The match idles in intermission.
		UE_LOG(LogDFMatchState, Log, TEXT("no map id (no lane graph in this level): the match has no waves"));
		return;
	}
	if (Settings.Seed == 0)
	{
		Settings.Seed = static_cast<uint32>(FMath::Rand()) | 1u;
	}
	FActorSpawnParameters Params;
	Params.Owner = this;
	ADFWaveDirector* Spawned = GetWorld()->SpawnActor<ADFWaveDirector>(ADFWaveDirector::StaticClass(), FTransform::Identity, Params);
	if (!Spawned)
	{
		return;
	}
	FString Error;
	if (!Spawned->Configure(Settings.Seed, MapId, Error))
	{
		UE_LOG(LogDFMatchState, Warning, TEXT("wave director for '%s' not configured (%s): the match has no waves"), *MapId.ToString(), *Error);
	}
	Director = Spawned;
	bOwnsDirector = true;
	BindDirector();
	UE_LOG(LogDFMatchState, Log, TEXT("match on '%s', seed %u, %d authored wave(s)%s%s"), *MapId.ToString(), Settings.Seed,
		Director->IsConfigured() ? Director->GetTables().Waves.Num() : 0,
		Settings.bEndless ? TEXT(", endless") : TEXT(""), Settings.bLobby ? TEXT(", lobby") : TEXT(""));
}

void ADFMatchState::BindDirector()
{
	if (Director && !ClearedHandle.IsValid())
	{
		ClearedHandle = Director->OnWaveCleared.AddUObject(this, &ADFMatchState::HandleWaveCleared);
	}
}

int32 ADFMatchState::GetConnectedPlayerCount() const
{
	int32 Count = 0;
	for (const APlayerState* PlayerState : PlayerArray)
	{
		const ADFPlayerState* Seated = Cast<ADFPlayerState>(PlayerState);
		if (Seated && Seated->GetSeat() > 0 && !Seated->IsOnlyASpectator() && !Seated->IsInactive())
		{
			++Count;
		}
	}
	return Count;
}

int32 ADFMatchState::GetLaunchSeat() const
{
	int32 Lowest = 0;
	for (const APlayerState* PlayerState : PlayerArray)
	{
		const ADFPlayerState* Seated = Cast<ADFPlayerState>(PlayerState);
		if (Seated && Seated->GetSeat() > 0 && !Seated->IsOnlyASpectator() && !Seated->IsInactive())
		{
			Lowest = Lowest == 0 ? Seated->GetSeat() : FMath::Min(Lowest, Seated->GetSeat());
		}
	}
	return Lowest == 0 ? 1 : Lowest;
}

bool ADFMatchState::ServerLaunch(int32 Seat)
{
	if (!HasAuthority() || Seat != GetLaunchSeat() || !Machine.Launch())
	{
		return false;
	}
	FDFMsg_Player Message;
	Message.PlayerId = Seat;
	ADFEventRelay::Publish(this, DFTags::Message_MatchLaunched, Message);
	Publish();
	return true;
}

bool ADFMatchState::ServerCallEarly(int32 Seat)
{
	if (!HasAuthority() || !Machine.CallEarly())
	{
		return false;
	}
	OnEarlyCalled.Broadcast(Seat);
	Publish();
	return true;
}

void ADFMatchState::AdvanceMatch(float DeltaSeconds)
{
	if (!bConfigured)
	{
		ConfigureMatch(Settings);
	}
	ApplyStep(Machine.Tick(DeltaSeconds, GetConnectedPlayerCount(), ReadLives()));
	Publish();
}

void ADFMatchState::HandleWaveCleared(int32 ClearedWave)
{
	// Called from inside the director's broadcast; the director has already closed the wave, so
	// beginning the next one from here (a zero intermission) is allowed.
	ApplyStep(Machine.WaveCleared(ClearedWave, ReadLives()));
	Publish();
}

void ADFMatchState::ApplyStep(EDFMatchStep Step)
{
	switch (Step)
	{
	case EDFMatchStep::None:
		return;

	case EDFMatchStep::BeginWave:
	{
		const int32 Wave = Machine.WaveIndex;
		const int32 PreviousLap = Lap;
		OnWaveBoundary.Broadcast(Wave);
		if (Director && Director->IsConfigured() && !Director->BeginWave(Wave, PlayersForPlan()))
		{
			UE_LOG(LogDFMatchState, Error, TEXT("the director refused wave %d"), Wave);
		}
		const FDFMsg_Wave Message = Describe(Wave);
		Threat = Message.Threat;
		Lap = Message.Lap;
		ADFEventRelay::Publish(this, DFTags::Message_WaveStarted, Message);
		if (Machine.bEndless && Message.Lap > PreviousLap && Message.Lap > 0)
		{
			ADFEventRelay::Publish(this, DFTags::Message_EndlessLap, Message);
		}
		return;
	}

	case EDFMatchStep::Intermission:
		ADFEventRelay::Publish(this, DFTags::Message_WaveCleared, Describe(Machine.WaveIndex));
		// The payload is the wave coming next, so the HUD can show its threat and condition during the break.
		ADFEventRelay::Publish(this, DFTags::Message_Intermission, Describe(Machine.WaveIndex + 1));
		return;

	case EDFMatchStep::Victory:
		ADFEventRelay::Publish(this, DFTags::Message_WaveCleared, Describe(Machine.WaveIndex));
		ADFEventRelay::Publish(this, DFTags::Message_Victory, Describe(Machine.WaveIndex));
		return;

	case EDFMatchStep::Defeat:
		if (Director && Director->IsWaveActive())
		{
			Director->AbortWave();
		}
		ADFEventRelay::Publish(this, DFTags::Message_Defeat, Describe(FMath::Max(0, Machine.WaveIndex)));
		return;
	}
}

TOptional<int32> ADFMatchState::ReadLives() const
{
	// WS-06's economy component answers (ADR-0024); until it exists, lives are unknown and never defeat.
	for (UActorComponent* Component : GetComponents())
	{
		if (const IDFMatchLivesSource* Source = Cast<IDFMatchLivesSource>(Component))
		{
			return Source->GetLives();
		}
	}
	return TOptional<int32>();
}

FDFMsg_Wave ADFMatchState::Describe(int32 Wave) const
{
	if (Director && Director->IsConfigured())
	{
		return Director->DescribeWave(FMath::Max(0, Wave), PlayersForPlan());
	}
	FDFMsg_Wave Message;
	Message.WaveIndex = FMath::Max(0, Wave);
	Message.TotalWaves = Machine.TotalWaves;
	return Message;
}

void ADFMatchState::Publish()
{
	bool bChanged = false;
	auto Set = [&bChanged](auto& Field, const auto& Value)
	{
		if (Field != Value)
		{
			Field = Value;
			bChanged = true;
		}
	};
	Set(Phase, Machine.Phase);
	Set(WaveIndex, Machine.WaveIndex);
	Set(TotalWaves, Machine.TotalWaves);
	Set(bLobby, Machine.bLobby);
	Set(bEndless, Machine.bEndless);
	const bool bWaveRunning = Director && Director->IsWaveActive();
	Set(EnemiesRemaining, bWaveRunning ? Director->GetAliveCount() + Director->GetSchedule().NumRemaining() : 0);

	// The clock replicates as the server time it runs out, so a client counts down on its own and the
	// field changes only when the clock starts, stops or jumps (an early call), never every frame.
	float EndsAt = -1.f;
	if (Machine.IsClockRunning(GetConnectedPlayerCount()))
	{
		EndsAt = static_cast<float>(GetServerWorldTimeSeconds()) + Machine.PhaseTimer;
	}
	if ((EndsAt < 0.f) != (PhaseEndsAtServerTime < 0.f) || FMath::Abs(EndsAt - PhaseEndsAtServerTime) > 0.05f)
	{
		PhaseEndsAtServerTime = EndsAt;
		bChanged = true;
	}
	if (bChanged)
	{
		ForceNetUpdate();
		OnMatchStateChanged.Broadcast(this);
	}
}

void ADFMatchState::OnRep_Match()
{
	OnMatchStateChanged.Broadcast(this);
}
