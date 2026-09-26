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

DEFINE_LOG_CATEGORY(LogDFCues);

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
	// Silence is the failure mode to avoid here: an unregistered family means every DF cue is
	// dropped by the cue set and nothing says so, so each reason warns once per process.
	static bool bWarnedNoManager = false;
	static bool bWarnedNoSet = false;
	static bool bWarnedNoTags = false;

	UGameplayCueManager* Manager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!Manager)
	{
		UE_CLOG(!bWarnedNoManager, LogDFCues, Warning, TEXT("no GameplayCueManager (InitGlobalData not run?): DF cues will not be handled"));
		bWarnedNoManager = true;
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
			UE_CLOG(!bWarnedNoSet, LogDFCues, Warning, TEXT("the GameplayCueManager has no runtime cue set: DF cues will not be handled"));
			bWarnedNoSet = true;
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
			// No leaf of that family in Config/Tags — or the tag was first asked for before the tag
			// manager had loaded the ini, which caches an invalid tag for the process.
			UE_CLOG(!bWarnedNoTags, LogDFCues, Warning, TEXT("a DF cue family root tag is invalid: %s handles nothing"), *GetNameSafe(Native.Class));
			bWarnedNoTags = true;
			continue;
		}
		// O(1), and precise: the acceleration map also carries parent tags pointing at a child's
		// entry, so the entry's own tag has to be the one we are looking for.
		const int32* Index = Set->GameplayCueDataMap.Find(Tag);
		const bool bRegistered = Index && Set->GameplayCueData.IsValidIndex(*Index) && Set->GameplayCueData[*Index].GameplayCueTag == Tag;
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
	static bool bReported = false;
	if (!bReported)
	{
		bReported = true;
		// One line that says whether the cue path can work at all: the families, and whether the ini
		// leaves the component fires resolve (they come from Config/Tags/DF_Gameplay.ini).
		const FGameplayTag SampleStatus = DFGameplayLocalTags::StatusCue(TEXT("chill"), TEXT("Applied"));
		const FGameplayTag SampleReaction = DFGameplayLocalTags::ReactionCue(TEXT("thermalShock"));
		UE_LOG(LogDFCues, Log, TEXT("native cue families: %d added, the runtime cue set holds %d cue(s); leaves: %s=%s, %s=%s"),
			ToAdd.Num(), Set->GameplayCueData.Num(),
			*DFGameplayLocalTags::StatusCueRoot().ToString(), SampleStatus.IsValid() ? TEXT("ok") : TEXT("MISSING"),
			*DFGameplayLocalTags::ReactionCueRoot().ToString(), SampleReaction.IsValid() ? TEXT("ok") : TEXT("MISSING"));
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
