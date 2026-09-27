#include "Screens/DFHudScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DFGameplayTags.h"
#include "HAL/IConsoleManager.h"
#include "Messages/DFMessages.h"
#include "Screens/DFHudParts.h"
#include "Screens/DFHudStyle.h"
#include "Screens/DFHudText.h"
#include "Screens/DFUITags.h"
#include "ViewModels/DFMatchViewModel.h"
#include "ViewModels/DFPlayerViewModel.h"

#define LOCTEXT_NAMESPACE "DFHud"

namespace
{
	/** The width of the hp bar, px at 1080p (hud-rail-w is the design's side rail; the bar is most of it). */
	constexpr float HpBarWidth = 240.f;
	constexpr float HpBarHeight = 10.f;

	// A feed writes every frame and so does Refresh; these keep a frame with no change from invalidating
	// the widgets it would otherwise touch.
	void SetShown(UWidget* Widget, bool bShown)
	{
		const ESlateVisibility Wanted = bShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
		if (Widget && Widget->GetVisibility() != Wanted)
		{
			Widget->SetVisibility(Wanted);
		}
	}

	void SetTextIfChanged(UTextBlock* Block, const FText& Text)
	{
		if (Block && !Block->GetText().ToString().Equals(Text.ToString(), ESearchCase::CaseSensitive))
		{
			Block->SetText(Text);
		}
	}

	void SetColorIfChanged(UTextBlock* Block, const FLinearColor& Color)
	{
		if (Block && Block->GetColorAndOpacity().GetSpecifiedColor() != Color)
		{
			Block->SetColorAndOpacity(FSlateColor(Color));
		}
	}

	UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* Widget, const FVector2D& Anchor, const FVector2D& Alignment, const FVector2D& Offset)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Widget);
		CanvasSlot->SetAnchors(FAnchors(static_cast<float>(Anchor.X), static_cast<float>(Anchor.Y)));
		CanvasSlot->SetAlignment(Alignment);
		CanvasSlot->SetPosition(Offset);
		CanvasSlot->SetAutoSize(true);
		return CanvasSlot;
	}

#if !UE_BUILD_SHIPPING
	// `DF.UI.SimulateRefusal insufficientFunds`: a DF.Message.BuildRejected on this machine's bus, as
	// Client_Refused would deliver it, to see the toast without hunting for a refusal in play.
	FAutoConsoleCommandWithWorldAndArgs GSimulateRefusal(
		TEXT("DF.UI.SimulateRefusal"),
		TEXT("Broadcast a local DF.Message.BuildRejected with the given reason (default insufficientFunds); the HUD shows its toast."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDFMessageBus* Bus = UDFMessageBus::Get(World))
			{
				FDFMsg_Rejected Refusal;
				Refusal.Reason = FName(Args.Num() > 0 ? *Args[0] : TEXT("insufficientFunds"));
				Bus->Broadcast(DFTags::Message_BuildRejected, Refusal);
			}
		}));
#endif
}

UDFHudScreen::UDFHudScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Default input: the HUD has no opinion, so whatever play set stays set. And it is never a focus
	// target: were CommonUI to focus it on activation, the keyboard would go to a widget, not the hero.
	InputMode = EDFScreenInputMode::Default;
	bSupportsActivationFocus = false;
}

void UDFHudScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// Not a class default: DF_UI.ini's tags are not loaded when a native CDO is built (DFHudParts.h).
	ScreenTag = FDFUITags::Get().Screen_Hud;
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Refresh(GetMatch());
}

