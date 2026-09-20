#include "ViewModels/DFFakeMatchFeed.h"

#include "DFGameplayTags.h"
#include "ViewModels/DFMatchViewModel.h"
#include "ViewModels/DFPickupViewModel.h"
#include "ViewModels/DFPlayerViewModel.h"
#include "ViewModels/DFStructureViewModel.h"
#include "ViewModels/DFVehicleViewModel.h"

namespace
{
	UDFPlayerViewModel& Player(UDFMatchViewModel& Match, int32 Id, const TCHAR* Name, const FGameplayTag& Faction, int32 Level, const FVector& Position)
	{
		UDFPlayerViewModel& P = *Match.FindOrAddPlayer(Id);
		P.SetName(Name);
		P.SetFaction(Faction);
		P.SetFactionLevel(Level);
		P.SetPosition(Position);
		P.SetMaxHp(100.f);
		P.SetHp(100.f);
		P.SetConnected(true);
		return P;
	}

	void Structure(UDFMatchViewModel& Match, int32 Id, const TCHAR* DefId, const TCHAR* Socket, int32 Owner, TArray<int32> Levels, float HpFrac, bool bSellable)
	{
		UDFStructureViewModel& S = *Match.FindOrAddStructure(Id);
		S.SetDefId(DefId);
		S.SetDefTag(DFTags::ForContentId(TEXT("DF.Tower"), DefId));
		S.SetSocketId(Socket);
		S.SetOwnerPlayerId(Owner);
		S.SetPathLevels(Levels);
		S.SetHpFrac(HpFrac);
		S.SetSellable(bSellable);
	}
}

