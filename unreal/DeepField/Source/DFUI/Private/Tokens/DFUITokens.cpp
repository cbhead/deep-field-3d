#include "Tokens/DFUITokens.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFUITokens, Log, All);

namespace
{
	const TCHAR* GTokenFiles[] = { TEXT("colors.css"), TEXT("spacing.css"), TEXT("typography.css"), TEXT("effects.css") };

	FString StripComments(const FString& Css)
	{
		FString Out;
		Out.Reserve(Css.Len());
		int32 i = 0;
		while (i < Css.Len())
		{
			if (Css[i] == TEXT('/') && i + 1 < Css.Len() && Css[i + 1] == TEXT('*'))
			{
				const int32 End = Css.Find(TEXT("*/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, i + 2);
				i = End == INDEX_NONE ? Css.Len() : End + 2;
				continue;
			}
			Out.AppendChar(Css[i++]);
		}
		return Out;
	}

	bool IsNameChar(TCHAR C)
	{
		return FChar::IsAlnum(C) || C == TEXT('-') || C == TEXT('_');
	}

	/** Every `--name: value;` in the text, in order. Selectors and ordinary properties are skipped. */
	void ReadDeclarations(const FString& Css, TArray<TPair<FString, FString>>& Out)
	{
		const FString Text = StripComments(Css);
		int32 i = 0;
		while ((i = Text.Find(TEXT("--"), ESearchCase::CaseSensitive, ESearchDir::FromStart, i)) != INDEX_NONE)
		{
			// A declaration starts a statement: the previous non-space character is {, ; or the start.
			int32 Back = i - 1;
			while (Back >= 0 && FChar::IsWhitespace(Text[Back]))
			{
				--Back;
			}
			int32 NameEnd = i + 2;
			while (NameEnd < Text.Len() && IsNameChar(Text[NameEnd]))
			{
				++NameEnd;
			}
			int32 Colon = NameEnd;
			while (Colon < Text.Len() && FChar::IsWhitespace(Text[Colon]))
			{
				++Colon;
			}
			const bool bStartsStatement = Back < 0 || Text[Back] == TEXT('{') || Text[Back] == TEXT(';');
			if (!bStartsStatement || Colon >= Text.Len() || Text[Colon] != TEXT(':') || NameEnd == i + 2)
			{
				i = NameEnd;
				continue;
			}
			int32 ValueEnd = Colon + 1;
			int32 Depth = 0;
			while (ValueEnd < Text.Len() && !(Depth == 0 && (Text[ValueEnd] == TEXT(';') || Text[ValueEnd] == TEXT('}'))))
			{
				Depth += Text[ValueEnd] == TEXT('(') ? 1 : Text[ValueEnd] == TEXT(')') ? -1 : 0;
				++ValueEnd;
			}
			Out.Emplace(Text.Mid(i + 2, NameEnd - i - 2), Text.Mid(Colon + 1, ValueEnd - Colon - 1).TrimStartAndEnd());
			i = ValueEnd;
		}
	}

	/** Replaces every var(--x) (a fallback after a comma is ignored) with x's resolved value. */
	bool Resolve(const FString& Name, const TMap<FString, FString>& Declared, TMap<FString, FString>& Resolved, TArray<FString>& Stack, TArray<FString>& Errors)
	{
		if (Resolved.Contains(Name))
		{
			return true;
		}
		if (Stack.Contains(Name))
		{
			Errors.Add(FString::Printf(TEXT("--%s: circular var() (%s)"), *Name, *FString::Join(Stack, TEXT(" -> "))));
			return false;
		}
		Stack.Push(Name);
		FString Value = Declared.FindChecked(Name);
		bool bOk = true;
		int32 At;
		while (bOk && (At = Value.Find(TEXT("var("), ESearchCase::CaseSensitive)) != INDEX_NONE)
		{
			const int32 Close = Value.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromStart, At);
			FString Ref = Close == INDEX_NONE ? FString() : Value.Mid(At + 4, Close - At - 4);
			int32 Comma;
			if (Ref.FindChar(TEXT(','), Comma))
			{
				Ref.LeftInline(Comma);
			}
			Ref.TrimStartAndEndInline();
			if (!Ref.RemoveFromStart(TEXT("--")) || !Declared.Contains(Ref))
			{
				Errors.Add(FString::Printf(TEXT("--%s: var(%s) names no token"), *Name, Close == INDEX_NONE ? TEXT("...") : *Value.Mid(At + 4, Close - At - 4)));
				bOk = false;
			}
			else if ((bOk = Resolve(Ref, Declared, Resolved, Stack, Errors)))
			{
				Value = Value.Left(At) + Resolved.FindChecked(Ref) + Value.Mid(Close + 1);
			}
		}
		Stack.Pop();
		if (bOk)
		{
			Resolved.Add(Name, Value);
		}
		return bOk;
	}

	/** "12", ".5", "-0.25", "1e3" and nothing else. */
	bool ParseNumber(const FString& Text, double& Out)
	{
		if (Text.IsEmpty())
		{
			return false;
		}
		bool bDigit = false;
		for (int32 i = 0; i < Text.Len(); ++i)
		{
			const TCHAR C = Text[i];
			bDigit |= FChar::IsDigit(C);
			if (!(FChar::IsDigit(C) || C == TEXT('.') || ((C == TEXT('-') || C == TEXT('+')) && i == 0)))
			{
				return false;
			}
		}
		Out = FCString::Atod(*Text);
		return bDigit;
	}

	bool ParseArguments(const FString& Value, const TCHAR* Function, int32 MinCount, int32 MaxCount, TArray<double>& Out)
	{
		FString Inner = Value;
		if (!Inner.RemoveFromStart(Function) || !Inner.RemoveFromStart(TEXT("(")) || !Inner.RemoveFromEnd(TEXT(")")))
		{
			return false;
		}
		TArray<FString> Parts;
		Inner.ParseIntoArray(Parts, TEXT(","), /*CullEmpty*/ false);
		for (FString& Part : Parts)
		{
			double Number = 0;
			if (!ParseNumber(Part.TrimStartAndEnd(), Number))
			{
				return false;
			}
			Out.Add(Number);
		}
		return Out.Num() >= MinCount && Out.Num() <= MaxCount;
	}

	FLinearColor FromSrgb(double R, double G, double B, double A)
	{
		FLinearColor Color = FLinearColor::FromSRGBColor(FColor(
			static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(R), 0, 255)),
			static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(G), 0, 255)),
			static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(B), 0, 255))));
		Color.A = static_cast<float>(FMath::Clamp(A, 0.0, 1.0));
		return Color;
	}

	bool ParseHex(const FString& Value, FLinearColor& Out)
	{
		const FString Hex = Value.RightChop(1);
		for (const TCHAR C : Hex)
		{
			if (!FChar::IsHexDigit(C))
			{
				return false;
			}
		}
		auto Pair = [&Hex](int32 Index) { return static_cast<double>(FParse::HexDigit(Hex[Index]) * 16 + FParse::HexDigit(Hex[Index + 1])); };
		auto Single = [&Hex](int32 Index) { return static_cast<double>(FParse::HexDigit(Hex[Index]) * 17); };
		switch (Hex.Len())
		{
		case 3: Out = FromSrgb(Single(0), Single(1), Single(2), 1.0); return true;
		case 6: Out = FromSrgb(Pair(0), Pair(2), Pair(4), 1.0); return true;
		case 8: Out = FromSrgb(Pair(0), Pair(2), Pair(4), Pair(6) / 255.0); return true;
		default: return false;
		}
	}
}

