#include "Messages/DFMessageBus.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"

UDFMessageBus* UDFMessageBus::Get(const UObject* WorldContext)
{
	if (const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr)
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<UDFMessageBus>();
		}
	}
	return nullptr;
}

void UDFMessageBus::BroadcastTeam(const FGameplayTag& Tag, const FInstancedStruct& Payload)
{
	Broadcast(Tag, Payload);
	if (TeamRelay)
	{
		TeamRelay(Tag, Payload);
	}
}

void UDFMessageBus::Broadcast(const FGameplayTag& Tag, const FInstancedStruct& Payload)
{
	++BroadcastCount;
	// Walk the tag and its parents: a subscriber to DF.Message hears every DF.Message.X.
	// Copy the delegate list per tag so a subscriber may (un)subscribe during delivery.
	FGameplayTag Current = Tag;
	bool bExact = true;
	while (Current.IsValid())
	{
		if (const TArray<FSubscription>* Subs = Subscriptions.Find(Current))
		{
			TArray<FSubscription> Snapshot = *Subs;
			for (const FSubscription& S : Snapshot)
			{
				if ((bExact || S.bIncludeChildren) && S.Callback.IsBound())
				{
					S.Callback.Execute(Tag, Payload);
				}
			}
		}
		Current = Current.RequestDirectParent();
		bExact = false;
	}
}

FDFMessageHandle UDFMessageBus::SubscribeRaw(const FGameplayTag& Tag, FDFMessageDelegate&& Callback, bool bIncludeChildren)
{
	FSubscription S;
	S.Id = NextId++;
	S.bIncludeChildren = bIncludeChildren;
	S.Callback = MoveTemp(Callback);
	Subscriptions.FindOrAdd(Tag).Add(MoveTemp(S));
	FDFMessageHandle H;
	H.Tag = Tag;
	H.Id = Subscriptions[Tag].Last().Id;
	return H;
}

void UDFMessageBus::Unsubscribe(const FDFMessageHandle& Handle)
{
	if (TArray<FSubscription>* Subs = Subscriptions.Find(Handle.Tag))
	{
		Subs->RemoveAll([&](const FSubscription& S) { return S.Id == Handle.Id; });
		if (Subs->IsEmpty())
		{
			Subscriptions.Remove(Handle.Tag);
		}
	}
}
