#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DFSeatHolder.generated.h"

// Who a player is, for the modules that cannot see the match (contract-append, WS-28, 2026-09-27).
// The seat lives on DFMatch's ADFPlayerState (layer 4); an enemy that dies to a hero's shot is
// DFEnemies' (layer 3) and must say which seat killed it (FDFMsg_Kill.KillerPlayerId, Step.cs:2090's
// `playerId`). So the player state answers through this interface, declared in the layer every
// module can see, and SeatOf finds it behind whatever a hit names as its instigator.

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UDFSeatHolder : public UInterface
{
	GENERATED_BODY()
};

class DFCORE_API IDFSeatHolder
{
	GENERATED_BODY()

public:
	/** The 1-based match seat (World.cs Player.Id: the PlayerId every C15 message carries); 0 while
	 *  unseated. Not a vehicle seat (UDFHeroStateComponent::GetSeatIndex). */
	virtual int32 GetMatchSeat() const = 0;

	/**
	 * The seat of the player behind Who: Who itself when it holds a seat, else the player state of Who
	 * as a pawn (a hero's shot names its pawn as the instigator) or as a controller. 0 when no seated
	 * player is behind it: a tower, an enemy, a world hazard, nothing.
	 */
	static int32 SeatOf(const UObject* Who);
};
