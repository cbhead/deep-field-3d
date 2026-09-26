#include "Tokens/DFUITokenNames.h"

namespace DFTokens
{
#define DF_UI_TOKEN(Name, Css, Kind) const FName Name(TEXT(Css));
#include "Tokens/DFUITokenList.inl"
#undef DF_UI_TOKEN

	const TArray<FEntry>& All()
	{
		static const TArray<FEntry> Entries = {
#define DF_UI_TOKEN(Name, Css, Kind) { Name, EDFUITokenKind::Kind },
#include "Tokens/DFUITokenList.inl"
#undef DF_UI_TOKEN
		};
		return Entries;
	}
}
