#include "Hero/DFHeroStateComponent.h"

#include "DFBalanceDial.h"
#include "DFGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Status/DFStatusComponent.h"

namespace DFHeroStatePrivate
{
	const APawn* HeroPawnOf(const UDFHeroStateComponent& State)
	{
		const AActor* Owner = State.GetOwner();
		if (const APlayerState* PlayerState = Cast<APlayerState>(Owner))
		{
			return PlayerState->GetPawn();
		}
		return Cast<APawn>(Owner);
	}
}

UDFHeroStateComponent::UDFHeroStateComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

UDFHeroStateComponent* UDFHeroStateComponent::Find(const AActor* PlayerStateOrPawn)
{
	if (PlayerStateOrPawn == nullptr)
	{
		return nullptr;
	}
	if (UDFHeroStateComponent* State = PlayerStateOrPawn->FindComponentByClass<UDFHeroStateComponent>())
	{
		return State;
	}
	if (const APawn* Pawn = Cast<APawn>(PlayerStateOrPawn))
	{
		if (const APlayerState* PlayerState = Pawn->GetPlayerState())
		{
			return PlayerState->FindComponentByClass<UDFHeroStateComponent>();
		}
	}
	return nullptr;
}

// ---- readers --------------------------------------------------------------------------------------

EDFHeroLife UDFHeroStateComponent::GetLife() const
{
	if (HasHostAuthority())
	{
		return LifeState.Life;
	}
	return Rep.Life <= static_cast<uint8>(EDFHeroLife::Respawning) ? static_cast<EDFHeroLife>(Rep.Life) : EDFHeroLife::Up;
}

bool UDFHeroStateComponent::IsBleeding() const
{
	if (HasHostAuthority())
	{
		return DFHeroLife::IsBleeding(LifeState);
	}
	if (!IsDowned())
	{
		return false;
	}
	return IsServerClockKnown() ? Rep.BleedoutEndsAt > Now() : Rep.BleedoutEndsAt > 0.f;
}

float UDFHeroStateComponent::GetReviveProgress() const
{
	if (HasHostAuthority())
	{
		return LifeState.ReviveProgress;
	}
	if (!IsDowned())
	{
		return 0.f;
	}
	if (!IsServerClockKnown())
	{
		return Rep.ReviveProgress;
	}
	// The host only writes on a change of hold; between writes progress moves at a fixed rate.
	const FDFHeroLifeRules& Rules = GetRules();
	const float Elapsed = FMath::Max(0.f, Now() - Rep.ReviveStampedAt);
	const float Progress = Rep.bReviveHeld
		? Rep.ReviveProgress + Elapsed
		: Rep.ReviveProgress - Elapsed * Rules.ReviveDecayRate;
	return FMath::Clamp(Progress, 0.f, Rules.ReviveSeconds);
}

float UDFHeroStateComponent::GetReviveFraction() const
{
	const float Seconds = GetRules().ReviveSeconds;
	return Seconds > 0.f ? FMath::Clamp(GetReviveProgress() / Seconds, 0.f, 1.f) : 0.f;
}

bool UDFHeroStateComponent::TryGetBleedoutSecondsLeft(float& OutSeconds) const
{
	OutSeconds = 0.f;
	if (HasHostAuthority())
	{
		OutSeconds = IsDowned() ? LifeState.BleedoutLeft : 0.f;
		return true;
	}
	if (!IsDowned())
	{
		return true;
	}
	if (!IsServerClockKnown())
	{
		return false;
	}
	OutSeconds = FMath::Max(0.f, Rep.BleedoutEndsAt - Now());
	return true;
}

bool UDFHeroStateComponent::TryGetRespawnSecondsLeft(float& OutSeconds) const
{
	OutSeconds = 0.f;
	if (HasHostAuthority())
	{
		OutSeconds = IsRespawning() ? LifeState.RespawnLeft : 0.f;
		return true;
	}
	if (!IsRespawning())
	{
		return true;
	}
	if (!IsServerClockKnown())
	{
		return false;
	}
	OutSeconds = FMath::Max(0.f, Rep.RespawnEndsAt - Now());
	return true;
}

void UDFHeroStateComponent::GetStateTags(FGameplayTagContainer& OutTags) const
{
	if (IsDowned())
	{
		OutTags.AddTag(DFTags::Player_State_Downed);
		if (IsBleeding())
		{
			OutTags.AddTag(DFTags::Player_State_Bleeding);
		}
	}
	if (Rep.SeatIndex != INDEX_NONE)
	{
		OutTags.AddTag(DFTags::Player_State_Seated);
	}
}