bool UDFUITokens::FillFromCss(const TArray<FString>& CssTexts, TArray<FString>& OutErrors)
{
	const int32 ErrorsBefore = OutErrors.Num();
	Colors.Reset();
	Lengths.Reset();
	Durations.Reset();
	Easings.Reset();
	Numbers.Reset();
	Raw.Reset();
	Reported.Reset();

	TMap<FString, FString> Declared;
	TArray<FString> Order;
	for (const FString& Css : CssTexts)
	{
		TArray<TPair<FString, FString>> Declarations;
		ReadDeclarations(Css, Declarations);
		for (const TPair<FString, FString>& Declaration : Declarations)
		{
			if (const FString* Existing = Declared.Find(Declaration.Key))
			{
				// CSS would let the later one win; in a token file it is a slip, and it would be a silent one.
				if (!Existing->Equals(Declaration.Value, ESearchCase::CaseSensitive))
				{
					OutErrors.Add(FString::Printf(TEXT("--%s: defined twice ('%s' and '%s')"), *Declaration.Key, **Existing, *Declaration.Value));
				}
				continue;
			}
			Declared.Add(Declaration.Key, Declaration.Value);
			Order.Add(Declaration.Key);
		}
	}

	TMap<FString, FString> Resolved;
	for (const FString& Name : Order)
	{
		TArray<FString> Stack;
		if (!Resolve(Name, Declared, Resolved, Stack, OutErrors))
		{
			continue;
		}
		const FString& Value = Resolved.FindChecked(Name);
		const FName Token(*Name);
		double Number = 0;
		TArray<double> Args;
		auto Malformed = [&](const TCHAR* What) { OutErrors.Add(FString::Printf(TEXT("--%s: '%s' is not a valid %s"), *Name, *Value, What)); };

		if (Value.StartsWith(TEXT("#")))
		{
			FLinearColor Color;
			if (ParseHex(Value, Color))
			{
				Colors.Add(Token, Color);
			}
			else
			{
				Malformed(TEXT("hex colour"));
			}
		}
		else if (Value.StartsWith(TEXT("rgba(")) || Value.StartsWith(TEXT("rgb(")))
		{
			const bool bAlpha = Value.StartsWith(TEXT("rgba("));
			if (ParseArguments(Value, bAlpha ? TEXT("rgba") : TEXT("rgb"), bAlpha ? 4 : 3, bAlpha ? 4 : 3, Args))
			{
				Colors.Add(Token, FromSrgb(Args[0], Args[1], Args[2], bAlpha ? Args[3] : 1.0));
			}
			else
			{
				Malformed(TEXT("rgb()/rgba() colour"));
			}
		}
		else if (Value.StartsWith(TEXT("cubic-bezier(")))
		{
			if (ParseArguments(Value, TEXT("cubic-bezier"), 4, 4, Args))
			{
				Easings.Add(Token, FVector4(Args[0], Args[1], Args[2], Args[3]));
			}
			else
			{
				Malformed(TEXT("cubic-bezier()"));
			}
		}
		else if (Value.EndsWith(TEXT("ms")) && ParseNumber(Value.LeftChop(2), Number))
		{
			Durations.Add(Token, static_cast<float>(Number / 1000.0));
		}
		else if (Value.EndsWith(TEXT("px")) && ParseNumber(Value.LeftChop(2), Number))
		{
			Lengths.Add(Token, static_cast<float>(Number));
		}
		else if (Value.EndsWith(TEXT("em")) && ParseNumber(Value.LeftChop(2), Number))
		{
			Numbers.Add(Token, static_cast<float>(Number));
		}
		else if (Value.EndsWith(TEXT("%")) && ParseNumber(Value.LeftChop(1), Number))
		{
			Numbers.Add(Token, static_cast<float>(Number / 100.0));
		}
		else if (Value.EndsWith(TEXT("s")) && ParseNumber(Value.LeftChop(1), Number))
		{
			Durations.Add(Token, static_cast<float>(Number));
		}
		else if (ParseNumber(Value, Number))
		{
			Numbers.Add(Token, static_cast<float>(Number));
			if (Number == 0.0)
			{
				Lengths.Add(Token, 0.f);   // a bare 0 is also a length (--space-0, --radius-0)
			}
		}
		else
		{
			Raw.Add(Token, Value);
		}
	}
	return OutErrors.Num() == ErrorsBefore;
}

