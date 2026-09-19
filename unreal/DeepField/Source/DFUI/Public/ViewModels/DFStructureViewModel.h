#pragma once

#include "MVVMViewModelBase.h"
#include "ViewModels/DFViewModelTypes.h"
#include "DFStructureViewModel.generated.h"

/** C12 — anything occupying a socket: a tower, a trap, a barricade (GameView.cs StructureView). */
UCLASS(BlueprintType)
class DFUI_API UDFStructureViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Structure", meta = (AllowPrivateAccess = "true")) int32 StructureId = 0;
	DF_VM_ACCESSORS(int32, StructureId)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Structure", meta = (AllowPrivateAccess = "true")) FGameplayTag DefTag;   // DF.Tower.* / DF.Trap.*
	DF_VM_ACCESSORS(FGameplayTag, DefTag)
	/** The same def as a content id ("lance"): the key into UDFContentSubsystem for name, costs, paths. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Structure", meta = (AllowPrivateAccess = "true")) FName DefId;
	DF_VM_ACCESSORS(FName, DefId)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Structure", meta = (AllowPrivateAccess = "true")) FName SocketId;
	DF_VM_ACCESSORS(FName, SocketId)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Structure", meta = (AllowPrivateAccess = "true")) int32 OwnerPlayerId = 0;
	DF_VM_ACCESSORS(int32, OwnerPlayerId)
	/** Level per upgrade path, in the def's path order (up to three). */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Structure", meta = (AllowPrivateAccess = "true")) TArray<int32> PathLevels;
	DF_VM_ACCESSORS(TArray<int32>, PathLevels)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Structure", meta = (AllowPrivateAccess = "true")) bool bTrap = false;
	DF_VM_BOOL_ACCESSORS(Trap)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Structure", meta = (AllowPrivateAccess = "true")) int32 Charges = 0;
	DF_VM_ACCESSORS(int32, Charges)
	/** 0..1. A fraction so no reader needs the def; what cannot be damaged is simply always 1. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Structure", meta = (AllowPrivateAccess = "true")) float HpFrac = 1.f;
	DF_VM_ACCESSORS(float, HpFrac)
	/** Filament ramp / Overclock load, 0..1. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Structure", meta = (AllowPrivateAccess = "true")) float Heat = 0.f;
	DF_VM_ACCESSORS(float, Heat)
	/** StructureId of the Overclock feeding this tower; 0 = none. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Structure", meta = (AllowPrivateAccess = "true")) int32 FedBy = 0;
	DF_VM_ACCESSORS(int32, FedBy)
	/** Whether the local player may sell it — the ownership rule's answer, never recomputed in UI. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Structure", meta = (AllowPrivateAccess = "true")) bool bSellable = false;
	DF_VM_BOOL_ACCESSORS(Sellable)

public:
	/** Level of one path; 0 if the structure has no such path. */
	UFUNCTION(BlueprintPure, Category = "DF|Structure")
	int32 PathLevel(int32 PathIndex) const { return PathLevels.IsValidIndex(PathIndex) ? PathLevels[PathIndex] : 0; }
};
