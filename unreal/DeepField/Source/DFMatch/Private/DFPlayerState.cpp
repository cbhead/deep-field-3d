#include "DFPlayerState.h"

#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DFPlayerState)

ADFPlayerState::ADFPlayerState()
{
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
