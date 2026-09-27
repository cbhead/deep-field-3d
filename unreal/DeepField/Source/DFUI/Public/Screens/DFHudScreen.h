#pragma once

#include "Messages/DFMessageBus.h"
#include "Screens/DFActivatableScreen.h"
#include "DFHudScreen.generated.h"

class UBorder;
class UProgressBar;
class UTextBlock;
class UWidget;
struct FDFMsg_Rejected;

/**
 * The first HUD (DF.UI.Screen.Hud, the one screen on DF.UI.Layer.Game), built in code so the game shows
 * something with no WBP_ in the project. Until WBP_HudLayout replaces it, it is the HUD layout: its
 * parts (UDFCrosshairPart, UDFPromptPart) are its children, as DFUIPartList.inl describes.
 *
 * It reads the match view model and nothing else (C12): the wave, its phase and the intermission
 * countdown, enemies remaining, money and lives (top corners), the local hero's hp (bottom left), its
 * gun's magazine (bottom right), and a victory / defeat banner. Refusals are discrete, so they arrive as DF.Message.*Rejected on this
 * client's bus (ADFPlayerController::Client_Refused) and show as a short toast over the prompt.
 *
 * It never takes focus or sets an input mode: play keeps the keyboard and mouse while it is up.
 */
UCLASS(NotBlueprintable)
class DFUI_API UDFHudScreen : public UDFActivatableScreen
{
	GENERATED_BODY()

public:
	UDFHudScreen(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** A toast is fully visible this long, then fades out over ToastFadeSeconds. */
	static constexpr float ToastHoldSeconds = 2.2f;
	static constexpr float ToastFadeSeconds = 0.4f;
	/** Under this fraction of max hp the bar turns bar-hp-low (the design's single threshold, HudRoot.cs). */
	static constexpr float LowHpFraction = 0.3f;
	/** At or under this many lives the count turns state-danger (HudRoot.cs's alarm). */
	static constexpr int32 LivesAlarm = 5;

	/** A one-line notice above the prompt; replaces whatever toast is showing. */
	void ShowToast(const FText& InText);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildTree();
	void Refresh(const UDFMatchViewModel* Match);
	void TickToast(float DeltaSeconds);
	void Unsubscribe();

	UPROPERTY(Transient) TObjectPtr<UWidget> WavePanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WaveTitle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PhaseLine;
	UPROPERTY(Transient) TObjectPtr<UWidget> EnemiesRow;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EnemiesValue;

	UPROPERTY(Transient) TObjectPtr<UWidget> EconomyPanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MoneyValue;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LivesValue;

	UPROPERTY(Transient) TObjectPtr<UWidget> VitalsPanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HpLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HpValue;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> HpBar;
	UPROPERTY(Transient) TObjectPtr<UWidget> AmmoPanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AmmoValue;

	UPROPERTY(Transient) TObjectPtr<UWidget> Toast;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ToastText;

	UPROPERTY(Transient) TObjectPtr<UWidget> BannerPanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BannerTitle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BannerDetail;

	TArray<FDFMessageHandle> RefusalHandles;
	/** Seconds since the toast appeared; negative while none is showing. */
	float ToastAge = -1.f;
};
