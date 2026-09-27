#include "Screens/DFHudStyle.h"

#include "Misc/PackageName.h"
#include "Styling/CoreStyle.h"
#include "Tokens/DFUITokenNames.h"
#include "Tokens/DFUITokens.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFHudStyle, Log, All);

namespace
{
	/** Where the importer saves the token asset (ws-12-ui.md, open question on DA_UITokens). */
	const TCHAR* TokensPackage = TEXT("/Game/DF/UI/DA_UITokens");
	const TCHAR* TokensObject = TEXT("/Game/DF/UI/DA_UITokens.DA_UITokens");

	FLinearColor Srgb(const TCHAR* Hex, float Alpha = 1.f)
	{
		FLinearColor Color = FLinearColor::FromSRGBColor(FColor::FromHex(Hex));
		Color.A = Alpha;
		return Color;
	}
}

FDFHudStyle FDFHudStyle::From(const UDFUITokens* Tokens)
{
	// Has() first: Color()/Length() log a missing token as an error, which is right for a WBP_ style
	// and wrong for this HUD (see the header).
	auto Color = [Tokens](FName Name, const FLinearColor& BuiltIn)
	{
		return Tokens && Tokens->Has(Name, EDFUITokenKind::Color) ? Tokens->Color(Name) : BuiltIn;
	};
	auto Length = [Tokens](FName Name, float BuiltIn)
	{
		return Tokens && Tokens->Has(Name, EDFUITokenKind::Length) ? Tokens->Length(Name) : BuiltIn;
	};

	// The built-in values are colors.css / spacing.css / typography.css, var() resolved by hand.
	FDFHudStyle Style;
	Style.Panel = Color(DFTokens::SurfaceGlass, Srgb(TEXT("131926"), 0.72f));
	Style.PanelEdge = Color(DFTokens::BorderPanel, Srgb(TEXT("2A3348")));
	Style.Scrim = Color(DFTokens::SurfaceOverlay, Srgb(TEXT("080B11"), 0.78f));
	Style.Text = Color(DFTokens::TextPrimary, Srgb(TEXT("ECF0F7")));
	Style.TextSecondary = Color(DFTokens::TextSecondary, Srgb(TEXT("A6B2C6")));
	Style.Accent = Color(DFTokens::TextAccent, Srgb(TEXT("E3BC66")));
	Style.Money = Color(DFTokens::ResGold, Srgb(TEXT("C89B3C")));
	Style.Lives = Color(DFTokens::Lives, Srgb(TEXT("E9614C")));
	Style.Danger = Color(DFTokens::StateDanger, Srgb(TEXT("C93B28")));
	Style.WaveIdle = Color(DFTokens::WaveIdle, Srgb(TEXT("5A6880")));
	Style.WaveActive = Color(DFTokens::WaveActive, Srgb(TEXT("E8862B")));
	Style.HpBar = Color(DFTokens::BarHp, Srgb(TEXT("7BC043")));
	Style.HpLow = Color(DFTokens::BarHpLow, Srgb(TEXT("C93B28")));
	Style.BarTrack = Color(DFTokens::BarTrack, Srgb(TEXT("0A0E16")));

	Style.Edge = Length(DFTokens::HudEdge, 16.f);
	Style.PadPanel = Length(DFTokens::PadPanelTight, 12.f);
	Style.Gap = Length(DFTokens::GapInline, 8.f);
	Style.Stroke = Length(DFTokens::StrokeHairline, 1.f);
	Style.SizeLabel = Length(DFTokens::SizeBodyLg, 16.f);
	Style.SizeValue = Length(DFTokens::SizeDisplaySm, 22.f);
	Style.SizeTitle = Length(DFTokens::SizeDisplayMd, 30.f);
	Style.SizeToast = Length(DFTokens::SizeTitle, 18.f);
	Style.SizeBanner = Length(DFTokens::SizeDisplayXl, 56.f);

	Style.Source = Tokens ? Tokens->GetName() : TEXT("built-in");
	return Style;
}

const FDFHudStyle& FDFHudStyle::Get()
{
	static const FDFHudStyle Style = []
	{
		if (FPackageName::DoesPackageExist(TokensPackage))
		{
			if (const UDFUITokens* Asset = LoadObject<UDFUITokens>(nullptr, TokensObject))
			{
				FDFHudStyle FromAsset = From(Asset);
				FromAsset.Source = TEXT("DA_UITokens");
				UE_LOG(LogDFHudStyle, Log, TEXT("HUD style from %s"), TokensObject);
				return FromAsset;
			}
		}
		// No asset yet: the importer's own entry point on the CSS, in place. The object only lives for
		// this call; the values are copied out before anything could collect it.
		UDFUITokens* Parsed = NewObject<UDFUITokens>(GetTransientPackage());
		TArray<FString> Errors;
		const FString Directory = UDFUITokens::DesignSystemDir();
		if (Parsed->FillFromDesignSystem(Directory, Errors))
		{
			FDFHudStyle FromCss = From(Parsed);
			FromCss.Source = TEXT("docs/design-system");
			UE_LOG(LogDFHudStyle, Log, TEXT("HUD style from %s (no DA_UITokens yet)"), *Directory);
			return FromCss;
		}
		UE_LOG(LogDFHudStyle, Log, TEXT("HUD style: no DA_UITokens and no readable design system at %s (%d problem(s)); using the built-in token values"),
			*Directory, Errors.Num());
		return From(nullptr);
	}();
	return Style;
}

FSlateFontInfo FDFHudStyle::Font(float CssPx, bool bBold)
{
	// Slate font sizes are points at 96 DPI (1 pt = 4/3 px), the tokens are px. No outline: every line
	// sits on a panel, and an outline turns small text to mush once 720p scales it by two thirds.
	return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", FMath::Max(1, FMath::RoundToInt(CssPx * 0.75f)));
}

FSlateBrush FDFHudStyle::PanelBrush(const FLinearColor& InFill, const FLinearColor& InEdge) const
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.TintColor = FSlateColor(InFill);
	Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(2.f, 2.f, 2.f, 2.f), FSlateColor(InEdge), Stroke);
	return Brush;
}
