#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DFViewModelTypes.generated.h"

// C12 support types and the accessor macros every DF view model uses.
//
// Rules of the house (CONTRACTS/viewmodel.md):
//  - A widget reads view models and nothing else. No widget, and no view model, asks whether it
//    is the host: DF.UI.NoNetBranching greps this module for the authority / net-mode calls
//    (not spelled here, or this comment would fail it).
//  - Fields are private and change only through their setter, which notifies only on a real
//    change — a feed may write every field every frame without waking a single binding.
//  - Content ids are FName exactly as the JSON spells them ("emberPistol"); `DefTag`, `Faction`,
//    `Tier` and `ConnectionState` are gameplay tags, as the messages carry them.

/** Getter + notify-on-change setter for a FieldNotify property declared on the line above. */
#define DF_VM_ACCESSORS(Type, Name) \
	public: \
		const Type& Get##Name() const { return Name; } \
		void Set##Name(const Type& InValue) { UE_MVVM_SET_PROPERTY_VALUE(Name, InValue); } \
	private:

/** The same for `bool bName`: IsName() / SetName(). */
#define DF_VM_BOOL_ACCESSORS(Name) \
	public: \
		bool Is##Name() const { return b##Name; } \
		void Set##Name(bool bInValue) { UE_MVVM_SET_PROPERTY_VALUE(b##Name, bInValue); } \
	private:

/** For TMap fields, which have no operator==. */
#define DF_VM_MAP_ACCESSORS(KeyType, ValueType, Name) \
	public: \
		const TMap<KeyType, ValueType>& Get##Name() const { return Name; } \
		void Set##Name(const TMap<KeyType, ValueType>& InValue) \
		{ \
			if (!Name.OrderIndependentCompareEqual(InValue)) \
			{ \
				Name = InValue; \
				UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Name); \
			} \
		} \
	private:

/** The boss bar (B§2.4). bActive false = no boss on the field; the rest is then meaningless. */
USTRUCT(BlueprintType)
struct DFUI_API FDFBossView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "DF|Boss") bool bActive = false;
	UPROPERTY(BlueprintReadOnly, Category = "DF|Boss") FGameplayTag Phase;          // DF.Boss.Phase.*
	UPROPERTY(BlueprintReadOnly, Category = "DF|Boss") float HpFrac = 1.f;
	UPROPERTY(BlueprintReadOnly, Category = "DF|Boss") int32 Plates = 0;            // armour plates still attached

	bool operator==(const FDFBossView& Other) const { return bActive == Other.bActive && Phase == Other.Phase && HpFrac == Other.HpFrac && Plates == Other.Plates; }
	bool operator!=(const FDFBossView& Other) const { return !(*this == Other); }
};

/** A teammate's ping (B§3, co-op verbs). */
USTRUCT(BlueprintType)
struct DFUI_API FDFPingView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "DF|Ping") int32 PlayerId = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DF|Ping") FName Kind;                   // "enemy", "build", "danger", ...
	UPROPERTY(BlueprintReadOnly, Category = "DF|Ping") FVector Position = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "DF|Ping") float SecondsLeft = 0.f;

	bool operator==(const FDFPingView& Other) const { return PlayerId == Other.PlayerId && Kind == Other.Kind && Position == Other.Position && SecondsLeft == Other.SecondsLeft; }
	bool operator!=(const FDFPingView& Other) const { return !(*this == Other); }
};
