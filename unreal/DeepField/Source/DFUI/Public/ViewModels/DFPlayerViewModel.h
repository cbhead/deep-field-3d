#pragma once

#include "Content/DFContentRows.h"
#include "MVVMViewModelBase.h"
#include "Match/DFMatchTypes.h"
#include "ViewModels/DFViewModelTypes.h"
#include "DFPlayerViewModel.generated.h"

/** C12 — one player, local or not (GameView.cs PlayerView + the Unreal-era fields). */
UCLASS(BlueprintType)
class DFUI_API UDFPlayerViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) int32 PlayerId = 0;
	DF_VM_ACCESSORS(int32, PlayerId)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) FString Name;
	DF_VM_ACCESSORS(FString, Name)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) FGameplayTag Faction;   // DF.Faction.*
	DF_VM_ACCESSORS(FGameplayTag, Faction)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) int32 FactionLevel = 1;
	DF_VM_ACCESSORS(int32, FactionLevel)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) FVector Position = FVector::ZeroVector;
	DF_VM_ACCESSORS(FVector, Position)

	// ---- vitals
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) float Hp = 0.f;
public:
	float GetHp() const { return Hp; }
	void SetHp(float InValue) { if (UE_MVVM_SET_PROPERTY_VALUE(Hp, InValue)) { UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(HpFrac); } }
private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) float MaxHp = 100.f;
public:
	float GetMaxHp() const { return MaxHp; }
	void SetMaxHp(float InValue) { if (UE_MVVM_SET_PROPERTY_VALUE(MaxHp, InValue)) { UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(HpFrac); } }
private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) bool bDowned = false;
	DF_VM_BOOL_ACCESSORS(Downed)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) bool bConnected = true;
	DF_VM_BOOL_ACCESSORS(Connected)
	/** 0..1 through the hold-to-revive channel; a teammate watching sees the same arc as the one reviving. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) float ReviveProgress = 0.f;
	DF_VM_ACCESSORS(float, ReviveProgress)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) float BleedoutSecondsLeft = 0.f;
	DF_VM_ACCESSORS(float, BleedoutSecondsLeft)

	// ---- loadout
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) FName Weapon = TEXT("sidearm");
	DF_VM_ACCESSORS(FName, Weapon)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) FName Melee = TEXT("wrench");
	DF_VM_ACCESSORS(FName, Melee)
	/** 0 = ready, 1 = just used. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) float AbilityCooldownFrac = 0.f;
	DF_VM_ACCESSORS(float, AbilityCooldownFrac)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) TMap<EDFScrapType, int32> Scrap;
	DF_VM_MAP_ACCESSORS(EDFScrapType, int32, Scrap)

	// ---- match record
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) int32 MatchXp = 0;
	DF_VM_ACCESSORS(int32, MatchXp)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) int32 Kills = 0;
	DF_VM_ACCESSORS(int32, Kills)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) float DamageDealt = 0.f;
	DF_VM_ACCESSORS(float, DamageDealt)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) int32 TowersBuilt = 0;
	DF_VM_ACCESSORS(int32, TowersBuilt)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) int32 Revives = 0;
	DF_VM_ACCESSORS(int32, Revives)

	// ---- gunsmith (drives the armory)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) TArray<FName> OwnedWeapons = { TEXT("sidearm") };
	DF_VM_ACCESSORS(TArray<FName>, OwnedWeapons)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) TArray<FName> CraftedAmmo = { TEXT("standard") };
	DF_VM_ACCESSORS(TArray<FName>, CraftedAmmo)
	/** Weapon id -> its build. A weapon with no entry is stock. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) TMap<FName, FDFWeaponBuild> Builds;
	DF_VM_MAP_ACCESSORS(FName, FDFWeaponBuild, Builds)

	// ---- where they are, what they are doing
	/** Seat index in whatever vehicle they are in, -1 on foot (UDFMatchViewModel::SeatOf names the vehicle). */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) int32 Seat = -1;
	DF_VM_ACCESSORS(int32, Seat)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) bool bHost = false;
	DF_VM_BOOL_ACCESSORS(Host)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) bool bCarrying = false;
	DF_VM_BOOL_ACCESSORS(Carrying)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) bool bDragging = false;
	DF_VM_BOOL_ACCESSORS(Dragging)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Player", meta = (AllowPrivateAccess = "true")) bool bInNest = false;
	DF_VM_BOOL_ACCESSORS(InNest)

public:
	/** Hp / MaxHp, 0..1; notifies whenever either changes. */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "DF|Player")
	float HpFrac() const { return MaxHp > 0.f ? FMath::Clamp(Hp / MaxHp, 0.f, 1.f) : 0.f; }

	UFUNCTION(BlueprintPure, Category = "DF|Player")
	int32 ScrapOf(EDFScrapType Type) const { return Scrap.FindRef(Type); }

	UFUNCTION(BlueprintPure, Category = "DF|Player")
	bool Owns(FName WeaponId) const { return OwnedWeapons.Contains(WeaponId); }

	UFUNCTION(BlueprintPure, Category = "DF|Player")
	int32 PackLevelFor(FName WeaponId) const;

	/** "standard" for a stock weapon. */
	UFUNCTION(BlueprintPure, Category = "DF|Player")
	FName AmmoFor(FName WeaponId) const;

	/** NAME_None for an empty slot. */
	UFUNCTION(BlueprintPure, Category = "DF|Player")
	FName AttachmentFor(FName WeaponId, EDFAttachmentSlot Slot) const;
};