const FDFHeroLifeRules& UDFHeroStateComponent::GetRules() const
{
	if (!CachedRules.IsSet())
	{
		FDFHeroLifeRules Rules;
		Rules.ReviveSeconds = DFBalance::Dial(this, TEXT("reviveSeconds"), Rules.ReviveSeconds);
		Rules.BleedoutSeconds = DFBalance::Dial(this, TEXT("bleedoutSeconds"), Rules.BleedoutSeconds);
		Rules.SoloRespawnSeconds = DFBalance::Dial(this, TEXT("soloRespawnSeconds"), Rules.SoloRespawnSeconds);
		Rules.ReviveRangeCm = DFBalance::Dial(this, TEXT("reviveRangeMeters"), Rules.ReviveRangeCm / 100.f) * 100.f;
		CachedRules = Rules;
	}
	return CachedRules.GetValue();
}

// ---- host ---------------------------------------------------------------------------------------

EDFHeroDown UDFHeroStateComponent::HostDeplete(int32 ConnectedPlayers)
{
	if (!HasHostAuthority())
	{
		return EDFHeroDown::Ignored;
	}
	const EDFHeroDown Down = DFHeroLife::Deplete(LifeState, GetRules(), ConnectedPlayers);
	if (Down == EDFHeroDown::Ignored)
	{
		return Down;
	}
	ActiveReviver.Reset();
	bReviveHeldLastStep = false;
	// A body bleeding out in a driver's seat is a vehicle nobody can use (Step.cs LeaveSeat "downed").
	ClearSeat();
	WriteRep();
	OnHeroLifeEvent.Broadcast(Down == EDFHeroDown::Downed ? EDFHeroLifeEvent::Downed : EDFHeroLifeEvent::SoloDowned, nullptr);
	return Down;
}

bool UDFHeroStateComponent::HostBeginRevive(UDFHeroStateComponent* Reviver)
{
	if (!HasHostAuthority() || Reviver == nullptr || Reviver == this)
	{
		return false;
	}
	if (LifeState.Life != EDFHeroLife::Downed || !Reviver->IsUp())
	{
		return false;
	}
	UDFHeroStateComponent* Current = ActiveReviver.Get();
	if (Current != nullptr && Current != Reviver && Current->IsUp())
	{
		return false;   // B§1.14: one reviver
	}
	if (Current != Reviver)
	{
		ActiveReviver = Reviver;
		WriteRep();
	}
	return true;
}

void UDFHeroStateComponent::HostEndRevive(UDFHeroStateComponent* Reviver)
{
	if (!HasHostAuthority() || Reviver == nullptr || ActiveReviver.Get() != Reviver)
	{
		return;
	}
	ActiveReviver.Reset();
	bReviveHeldLastStep = false;
	WriteRep();
}

void UDFHeroStateComponent::HostAdvance(float DeltaSeconds, bool bReviveHeld)
{
	if (!HasHostAuthority() || LifeState.Life == EDFHeroLife::Up)
	{
		return;
	}
	const bool bHeld = bReviveHeld && LifeState.Life == EDFHeroLife::Downed;
	const DFHeroLife::FTickResult Result = DFHeroLife::Tick(LifeState, GetRules(), DeltaSeconds, bHeld);
	if (Result.bRevived)
	{
		UDFHeroStateComponent* By = ActiveReviver.Get();
		ActiveReviver.Reset();
		bReviveHeldLastStep = false;
		WriteRep();
		OnHeroLifeEvent.Broadcast(EDFHeroLifeEvent::Revived, By);
		return;
	}
	if (Result.bRespawned)
	{
		bReviveHeldLastStep = false;
		ClearSeat();
		WriteRep();
		OnHeroLifeEvent.Broadcast(EDFHeroLifeEvent::Respawned, nullptr);
		return;
	}
	if (bHeld != bReviveHeldLastStep)
	{
		bReviveHeldLastStep = bHeld;
		WriteRep();
	}
}

bool UDFHeroStateComponent::ShouldRespawnAtWaveBoundary() const
{
	return HasHostAuthority() && DFHeroLife::ShouldRespawnAtWaveBoundary(LifeState);
}

void UDFHeroStateComponent::HostRespawn()
{
	if (!HasHostAuthority() || LifeState.Life == EDFHeroLife::Up)
	{
		return;
	}
	DFHeroLife::Respawn(LifeState);
	ActiveReviver.Reset();
	bReviveHeldLastStep = false;
	ClearSeat();
	WriteRep();
	OnHeroLifeEvent.Broadcast(EDFHeroLifeEvent::Respawned, nullptr);
}

