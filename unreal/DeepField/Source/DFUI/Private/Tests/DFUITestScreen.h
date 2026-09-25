#pragma once

#include "Screens/DFActivatableScreen.h"
#include "DFUITestScreen.generated.h"

/** The smallest concrete screen, for DF.UI.Screens.* (the real ones are WBPs). */
UCLASS(NotBlueprintable, Hidden)
class UDFUITestScreen : public UDFActivatableScreen
{
	GENERATED_BODY()

public:
	void Configure(const FGameplayTag& InScreenTag, EDFScreenInputMode InInputMode)
	{
		ScreenTag = InScreenTag;
		InputMode = InInputMode;
	}

	/** UDFUILayout::PushScreen reads the class default object, so a test that pushes this *class*
	 *  has to say which screen the class is. Scoped, because a CDO outlives the test. */
	struct FScopedScreenTag
	{
		explicit FScopedScreenTag(const FGameplayTag& Tag)
			: Previous(GetMutableDefault<UDFUITestScreen>()->ScreenTag)
		{
			GetMutableDefault<UDFUITestScreen>()->ScreenTag = Tag;
		}
		~FScopedScreenTag() { GetMutableDefault<UDFUITestScreen>()->ScreenTag = Previous; }
		FScopedScreenTag(const FScopedScreenTag&) = delete;
		FScopedScreenTag& operator=(const FScopedScreenTag&) = delete;

	private:
		FGameplayTag Previous;
	};
};
