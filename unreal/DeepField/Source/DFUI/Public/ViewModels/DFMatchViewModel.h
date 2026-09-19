#pragma once

#include "Content/DFContentRows.h"
#include "MVVMViewModelBase.h"
#include "Match/DFMatchTypes.h"
#include "ViewModels/DFViewModelTypes.h"
#include "DFMatchViewModel.generated.h"

class UDFPickupViewModel;
class UDFPlayerViewModel;
class UDFStructureViewModel;
class UDFVehicleViewModel;

/** C12 — the single read model of a match (GameView.cs). Every screen reads this and its children
 *  and nothing else; whatever fills it (the replicated-state binder once ADFMatchState lands, the
 *  fake feed in L_Test_UI) goes through the setters, identically on host and client.
 *
 *  Children are found-or-added by id and keep their identity for as long as the thing exists, so
 *  a widget bound to one player or one tower stays bound while its fields change under it. */
UCLASS(BlueprintType)
class DFUI_API UDFMatchViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

private:
	/** False until a feed has written a first full state; screens show nothing match-shaped before that. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) bool bValid = false;
	DF_VM_BOOL_ACCESSORS(Valid)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) int32 LocalPlayerId = 0;
public:
	int32 GetLocalPlayerId() const { return LocalPlayerId; }
	void SetLocalPlayerId(int32 InValue) { if (UE_MVVM_SET_PROPERTY_VALUE(LocalPlayerId, InValue)) { UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Local); } }
private:

	// ---- economy and progress
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) int32 Money = 0;
	DF_VM_ACCESSORS(int32, Money)
	/** Money the local player has asked to spend that the host has not yet confirmed or refused (§3.3). */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) int32 PendingSpend = 0;
	DF_VM_ACCESSORS(int32, PendingSpend)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) int32 Lives = 0;
	DF_VM_ACCESSORS(int32, Lives)
	/** Zero-based; -1 before the first wave. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) int32 WaveIndex = -1;
	DF_VM_ACCESSORS(int32, WaveIndex)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) int32 TotalWaves = 0;
	DF_VM_ACCESSORS(int32, TotalWaves)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) EDFMatchPhase Phase = EDFMatchPhase::Intermission;
	DF_VM_ACCESSORS(EDFMatchPhase, Phase)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) float PhaseSecondsLeft = 0.f;
	DF_VM_ACCESSORS(float, PhaseSecondsLeft)
	/** The party is still assembling; the match has not launched. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) bool bLobby = false;
	DF_VM_BOOL_ACCESSORS(Lobby)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) bool bEndless = false;
	DF_VM_BOOL_ACCESSORS(Endless)
	/** The hp multiplier the current wave spawned with — the wave plan's own number. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) float Threat = 1.f;
	DF_VM_ACCESSORS(float, Threat)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) int32 EnemiesRemaining = 0;
	DF_VM_ACCESSORS(int32, EnemiesRemaining)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) TMap<EDFScrapType, int32> TeamScrap;
	DF_VM_MAP_ACCESSORS(EDFScrapType, int32, TeamScrap)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) int32 BestWave = 0;
	DF_VM_ACCESSORS(int32, BestWave)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) FGameplayTag Tier;   // DF.Tier.*
	DF_VM_ACCESSORS(FGameplayTag, Tier)

	// ---- the world's state
	/** Condition ids from conditions.json; NAME_None = clear. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) FName ActiveCondition;
	DF_VM_ACCESSORS(FName, ActiveCondition)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) FName NextCondition;
	DF_VM_ACCESSORS(FName, NextCondition)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) FDFBossView Boss;
	DF_VM_ACCESSORS(FDFBossView, Boss)
	/** Gate id -> open. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) TMap<FName, bool> LaneGateStates;
	DF_VM_MAP_ACCESSORS(FName, bool, LaneGateStates)
	/** Mutable id -> state id (floodgate "open", wall "breached", ...). */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) TMap<FName, FName> MutableStates;
	DF_VM_MAP_ACCESSORS(FName, FName, MutableStates)

	// ---- the session
	/** Last DF.Message.ConnectionState payload tag (UDFViewModelSubsystem keeps it current). */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) FGameplayTag ConnectionState;
	DF_VM_ACCESSORS(FGameplayTag, ConnectionState)
	/** PlayerId -> voted to call the next wave early. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) TMap<int32, bool> EarlyCallVotes;
	DF_VM_MAP_ACCESSORS(int32, bool, EarlyCallVotes)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) TArray<FDFPingView> Pings;
	DF_VM_ACCESSORS(TArray<FDFPingView>, Pings)

	// ---- children (changed through FindOrAdd*/Remove*/Reset* below; the arrays notify on add and remove)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) TArray<TObjectPtr<UDFPlayerViewModel>> Players;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) TArray<TObjectPtr<UDFStructureViewModel>> Structures;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) TArray<TObjectPtr<UDFPickupViewModel>> Pickups;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Match", meta = (AllowPrivateAccess = "true")) TArray<TObjectPtr<UDFVehicleViewModel>> Vehicles;

