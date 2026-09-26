#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DFUITokens.generated.h"

// The design system as data (Appendix C§6). `docs/design-system/*.css` is the source: every
// `--name: value;` custom property becomes a token here, `var(--x)` already resolved, sorted by
// what the value is. Widgets and styles ask for tokens by name (DFTokens::SurfacePanel, see
// DFUITokenNames.h) and never carry a colour, a size or a duration of their own.
//
// What a value becomes:
//   #RGB / #RRGGBB / #RRGGBBAA / rgb() / rgba()   -> Colors     (sRGB in, FLinearColor out)
//   12px, and a bare 0                            -> Lengths    (px at the 1080p reference)
//   140ms / 1.6s                                  -> Durations  (seconds)
//   cubic-bezier(a, b, c, d)                      -> Easings
//   400 / 1.25 / .14em / 50%                      -> Numbers    (em as a plain factor, % as 0..1)
//   everything else (shadows, gradients, clip     -> Raw        kept verbatim: those are look rules
//   polygons, font stacks, composite type roles)                built as materials and styles
//
// A token that is asked for and missing is an error naming it, once, and a value nobody can
// mistake for a design decision (magenta / 0) — the same rule as the missing-icon chip.

UENUM(BlueprintType)
enum class EDFUITokenKind : uint8 { Color, Length, Duration, Easing, Number, Raw };

UCLASS(BlueprintType)
class DFUI_API UDFUITokens : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tokens") TMap<FName, FLinearColor> Colors;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tokens") TMap<FName, float> Lengths;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tokens") TMap<FName, float> Durations;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tokens") TMap<FName, FVector4> Easings;      // cubic-bezier x1, y1, x2, y2
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tokens") TMap<FName, float> Numbers;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tokens") TMap<FName, FString> Raw;

	UFUNCTION(BlueprintPure, Category = "DF|UI|Tokens") FLinearColor Color(FName Token) const;
	UFUNCTION(BlueprintPure, Category = "DF|UI|Tokens") float Length(FName Token) const;
	UFUNCTION(BlueprintPure, Category = "DF|UI|Tokens") float Duration(FName Token) const;
	UFUNCTION(BlueprintPure, Category = "DF|UI|Tokens") float Number(FName Token) const;

	/** Alpha 0..1 through the named cubic-bezier; a missing easing is linear. */
	UFUNCTION(BlueprintPure, Category = "DF|UI|Tokens") float Ease(FName Token, float Alpha) const;

	UFUNCTION(BlueprintPure, Category = "DF|UI|Tokens") bool Has(FName Token, EDFUITokenKind Kind) const;

	int32 Num() const { return Colors.Num() + Lengths.Num() + Durations.Num() + Easings.Num() + Numbers.Num() + Raw.Num(); }

	/** Replaces every table from CSS text (one string per file, cascade order). Errors name the
	 *  token: an unknown or circular var(), a value that starts like a colour / length / duration /
	 *  easing but is malformed, a token redefined with a different value. Returns true if none. */
	UFUNCTION(BlueprintCallable, Category = "DF|UI|Tokens")
	bool FillFromCss(const TArray<FString>& CssTexts, TArray<FString>& OutErrors);

	/** FillFromCss over the token files of a design-system directory (colors, spacing, typography,
	 *  effects). This is the whole of what an importer has to call before saving DA_UITokens. */
	UFUNCTION(BlueprintCallable, Category = "DF|UI|Tokens")
	bool FillFromDesignSystem(const FString& Directory, TArray<FString>& OutErrors);

	/** <repo>/docs/design-system, resolved from the project directory (editor / tests only). */
	static FString DesignSystemDir();

	/** y of a CSS cubic-bezier at x = Alpha. */
	static float EvaluateBezier(const FVector4& Curve, float Alpha);

private:
	void ReportMissing(FName Token, const TCHAR* Kind) const;
	mutable TSet<FName> Reported;
};
