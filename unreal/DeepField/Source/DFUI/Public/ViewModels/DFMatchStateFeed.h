#pragma once

#include "CoreMinimal.h"

class ADFMatchState;
class ADFPlayerState;
class APlayerState;
class UDFMatchViewModel;
class UDFPlayerViewModel;

/**
 * The real feed (C12 "Sources"): the match as the replicated actors describe it, written into the view
 * models through their setters. It reads ADFMatchState (WS-28's fields), the UDFEconomyStateComponent on
 * it (WS-06: money, lives), every ADFPlayerState in its PlayerArray (seat, stats, WS-03's hero state) and
 * each player's hero pawn (UDFHealthSet: hp). Those are replicated properties, and the listen host reads
 * its own authoritative actors through the same getters, so this code runs unchanged on host and client.
 *
 * It is a whole-state copy, safe to run every frame: a setter notifies only on a real change, and the
 * intermission countdown is a reading of the replicated end time that moves every frame anyway. What it
 * does not write (team scrap, which does not replicate yet; faction; loadouts; structures; pickups;
 * vehicles; boss; conditions) keeps its default until the component behind it lands (ADR-0024), and
 * FDFFakeMatchFeed stays the source for L_Test_UI, which has no match state.
 *
 * PlayerId is the seat (World.cs Player.Id, the id the messages carry); a player not yet seated is left
 * out until the game mode seats them.
 */
struct DFUI_API FDFMatchStateFeed
{
	/** One full pass. LocalPlayer is this machine's player (its seat becomes LocalPlayerId); null on a
	 *  machine with no player, where Local() stays null. Marks the match valid. */
	static void Fill(UDFMatchViewModel& Match, const ADFMatchState& State, const APlayerState* LocalPlayer);

	/** One player: identity, match record, hero state, and hp from the hero pawn when it has one. */
	static void FillPlayer(UDFPlayerViewModel& Player, const ADFPlayerState& State);
};
