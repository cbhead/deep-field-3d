#include "DFEventRelay.h"

#include "DFMatchState.h"
#include "Engine/World.h"
#include "Messages/DFMessageBus.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DFEventRelay)

ADFEventRelay::ADFEventRelay()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	PrimaryActorTick.bCanEverTick = false;
}

void ADFEventRelay::Publish(const UObject* WorldContext, const FGameplayTag& Tag, const FInstancedStruct& Payload)
{
	if (UDFMessageBus* Bus = UDFMessageBus::Get(WorldContext))
	{
		Bus->Broadcast(Tag, Payload);
	}
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const ADFMatchState* MatchState = World ? World->GetGameState<ADFMatchState>() : nullptr;
	ADFEventRelay* Relay = MatchState ? MatchState->GetEventRelay() : nullptr;
	if (Relay && Relay->HasAuthority() && World->GetNetMode() != NM_Standalone)
	{
		Relay->Multicast_Event(Tag, Payload);
	}
}

void ADFEventRelay::Multicast_Event_Implementation(FGameplayTag Tag, const FInstancedStruct& Payload)
{
	// The host broadcast before multicasting; a NetMulticast also runs on the server, so skip it there.
	if (HasAuthority())
	{
		return;
	}
	if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
	{
		Bus->Broadcast(Tag, Payload);
	}
}
