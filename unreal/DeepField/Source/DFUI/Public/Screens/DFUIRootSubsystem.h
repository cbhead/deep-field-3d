#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "DFUIRootSubsystem.generated.h"

class APlayerController;
class UDFUILayout;

/**
 * Puts a player's UI on screen, from DFUI, so nothing below it has to know DFUI exists (DFMatch is a
 * lower layer and cannot name a widget; modules.json). One per local player, so there is none on a
 * dedicated server and none in a unit-test world, which have no local players.
 *
 * Each time the local player is given a controller - joining, or a new map's controller after travel -
 * the previous layout is taken down and, if that controller's world is one that shows UI, a
 * UDFUILayout is put on the player's screen with the HUD (UDFHudScreen) on DF.UI.Layer.Game. One
 * controller, one layout: the same controller reported twice (the engine does) changes nothing, so
 * there is never a second HUD.
 *
 * "Shows UI": a game or PIE world, in a process that renders (FApp::CanEverRender: not -nullrhi, not a
 * dedicated server, not a commandlet), outside an automation run (the landing gate's DF.Func tests play
 * real levels in PIE and must not grow a HUD).
 */
UCLASS()
class DFUI_API UDFUIRootSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	/** This player's UI root while it is on screen; null before, and in worlds that show no UI. Screens
	 *  are pushed through it (UDFUILayout::PushScreen). */
	UDFUILayout* GetLayout() const { return Layout.Get(); }

	/** Whether a controller's world gets a UI at all (see the class comment). */
	static bool ShouldShowUI(const APlayerController* Controller);

private:
	void Show(APlayerController* Controller);
	void Hide();

	/** Weak: the layout belongs to its controller's world, and a reference from here (the local player
	 *  outlives every world) would keep a travelled-from world alive. The viewport keeps it alive. */
	TWeakObjectPtr<UDFUILayout> Layout;
	TWeakObjectPtr<APlayerController> ShownFor;
};
