#include "Screens/DFUIRootSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"
#include "Screens/DFHudScreen.h"
#include "Screens/DFUILayout.h"
#include "Screens/DFUITags.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFUIRoot, Log, All);

bool UDFUIRootSubsystem::ShouldShowUI(const APlayerController* Controller)
{
	const UWorld* World = Controller ? Controller->GetWorld() : nullptr;
	return World != nullptr && World->IsGameWorld() && FApp::CanEverRender() && !GIsAutomationTesting;
}

void UDFUIRootSubsystem::Deinitialize()
{
	Hide();
	ShownFor.Reset();
	Super::Deinitialize();
}

void UDFUIRootSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);
	// SwitchController and ReceivedPlayer both report the same controller: the second is not a change.
	if (NewPlayerController != nullptr && NewPlayerController == ShownFor.Get())
	{
		return;
	}
	Hide();
	ShownFor = NewPlayerController;
	if (!ShouldShowUI(NewPlayerController))
	{
		return;
	}
	// The controller is handed over in the middle of a login or a map load, before its world has begun
	// play; the layout goes up on that world's next tick. A controller replaced before then is dropped.
	TWeakObjectPtr<UDFUIRootSubsystem> WeakThis(this);
	TWeakObjectPtr<APlayerController> WeakController(NewPlayerController);
	NewPlayerController->GetWorldTimerManager().SetTimerForNextTick([WeakThis, WeakController]()
	{
		UDFUIRootSubsystem* Self = WeakThis.Get();
		APlayerController* Controller = WeakController.Get();
		if (Self && Controller && Self->ShownFor.Get() == Controller && !Self->Layout.IsValid() && ShouldShowUI(Controller))
		{
			Self->Show(Controller);
		}
	});
}

void UDFUIRootSubsystem::Show(APlayerController* Controller)
{
	UDFUILayout* NewLayout = CreateWidget<UDFUILayout>(Controller, UDFUILayout::StaticClass());
	if (!NewLayout)
	{
		UE_LOG(LogDFUIRoot, Error, TEXT("could not create the UI layout for %s"), *GetNameSafe(Controller));
		return;
	}
	// On screen first: a layer stack only accepts a screen once its Slate widgets exist.
	NewLayout->AddToPlayerScreen();
	Layout = NewLayout;
	// PushToLayer, not PushScreen: PushScreen finds the layer from the class default's ScreenTag, and a
	// native screen can only take its tag on initialize (DFHudParts.h). The HUD is Layer.Game's one screen.
	if (!NewLayout->PushToLayer(FDFUITags::Get().Layer_Game, UDFHudScreen::StaticClass()))
	{
		UE_LOG(LogDFUIRoot, Error, TEXT("the HUD could not be pushed for %s"), *GetNameSafe(Controller));
		return;
	}
	UE_LOG(LogDFUIRoot, Log, TEXT("HUD on screen for %s in %s"), *GetNameSafe(Controller), *GetNameSafe(Controller->GetWorld()));
}

void UDFUIRootSubsystem::Hide()
{
	if (UDFUILayout* Old = Layout.Get())
	{
		Old->RemoveFromParent();
	}
	Layout.Reset();
}
