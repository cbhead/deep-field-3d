#pragma once

#include "CoreMinimal.h"

class UDFMatchViewModel;

/** A plausible match written straight into the view models — what L_Test_UI shows every screen
 *  against (WS-12 DoD: "every screen reachable with a fake view model") and what the tests read.
 *  It is a feed like any other: it only calls setters. Deterministic; no world, no net, no GAS. */
struct DFUI_API FDFFakeMatchFeed
{
	/** Foundry, wave 4 of 10 in progress, three players (local = 1, forge; one downed, one in a
	 *  buggy's passenger seat), four structures, two pickups, fog incoming. */
	static void FillMidWave(UDFMatchViewModel& Match);

	/** The party screen: lobby flag up, no wave yet, two players. */
	static void FillLobby(UDFMatchViewModel& Match);

	/** Advance the continuous fields the way a live feed would between two frames. */
	static void Tick(UDFMatchViewModel& Match, float DeltaSeconds);
};