bool UDFUITokens::FillFromDesignSystem(const FString& Directory, TArray<FString>& OutErrors)
{
	TArray<FString> Texts;
	for (const TCHAR* File : GTokenFiles)
	{
		FString Text;
		if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(Directory, File)))
		{
			Texts.Add(MoveTemp(Text));
		}
		else
		{
			OutErrors.Add(FString::Printf(TEXT("cannot read %s"), *FPaths::Combine(Directory, File)));
		}
	}
	const bool bRead = Texts.Num() == UE_ARRAY_COUNT(GTokenFiles);
	return FillFromCss(Texts, OutErrors) && bRead;
}

FString UDFUITokens::DesignSystemDir()
{
	// <repo>/unreal/DeepField/ -> <repo>/docs/design-system
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT(".."), TEXT("docs"), TEXT("design-system")));
}

// ---------------------------------------------------------------------- reading

FLinearColor UDFUITokens::Color(FName Token) const
{
	if (const FLinearColor* Found = Colors.Find(Token))
	{
		return *Found;
	}
	ReportMissing(Token, TEXT("colour"));
	return FLinearColor(1.f, 0.f, 1.f, 1.f);
}

float UDFUITokens::Length(FName Token) const
{
	if (const float* Found = Lengths.Find(Token))
	{
		return *Found;
	}
	ReportMissing(Token, TEXT("length"));
	return 0.f;
}

