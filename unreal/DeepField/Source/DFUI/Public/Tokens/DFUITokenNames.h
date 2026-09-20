#pragma once

#include "CoreMinimal.h"
#include "Tokens/DFUITokens.h"

// Token names for code: `Tokens->Color(DFTokens::SurfacePanel)`. A typo is a compile error here
// rather than a magenta panel at runtime, and the list is tested against the CSS both ways.
namespace DFTokens
{
#define DF_UI_TOKEN(Name, Css, Kind) DFUI_API extern const FName Name;
#include "Tokens/DFUITokenList.inl"
#undef DF_UI_TOKEN

	struct FEntry
	{
		FName Name;
		EDFUITokenKind Kind;
	};

	/** Every line of DFUITokenList.inl, in order. */
	DFUI_API const TArray<FEntry>& All();
}
