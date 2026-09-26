#include "DFPlayerState.h"

#include "DFEventRelay.h"
#include "DFGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "Messages/DFMessages.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DFPlayerState)

namespace DFPlayerStatePrivate
{
	FDFMsg_Player PlayerMessage(const ADFPlayerState& State)
	{
		FDFMsg_Player Msg;
		Msg.PlayerId = State.GetSeat();
		Msg.Name = FName(*State.GetPlayerName());
		if (const APawn* Pawn = State.GetPawn())
		{
			Msg.Location = Pawn->GetActorLocation();
		}
		return Msg;
	}
}

ADFPlayerState::ADFPlayerState()
{
	HeroState = CreateDefaultSubobject<UDFHeroStateComponent>(TEXT("HeroState"));
}

void ADFPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// Only the host raises life events, so binding on every machine is harmless and keeps one path.
	HeroState->OnHeroLifeEvent.AddUObject(this, &ADFPlayerState::HandleHeroLifeEvent);
}

void ADFPlayerState::HandleHeroLifeEvent(EDFHeroLifeEvent Event, UDFHeroStateComponent* Reviver)
{
	if (!HasAuthority())
	{
		return;
	}
	switch (Event)
	{
	case EDFHeroLifeEvent::Downed:
	case EDFHeroLifeEvent::SoloDowned:
		// Step.cs emits PlayerDowned for both.
		ADFEventRelay::Publish(this, DFTags::Message_PlayerDowned, DFPlayerStatePrivate::PlayerMessage(*this));
		break;

	case EDFHeroLifeEvent::Revived:
	{
		ADFPlayerState* By = Reviver != nullptr ? Cast<ADFPlayerState>(Reviver->GetOwner()) : nullptr;
		FDFMsg_Player Msg = By != nullptr ? DFPlayerStatePrivate::PlayerMessage(*By) : FDFMsg_Player();
		Msg.TargetPlayerId = Seat;
		if (By != nullptr)
		{
			By->AddRevive();
			By->AddMatchXp(ReviveMatchXp);
		}
		ADFEventRelay::Publish(this, DFTags::Message_PlayerRevived, Msg);
		break;
	}

	case EDFHeroLifeEvent::Respawned:
		MovePawnToHeroSpawn();
		ADFEventRelay::Publish(this, DFTags::Message_PlayerRespawned, DFPlayerStatePrivate::PlayerMessage(*this));
		break;
	}
}

void ADFPlayerState::MovePawnToHeroSpawn()
{
	APawn* Pawn = GetPawn();
	AController* Controller = Cast<AController>(GetOwner());
	UWorld* World = GetWorld();
	AGameModeBase* GameMode = World != nullptr ? World->GetAuthGameMode() : nullptr;
	if (Pawn == nullptr || Controller == nullptr || GameMode == nullptr)
	{
		return;
	}
	if (AActor* Start = GameMode->FindPlayerStart(Controller))
	{
		Pawn->TeleportTo(Start->GetActorLocation(), Start->GetActorRotation());
	}
}

void ADFPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADFPlayerState, Seat);
	DOREPLIFETIME(ADFPlayerState, Kills);
	DOREPLIFETIME(ADFPlayerState, DamageDealt);
	DOREPLIFETIME(ADFPlayerState, TowersBuilt);
	DOREPLIFETIME(ADFPlayerState, Revives);
	DOREPLIFETIME(ADFPlayerState, MatchXp);
}

void ADFPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);
	// Seamless travel (and, once WS-11's rejoin flow restores a held state, a rejoin): the seat and the
	// match record carry over.
	if (ADFPlayerState* Target = Cast<ADFPlayerState>(PlayerState))
	{
		Target->Seat = Seat;
		Target->Kills = Kills;
		Target->DamageDealt = DamageDealt;
		Target->TowersBuilt = TowersBuilt;
		Target->Revives = Revives;
		Target->MatchXp = MatchXp;
		Target->Changed();
	}
}

void ADFPlayerState::SetSeat(int32 InSeat)
{
	if (Seat != InSeat)
	{
		Seat = InSeat;
		Changed();
	}
}

void ADFPlayerState::AddKill()
{
	++Kills;
	Changed();
}

void ADFPlayerState::AddDamageDealt(float Amount)
{
	if (Amount > 0.f)
	{
		DamageDealt += Amount;
		Changed();
	}
}

void ADFPlayerState::AddTowerBuilt()
{
	++TowersBuilt;
	Changed();
}

void ADFPlayerState::AddRevive()
{
	++Revives;
	Changed();
}

void ADFPlayerState::AddMatchXp(int32 Amount)
{
	if (Amount > 0)
	{
		MatchXp += Amount;
		Changed();
	}
}

void ADFPlayerState::OnRep_Match()
{
	OnPlayerStateChanged.Broadcast(this);
}

void ADFPlayerState::Changed()
{
	ForceNetUpdate();
	OnPlayerStateChanged.Broadcast(this);
}
