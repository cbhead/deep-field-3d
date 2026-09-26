#include "ViewModels/DFMatchViewModel.h"
#include "ViewModels/DFPickupViewModel.h"
#include "ViewModels/DFPlayerViewModel.h"
#include "ViewModels/DFStructureViewModel.h"
#include "ViewModels/DFVehicleViewModel.h"

// ---------------------------------------------------------------------- player

int32 UDFPlayerViewModel::PackLevelFor(FName WeaponId) const
{
	const FDFWeaponBuild* Build = Builds.Find(WeaponId);
	return Build ? Build->PackLevel : 0;
}

FName UDFPlayerViewModel::AmmoFor(FName WeaponId) const
{
	const FDFWeaponBuild* Build = Builds.Find(WeaponId);
	return Build ? Build->AmmoId : FDFWeaponBuild().AmmoId;
}

FName UDFPlayerViewModel::AttachmentFor(FName WeaponId, EDFAttachmentSlot Slot) const
{
	const FDFWeaponBuild* Build = Builds.Find(WeaponId);
	return Build ? Build->Attachments.FindRef(Slot) : NAME_None;
}

// ---------------------------------------------------------------------- match: children

namespace
{
	template <typename TViewModel, typename TId, typename TGetId>
	TViewModel* FindChild(const TArray<TObjectPtr<TViewModel>>& List, const TId& Id, TGetId GetId)
	{
		for (TViewModel* Item : List)
		{
			if (Item && GetId(*Item) == Id)
			{
				return Item;
			}
		}
		return nullptr;
	}

	template <typename TViewModel, typename TKeep>
	bool RemoveChildren(TArray<TObjectPtr<TViewModel>>& List, TKeep Keep)
	{
		return List.RemoveAll([&Keep](const TObjectPtr<TViewModel>& Item) { return !Item || !Keep(*Item); }) > 0;
	}
}

UDFPlayerViewModel* UDFMatchViewModel::FindPlayer(int32 PlayerId) const
{
	return FindChild(Players, PlayerId, [](const UDFPlayerViewModel& P) { return P.GetPlayerId(); });
}

UDFPlayerViewModel* UDFMatchViewModel::FindOrAddPlayer(int32 PlayerId)
{
	if (UDFPlayerViewModel* Existing = FindPlayer(PlayerId))
	{
		return Existing;
	}
	UDFPlayerViewModel* Added = NewObject<UDFPlayerViewModel>(this);
	Added->SetPlayerId(PlayerId);
	Players.Add(Added);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Players);
	if (PlayerId == LocalPlayerId)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Local);
	}
	return Added;
}

UDFStructureViewModel* UDFMatchViewModel::FindOrAddStructure(int32 StructureId)
{
	if (UDFStructureViewModel* Existing = FindChild(Structures, StructureId, [](const UDFStructureViewModel& S) { return S.GetStructureId(); }))
	{
		return Existing;
	}
	UDFStructureViewModel* Added = NewObject<UDFStructureViewModel>(this);
	Added->SetStructureId(StructureId);
	Structures.Add(Added);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Structures);
	return Added;
}

UDFPickupViewModel* UDFMatchViewModel::FindOrAddPickup(int32 PickupId)
{
	if (UDFPickupViewModel* Existing = FindChild(Pickups, PickupId, [](const UDFPickupViewModel& P) { return P.GetId(); }))
	{
		return Existing;
	}
	UDFPickupViewModel* Added = NewObject<UDFPickupViewModel>(this);
	Added->SetId(PickupId);
	Pickups.Add(Added);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Pickups);
	return Added;
}

UDFVehicleViewModel* UDFMatchViewModel::VehicleById(FName VehicleId) const
{
	return FindChild(Vehicles, VehicleId, [](const UDFVehicleViewModel& V) { return V.GetVehicleId(); });
}

UDFVehicleViewModel* UDFMatchViewModel::FindOrAddVehicle(FName VehicleId)
{
	if (UDFVehicleViewModel* Existing = VehicleById(VehicleId))
	{
		return Existing;
	}
	UDFVehicleViewModel* Added = NewObject<UDFVehicleViewModel>(this);
	Added->SetVehicleId(VehicleId);
	Vehicles.Add(Added);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Vehicles);
	return Added;
}

void UDFMatchViewModel::RetainPlayers(const TSet<int32>& PlayerIds)
{
	const bool bHadLocal = Local() != nullptr;
	if (RemoveChildren(Players, [&PlayerIds](const UDFPlayerViewModel& P) { return PlayerIds.Contains(P.GetPlayerId()); }))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Players);
		if (bHadLocal && !Local())
		{
			UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Local);
		}
	}
}

void UDFMatchViewModel::RetainStructures(const TSet<int32>& StructureIds)
{
	if (RemoveChildren(Structures, [&StructureIds](const UDFStructureViewModel& S) { return StructureIds.Contains(S.GetStructureId()); }))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Structures);
	}
}

