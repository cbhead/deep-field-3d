#include "DFGameplayLocalTags.h"

#include "GameplayTagsManager.h"

namespace
{
	FGameplayTag Cached(const TCHAR* Name)
	{
		// ErrorIfNotFound=false: an absent ini tag is a missing cue mapping, not a crash.
		return UGameplayTagsManager::Get().RequestGameplayTag(FName(Name), /*ErrorIfNotFound*/ false);
	}
}

namespace DFGameplayLocalTags
{
	FGameplayTag SetByCaller_AmmoFactor()
	{
		static const FGameplayTag Tag = Cached(TEXT("DF.SetByCaller.AmmoFactor"));
		return Tag;
	}

	FGameplayTag SetByCaller_WeakPointFactor()
	{
		static const FGameplayTag Tag = Cached(TEXT("DF.SetByCaller.WeakPointFactor"));
		return Tag;
	}

	FGameplayTag SetByCaller_PackAPunchFactor()
	{
		static const FGameplayTag Tag = Cached(TEXT("DF.SetByCaller.PackAPunchFactor"));
		return Tag;
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
		static const FGameplayTag Tag = Cached(TEXT("GameplayCue.DF.Status"));
		return Tag;
	}

	FGameplayTag ReactionCueRoot()
	{
		static const FGameplayTag Tag = Cached(TEXT("GameplayCue.DF.Reaction"));
		return Tag;
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
