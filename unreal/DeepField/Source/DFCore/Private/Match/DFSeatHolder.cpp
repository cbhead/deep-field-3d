#include "Match/DFSeatHolder.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

int32 IDFSeatHolder::SeatOf(const UObject* Who)
{
	if (!IsValid(Who))
	{
		return 0;
	}
	if (const IDFSeatHolder* Holder = Cast<IDFSeatHolder>(Who))
	{
		return Holder->GetMatchSeat();
	}
	const APlayerState* PlayerState = nullptr;
	if (const APawn* Pawn = Cast<APawn>(Who))
	{
		PlayerState = Pawn->GetPlayerState();
	}
	else if (const AController* Controller = Cast<AController>(Who))
	{
		PlayerState = Controller->PlayerState;
	}
	const IDFSeatHolder* Holder = Cast<IDFSeatHolder>(PlayerState);
	return Holder ? Holder->GetMatchSeat() : 0;
}
