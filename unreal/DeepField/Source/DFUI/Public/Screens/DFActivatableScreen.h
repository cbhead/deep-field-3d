#pragma once

#include "CommonActivatableWidget.h"
#include "GameplayTagContainer.h"
#include "DFActivatableScreen.generated.h"

class UDFMatchViewModel;

/** What a screen wants from input while it is the top active one. */
UENUM(BlueprintType)
enum class EDFScreenInputMode : uint8
{
	/** No opinion: whatever is active beneath decides (HUD parts). */
	Default,
	/** The game keeps the mouse; the screen only listens for its own actions (wheel held open). */
	Game,
	/** Cursor free, game input still flows (armory, upgrade panel - play continues around them). */
	GameAndMenu,
	/** The screen has all input (lobby, pause, modals). */
	Menu,
};

/** Base of every DF screen (`WBP_<Screen>` derives from this, Appendix C§6). Carries the screen's
 *  identity (its DF.UI.Screen.* tag, which also fixes its layer) and its input wish; hands
 *  subclasses the match view model. Layout and bindings are the WBP's business, logic is not:
 *  a screen with more than glue in its graph belongs in a C++ subclass of this. */
UCLASS(Abstract, Blueprintable)
class DFUI_API UDFActivatableScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	const FGameplayTag& GetScreenTag() const { return ScreenTag; }

	/** The layer this screen is pushed to: FDFUITags::LayerOf(ScreenTag). */
	UFUNCTION(BlueprintPure, Category = "DF|UI")
	FGameplayTag GetLayerTag() const;

	/** The one match view model (UDFViewModelSubsystem); null only outside a game instance. */
	UFUNCTION(BlueprintPure, Category = "DF|UI")
	UDFMatchViewModel* GetMatch() const;

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "DF|Screen", meta = (Categories = "DF.UI.Screen"))
	FGameplayTag ScreenTag;

	UPROPERTY(EditDefaultsOnly, Category = "DF|Screen")
	EDFScreenInputMode InputMode = EDFScreenInputMode::Default;

	/** Mouse capture while InputMode is Game or GameAndMenu. */
	UPROPERTY(EditDefaultsOnly, Category = "DF|Screen")
	EMouseCaptureMode GameMouseCaptureMode = EMouseCaptureMode::CapturePermanently;
};