public:
	const TArray<TObjectPtr<UDFPlayerViewModel>>& GetPlayers() const { return Players; }
	const TArray<TObjectPtr<UDFStructureViewModel>>& GetStructures() const { return Structures; }
	const TArray<TObjectPtr<UDFPickupViewModel>>& GetPickups() const { return Pickups; }
	const TArray<TObjectPtr<UDFVehicleViewModel>>& GetVehicles() const { return Vehicles; }

	// ---- feeds write children through these
	UDFPlayerViewModel* FindOrAddPlayer(int32 PlayerId);
	UDFStructureViewModel* FindOrAddStructure(int32 StructureId);
	UDFPickupViewModel* FindOrAddPickup(int32 PickupId);
	UDFVehicleViewModel* FindOrAddVehicle(FName VehicleId);
	bool RemovePlayer(int32 PlayerId);
	bool RemoveStructure(int32 StructureId);
	bool RemovePickup(int32 PickupId);
	bool RemoveVehicle(FName VehicleId);
	/** Drop every child whose id is not in the set — one call per feed pass keeps the lists exact. */
	void RetainPlayers(const TSet<int32>& PlayerIds);
	void RetainStructures(const TSet<int32>& StructureIds);
	void RetainPickups(const TSet<int32>& PickupIds);
	void RetainVehicles(const TSet<FName>& VehicleIds);
	/** Back to a just-constructed state (leaving a match). */
	void Reset();

	// ---- GameView.cs helpers
	/** The local player's view model; null before it has joined. Notifies when the player list or LocalPlayerId changes. */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "DF|Match")
	UDFPlayerViewModel* Local() const;

	UFUNCTION(BlueprintPure, Category = "DF|Match")
	UDFPlayerViewModel* FindPlayer(int32 PlayerId) const;

	UFUNCTION(BlueprintPure, Category = "DF|Match")
	UDFStructureViewModel* AtSocket(FName SocketId) const;

	UFUNCTION(BlueprintPure, Category = "DF|Match")
	bool IsSocketOccupied(FName SocketId) const { return AtSocket(SocketId) != nullptr; }

	UFUNCTION(BlueprintPure, Category = "DF|Match")
	UDFVehicleViewModel* VehicleById(FName VehicleId) const;

	UFUNCTION(BlueprintPure, Category = "DF|Match")
	int32 TeamScrapOf(EDFScrapType Type) const { return TeamScrap.FindRef(Type); }

	/** The local player's own scrap. */
	UFUNCTION(BlueprintPure, Category = "DF|Match")
	int32 PersonalScrapOf(EDFScrapType Type) const;

	/** Which vehicle and seat a player is in; false (and null / -1) on foot. */
	UFUNCTION(BlueprintPure, Category = "DF|Match")
	bool SeatOf(int32 PlayerId, UDFVehicleViewModel*& OutVehicle, int32& OutSeat) const;

	/** Money the player can still commit: Money - PendingSpend, never negative. */
	UFUNCTION(BlueprintPure, Category = "DF|Match")
	int32 SpendableMoney() const { return FMath::Max(0, Money - PendingSpend); }
};
