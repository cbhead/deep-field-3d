#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "DFEventRelay.generated.h"

/**
 * The one multicast path for discrete state (ADR-0004, C15's "who broadcasts" ruling): the host
 * broadcasts a message on its own UDFMessageBus, then multicasts it here, and every client re-broadcasts
 * it on its own bus on receipt. UI, audio and VFX subscribe to the bus and never know which machine
 * they are on. A refusal is not relayed — it goes to the issuing client only (ADFPlayerController).
 *
 * On a networked host it registers itself as the bus's team relay (UDFMessageBus::SetTeamRelay), so
 * UDFMessageBus::BroadcastTeam from any module, towers and enemies included, reaches every client.
 *
 * Spawned by ADFMatchState on the host; always relevant, so a client that joins mid-match receives
 * every message sent after its channel opens (continuous state it missed is on the replicated actors).
 */
UCLASS(NotBlueprintable, NotPlaceable)
class DFMATCH_API ADFEventRelay : public AInfo
{
	GENERATED_BODY()

public:
	ADFEventRelay();

	/**
	 * Host: broadcast locally, then to every client. Anywhere else, or with no relay in the world
	 * (a unit-test world, a dev map without a match state), it is a local broadcast only.
	 */
	static void Publish(const UObject* WorldContext, const FGameplayTag& Tag, const FInstancedStruct& Payload);

	template <typename T>
	static void Publish(const UObject* WorldContext, const FGameplayTag& Tag, const T& Payload)
	{
		Publish(WorldContext, Tag, FInstancedStruct::Make<T>(Payload));
	}

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	bool bRegisteredWithBus = false;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Event(FGameplayTag Tag, const FInstancedStruct& Payload);
};
