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
};
