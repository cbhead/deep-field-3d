#pragma once

#include "NativeGameplayTags.h"

// C1 — native contract tags (ADR-0009). Usage: `DFTags::Status_Burn` is an FNativeGameplayTag;
// pass it where an FGameplayTag is expected. The list lives in DFGameplayTagList.inl and the
// message leaf tags in Messages/DFMessageList.inl so both files stay append-only.
namespace DFTags
{
#define DF_TAG(Name, Str) DFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Name);
#include "DFGameplayTagList.inl"
#undef DF_TAG

#define DF_MSG(Name, Str, Struct) DFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_##Name);
#include "Messages/DFMessageList.inl"
#undef DF_MSG

	/** Content id (lower camelCase, as the JSON spells it) -> the PascalCase tag under a root, e.g.
	 *  ("DF.Tower", "emberPistol") -> DF.Tower.EmberPistol. Returns an invalid tag if it does not exist. */
	DFCORE_API FGameplayTag ForContentId(const FString& Root, FName ContentId);
}
