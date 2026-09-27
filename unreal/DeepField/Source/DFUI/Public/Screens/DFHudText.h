#pragma once

#include "CoreMinimal.h"
#include "Match/DFMatchTypes.h"

class UDFMatchViewModel;

/**
 * What the first HUD says (UDFHudScreen and its parts), as pure functions of the match view model and
 * of a refusal's reason, so the wording is tested without a widget (DF.UI.Hud.*). Localisable: every
 * string is in the "DFHud" namespace.
 */
namespace DFHudText
{
	/** "Wave 2 / 10"; "Wave -- / 10" before the first wave (as the Godot HUD's "--"); "Endless, lap 2 · wave 23"
	 *  once an endless arc has wrapped; "Lobby" while the party assembles; "No waves on this map" on a dev map. */
	DFUI_API FText WaveTitle(const UDFMatchViewModel& Match);

	/** The line under the title: "First wave in 8s" / "Next wave in 5s" while the intermission clock runs
	 *  (whole seconds, rounded up), "Wave in progress", "Waiting for players", "Waiting for the host to launch",
	 *  "Victory", "Defeat". */
	DFUI_API FText PhaseLine(const UDFMatchViewModel& Match);

	/** Big centred text for a finished match: "VICTORY" / "DEFEAT"; empty while it runs. */
	DFUI_API FText Banner(EDFMatchPhase Phase);

	/** The line under the banner: "All 10 waves held" / "The core fell on wave 4" / "Reached wave 23". */
	DFUI_API FText BannerDetail(const UDFMatchViewModel& Match);

	/** Under the banner's detail while a new match follows this one (RestartPending, ADFMatchState's
	 *  play-again): "New match in 12s" (PhaseSecondsLeft, whole seconds rounded up), then "Starting new
	 *  match…" once the countdown is over and the host is loading it. Empty while the match runs, and when
	 *  no new match is coming (the restart is off, or the host could not start one). */
	DFUI_API FText RestartLine(const UDFMatchViewModel& Match);

	/** The toast for a DF.Message.*Rejected (FDFMsg_Rejected.Reason, messages.md's vocabulary): "Not enough
	 *  money" for insufficientFunds, otherwise the reason exactly as the host sent it. */
	DFUI_API FText Refusal(FName Reason);

	/** The Prompts part's line until the build wheel exists: "Wheel: Lance (75)   Hold E on a pad: build   Hold U
	 *  on a tower: upgrade   Hold X: sell". TowerId is the towers.json id hold-E builds; its display name is
	 *  the id, capitalised. */
	DFUI_API FText BuildHint(FName TowerId, int32 Cost);

	/** The ammo readout: "18 / 24"; "RELOADING 40%" (whole percent) during a reload. */
	DFUI_API FText Ammo(int32 InMagazine, int32 MagazineSize, bool bReloading, float ReloadFrac);
}
