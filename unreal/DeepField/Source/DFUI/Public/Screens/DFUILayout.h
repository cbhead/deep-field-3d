#pragma once

#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"
#include "DFUILayout.generated.h"

class UCommonActivatableWidget;
class UCommonActivatableWidgetContainerBase;
class UDFActivatableScreen;

/** The root widget of a player's UI: four stacks, one per DF.UI.Layer.*, bottom to top. The
 *  `WBP_Layout` subclass places the stacks and registers each in its construction graph; screens
 *  are pushed by class and land on the layer their ScreenTag names, so no caller picks a layer.
 *  Refusals (an unregistered layer, a screen with no tag) are logged errors and a null return.
 *
 *  Created from this class itself (no WBP_, no designer tree), it builds the same four stacks in
 *  code, full-screen and bottom to top, so the game has a working layout before WBP_Layout exists
 *  (UDFUIRootSubsystem puts one on screen per local player). */
UCLASS(Blueprintable)
class DFUI_API UDFUILayout : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

public:
	/** One container per layer tag; registering a second for the same layer is refused. */
	UFUNCTION(BlueprintCallable, Category = "DF|UI")
	bool RegisterLayer(UPARAM(meta = (Categories = "DF.UI.Layer")) FGameplayTag LayerTag, UCommonActivatableWidgetContainerBase* Container);

	UFUNCTION(BlueprintPure, Category = "DF|UI")
	UCommonActivatableWidgetContainerBase* GetLayer(FGameplayTag LayerTag) const;

	/** Every DF.UI.Layer.* has a container. */
	UFUNCTION(BlueprintPure, Category = "DF|UI")
	bool HasAllLayers() const;

	/** Pushes a screen onto the layer its ScreenTag belongs to. A layer shows one widget at a time,
	 *  so pushing a screen that is already open would bury the live one under an identical copy:
	 *  that is refused, and the open instance is returned instead. */
	UFUNCTION(BlueprintCallable, Category = "DF|UI")
	UDFActivatableScreen* PushScreen(TSubclassOf<UDFActivatableScreen> ScreenClass);

	/** For the rare widget that is not a DF screen (an engine dialog). */
	UFUNCTION(BlueprintCallable, Category = "DF|UI")
	UCommonActivatableWidget* PushToLayer(UPARAM(meta = (Categories = "DF.UI.Layer")) FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass);

	/** Removes a pushed widget from whichever layer holds it. */
	UFUNCTION(BlueprintCallable, Category = "DF|UI")
	bool PopScreen(UCommonActivatableWidget* Screen);

	UFUNCTION(BlueprintCallable, Category = "DF|UI")
	void ClearLayer(UPARAM(meta = (Categories = "DF.UI.Layer")) FGameplayTag LayerTag);

	/** The open instance of a screen, by its DF.UI.Screen.* tag; null if it is not open. If several
	 *  are somehow stacked, the topmost (the displayed one) is returned, never a buried one. */
	UFUNCTION(BlueprintPure, Category = "DF|UI")
	UDFActivatableScreen* FindOpenScreen(UPARAM(meta = (Categories = "DF.UI.Screen")) FGameplayTag ScreenTag) const;

private:
	/** The code-only layout: an overlay of four activatable stacks, registered as the four layers. */
	void BuildDefaultLayers();

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UCommonActivatableWidgetContainerBase>> Layers;
};
