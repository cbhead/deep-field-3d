#include "Cues/DFGameplayCueNotify_Status.h"

#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFCues, Log, All);

FString UDFGameplayCueNotify_Status::VerbOf(const FGameplayTag& CueTag)
{
	FString Leaf = CueTag.ToString();
	int32 Dot = INDEX_NONE;
	return Leaf.FindLastChar(TEXT('.'), Dot) ? Leaf.RightChop(Dot + 1) : FString();
}

bool UDFGameplayCueNotify_Status::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	// OriginalTag is the leaf the component fired; GameplayCueTag is what this notify registered for
	// (the family root GameplayCue.DF.Status on the native fallback).
	const FGameplayTag Cue = Parameters.OriginalTag.IsValid() ? Parameters.OriginalTag : GameplayCueTag;
	if (VerbOf(Cue) == TEXT("Tick"))
	{
		UE_LOG(LogDFCues, VeryVerbose, TEXT("%s on %s: %.3f"), *Cue.ToString(), *GetNameSafe(MyTarget), Parameters.RawMagnitude);
	}
	else
	{
		UE_LOG(LogDFCues, Log, TEXT("%s on %s (magnitude %.2f, handled by %s)"), *Cue.ToString(), *GetNameSafe(MyTarget), Parameters.RawMagnitude, *GetClass()->GetName());
	}
	return true;
}
