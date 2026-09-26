#pragma once

#include "Content/DFContentRows.h"
#include "MVVMViewModelBase.h"
#include "ViewModels/DFViewModelTypes.h"
#include "DFPickupViewModel.generated.h"

/** C12 — scrap on the floor (GameView.cs PickupView). */
UCLASS(BlueprintType)
class DFUI_API UDFPickupViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Pickup", meta = (AllowPrivateAccess = "true")) int32 Id = 0;
	DF_VM_ACCESSORS(int32, Id)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Pickup", meta = (AllowPrivateAccess = "true")) EDFScrapType ScrapType = EDFScrapType::Alloy;
	DF_VM_ACCESSORS(EDFScrapType, ScrapType)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Pickup", meta = (AllowPrivateAccess = "true")) int32 Amount = 0;
	DF_VM_ACCESSORS(int32, Amount)
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Pickup", meta = (AllowPrivateAccess = "true")) FVector Position = FVector::ZeroVector;
	DF_VM_ACCESSORS(FVector, Position)
	/** Until it leaves the floor for the team pool (the 45 s rule). */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Pickup", meta = (AllowPrivateAccess = "true")) float SecondsLeft = 0.f;
	DF_VM_ACCESSORS(float, SecondsLeft)
	/** Only the local player can collect it (their half of a kill). */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "DF|Pickup", meta = (AllowPrivateAccess = "true")) bool bPersonal = false;
	DF_VM_BOOL_ACCESSORS(Personal)
};
