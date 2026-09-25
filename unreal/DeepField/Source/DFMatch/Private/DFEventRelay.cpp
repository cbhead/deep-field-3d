#include "DFEventRelay.h"

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

void ADFEventRelay::BeginPlay()
{
	Super::BeginPlay();
	// On a networked host, become the bus's team relay, so every system that produces a discrete fact
	// (UDFMessageBus::BroadcastTeam, including those in layers below DFMatch) reaches the clients.
	UWorld* World = GetWorld();
	if (HasAuthority() && World && World->GetNetMode() != NM_Standalone && World->GetNetMode() != NM_Client)
	{
		if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
		{
			TWeakObjectPtr<ADFEventRelay> WeakThis(this);
			Bus->SetTeamRelay([WeakThis](const FGameplayTag& Tag, const FInstancedStruct& Payload)
			{
				if (ADFEventRelay* Relay = WeakThis.Get())
				{
					Relay->Multicast_Event(Tag, Payload);
				}
			});
			bRegisteredWithBus = true;
		}
	}
}

void ADFEventRelay::EndPlay(const EEndPlayReason::Type Reason)
{
	if (bRegisteredWithBus)
	{
		if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
		{
			Bus->SetTeamRelay(nullptr);
		}
		bRegisteredWithBus = false;
	}
	Super::EndPlay(Reason);
}

void ADFEventRelay::Publish(const UObject* WorldContext, const FGameplayTag& Tag, const FInstancedStruct& Payload)
{
	// The relay is registered with the bus (BeginPlay), so a team send is the bus's: local broadcast,
	// then the multicast when a relay is registered.
	if (UDFMessageBus* Bus = UDFMessageBus::Get(WorldContext))
	{
		Bus->BroadcastTeam(Tag, Payload);
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
