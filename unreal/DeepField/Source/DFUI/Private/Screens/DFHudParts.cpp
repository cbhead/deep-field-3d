#include "Screens/DFHudParts.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Content/DFContentSubsystem.h"
#include "Screens/DFHudStyle.h"
#include "Screens/DFHudText.h"
#include "Screens/DFUITags.h"
#include "ViewModels/DFMatchViewModel.h"
#include "ViewModels/DFPlayerViewModel.h"

// ---------------------------------------------------------------------- crosshair

void UDFCrosshairPart::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	PartTag = FDFUITags::Get().Part_Crosshairs;
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	const FDFHudStyle& Style = FDFHudStyle::Get();

	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CrosshairBox"));
	Box->SetWidthOverride(Size);
	Box->SetHeightOverride(Size);
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CrosshairCanvas"));
	Box->AddChild(Canvas);

	// A dot and four ticks with a gap around the centre, each over a dark copy offset down and right,
	// so the cross reads against the sky as well as the lane. px at 1080p (two thirds of that at 720p).
	const float C = Size * 0.5f;
	constexpr float Thick = 3.f;
	constexpr float Gap = 6.f;
	constexpr float Tick = 8.f;
	struct FBar { float X, Y, W, H; };
	const FBar Bars[] = {
		{ C - Thick * 0.5f, C - Thick * 0.5f, Thick, Thick },     // dot
		{ C - Gap - Tick, C - Thick * 0.5f, Tick, Thick },        // left
		{ C + Gap, C - Thick * 0.5f, Tick, Thick },               // right
		{ C - Thick * 0.5f, C - Gap - Tick, Thick, Tick },        // top
		{ C - Thick * 0.5f, C + Gap, Thick, Tick },               // bottom
	};
	auto Add = [this, Canvas](const FBar& Bar, float Offset, const FLinearColor& Color)
	{
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Image->SetColorAndOpacity(Color);
		UCanvasPanelSlot* BarSlot = Canvas->AddChildToCanvas(Image);
		BarSlot->SetPosition(FVector2D(Bar.X + Offset, Bar.Y + Offset));
		BarSlot->SetSize(FVector2D(Bar.W, Bar.H));
	};
	for (const FBar& Bar : Bars)
	{
		Add(Bar, 1.5f, FLinearColor(0.f, 0.f, 0.f, 0.55f));
	}
	for (const FBar& Bar : Bars)
	{
		Add(Bar, 0.f, Style.Text);
	}
	WidgetTree->RootWidget = Box;
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

// ---------------------------------------------------------------------- prompts

const FName UDFPromptPart::QuickBuildTowerId(TEXT("lance"));

void UDFPromptPart::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	PartTag = FDFUITags::Get().Part_Prompts;
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	const FDFHudStyle& Style = FDFHudStyle::Get();

	Line = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PromptLine"));
	Line->SetFont(FDFHudStyle::Font(Style.SizeToast));
	Line->SetColorAndOpacity(FSlateColor(Style.Text));
	Line->SetJustification(ETextJustify::Center);
	ShowChoice(QuickBuildTowerId);
	// On glass: bare text at the bottom of the screen sits on whatever the ground is, often pale.
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PromptPanel"));
	Panel->SetBrush(Style.PanelBrush(Style.Panel, Style.PanelEdge));
	Panel->SetPadding(FMargin(Style.PadPanel, Style.PadPanel * 0.5f));
	Panel->SetContent(Line);
	WidgetTree->RootWidget = Panel;
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UDFPromptPart::ShowChoice(FName TowerId)
{
	ShownChoice = TowerId;
	int32 Cost = FallbackCost;
	if (const UDFContentSubsystem* Content = UDFContentSubsystem::Get(this); Content && Content->IsReady())
	{
		if (const FDFTowerRow* Row = Content->Tower(TowerId))
		{
			Cost = Row->Cost;
		}
	}
	if (Line)
	{
		Line->SetText(DFHudText::BuildHint(TowerId, Cost));
	}
}

void UDFPromptPart::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Building is for a running match; over the victory or defeat banner the line would only be noise.
	const UDFMatchViewModel* Match = GetMatch();
	if (const UDFPlayerViewModel* Me = Match ? Match->Local() : nullptr; Me && !Me->GetBuildChoice().IsNone() && Me->GetBuildChoice() != ShownChoice)
	{
		ShowChoice(Me->GetBuildChoice());
	}
	const bool bShow = Match && Match->IsValid() && !Match->IsLobby()
		&& Match->GetPhase() != EDFMatchPhase::Victory && Match->GetPhase() != EDFMatchPhase::Defeat;
	UWidget* Root = WidgetTree ? WidgetTree->RootWidget.Get() : nullptr;
	const ESlateVisibility Wanted = bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;
	if (Root && Root->GetVisibility() != Wanted)
	{
		Root->SetVisibility(Wanted);
	}
}
