#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DFMatchSeams.generated.h"

// Seams the match flow reads through (contract-append, WS-28 / ADR-0023). ADFMatchState (DFMatch,
// layer 4) hosts state components that the owning domains write in their own, lower modules; the
// phase machine needs a few of their answers and must not name their classes. The component answers
// through an interface declared here, in the layer every domain can see.

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UDFMatchLivesSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * The core's lives, as the match flow reads them for Defeat. Implemented by WS-06's economy state
 * component on ADFMatchState. The match flow reads lives and never writes them: an enemy's leak
 * reaches the economy as a message, and the economy takes the lives off.
 *
 * Ordering the phase machine relies on (Step.cs CheckEndState is lives-first): a leak must take its
 * lives BEFORE the leaking body is reported to the wave director (NotifyEnemyRemoved), so that when
 * that report clears the wave, the lives the match flow reads already include the leak — a last enemy
 * that leaks the core to zero is a Defeat, never a Victory.
 */
class DFCORE_API IDFMatchLivesSource
{
	GENERATED_BODY()

public:
	virtual int32 GetLives() const = 0;
};
