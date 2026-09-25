#include "Screens/DFActivatableScreen.h"

#include "Input/UIActionBindingHandle.h"
#include "Screens/DFUITags.h"
#include "ViewModels/DFViewModelSubsystem.h"

FGameplayTag UDFActivatableScreen::GetLayerTag() const
{
	return FDFUITags::Get().LayerOf(ScreenTag);
}

UDFMatchViewModel* UDFActivatableScreen::GetMatch() const
{
	const UDFViewModelSubsystem* ViewModels = UDFViewModelSubsystem::Get(this);
	return ViewModels ? ViewModels->GetMatch() : nullptr;
}

TOptional<FUIInputConfig> UDFActivatableScreen::GetDesiredInputConfig() const
{
	switch (InputMode)
	{
	case EDFScreenInputMode::Game:        return FUIInputConfig(ECommonInputMode::Game, GameMouseCaptureMode);
	case EDFScreenInputMode::GameAndMenu: return FUIInputConfig(ECommonInputMode::All, GameMouseCaptureMode);
	case EDFScreenInputMode::Menu:        return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	case EDFScreenInputMode::Default:
	default:                              return TOptional<FUIInputConfig>();
	}
}
