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

#define DF_UI_PART(Name, Str) FGameplayTag Part_##Name;
#include "Screens/DFUIPartList.inl"
#undef DF_UI_PART

	/** Bottom to top. */
	const TArray<FGameplayTag>& Layers() const { return AllLayers; }
	const TArray<FGameplayTag>& Screens() const { return AllScreens; }

	/** Children of the HUD layout: visible alongside it and each other, never pushed to a layer. */
	const TArray<FGameplayTag>& Parts() const { return AllParts; }

	/** Screens that share a layer hide one another, so a layer that must show several things at
	 *  once is a design error. Layer.Game is the one that must: it holds the HUD alone. */
	int32 ScreenCountOn(const FGameplayTag& Layer) const;

	/** The layer a screen lives on; invalid for anything that is not a screen. */
	FGameplayTag LayerOf(const FGameplayTag& Screen) const { return ScreenLayers.FindRef(Screen); }

	/** The tag strings as written in code, for the test that compares them with the ini. */
	static TArray<FString> DeclaredNames();

private:
	FDFUITags();
	TArray<FGameplayTag> AllLayers;
	TArray<FGameplayTag> AllScreens;
	TArray<FGameplayTag> AllParts;
	TMap<FGameplayTag, FGameplayTag> ScreenLayers;
};