void FDFFakeMatchFeed::FillMidWave(UDFMatchViewModel& Match)
{
	Match.Reset();
	Match.SetLocalPlayerId(1);
	Match.SetMoney(412);
	Match.SetPendingSpend(75);
	Match.SetLives(17);
	Match.SetWaveIndex(3);
	Match.SetTotalWaves(10);
	Match.SetPhase(EDFMatchPhase::Wave);
	Match.SetPhaseSecondsLeft(0.f);
	Match.SetThreat(1.82f);
	Match.SetEnemiesRemaining(14);
	Match.SetTeamScrap({ { EDFScrapType::Alloy, 23 }, { EDFScrapType::Flux, 9 }, { EDFScrapType::Plating, 6 }, { EDFScrapType::Gravium, 1 } });
	Match.SetBestWave(7);
	Match.SetTier(DFTags::Tier_Standard);
	Match.SetNextCondition(TEXT("fog"));
	Match.SetLaneGateStates({ { TEXT("b1"), true }, { TEXT("b2"), false } });

	UDFPlayerViewModel& Me = Player(Match, 1, TEXT("Rook"), DFTags::Faction_Forge, 3, FVector(1200, -400, 90));
	Me.SetHost(true);
	Me.SetHp(64.f);
	Me.SetWeapon(TEXT("rifle"));
	Me.SetAbilityCooldownFrac(0.4f);
	Me.SetScrap({ { EDFScrapType::Alloy, 11 }, { EDFScrapType::Flux, 4 } });
	Me.SetOwnedWeapons({ TEXT("sidearm"), TEXT("rifle") });
	Me.SetCraftedAmmo({ TEXT("standard"), TEXT("ap") });
	FDFWeaponBuild Rifle;
	Rifle.Attachments.Add(EDFAttachmentSlot::Barrel, TEXT("longBarrel"));
	Rifle.Attachments.Add(EDFAttachmentSlot::Infusion, TEXT("emberCoil"));
	Rifle.AmmoId = TEXT("ap");
	Rifle.PackLevel = 2;
	Me.SetBuilds({ { TEXT("rifle"), Rifle } });
	Me.SetKills(31);
	Me.SetDamageDealt(1840.f);
	Me.SetTowersBuilt(3);
	Me.SetMatchXp(46);

	UDFPlayerViewModel& Downed = Player(Match, 2, TEXT("Vesper"), DFTags::Faction_Ember, 5, FVector(900, 250, 90));
	Downed.SetHp(0.f);
	Downed.SetDowned(true);
	Downed.SetBleedoutSecondsLeft(21.5f);
	Downed.SetReviveProgress(0.35f);

	UDFPlayerViewModel& Rider = Player(Match, 3, TEXT("Oslo"), DFTags::Faction_Glacier, 1, FVector(-2000, 3100, 120));
	Rider.SetSeat(1);

	UDFVehicleViewModel& Buggy = *Match.FindOrAddVehicle(TEXT("buggy_1"));
	Buggy.SetDefId(TEXT("buggy"));
	Buggy.SetPosition(FVector(-2000, 3100, 60));
	Buggy.SetSeats({ 0, 3 });
	Buggy.SetHpFrac(0.8f);

	Structure(Match, 101, TEXT("lance"), TEXT("g03"), 1, { 4, 1, 0 }, 1.f, true);
	Structure(Match, 102, TEXT("nova"), TEXT("g07"), 2, { 2, 0, 0 }, 0.55f, false);
	Structure(Match, 103, TEXT("filament"), TEXT("w02"), 1, { 1, 3, 0 }, 1.f, true);
	Match.AtSocket(TEXT("w02"))->SetHeat(0.7f);
	UDFStructureViewModel& Spike = *Match.FindOrAddStructure(104);
	Spike.SetDefId(TEXT("spike"));
	Spike.SetDefTag(DFTags::Trap_Spike);
	Spike.SetSocketId(TEXT("t05"));
	Spike.SetOwnerPlayerId(3);
	Spike.SetTrap(true);
	Spike.SetCharges(2);

	UDFPickupViewModel& Alloy = *Match.FindOrAddPickup(9001);
	Alloy.SetScrapType(EDFScrapType::Alloy);
	Alloy.SetAmount(2);
	Alloy.SetPosition(FVector(1100, -300, 20));
	Alloy.SetSecondsLeft(38.f);
	Alloy.SetPersonal(true);
	UDFPickupViewModel& Flux = *Match.FindOrAddPickup(9002);
	Flux.SetScrapType(EDFScrapType::Flux);
	Flux.SetAmount(1);
	Flux.SetPosition(FVector(700, 40, 20));
	Flux.SetSecondsLeft(12.f);

	FDFPingView Ping;
	Ping.PlayerId = 2;
	Ping.Kind = TEXT("danger");
	Ping.Position = FVector(900, 250, 90);
	Ping.SecondsLeft = 4.f;
	Match.SetPings({ Ping });

	Match.SetValid(true);
}

void FDFFakeMatchFeed::FillLobby(UDFMatchViewModel& Match)
{
	Match.Reset();
	Match.SetLocalPlayerId(1);
	Match.SetLobby(true);
	Match.SetTotalWaves(10);
	Match.SetTier(DFTags::Tier_Standard);
	Player(Match, 1, TEXT("Rook"), DFTags::Faction_Forge, 3, FVector::ZeroVector).SetHost(true);
	Player(Match, 2, TEXT("Vesper"), DFTags::Faction_Ember, 5, FVector::ZeroVector);
	Match.SetValid(true);
}

void FDFFakeMatchFeed::Tick(UDFMatchViewModel& Match, float DeltaSeconds)
{
	Match.SetPhaseSecondsLeft(FMath::Max(0.f, Match.GetPhaseSecondsLeft() - DeltaSeconds));
	for (UDFPlayerViewModel* P : Match.GetPlayers())
	{
		if (P->IsDowned())
		{
			P->SetBleedoutSecondsLeft(FMath::Max(0.f, P->GetBleedoutSecondsLeft() - DeltaSeconds));
		}
		P->SetAbilityCooldownFrac(FMath::Max(0.f, P->GetAbilityCooldownFrac() - DeltaSeconds / 30.f));
	}
	for (UDFPickupViewModel* Pickup : Match.GetPickups())
	{
		Pickup->SetSecondsLeft(FMath::Max(0.f, Pickup->GetSecondsLeft() - DeltaSeconds));
	}
}
