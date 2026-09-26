#include "DFGameplayTags.h"

#include "GameplayTagsManager.h"

namespace DFTags
{
	// UE_DEFINE_GAMEPLAY_TAG static-asserts that __FILE__ ends in ".cpp", which an X-macro
	// list included from an .inl cannot satisfy; this is the macro's own expansion, minus
	// that assert. The list files are included from this .cpp only.
#define DF_TAG(Name, Str) FNativeGameplayTag Name(UE_PLUGIN_NAME, UE_MODULE_NAME, Str, TEXT(""), ENativeGameplayTagToken::PRIVATE_USE_MACRO_INSTEAD);
#include "DFGameplayTagList.inl"
#undef DF_TAG

#define DF_MSG(Name, Str, Struct) FNativeGameplayTag Message_##Name(UE_PLUGIN_NAME, UE_MODULE_NAME, Str, TEXT(""), ENativeGameplayTagToken::PRIVATE_USE_MACRO_INSTEAD);
#include "Messages/DFMessageList.inl"
#undef DF_MSG

	FGameplayTag ForContentId(const FString& Root, FName ContentId)
	{
		FString Id = ContentId.ToString();
		if (Id.IsEmpty())
		{
			return FGameplayTag();
		}
		Id[0] = FChar::ToUpper(Id[0]);
		return UGameplayTagsManager::Get().RequestGameplayTag(FName(*(Root + TEXT(".") + Id)), /*ErrorIfNotFound*/ false);
	}
}