float UDFUITokens::Duration(FName Token) const
{
	if (const float* Found = Durations.Find(Token))
	{
		return *Found;
	}
	ReportMissing(Token, TEXT("duration"));
	return 0.f;
}

float UDFUITokens::Number(FName Token) const
{
	if (const float* Found = Numbers.Find(Token))
	{
		return *Found;
	}
	ReportMissing(Token, TEXT("number"));
	return 0.f;
}

float UDFUITokens::Ease(FName Token, float Alpha) const
{
	if (const FVector4* Found = Easings.Find(Token))
	{
		return EvaluateBezier(*Found, Alpha);
	}
	ReportMissing(Token, TEXT("easing"));
	return FMath::Clamp(Alpha, 0.f, 1.f);
}

bool UDFUITokens::Has(FName Token, EDFUITokenKind Kind) const
{
	switch (Kind)
	{
	case EDFUITokenKind::Color:    return Colors.Contains(Token);
	case EDFUITokenKind::Length:   return Lengths.Contains(Token);
	case EDFUITokenKind::Duration: return Durations.Contains(Token);
	case EDFUITokenKind::Easing:   return Easings.Contains(Token);
	case EDFUITokenKind::Number:   return Numbers.Contains(Token);
	case EDFUITokenKind::Raw:      return Raw.Contains(Token);
	}
	return false;
}

float UDFUITokens::EvaluateBezier(const FVector4& Curve, float Alpha)
{
	const double X = FMath::Clamp(static_cast<double>(Alpha), 0.0, 1.0);
	if (X <= 0.0 || X >= 1.0)
	{
		return static_cast<float>(X);
	}
	// P0 = (0,0), P3 = (1,1); x(t) is monotonic for x1, x2 in [0,1], so bisection always lands.
	auto Axis = [](double P1, double P2, double T) { const double U = 1.0 - T; return 3.0 * U * U * T * P1 + 3.0 * U * T * T * P2 + T * T * T; };
	const double X1 = FMath::Clamp(Curve.X, 0.0, 1.0);
	const double X2 = FMath::Clamp(Curve.Z, 0.0, 1.0);
	double Low = 0.0, High = 1.0, T = X;
	for (int32 i = 0; i < 32; ++i)
	{
		const double At = Axis(X1, X2, T);
		if (FMath::Abs(At - X) < 1e-6)
		{
			break;
		}
		(At < X ? Low : High) = T;
		T = 0.5 * (Low + High);
	}
	return static_cast<float>(Axis(Curve.Y, Curve.W, T));
}

void UDFUITokens::ReportMissing(FName Token, const TCHAR* Kind) const
{
	if (!Reported.Contains(Token))
	{
		Reported.Add(Token);
		UE_LOG(LogDFUITokens, Error, TEXT("missing UI token: no %s named '--%s' in %s"), Kind, *Token.ToString(), *GetName());
	}
}
