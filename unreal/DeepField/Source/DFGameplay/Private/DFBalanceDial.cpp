#include "DFBalanceDial.h"

#include "Content/DFContentRows.h"
#include "Content/DFContentSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFBalance, Log, All);

float DFBalance::Dial(const UObject* WorldContext, FName Name, float Default)
{
	const UDFContentSubsystem* Content = WorldContext ? UDFContentSubsystem::Get(WorldContext) : nullptr;
	if (!Content || !Content->IsReady())
	{
		return Default;
	}
	if (const FDFBalanceRow* Row = Content->Find<FDFBalanceRow>(TEXT("balance"), TEXT("default")))
	{
		if (const float* Value = Row->Dials.Find(Name))
		{
			return *Value;
		}
		static TSet<FName> Reported;
		if (!Reported.Contains(Name))
		{
			Reported.Add(Name);
			UE_LOG(LogDFBalance, Warning, TEXT("balance.json has no dial '%s'; using the sim's %g (RFC: balance.json / content-rows)"), *Name.ToString(), Default);
		}
	}
	return Default;
}
