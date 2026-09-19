#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DFMessages.generated.h"

// C15 — message payloads. Each DF_MSG entry in DFMessageList.inl names one of these structs.
// Payload structs are plain, replication-friendly (fixed fields, no object pointers except
// weak actor refs) and carry ids the way the sim's Events.cs did: PlayerId (seat), EnemyId,
// StructureId (server-assigned int32s), content ids as FName, and refusal reasons as FName
// ("factionTaken", "insufficientScrap", "wouldSeal", "notOwner", ...).

USTRUCT(BlueprintType)
struct DFCORE_API FDFMsg_Player
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 PlayerId = 0;
	UPROPERTY(BlueprintReadOnly) int32 TargetPlayerId = 0;   // revive/drag/kick target when relevant
	UPROPERTY(BlueprintReadOnly) FName Name;
	UPROPERTY(BlueprintReadOnly) FGameplayTag Faction;
	UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFMsg_Rejected
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 PlayerId = 0;
	UPROPERTY(BlueprintReadOnly) FName Reason;               // machine-readable, see messages.md
	UPROPERTY(BlueprintReadOnly) FName Subject;              // content id / socket id / gate id the refusal is about
	UPROPERTY(BlueprintReadOnly) FString Detail;             // e.g. "you are on X, the host is on Y"
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFMsg_Wave
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 WaveIndex = 0;
	UPROPERTY(BlueprintReadOnly) int32 TotalWaves = 0;
	UPROPERTY(BlueprintReadOnly) int32 Lap = 0;
	UPROPERTY(BlueprintReadOnly) float Threat = 1.f;
	UPROPERTY(BlueprintReadOnly) FGameplayTag Condition;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFMsg_Amount
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 PlayerId = 0;
	UPROPERTY(BlueprintReadOnly) float Amount = 0.f;         // lives lost, revive fraction, reload seconds, bonus money
	UPROPERTY(BlueprintReadOnly) FName Subject;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFMsg_Tagged
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 PlayerId = 0;
	UPROPERTY(BlueprintReadOnly) FGameplayTag Tag;
	UPROPERTY(BlueprintReadOnly) FName Id;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFMsg_Vote
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 PlayerId = 0;
	UPROPERTY(BlueprintReadOnly) bool bYes = false;          // this voter's vote
	UPROPERTY(BlueprintReadOnly) int32 YesCount = 0;         // tally so far
	UPROPERTY(BlueprintReadOnly) int32 Needed = 0;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFMsg_Structure
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 StructureId = 0;
	UPROPERTY(BlueprintReadOnly) int32 PlayerId = 0;          // placer / seller / owner
	UPROPERTY(BlueprintReadOnly) FName DefId;
	UPROPERTY(BlueprintReadOnly) FName SocketId;
	UPROPERTY(BlueprintReadOnly) int32 Refund = 0;
	UPROPERTY(BlueprintReadOnly) float HpFraction = 1.f;
	UPROPERTY(BlueprintReadOnly) FName State;                 // barricade: intact/damaged/broken; trap: armed/triggered/spent
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFMsg_Upgrade
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 StructureId = 0;
	UPROPERTY(BlueprintReadOnly) int32 PlayerId = 0;
	UPROPERTY(BlueprintReadOnly) FName PathId;
	UPROPERTY(BlueprintReadOnly) int32 NewLevel = 1;
	UPROPERTY(BlueprintReadOnly) bool bBreakpoint = false;    // L4/L7/L10
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFMsg_Damage
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 TargetId = 0;           // enemy / player / structure id
	UPROPERTY(BlueprintReadOnly) int32 SourcePlayerId = 0;
	UPROPERTY(BlueprintReadOnly) int32 SourceStructureId = 0;
	UPROPERTY(BlueprintReadOnly) FGameplayTag DamageType;
	UPROPERTY(BlueprintReadOnly) FGameplayTag DamageSource;
	UPROPERTY(BlueprintReadOnly) FName Zone;                  // body / weak_n / plate_n / shield
	UPROPERTY(BlueprintReadOnly) float Amount = 0.f;
	UPROPERTY(BlueprintReadOnly) float RemainingFraction = 1.f;
	UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFMsg_Link
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 SourceId = 0;          // overclock / nullifier
	UPROPERTY(BlueprintReadOnly) int32 TargetId = 0;          // fed / tethered structure
	UPROPERTY(BlueprintReadOnly) float Rate = 0.f;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFMsg_Shot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 StructureId = 0;
	UPROPERTY(BlueprintReadOnly) int32 TargetId = 0;
	UPROPERTY(BlueprintReadOnly) FName DefId;
	UPROPERTY(BlueprintReadOnly) FVector Origin = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FVector Impact = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) int32 Tier = 1;
	UPROPERTY(BlueprintReadOnly) float Heat = 0.f;
	UPROPERTY(BlueprintReadOnly) bool bIndirect = false;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFMsg_Enemy
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 EnemyId = 0;
	UPROPERTY(BlueprintReadOnly) FName DefId;
	UPROPERTY(BlueprintReadOnly) FGameplayTagContainer EliteMods;
	UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) int32 Phase = 0;             // boss phase; leak lives; plate index
	UPROPERTY(BlueprintReadOnly) FName Detail;                // route id / plate id / target
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFMsg_Kill
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 EnemyId = 0;
	UPROPERTY(BlueprintReadOnly) FName DefId;
	UPROPERTY(BlueprintReadOnly) int32 KillerPlayerId = 0;    // 0 = a tower/trap
	UPROPERTY(BlueprintReadOnly) int32 KillerStructureId = 0;
	UPROPERTY(BlueprintReadOnly) bool bMelee = false;
	UPROPERTY(BlueprintReadOnly) bool bExecute = false;
	UPROPERTY(BlueprintReadOnly) FGameplayTag ReactionKill;   // set when a reaction dealt the killing burst
	UPROPERTY(BlueprintReadOnly) int32 Bounty = 0;
	UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FVector Impulse = FVector::ZeroVector;   // for the ragdoll
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFMsg_Status
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 TargetId = 0;
	UPROPERTY(BlueprintReadOnly) FGameplayTag Status;         // DF.Status.* or DF.Reaction.*
	UPROPERTY(BlueprintReadOnly) FGameplayTag Channel;
	UPROPERTY(BlueprintReadOnly) int32 SourcePlayerId = 0;
	UPROPERTY(BlueprintReadOnly) int32 SourceStructureId = 0;
	UPROPERTY(BlueprintReadOnly) float Magnitude = 0.f;
	UPROPERTY(BlueprintReadOnly) float Duration = 0.f;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFMsg_Ability
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 PlayerId = 0;
	UPROPERTY(BlueprintReadOnly) int32 OtherPlayerId = 0;     // combo partner
	UPROPERTY(BlueprintReadOnly) FGameplayTag Ability;        // DF.Ability.* or DF.Ability.Combo.*
	UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) float Radius = 0.f;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFMsg_Ping
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 PlayerId = 0;
	UPROPERTY(BlueprintReadOnly) FName Kind;                  // enemy / socket / cache / scrap / danger / go
	UPROPERTY(BlueprintReadOnly) int32 TargetId = 0;
	UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFMsg_Scrap
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 PickupId = 0;
	UPROPERTY(BlueprintReadOnly) int32 PlayerId = 0;          // collector / opener; 0 = team pool
	UPROPERTY(BlueprintReadOnly) FGameplayTag ScrapType;
	UPROPERTY(BlueprintReadOnly) int32 Amount = 0;
	UPROPERTY(BlueprintReadOnly) bool bPersonal = false;
	UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFMsg_Craft
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 PlayerId = 0;
	UPROPERTY(BlueprintReadOnly) FName WeaponId;
	UPROPERTY(BlueprintReadOnly) FName ItemId;                // attachment / ammo / mastery level as name
	UPROPERTY(BlueprintReadOnly) FGameplayTag Slot;
	UPROPERTY(BlueprintReadOnly) int32 Level = 0;             // pack-a-punch / mastery level
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFMsg_Mutable
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName MutableId;             // gate id / wall id / barrel id
	UPROPERTY(BlueprintReadOnly) FGameplayTag Kind;
	UPROPERTY(BlueprintReadOnly) FName EdgeId;
	UPROPERTY(BlueprintReadOnly) FName State;                 // open / closed / broken / washed
	UPROPERTY(BlueprintReadOnly) int32 PlayerId = 0;
	UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFMsg_Vehicle
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName VehicleId;
	UPROPERTY(BlueprintReadOnly) FName DefId;
	UPROPERTY(BlueprintReadOnly) int32 PlayerId = 0;
	UPROPERTY(BlueprintReadOnly) int32 Seat = 0;
	UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector;
};