void UDFHudScreen::BuildTree()
{
	const FDFHudStyle& Style = FDFHudStyle::Get();

	auto Text = [this](const TCHAR* Name, float Size, const FLinearColor& Color, bool bBold = true)
	{
		UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Block->SetFont(FDFHudStyle::Font(Size, bBold));
		Block->SetColorAndOpacity(FSlateColor(Color));
		return Block;
	};
	// Labels are the design's small caps: bold, upper case, tracked out (tracking-label = .14em).
	auto Label = [&Text, &Style](const TCHAR* Name, const FText& Caption)
	{
		UTextBlock* Block = Text(Name, Style.SizeLabel, Style.TextSecondary);
		FSlateFontInfo Font = FDFHudStyle::Font(Style.SizeLabel);
		Font.LetterSpacing = 140;
		Block->SetFont(Font);
		Block->SetText(Caption);
		return Block;
	};
	auto Panel = [this, &Style](const TCHAR* Name, UWidget* Content, const FLinearColor& Edge)
	{
		UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Border->SetBrush(Style.PanelBrush(Style.Panel, Edge));
		Border->SetPadding(FMargin(Style.PadPanel, Style.PadPanel * 0.75f));
		Border->SetContent(Content);
		return Border;
	};
	auto Gap = [this](float Width, float Height)
	{
		USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		Spacer->SetSize(FVector2D(Width, Height));
		return Spacer;
	};

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HudCanvas"));
	Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Canvas;

	// ---- top left: the wave, its phase, what is left of it
	{
		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("WaveBox"));
		WaveTitle = Text(TEXT("WaveTitle"), Style.SizeTitle, Style.Text);
		PhaseLine = Text(TEXT("PhaseLine"), Style.SizeToast, Style.Accent);
		Box->AddChildToVerticalBox(WaveTitle);
		Box->AddChildToVerticalBox(PhaseLine);

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("EnemiesRow"));
		UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label(TEXT("EnemiesLabel"), LOCTEXT("EnemiesLabel", "ENEMIES")));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		Row->AddChildToHorizontalBox(Gap(Style.Gap, 0.f));
		EnemiesValue = Text(TEXT("EnemiesValue"), Style.SizeValue, Style.Text);
		Row->AddChildToHorizontalBox(EnemiesValue)->SetVerticalAlignment(VAlign_Center);
		EnemiesRow = Row;
		Box->AddChildToVerticalBox(Gap(0.f, Style.Gap * 0.5f));
		Box->AddChildToVerticalBox(Row);

		WavePanel = Panel(TEXT("WavePanel"), Box, Style.PanelEdge);
		Place(Canvas, WavePanel, FVector2D(0.f, 0.f), FVector2D(0.f, 0.f), FVector2D(Style.Edge, Style.Edge));
	}

	// ---- top right: money and lives
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("EconomyRow"));
		auto Stat = [&](const TCHAR* LabelName, const FText& Caption, const TCHAR* ValueName, const FLinearColor& Color, TObjectPtr<UTextBlock>& OutValue)
		{
			UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			Column->AddChildToVerticalBox(Label(LabelName, Caption))->SetHorizontalAlignment(HAlign_Right);
			OutValue = Text(ValueName, Style.SizeValue, Color);
			OutValue->SetJustification(ETextJustify::Right);
			Column->AddChildToVerticalBox(OutValue)->SetHorizontalAlignment(HAlign_Right);
			return Column;
		};
		Row->AddChildToHorizontalBox(Stat(TEXT("MoneyLabel"), LOCTEXT("MoneyLabel", "MONEY"), TEXT("MoneyValue"), Style.Money, MoneyValue));
		Row->AddChildToHorizontalBox(Gap(Style.Gap * 3.f, 0.f));
		Row->AddChildToHorizontalBox(Stat(TEXT("LivesLabel"), LOCTEXT("LivesLabel", "LIVES"), TEXT("LivesValue"), Style.Lives, LivesValue));

		EconomyPanel = Panel(TEXT("EconomyPanel"), Row, Style.PanelEdge);
		Place(Canvas, EconomyPanel, FVector2D(1.f, 0.f), FVector2D(1.f, 0.f), FVector2D(-Style.Edge, Style.Edge));
	}

	// ---- bottom left: the local hero's hp
	{
		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VitalsBox"));
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HpRow"));
		HpLabel = Label(TEXT("HpLabel"), LOCTEXT("HpLabel", "HEALTH"));
		Row->AddChildToHorizontalBox(HpLabel)->SetVerticalAlignment(VAlign_Center);
		UHorizontalBoxSlot* Fill = Row->AddChildToHorizontalBox(Gap(Style.Gap, 0.f));
		Fill->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		HpValue = Text(TEXT("HpValue"), Style.SizeValue, Style.Text);
		Row->AddChildToHorizontalBox(HpValue)->SetVerticalAlignment(VAlign_Center);
		Box->AddChildToVerticalBox(Row);
		Box->AddChildToVerticalBox(Gap(0.f, Style.Gap * 0.5f));

		HpBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HpBar"));
		FProgressBarStyle BarStyle;
		BarStyle.SetBackgroundImage(FSlateColorBrush(Style.BarTrack));
		BarStyle.SetFillImage(FSlateColorBrush(FLinearColor::White));   // tinted by FillColorAndOpacity
		BarStyle.SetMarqueeImage(FSlateColorBrush(FLinearColor::White));
		HpBar->SetWidgetStyle(BarStyle);
		HpBar->SetFillColorAndOpacity(Style.HpBar);
		HpBar->SetPercent(1.f);
		USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HpBarBox"));
		BarBox->SetWidthOverride(HpBarWidth);
		BarBox->SetHeightOverride(HpBarHeight);
		BarBox->AddChild(HpBar);
		Box->AddChildToVerticalBox(BarBox);

		VitalsPanel = Panel(TEXT("VitalsPanel"), Box, Style.PanelEdge);
		Place(Canvas, VitalsPanel, FVector2D(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(Style.Edge, -Style.Edge));
	}

	// ---- bottom right: the local hero's magazine
	{
		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AmmoBox"));
		Box->AddChildToVerticalBox(Label(TEXT("AmmoLabel"), LOCTEXT("AmmoLabel", "AMMO")))->SetHorizontalAlignment(HAlign_Right);
		AmmoValue = Text(TEXT("AmmoValue"), Style.SizeValue, Style.Text);
		AmmoValue->SetJustification(ETextJustify::Right);
		Box->AddChildToVerticalBox(AmmoValue)->SetHorizontalAlignment(HAlign_Right);
		AmmoPanel = Panel(TEXT("AmmoPanel"), Box, Style.PanelEdge);
		Place(Canvas, AmmoPanel, FVector2D(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-Style.Edge, -Style.Edge));
	}

	// ---- centre: the crosshair part
	{
		UDFCrosshairPart* Crosshair = WidgetTree->ConstructWidget<UDFCrosshairPart>(UDFCrosshairPart::StaticClass(), TEXT("Part_Crosshairs"));
		Place(Canvas, Crosshair, FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	}

	// ---- bottom centre: the toast over the prompt part
	{
		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PromptBox"));
		ToastText = Text(TEXT("ToastText"), Style.SizeToast, Style.Text);
		ToastText->SetJustification(ETextJustify::Center);
		Toast = Panel(TEXT("Toast"), ToastText, Style.Danger);
		Box->AddChildToVerticalBox(Toast)->SetHorizontalAlignment(HAlign_Center);
		Box->AddChildToVerticalBox(Gap(0.f, Style.Gap));
		UDFPromptPart* Prompt = WidgetTree->ConstructWidget<UDFPromptPart>(UDFPromptPart::StaticClass(), TEXT("Part_Prompts"));
		Box->AddChildToVerticalBox(Prompt)->SetHorizontalAlignment(HAlign_Center);
		SetShown(Toast, false);
		Place(Canvas, Box, FVector2D(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -Style.Edge * 2.f));
	}

	// ---- above centre: the end-of-match banner
	{
		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BannerBox"));
		BannerTitle = Text(TEXT("BannerTitle"), Style.SizeBanner, Style.Accent);
		BannerTitle->SetJustification(ETextJustify::Center);
		BannerDetail = Text(TEXT("BannerDetail"), Style.SizeToast, Style.TextSecondary);
		BannerDetail->SetJustification(ETextJustify::Center);
		BannerRestart = Text(TEXT("BannerRestart"), Style.SizeToast, Style.Accent);
		BannerRestart->SetJustification(ETextJustify::Center);
		Box->AddChildToVerticalBox(BannerTitle)->SetHorizontalAlignment(HAlign_Center);
		Box->AddChildToVerticalBox(BannerDetail)->SetHorizontalAlignment(HAlign_Center);
		Box->AddChildToVerticalBox(Gap(0.f, Style.Gap));
		Box->AddChildToVerticalBox(BannerRestart)->SetHorizontalAlignment(HAlign_Center);
		UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BannerPanel"));
		Border->SetBrush(Style.PanelBrush(Style.Scrim, Style.PanelEdge));
		Border->SetPadding(FMargin(Style.PadPanel * 4.f, Style.PadPanel * 1.5f));
		Border->SetContent(Box);
		BannerPanel = Border;
		SetShown(BannerPanel, false);
		Place(Canvas, BannerPanel, FVector2D(0.5f, 0.38f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	}
}

void UDFHudScreen::NativeConstruct()
{
	Super::NativeConstruct();
	Unsubscribe();
	UDFMessageBus* Bus = UDFMessageBus::Get(this);
	if (!Bus)
	{
		return;
	}
	// The build verbs' refusals. They reach only the client that asked (Client_Refused), so every one
	// that arrives here is this player's.
	const FGameplayTag Refusals[] = { DFTags::Message_BuildRejected, DFTags::Message_UpgradeRejected, DFTags::Message_SellRejected };
	for (const FGameplayTag& Tag : Refusals)
	{
		RefusalHandles.Add(Bus->Subscribe<FDFMsg_Rejected>(Tag,
			[WeakThis = TWeakObjectPtr<UDFHudScreen>(this)](const FGameplayTag&, const FDFMsg_Rejected& Msg)
			{
				if (UDFHudScreen* Hud = WeakThis.Get())
				{
					Hud->ShowToast(DFHudText::Refusal(Msg.Reason));
				}
			}, /*bIncludeChildren*/ false));
	}
}

void UDFHudScreen::NativeDestruct()
{
	Unsubscribe();
	Super::NativeDestruct();
}

void UDFHudScreen::Unsubscribe()
{
	if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
	{
		for (const FDFMessageHandle& Handle : RefusalHandles)
		{
			Bus->Unsubscribe(Handle);
		}
	}
	RefusalHandles.Reset();
}

void UDFHudScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Refresh(GetMatch());
	TickToast(InDeltaTime);
}

void UDFHudScreen::ShowToast(const FText& InText)
{
	SetTextIfChanged(ToastText, InText);
	ToastAge = 0.f;
	if (Toast)
	{
		Toast->SetRenderOpacity(1.f);
	}
	SetShown(Toast, true);
}

void UDFHudScreen::TickToast(float DeltaSeconds)
{
	if (ToastAge < 0.f || !Toast)
	{
		return;
	}
	ToastAge += DeltaSeconds;
	const float Fade = FMath::Clamp((ToastAge - ToastHoldSeconds) / ToastFadeSeconds, 0.f, 1.f);
	Toast->SetRenderOpacity(1.f - Fade);
	if (Fade >= 1.f)
	{
		ToastAge = -1.f;
		SetShown(Toast, false);
	}
}

void UDFHudScreen::Refresh(const UDFMatchViewModel* Match)
{
	const FDFHudStyle& Style = FDFHudStyle::Get();
	// Nothing match-shaped until a feed has written a whole state (bValid, C12); the crosshair and the
	// prompt part look after themselves.
	const bool bValid = Match && Match->IsValid();
	SetShown(WavePanel, bValid);
	SetShown(EconomyPanel, bValid);
	if (!bValid)
	{
		SetShown(VitalsPanel, false);
		SetShown(AmmoPanel, false);
		SetShown(BannerPanel, false);
		return;
	}

	// ---- wave
	const EDFMatchPhase Phase = Match->GetPhase();
	SetTextIfChanged(WaveTitle, DFHudText::WaveTitle(*Match));
	SetTextIfChanged(PhaseLine, DFHudText::PhaseLine(*Match));
	SetColorIfChanged(PhaseLine, Phase == EDFMatchPhase::Wave ? Style.WaveActive : Phase == EDFMatchPhase::Defeat ? Style.Danger : Style.Accent);
	const bool bEnemies = Phase == EDFMatchPhase::Wave || Match->GetEnemiesRemaining() > 0;
	SetShown(EnemiesRow, bEnemies);
	SetTextIfChanged(EnemiesValue, FText::AsNumber(Match->GetEnemiesRemaining()));

	// ---- economy
	SetTextIfChanged(MoneyValue, FText::AsNumber(Match->GetMoney()));
	SetTextIfChanged(LivesValue, FText::AsNumber(Match->GetLives()));
	SetColorIfChanged(LivesValue, Match->GetLives() <= LivesAlarm ? Style.Danger : Style.Lives);

	// ---- the local hero
	const UDFPlayerViewModel* Local = Match->Local();
	SetShown(VitalsPanel, Local != nullptr);
	SetShown(AmmoPanel, Local != nullptr && Local->GetMagazineSize() > 0);
	if (Local && Local->GetMagazineSize() > 0)
	{
		SetTextIfChanged(AmmoValue, DFHudText::Ammo(Local->GetAmmoInMagazine(), Local->GetMagazineSize(), Local->IsReloading(), Local->GetReloadFrac()));
		const bool bLow = !Local->IsReloading() && Local->GetAmmoInMagazine() * 4 <= Local->GetMagazineSize();
		SetColorIfChanged(AmmoValue, Local->IsReloading() ? Style.Accent : bLow ? Style.Danger : Style.Text);
	}
	if (Local)
	{
		const float Fraction = Local->HpFrac();
		if (Local->IsDowned())
		{
			SetTextIfChanged(HpLabel, LOCTEXT("HpDowned", "DOWNED"));
			SetColorIfChanged(HpLabel, Style.Danger);
			const int32 Bleed = FMath::CeilToInt(Local->GetBleedoutSecondsLeft());
			SetTextIfChanged(HpValue, Bleed > 0 ? FText::Format(LOCTEXT("HpBleed", "{0}s"), Bleed) : FText::GetEmpty());
		}
		else
		{
			SetTextIfChanged(HpLabel, LOCTEXT("HpLabel", "HEALTH"));
			SetColorIfChanged(HpLabel, Style.TextSecondary);
			// Rounded up: a hero on 0.4 hp is alive and reads 1, never 0.
			SetTextIfChanged(HpValue, FText::Format(LOCTEXT("HpOf", "{0} / {1}"), FMath::CeilToInt(Local->GetHp()), FMath::RoundToInt(Local->GetMaxHp())));
		}
		if (HpBar)
		{
			if (!FMath::IsNearlyEqual(HpBar->GetPercent(), Fraction))
			{
				HpBar->SetPercent(Fraction);
			}
			const FLinearColor Fill = Fraction < LowHpFraction ? Style.HpLow : Style.HpBar;
			if (HpBar->GetFillColorAndOpacity() != Fill)
			{
				HpBar->SetFillColorAndOpacity(Fill);
			}
		}
	}

	// ---- the end
	const bool bOver = Phase == EDFMatchPhase::Victory || Phase == EDFMatchPhase::Defeat;
	SetShown(BannerPanel, bOver);
	if (bOver)
	{
		SetTextIfChanged(BannerTitle, DFHudText::Banner(Phase));
		SetColorIfChanged(BannerTitle, Phase == EDFMatchPhase::Victory ? Style.Accent : Style.Danger);
		SetTextIfChanged(BannerDetail, DFHudText::BannerDetail(*Match));
		const FText Restart = DFHudText::RestartLine(*Match);
		SetShown(BannerRestart, !Restart.IsEmpty());
		SetTextIfChanged(BannerRestart, Restart);
	}
}

#undef LOCTEXT_NAMESPACE
