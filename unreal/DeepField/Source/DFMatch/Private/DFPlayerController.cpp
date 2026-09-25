#include "DFPlayerController.h"

#include "DFMatchState.h"
#include "DFPlayerState.h"
#include "Engine/World.h"
#include "Messages/DFMessageBus.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DFPlayerController)

int32 ADFPlayerController::GetSeat() const
{
	const ADFPlayerState* Seated = GetPlayerState<ADFPlayerState>();
	return Seated ? Seated->GetSeat() : 0;
}

void ADFPlayerController::Server_Launch_Implementation()
{
	// As Step.cs ApplyLaunch: a launch from anyone but the launch seat, or outside the lobby, is
	// silently ignored — the lobby screen only offers the button to the seat that may press it.
	if (ADFMatchState* Match = GetWorld() ? GetWorld()->GetGameState<ADFMatchState>() : nullptr)
	{
		Match->ServerLaunch(GetSeat());
	}
}

void ADFPlayerController::Server_CallEarly_Implementation()
{
	if (ADFMatchState* Match = GetWorld() ? GetWorld()->GetGameState<ADFMatchState>() : nullptr)
	{
		Match->ServerCallEarly(GetSeat());
	}
}

void ADFPlayerController::Client_Refused_Implementation(FGameplayTag Tag, const FDFMsg_Rejected& Payload)
{
	if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
	{
		Bus->Broadcast(Tag, Payload);
	}
}
