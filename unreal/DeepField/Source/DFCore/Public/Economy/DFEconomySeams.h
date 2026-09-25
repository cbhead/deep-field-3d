#pragma once

#include "Content/DFContentRows.h"
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DFEconomySeams.generated.h"

// The team's purse, as the systems that spend it see it (contract-append, WS-04, 2026-09-25).
// Money and team scrap live in WS-06's UDFEconomyStateComponent on ADFMatchState (ADR-0024). The
// build subsystem (DFTowers), the gunsmith and the trap placer sit below DFMatch and must not name
// that component, so they spend through this interface, declared in the layer all of them see. The
// component implements it; the spenders find it on the game state (or are handed one in a test).

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UDFTeamWallet : public UInterface
{
	GENERATED_BODY()
};

/**
 * Host only. Every spend is all-or-nothing: TrySpend takes the money and every scrap line of the
 * bundle, or takes nothing and returns false. Callers check their own rules first (the sim refuses
 * with a reason before anything moves); TrySpend's false is the last guard, not the refusal path.
 */
class DFCORE_API IDFTeamWallet
{
	GENERATED_BODY()

public:
	/** The team's money (the sim's World.Money). */
	virtual int32 GetMoney() const = 0;
	/** The team scrap pool (the sim's World.TeamScrap). A type that is absent is zero. */
	virtual const TMap<EDFScrapType, int32>& GetTeamScrap() const = 0;
	/** Take Money and, when Scrap is given, every line of it from the team pool; or take nothing. */
	virtual bool TrySpend(int32 Money, const FDFScrapBundle* Scrap) = 0;
	/** Give money back (a sale's refund, an undone build). */
	virtual void AddMoney(int32 Money) = 0;
};
