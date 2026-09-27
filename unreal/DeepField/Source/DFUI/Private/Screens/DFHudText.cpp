#include "Screens/DFHudText.h"

#include "ViewModels/DFMatchViewModel.h"

#define LOCTEXT_NAMESPACE "DFHud"

namespace
{
	/** DFTowerMath::Reasons::InsufficientFunds, spelled here because DFUI does not depend on DFTowers:
	 *  the reasons are messages.md's vocabulary, which the refusal carries as a plain name. */
	const FName InsufficientFunds(TEXT("insufficientFunds"));
}

FText DFHudText::WaveTitle(const UDFMatchViewModel& Match)
{
	if (Match.IsLobby())
	{
		return LOCTEXT("Lobby", "Lobby");
	}
	const int32 Wave = Match.GetWaveIndex() + 1;   // people count from one
	if (Match.IsEndless() && Match.GetLap() > 0)
	{
		return FText::Format(LOCTEXT("EndlessLap", "Endless, lap {0} · wave {1}"), Match.GetLap(), Wave);
	}
	if (Match.GetTotalWaves() <= 0)
	{
		// A dev map with no lane graph: the match idles in intermission with nothing to count.
		return LOCTEXT("NoWaves", "No waves on this map");
	}
	if (Match.GetWaveIndex() < 0)
	{
		return FText::Format(LOCTEXT("WaveNotYet", "Wave -- / {0}"), Match.GetTotalWaves());
	}
	return FText::Format(LOCTEXT("WaveOf", "Wave {0} / {1}"), Wave, Match.GetTotalWaves());
}

FText DFHudText::PhaseLine(const UDFMatchViewModel& Match)
{
	switch (Match.GetPhase())
	{
	case EDFMatchPhase::Victory: return LOCTEXT("PhaseVictory", "Victory");
	case EDFMatchPhase::Defeat:  return LOCTEXT("PhaseDefeat", "Defeat");
	case EDFMatchPhase::Wave:    return LOCTEXT("PhaseWave", "Wave in progress");
	case EDFMatchPhase::Intermission:
	default:
		break;
	}
	if (Match.IsLobby())
	{
		return LOCTEXT("PhaseLobby", "Waiting for the host to launch");
	}
	if (Match.GetPhaseSecondsLeft() > 0.f)
	{
		// Rounded up: "1s" until the wave actually starts, never "0s" while it has not.
		const int32 Seconds = FMath::CeilToInt(Match.GetPhaseSecondsLeft());
		return FText::Format(Match.GetWaveIndex() < 0 ? LOCTEXT("FirstWaveIn", "First wave in {0}s") : LOCTEXT("NextWaveIn", "Next wave in {0}s"), Seconds);
	}
	// Intermission with the clock stopped: the game mode is waiting for someone to take a seat.
	return LOCTEXT("PhaseWaiting", "Waiting for players");
}

FText DFHudText::Banner(EDFMatchPhase Phase)
{
	switch (Phase)
	{
	case EDFMatchPhase::Victory: return LOCTEXT("BannerVictory", "VICTORY");
	case EDFMatchPhase::Defeat:  return LOCTEXT("BannerDefeat", "DEFEAT");
	default:                     return FText::GetEmpty();
	}
}

FText DFHudText::BannerDetail(const UDFMatchViewModel& Match)
{
	const int32 Wave = FMath::Max(0, Match.GetWaveIndex()) + 1;
	switch (Match.GetPhase())
	{
	case EDFMatchPhase::Victory:
		return FText::Format(LOCTEXT("DetailVictory", "All {0} waves held"), Match.GetTotalWaves());
	case EDFMatchPhase::Defeat:
		return FText::Format(LOCTEXT("DetailDefeat", "The core fell on wave {0}"), Wave);
	default:
		return FText::GetEmpty();
	}
}

FText DFHudText::Refusal(FName Reason)
{
	if (Reason == InsufficientFunds)
	{
		return LOCTEXT("RefusedFunds", "Not enough money");
	}
	if (Reason.IsNone())
	{
		return LOCTEXT("RefusedNoReason", "Refused");
	}
	// Anything else is shown as the host spelled it until it earns its own wording: a raw reason is
	// still more use to a player than a generic "can't build here".
	return FText::AsCultureInvariant(Reason.ToString());
}

FText DFHudText::BuildHint(FName TowerId, int32 Cost)
{
	FString Name = TowerId.ToString();
	if (!Name.IsEmpty())
	{
		Name[0] = FChar::ToUpper(Name[0]);
	}
	return FText::Format(LOCTEXT("BuildHint", "Hold E on a pad: build {0} ({1})   Hold U on a tower: upgrade   Hold X: sell"),
		FText::AsCultureInvariant(Name), FText::AsNumber(Cost));
}

FText DFHudText::Ammo(int32 InMagazine, int32 MagazineSize, bool bReloading, float ReloadFrac)
{
	if (bReloading)
	{
		return FText::Format(LOCTEXT("AmmoReloading", "RELOADING {0}%"), FMath::Clamp(FMath::FloorToInt(ReloadFrac * 100.f), 0, 99));
	}
	return FText::Format(LOCTEXT("AmmoOf", "{0} / {1}"), FMath::Max(0, InMagazine), FMath::Max(0, MagazineSize));
}

#undef LOCTEXT_NAMESPACE
