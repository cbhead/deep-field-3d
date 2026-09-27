#pragma once

#include "Screens/DFUIPart.h"
#include "DFHudParts.generated.h"

class UTextBlock;

// The first HUD's parts, built in code (no WBP_Part_ yet): children of UDFHudScreen, drawn at the same
// time as it (DFUIPartList.inl). A part's PartTag cannot be written into a native class default -
// DF_UI.ini's tags are not loaded yet when the CDO is built - so each takes its tag on initialize.

/** DF.UI.Part.Crosshairs, first state only: a centre dot and four short ticks in text-primary. The six
 *  states (build, sell, revive, ...) arrive with the weapons and the build wheel. */
UCLASS(NotBlueprintable)
class DFUI_API UDFCrosshairPart : public UDFUIPart
{
	GENERATED_BODY()

public:
	/** Width and height of the drawn cross, px at 1080p. */
	static constexpr float Size = 32.f;

protected:
	virtual void NativeOnInitialized() override;
};

/** DF.UI.Part.Prompts, first line of the hold-E chain: what hold E and hold X do until the build wheel
 *  exists (ADFPlayerController binds them itself for now). The tower and its cost come from content. */
UCLASS(NotBlueprintable)
class DFUI_API UDFPromptPart : public UDFUIPart
{
	GENERATED_BODY()

public:
	/** The towers.json id hold-E builds: ADFPlayerController::QuickBuildTowerId's default (DFMatch keeps it
	 *  protected, and it becomes the wheel's choice once the wheel exists). */
	static const FName QuickBuildTowerId;
	/** Its cost when content has not loaded (towers.json lance.cost). */
	static constexpr int32 FallbackCost = 75;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Line;
};
