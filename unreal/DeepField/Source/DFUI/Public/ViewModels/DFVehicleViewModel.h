#pragma once

#include "MVVMViewModelBase.h"
#include "ViewModels/DFViewModelTypes.h"
#include "DFVehicleViewModel.generated.h"

/** C12 — a vehicle as the UI needs it: where it is and who is in it (GameView.cs VehicleView).
 *  Seats are the server's answer; a client that decided for itself who was driving would put two
 *  players in one seat the first time two of them pressed E at once. */
UCLASS(BlueprintType)
class DFUI_API UDFVehicleViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Vehicle", meta = (AllowPrivateAccess = "true")) FName VehicleId;
	DF_VM_ACCESSORS(FName, VehicleId)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Vehicle", meta = (AllowPrivateAccess = "true")) FGameplayTag DefTag;   // DF.Vehicle.*
	DF_VM_ACCESSORS(FGameplayTag, DefTag)
	/** Content id ("buggy"). C1 has no DF.Vehicle.* root yet, so until WS-08 adds one this is the usable key. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Vehicle", meta = (AllowPrivateAccess = "true")) FName DefId;
	DF_VM_ACCESSORS(FName, DefId)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Vehicle", meta = (AllowPrivateAccess = "true")) FVector Position = FVector::ZeroVector;
	DF_VM_ACCESSORS(FVector, Position)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Vehicle", meta = (AllowPrivateAccess = "true")) float Yaw = 0.f;
	DF_VM_ACCESSORS(float, Yaw)
	/** PlayerId per seat, 0 = empty; seat 0 drives. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Vehicle", meta = (AllowPrivateAccess = "true")) TArray<int32> Seats;
	DF_VM_ACCESSORS(TArray<int32>, Seats)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Vehicle", meta = (AllowPrivateAccess = "true")) float HpFrac = 1.f;
	DF_VM_ACCESSORS(float, HpFrac)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Vehicle", meta = (AllowPrivateAccess = "true")) bool bWreck = false;
	DF_VM_BOOL_ACCESSORS(Wreck)

public:
	/** Seat index of a player, or -1. */
	UFUNCTION(BlueprintPure, Category = "DF|Vehicle")
	int32 SeatOf(int32 PlayerId) const { return PlayerId != 0 ? Seats.IndexOfByKey(PlayerId) : INDEX_NONE; }

	UFUNCTION(BlueprintPure, Category = "DF|Vehicle")
	bool IsSeatFree(int32 SeatIndex) const { return Seats.IsValidIndex(SeatIndex) && Seats[SeatIndex] == 0; }

	UFUNCTION(BlueprintPure, Category = "DF|Vehicle")
	int32 DriverId() const { return Seats.Num() > 0 ? Seats[0] : 0; }
};