bool UDFHeroStateComponent::HostTakeSeat(AActor* Vehicle, int32 SeatIndex)
{
	if (!HasHostAuthority() || Vehicle == nullptr || SeatIndex < 0 || LifeState.Life != EDFHeroLife::Up)
	{
		return false;
	}
	Rep.SeatVehicle = Vehicle;
	Rep.SeatIndex = SeatIndex;
	WriteRep();
	return true;
}

void UDFHeroStateComponent::HostLeaveSeat()
{
	if (!HasHostAuthority() || Rep.SeatIndex == INDEX_NONE)
	{
		return;
	}
	ClearSeat();
	WriteRep();
}

// ---- engine -------------------------------------------------------------------------------------

void UDFHeroStateComponent::BeginPlay()
{
	Super::BeginPlay();
	// Only the host steps the state; clients read Rep.
	if (!HasHostAuthority())
	{
		SetComponentTickEnabled(false);
	}
}

void UDFHeroStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!HasHostAuthority() || LifeState.Life == EDFHeroLife::Up)
	{
		return;
	}
	bool bHeld = false;
	if (LifeState.Life == EDFHeroLife::Downed)
	{
		UDFHeroStateComponent* Reviver = ActiveReviver.Get();
		if (Reviver != nullptr && !Reviver->IsUp())
		{
			// The reviver went down or left: their hold ends with them.
			ActiveReviver.Reset();
			Reviver = nullptr;
			WriteRep();
		}
		bHeld = Reviver != nullptr && IsWithinReviveRange(*Reviver);
	}
	HostAdvance(DeltaTime, bHeld);
}

void UDFHeroStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDFHeroStateComponent, Rep);
}

void UDFHeroStateComponent::OnRep_State()
{
	OnHeroStateChanged.Broadcast(this);
}

// ---- private ------------------------------------------------------------------------------------

bool UDFHeroStateComponent::HasHostAuthority() const
{
	const AActor* Owner = GetOwner();
	return Owner != nullptr && Owner->HasAuthority();
}

float UDFHeroStateComponent::Now() const
{
	// The one clock host and clients share (see UDFStatusComponent::Now for why a client's own world
	// time will not do). No game state: a dev map or a unit-test world, where there is only one clock.
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return 0.f;
	}
	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return static_cast<float>(GameState->GetServerWorldTimeSeconds());
	}
	return World->GetTimeSeconds();
}

bool UDFHeroStateComponent::IsServerClockKnown() const
{
	const UWorld* World = GetWorld();
	return World != nullptr && UDFStatusComponent::IsServerClockKnown(World->GetGameState() != nullptr, World->GetNetMode());
}

bool UDFHeroStateComponent::IsWithinReviveRange(const UDFHeroStateComponent& Reviver) const
{
	const APawn* Mine = DFHeroStatePrivate::HeroPawnOf(*this);
	const APawn* Theirs = DFHeroStatePrivate::HeroPawnOf(Reviver);
	if (Mine == nullptr || Theirs == nullptr)
	{
		return false;
	}
	const float DistanceCm = static_cast<float>(FVector::Dist(Mine->GetActorLocation(), Theirs->GetActorLocation()));
	return DFHeroLife::InReviveRange(DistanceCm, GetRules());
}

void UDFHeroStateComponent::ClearSeat()
{
	Rep.SeatIndex = INDEX_NONE;
	Rep.SeatVehicle.Reset();
}

void UDFHeroStateComponent::WriteRep()
{
	const float T = Now();
	Rep.Life = static_cast<uint8>(LifeState.Life);
	Rep.BleedoutEndsAt = LifeState.Life == EDFHeroLife::Downed ? T + LifeState.BleedoutLeft : 0.f;
	Rep.RespawnEndsAt = LifeState.Life == EDFHeroLife::Respawning ? T + LifeState.RespawnLeft : 0.f;
	Rep.ReviveProgress = LifeState.ReviveProgress;
	Rep.ReviveStampedAt = T;
	Rep.bReviveHeld = bReviveHeldLastStep;
	UDFHeroStateComponent* Reviver = ActiveReviver.Get();
	if (Reviver != nullptr)
	{
		Rep.Reviver = Reviver->GetOwner();
	}
	else
	{
		Rep.Reviver.Reset();
	}

	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
	OnHeroStateChanged.Broadcast(this);
}
