#pragma once

#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"
#include "DFUIPart.generated.h"

class UDFMatchViewModel;

/** Base of a HUD part: a `WBP_Part_<Name>` that lives *inside* the HUD layout and is visible at the
 *  same time as it and as the other parts (crosshairs over overheads over prompts...). A part is
 *  never pushed to a layer - that is what makes it a part and not a screen (DFUIPartList.inl).
 *
 *  It is a plain CommonUI widget: no activation, no input config, no focus. The HUD layout shows and
 *  hides it. Like a screen, it reads the match view model and nothing else. */
UCLASS(Abstract, Blueprintable)
class DFUI_API UDFUIPart : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	const FGameplayTag& GetPartTag() const { return PartTag; }

	/** The one match view model (UDFViewModelSubsystem); null only outside a game instance. */
	UFUNCTION(BlueprintPure, Category = "DF|UI")
	UDFMatchViewModel* GetMatch() const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "DF|Part", meta = (Categories = "DF.UI.Part"))
	FGameplayTag PartTag;
};
