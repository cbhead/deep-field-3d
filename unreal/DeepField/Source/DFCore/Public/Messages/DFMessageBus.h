#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DFMessageBus.generated.h"

// The in-process message bus (ADR-0004; R9 fallback — GameplayMessageRouter is a Lyra plugin,
// not in the engine). Discrete gameplay state travels as (tag, payload struct): the host
// broadcasts locally and via ADFEventRelay; every client re-broadcasts on receipt. UI, audio
// and VFX subscribe here and never poll gameplay actors. Typed helpers keep call sites terse:
//
//   Bus->Broadcast(DFTags::Message_TowerPlaced, FDFMsg_Structure{...});
//   FDFMessageHandle H = Bus->Subscribe<FDFMsg_Structure>(DFTags::Message_TowerPlaced,
//       [](const FGameplayTag& Tag, const FDFMsg_Structure& Msg) { ... });
//   Bus->Unsubscribe(H);
//
// Subscribing to a parent tag (e.g. DF.Message) receives every child. Delivery is synchronous,
// on the game thread, in subscription order.

DECLARE_DELEGATE_TwoParams(FDFMessageDelegate, const FGameplayTag& /*Tag*/, const FInstancedStruct& /*Payload*/);

USTRUCT(BlueprintType)
struct DFCORE_API FDFMessageHandle
{
	GENERATED_BODY()
	UPROPERTY() FGameplayTag Tag;
	UPROPERTY() int32 Id = 0;
	bool IsValid() const { return Id != 0; }
};

UCLASS()
class DFCORE_API UDFMessageBus : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UDFMessageBus* Get(const UObject* WorldContext);

	/** Untyped broadcast. Payload is copied into subscribers by const reference. */
	void Broadcast(const FGameplayTag& Tag, const FInstancedStruct& Payload);

	template <typename T>
	void Broadcast(const FGameplayTag& Tag, const T& Payload)
	{
		Broadcast(Tag, FInstancedStruct::Make<T>(Payload));
	}

	/** Untyped subscribe; the callback checks the payload type itself. */
	FDFMessageHandle SubscribeRaw(const FGameplayTag& Tag, FDFMessageDelegate&& Callback, bool bIncludeChildren = true);

	template <typename T, typename TFunc>
	FDFMessageHandle Subscribe(const FGameplayTag& Tag, TFunc&& Func, bool bIncludeChildren = true)
	{
		return SubscribeRaw(Tag, FDFMessageDelegate::CreateLambda(
			[Func = Forward<TFunc>(Func)](const FGameplayTag& InTag, const FInstancedStruct& Payload)
			{
				if (const T* Typed = Payload.GetPtr<T>())
				{
					Func(InTag, *Typed);
				}
			}), bIncludeChildren);
	}

	void Unsubscribe(const FDFMessageHandle& Handle);

	// ---- team-wide sends (C15 append, 2026-09-25) --------------------------------------------------
	// A system that produces a discrete fact calls BroadcastTeam: it broadcasts here, then hands the
	// message to the team relay so every client re-broadcasts it (messages.md, "who broadcasts"). The
	// relay is DFMatch's ADFEventRelay, which registers itself on the host. Lower layers (towers,
	// enemies, economy) cannot name it, so they reach it through this hook. With no relay registered
	// (a client, a unit-test world, a dev map without a match), BroadcastTeam is a local broadcast.
	void BroadcastTeam(const FGameplayTag& Tag, const FInstancedStruct& Payload);

	template <typename T>
	void BroadcastTeam(const FGameplayTag& Tag, const T& Payload)
	{
		BroadcastTeam(Tag, FInstancedStruct::Make<T>(Payload));
	}

	using FTeamRelay = TFunction<void(const FGameplayTag& /*Tag*/, const FInstancedStruct& /*Payload*/)>;
	/** The host's relay registers here (and passes nullptr to unregister). One relay per game instance. */
	void SetTeamRelay(FTeamRelay InRelay) { TeamRelay = MoveTemp(InRelay); }
	bool HasTeamRelay() const { return static_cast<bool>(TeamRelay); }

	/** Number of messages broadcast since startup (tests and the INT smoke read it). */
	int64 GetBroadcastCount() const { return BroadcastCount; }

private:
	struct FSubscription
	{
		int32 Id = 0;
		bool bIncludeChildren = true;
		FDFMessageDelegate Callback;
	};

	FTeamRelay TeamRelay;
	TMap<FGameplayTag, TArray<FSubscription>> Subscriptions;
	int32 NextId = 1;
	int64 BroadcastCount = 0;
};
