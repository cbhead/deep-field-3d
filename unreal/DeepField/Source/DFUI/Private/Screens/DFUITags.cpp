#include "Screens/DFUITags.h"

#include "GameplayTagsManager.h"

namespace
{
	FGameplayTag Request(const TCHAR* Name)
	{
		return FGameplayTag::RequestGameplayTag(FName(Name), /*ErrorIfNotFound*/ false);
	}
}

const FDFUITags& FDFUITags::Get()
{
	static const FDFUITags Tags;
	return Tags;
}

FDFUITags::FDFUITags()
	: Layer_Game(Request(TEXT("DF.UI.Layer.Game")))
	, Layer_GameMenu(Request(TEXT("DF.UI.Layer.GameMenu")))
	, Layer_Menu(Request(TEXT("DF.UI.Layer.Menu")))
	, Layer_Modal(Request(TEXT("DF.UI.Layer.Modal")))
{
	AllLayers = { Layer_Game, Layer_GameMenu, Layer_Menu, Layer_Modal };
#define DF_UI_SCREEN(Name, Str, Layer) \
	Screen_##Name = Request(TEXT(Str)); \
	AllScreens.Add(Screen_##Name); \
	ScreenLayers.Add(Screen_##Name, Layer_##Layer);
#include "Screens/DFUIScreenList.inl"
#undef DF_UI_SCREEN

#define DF_UI_PART(Name, Str) \
	Part_##Name = Request(TEXT(Str)); \
	AllParts.Add(Part_##Name);
#include "Screens/DFUIPartList.inl"
#undef DF_UI_PART
}

int32 FDFUITags::ScreenCountOn(const FGameplayTag& Layer) const
{
	int32 Count = 0;
	for (const TPair<FGameplayTag, FGameplayTag>& Pair : ScreenLayers)
	{
		Count += Pair.Value == Layer ? 1 : 0;
	}
	return Count;
}

TArray<FString> FDFUITags::DeclaredNames()
{
	TArray<FString> Names = { TEXT("DF.UI.Layer.Game"), TEXT("DF.UI.Layer.GameMenu"), TEXT("DF.UI.Layer.Menu"), TEXT("DF.UI.Layer.Modal") };
#define DF_UI_SCREEN(Name, Str, Layer) Names.Add(TEXT(Str));
#include "Screens/DFUIScreenList.inl"
#undef DF_UI_SCREEN
#define DF_UI_PART(Name, Str) Names.Add(TEXT(Str));
#include "Screens/DFUIPartList.inl"
#undef DF_UI_PART
	return Names;
}