void UDFMatchViewModel::RetainPickups(const TSet<int32>& PickupIds)
{
	if (RemoveChildren(Pickups, [&PickupIds](const UDFPickupViewModel& P) { return PickupIds.Contains(P.GetId()); }))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Pickups);
	}
}

void UDFMatchViewModel::RetainVehicles(const TSet<FName>& VehicleIds)
{
	if (RemoveChildren(Vehicles, [&VehicleIds](const UDFVehicleViewModel& V) { return VehicleIds.Contains(V.GetVehicleId()); }))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Vehicles);
	}
}

bool UDFMatchViewModel::RemovePlayer(int32 PlayerId)
{
	const bool bWasLocal = PlayerId == LocalPlayerId && Local() != nullptr;
	if (!RemoveChildren(Players, [PlayerId](const UDFPlayerViewModel& P) { return P.GetPlayerId() != PlayerId; }))
	{
		return false;
	}
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Players);
	if (bWasLocal)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Local);
	}
	return true;
}

bool UDFMatchViewModel::RemoveStructure(int32 StructureId)
{
	if (!RemoveChildren(Structures, [StructureId](const UDFStructureViewModel& S) { return S.GetStructureId() != StructureId; }))
	{
		return false;
	}
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Structures);
	return true;
}

bool UDFMatchViewModel::RemovePickup(int32 PickupId)
{
	if (!RemoveChildren(Pickups, [PickupId](const UDFPickupViewModel& P) { return P.GetId() != PickupId; }))
	{
		return false;
	}
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Pickups);
	return true;
}

bool UDFMatchViewModel::RemoveVehicle(FName VehicleId)
{
	if (!RemoveChildren(Vehicles, [VehicleId](const UDFVehicleViewModel& V) { return V.GetVehicleId() != VehicleId; }))
	{
		return false;
	}
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Vehicles);
	return true;
}

void UDFMatchViewModel::Reset()
{
	const UDFMatchViewModel* Fresh = GetDefault<UDFMatchViewModel>();
	RetainPlayers({});
	RetainStructures({});
	RetainPickups({});
	RetainVehicles({});
	SetValid(false);
	SetLocalPlayerId(Fresh->LocalPlayerId);
	SetMoney(Fresh->Money);
	SetPendingSpend(Fresh->PendingSpend);
	SetLives(Fresh->Lives);
	SetWaveIndex(Fresh->WaveIndex);
	SetTotalWaves(Fresh->TotalWaves);
	SetPhase(Fresh->Phase);
	SetPhaseSecondsLeft(Fresh->PhaseSecondsLeft);
	SetLobby(Fresh->bLobby);
	SetEndless(Fresh->bEndless);
	SetThreat(Fresh->Threat);
	SetEnemiesRemaining(Fresh->EnemiesRemaining);
	SetTeamScrap(Fresh->TeamScrap);
	SetBestWave(Fresh->BestWave);
	SetTier(Fresh->Tier);
	SetActiveCondition(Fresh->ActiveCondition);
	SetNextCondition(Fresh->NextCondition);
	SetBoss(Fresh->Boss);
	SetLaneGateStates(Fresh->LaneGateStates);
	SetMutableStates(Fresh->MutableStates);
	SetEarlyCallVotes(Fresh->EarlyCallVotes);
	SetPings(Fresh->Pings);
	// ConnectionState is the session's, not the match's: it survives leaving a match.
}

// ---------------------------------------------------------------------- match: helpers

UDFPlayerViewModel* UDFMatchViewModel::Local() const
{
	return LocalPlayerId != 0 ? FindPlayer(LocalPlayerId) : nullptr;
}

UDFStructureViewModel* UDFMatchViewModel::AtSocket(FName SocketId) const
{
	return SocketId.IsNone() ? nullptr : FindChild(Structures, SocketId, [](const UDFStructureViewModel& S) { return S.GetSocketId(); });
}

int32 UDFMatchViewModel::PersonalScrapOf(EDFScrapType Type) const
{
	const UDFPlayerViewModel* Me = Local();
	return Me ? Me->ScrapOf(Type) : 0;
}

bool UDFMatchViewModel::SeatOf(int32 PlayerId, UDFVehicleViewModel*& OutVehicle, int32& OutSeat) const
{
	for (UDFVehicleViewModel* Vehicle : Vehicles)
	{
		const int32 Seat = Vehicle ? Vehicle->SeatOf(PlayerId) : INDEX_NONE;
		if (Seat != INDEX_NONE)
		{
			OutVehicle = Vehicle;
			OutSeat = Seat;
			return true;
		}
	}
	OutVehicle = nullptr;
	OutSeat = INDEX_NONE;
	return false;
}
