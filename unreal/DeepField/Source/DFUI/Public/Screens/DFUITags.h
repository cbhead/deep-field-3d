#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/** WS-12's local tags (Config/Tags/DF_UI.ini, ADR-0009), resolved once. A tag missing from the ini
 *  resolves invalid and DF.UI.Screens.TagsResolve says which. */
struct DFUI_API FDFUITags
{
	static const FDFUITags& Get();

	FGameplayTag Layer_Game;
	FGameplayTag Layer_GameMenu;
	FGameplayTag Layer_Menu;
	FGameplayTag Layer_Modal;

#define DF_UI_SCREEN(Name, Str, Layer) FGameplayTag Screen_##Name;
#include "Screens/DFUIScreenList.inl"
#undef DF_UI_SCREEN

	/** Bottom to top. */
	const TArray<FGameplayTag>& Layers() const { return AllLayers; }
	const TArray<FGameplayTag>& Screens() const { return AllScreens; }

	/** The layer a screen lives on; invalid for anything that is not a screen. */
	FGameplayTag LayerOf(const FGameplayTag& Screen) const { return ScreenLayers.FindRef(Screen); }

	/** The tag strings as written in code, for the test that compares them with the ini. */
	static TArray<FString> DeclaredNames();

private:
	FDFUITags();
	TArray<FGameplayTag> AllLayers;
	TArray<FGameplayTag> AllScreens;
	TMap<FGameplayTag, FGameplayTag> ScreenLayers;
};
