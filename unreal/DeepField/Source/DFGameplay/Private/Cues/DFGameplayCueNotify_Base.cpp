#include "Cues/DFGameplayCueNotify_Base.h"

#include "AbilitySystemGlobals.h"
#include "Cues/DFGameplayCueNotify_Reaction.h"
#include "Cues/DFGameplayCueNotify_Status.h"
#include "DFGameplayLocalTags.h"
#include "GameFramework/Actor.h"
#include "GameplayCueManager.h"
#include "GameplayCueSet.h"
#include "GameplayEffectTypes.h"
#include "Messages/DFMessageBus.h"
#include "Status/DFStatusComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFCues, Log, All);

namespace
{
	EDFCueEvent ToDFEvent(EGameplayCueEvent::Type EventType)
	{
		switch (EventType)
		{
		case EGameplayCueEvent::OnActive:    return EDFCueEvent::Active;
		case EGameplayCueEvent::WhileActive: return EDFCueEvent::WhileActive;
		case EGameplayCueEvent::Removed:     return EDFCueEvent::Removed;
		case EGameplayCueEvent::Executed:
		default:                             return EDFCueEvent::Executed;
		}
	}

	struct FNativeCue
	{
		FGameplayTag Tag;
		UClass* Class;
	};
}

void UDFGameplayCueNotify_Base::RegisterNativeCues()
{
	UGameplayCueManager* Manager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!Manager)
	{
		return;
	}
	UGameplayCueSet* Set = Manager->GetRuntimeCueSet();
	if (!Set)
	{
		// The engine builds the runtime library at OnEngineInitComplete; an ASC that comes up before
		// that (or in a process that never calls InitGlobalData) builds it now — the scan is
		// synchronous (ShouldDeferScanningRuntimeLibraries is false) and idempotent.
		Manager->InitializeRuntimeObjectLibrary();
		Set = Manager->GetRuntimeCueSet();
		if (!Set)
		{
			return;
		}
	}
	const FNativeCue Natives[] = {
		{ DFGameplayLocalTags::StatusCueRoot(),   UDFGameplayCueNotify_Status::StaticClass() },
		{ DFGameplayLocalTags::ReactionCueRoot(), UDFGameplayCueNotify_Reaction::StaticClass() },
	};
	TArray<FGameplayCueReferencePair> ToAdd;
	for (const FNativeCue& Native : Natives)
	{
		const FGameplayTag& Tag = Native.Tag;
		if (!Tag.IsValid())
		{
			continue;   // no leaf of that family in Config/Tags: nothing to handle
		}
		const bool bRegistered = Set->GameplayCueData.ContainsByPredicate([&Tag](const FGameplayCueNotifyData& Data) { return Data.GameplayCueTag == Tag; });
		if (!bRegistered)
		{
			ToAdd.Emplace(Tag, FSoftObjectPath(Native.Class));
		}
	}
	if (ToAdd.Num() > 0)
	{
		Set->AddCues(ToAdd);
		for (const FGameplayCueReferencePair& Pair : ToAdd)
		{
			UE_LOG(LogDFCues, Log, TEXT("native cue handler %s -> %s"), *Pair.GameplayCueTag.ToString(), *Pair.StringRef.ToString());
		}
	}
}

int32 UDFGameplayCueNotify_Base::TargetIdOf(const AActor* Target)
{
	const UDFStatusComponent* Status = Target ? Target->FindComponentByClass<UDFStatusComponent>() : nullptr;
	return Status ? Status->TargetId : 0;
}

void UDFGameplayCueNotify_Base::ForwardToBus(AActor* MyTarget, EDFCueEvent Event, const FGameplayCueParameters& Parameters) const
{
	UDFMessageBus* Bus = MyTarget ? UDFMessageBus::Get(MyTarget) : nullptr;
	if (!Bus)
	{
		return;
	}
	FDFMsg_GameplayCue Msg;
	Msg.Target = MyTarget;
	Msg.TargetId = TargetIdOf(MyTarget);
	Msg.Cue = Parameters.OriginalTag.IsValid() ? Parameters.OriginalTag : GameplayCueTag;
	Msg.HandlerTag = Parameters.MatchedTagName.IsValid() ? Parameters.MatchedTagName : GameplayCueTag;
	Msg.Event = Event;
	Msg.Instigator = Parameters.GetInstigator();
	Msg.RawMagnitude = Parameters.RawMagnitude;
	Msg.NormalizedMagnitude = Parameters.NormalizedMagnitude;
	const FVector CueLocation(Parameters.Location);
	Msg.Location = CueLocation.IsNearlyZero() ? MyTarget->GetActorLocation() : CueLocation;
	if (Msg.Cue.IsValid())
	{
		Bus->Broadcast(Msg.Cue, Msg);
	}
}

void UDFGameplayCueNotify_Base::HandleGameplayCue(AActor* MyTarget, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
	Super::HandleGameplayCue(MyTarget, EventType, Parameters);   // OnExecute / OnActive / ... in C++ and Blueprint
	if (bForwardToMessageBus)
	{
		ForwardToBus(MyTarget, ToDFEvent(EventType), Parameters);
	}
}
