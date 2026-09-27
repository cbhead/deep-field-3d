#include "ViewModels/DFMatchStateFeed.h"

#include "Attributes/DFHealthSet.h"
#include "DFHeroCharacter.h"
#include "DFPlayerController.h"
#include "Weapons/DFHeroWeaponComponent.h"
#include "DFMatchState.h"
#include "DFPlayerState.h"
#include "Economy/DFEconomyStateComponent.h"
#include "Hero/DFHeroStateComponent.h"
#include "ViewModels/DFMatchViewModel.h"
#include "ViewModels/DFPlayerViewModel.h"

void FDFMatchStateFeed::Fill(UDFMatchViewModel& Match, const ADFMatchState& State, const APlayerState* LocalPlayer)
{
	// ---- WS-28's own replicated fields
	Match.SetLobby(State.IsLobby());
	Match.SetEndless(State.IsEndless());
	Match.SetWaveIndex(State.GetWaveIndex());
	Match.SetTotalWaves(State.GetTotalWaves());
	Match.SetLap(State.GetLap());
	Match.SetPhase(State.GetPhase());
	// The clock replicates as the server time it runs out; the seconds left are this machine's reading of
	// it. When it is not running (a wave, the lobby, waiting for players) there is no countdown to show.
	Match.SetPhaseSecondsLeft(State.IsPhaseClockRunning() ? State.GetPhaseSecondsLeft() : 0.f);
	Match.SetThreat(State.GetThreat());
	Match.SetEnemiesRemaining(State.GetEnemiesRemaining());

	// ---- WS-06's component (ADR-0024). Team scrap stays host-only until the scrap half of WS-06
	// replicates it, so it is not copied: a client would show nothing where the host shows a pool.
	if (const UDFEconomyStateComponent* Economy = State.GetEconomy())
	{
		Match.SetMoney(Economy->GetMoney());
		Match.SetLives(Economy->GetLives());
	}

	// ---- the seats
	TSet<int32> Seats;
	for (const APlayerState* Each : State.PlayerArray)
	{
		const ADFPlayerState* Seated = Cast<ADFPlayerState>(Each);
		if (!Seated || Seated->GetSeat() <= 0)
		{
			continue;
		}
		Seats.Add(Seated->GetSeat());
		FillPlayer(*Match.FindOrAddPlayer(Seated->GetSeat()), *Seated);
	}
	Match.RetainPlayers(Seats);

	const ADFPlayerState* Local = Cast<ADFPlayerState>(LocalPlayer);
	Match.SetLocalPlayerId(Local ? Local->GetSeat() : 0);
	// The build choice lives on this machine's own controller (it is sent with each build request).
	if (Local)
	{
		if (UDFPlayerViewModel* Me = Match.FindPlayer(Local->GetSeat()))
		{
			if (const ADFPlayerController* Controller = Cast<ADFPlayerController>(Local->GetOwningController()))
			{
				Me->SetBuildChoice(Controller->GetQuickBuildTowerId());
			}
		}
	}
	Match.SetValid(true);
}

void FDFMatchStateFeed::FillPlayer(UDFPlayerViewModel& Player, const ADFPlayerState& State)
{
	Player.SetPlayerId(State.GetSeat());
	Player.SetName(State.GetPlayerName());
	Player.SetConnected(!State.IsInactive());
	Player.SetKills(State.GetKills());
	Player.SetDamageDealt(State.GetDamageDealt());
	Player.SetTowersBuilt(State.GetTowersBuilt());
	Player.SetRevives(State.GetRevives());
	Player.SetMatchXp(State.GetMatchXp());

	if (const UDFHeroStateComponent* Hero = State.GetHeroState())
	{
		Player.SetDowned(Hero->IsDowned());
		Player.SetReviveProgress(Hero->GetReviveFraction());
		// R13: a client that does not know the server clock yet shows no countdown, not a wrong one.
		float BleedoutSeconds = 0.f;
		Player.SetBleedoutSecondsLeft(Hero->TryGetBleedoutSecondsLeft(BleedoutSeconds) ? BleedoutSeconds : 0.f);
		Player.SetSeat(Hero->GetSeatIndex());
	}

	// The attribute set replicates with the pawn, so every machine reads every hero's hp the same way.
	// No pawn yet (joining, or between a death and a respawn in a later mode): keep the last values.
	if (const ADFHeroCharacter* Pawn = State.GetPawn<ADFHeroCharacter>())
	{
		Player.SetPosition(Pawn->GetActorLocation());
		if (const UDFHealthSet* Health = Pawn->GetHealthSet())
		{
			Player.SetMaxHp(Health->GetMaxHealth());
			Player.SetHp(Health->GetHealth());
		}
		if (const UDFHeroWeaponComponent* Gun = Pawn->GetWeapon())
		{
			Player.SetAmmoInMagazine(Gun->GetAmmoInMagazine());
			Player.SetMagazineSize(Gun->GetMagazineSize());
			Player.SetReloading(Gun->IsReloading());
			Player.SetReloadFrac(Gun->IsReloading() ? Gun->GetReloadFraction() : 0.f);
		}
	}
}
