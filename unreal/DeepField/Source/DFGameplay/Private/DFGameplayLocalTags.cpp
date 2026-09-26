#include "DFGameplayLocalTags.h"

#include "GameplayTagsManager.h"
#include "NativeGameplayTags.h"

// The two cue FAMILY ROOTS are native, not ini rows: they are structure, not content —
// UDFGameplayCueNotify_Base registers its fallback handlers on them, and an implicit parent
// of an ini leaf is not requestable, so asking for them by string returns an invalid tag and
// the whole family silently handles nothing.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_DF_Cue_Status, "GameplayCue.DF.Status");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_DF_Cue_Reaction, "GameplayCue.DF.Reaction");

namespace
{
	FGameplayTag Cached(const TCHAR* Name)
	{
		// ErrorIfNotFound=false: an absent ini tag is a missing cue mapping, not a crash.
		return UGameplayTagsManager::Get().RequestGameplayTag(FName(Name), /*ErrorIfNotFound*/ false);
	}

	/** Look the tag up once and keep it — but only once it RESOLVES. A plain `static const FGameplayTag
	 *  Tag = Cached(...)` latches whatever the first caller saw, and a caller that runs before the tag
	 *  manager has read Config/Tags (a CDO, an early InitAbilityActorInfo) latches an INVALID tag for
	 *  the life of the process: the cue families then silently handle nothing. */
	FGameplayTag CachedOnceValid(FGameplayTag& Slot, const TCHAR* Name)
	{
		if (!Slot.IsValid())
		{
			Slot = Cached(Name);
		}
		return Slot;
	}
}

namespace DFGameplayLocalTags
{
	FGameplayTag SetByCaller_AmmoFactor()
	{
		static FGameplayTag Tag;
		return CachedOnceValid(Tag, TEXT("DF.SetByCaller.AmmoFactor"));
	}

	FGameplayTag SetByCaller_WeakPointFactor()
	{
		static FGameplayTag Tag;
		return CachedOnceValid(Tag, TEXT("DF.SetByCaller.WeakPointFactor"));
	}

	FGameplayTag SetByCaller_PackAPunchFactor()
	{
		static FGameplayTag Tag;
		return CachedOnceValid(Tag, TEXT("DF.SetByCaller.PackAPunchFactor"));
	}

	FString PascalCase(FName ContentId)
	{
		FString Id = ContentId.ToString();
		if (!Id.IsEmpty())
		{
			Id[0] = FChar::ToUpper(Id[0]);
		}
		return Id;
	}

	FGameplayTag StatusCue(FName StatusId, const TCHAR* Verb)
	{
		if (StatusId.IsNone())
		{
			return FGameplayTag();
		}
		return Cached(*FString::Printf(TEXT("GameplayCue.DF.Status.%s.%s"), *PascalCase(StatusId), Verb));
	}

	FGameplayTag ReactionCue(FName ReactionId)
	{
		if (ReactionId.IsNone())
		{
			return FGameplayTag();
		}
		return Cached(*FString::Printf(TEXT("GameplayCue.DF.Reaction.%s"), *PascalCase(ReactionId)));
	}

	FGameplayTag StatusCueRoot()
	{
		return TAG_DF_Cue_Status;
	}

	FGameplayTag ReactionCueRoot()
	{
		return TAG_DF_Cue_Reaction;
	}

	FName ContentIdFromTag(const FGameplayTag& Tag)
	{
		if (!Tag.IsValid())
		{
			return NAME_None;
		}
		FString Leaf = Tag.ToString();
		int32 Dot = INDEX_NONE;
		if (Leaf.FindLastChar(TEXT('.'), Dot))
		{
			Leaf.RightChopInline(Dot + 1);
		}
		if (!Leaf.IsEmpty())
		{
			Leaf[0] = FChar::ToLower(Leaf[0]);
		}
		return FName(*Leaf);
	}
}
