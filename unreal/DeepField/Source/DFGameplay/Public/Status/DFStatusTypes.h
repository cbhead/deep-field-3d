#pragma once

#include "Content/DFContentRows.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DFStatusTypes.generated.h"

// C5 value types shared by FDFStatusResolver (pure) and UDFStatusComponent (world).

/** What Apply() did with a status. The first six are the contract's; Reacted and NoRow are documented extras. */
UENUM(BlueprintType)
enum class EDFStatusApplyResult : uint8
{
	Applied,           // written to an empty slot, or replaced a weaker status in the channel
	Refreshed,         // same id already active: duration and source refreshed
	DroppedWeaker,     // a stronger (or equal) status holds the channel; nothing changed
	RejectedShield,    // Thermal on a target with Shield > 0
	RejectedCcResist,  // hard control while the cc-resist gauge is full
	RejectedImmune,    // Tether on Mass >= TetherImmuneMass / an immune tag / bTetherImmune
	Reacted,           // an active partner matched a reaction: both inputs consumed, outputs written
	NoRow,             // unknown status id, or called without authority
};

/** One channel slot. Active when StatusId is set; EndTime is in the owner's clock (world seconds). */
USTRUCT(BlueprintType)
struct DFGAMEPLAY_API FDFStatusSlot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName StatusId;
	UPROPERTY(BlueprintReadOnly) EDFStatusChannel Channel = EDFStatusChannel::Movement;
	UPROPERTY(BlueprintReadOnly) float Magnitude = 0.f;
	UPROPERTY(BlueprintReadOnly) float EndTime = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 SourceId = 0;
	/** How fast this status fills the cc-resist gauge (row CcResistFill for hard control / Tether, else 0). */
	UPROPERTY(BlueprintReadOnly) float CcFillScale = 0.f;
	UPROPERTY(BlueprintReadOnly) bool bHardControl = false;

	bool IsActive() const { return !StatusId.IsNone(); }
	void Clear()
	{
		StatusId = NAME_None;
		Magnitude = 0.f;
		EndTime = 0.f;
		SourceId = 0;
		CcFillScale = 0.f;
		bHardControl = false;
	}
};

/** The replicated shape of a slot (C5: ADFEnemy::StatusSlots[8] {StatusTag, EndTimeServer}). */
USTRUCT(BlueprintType)
struct DFGAMEPLAY_API FDFStatusSlotRep
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FGameplayTag StatusTag;
	UPROPERTY(BlueprintReadOnly) float EndTimeServer = 0.f;
	UPROPERTY(BlueprintReadOnly) float Magnitude = 0.f;

	bool IsActive() const { return StatusTag.IsValid(); }
	bool operator==(const FDFStatusSlotRep& Other) const
	{
		return StatusTag == Other.StatusTag && EndTimeServer == Other.EndTimeServer && Magnitude == Other.Magnitude;
	}
};

/** The facts about the target the gates need (read from the actor by the component, given by the test). */
USTRUCT(BlueprintType)
struct DFGAMEPLAY_API FDFStatusTargetState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) float Shield = 0.f;
	UPROPERTY(BlueprintReadWrite) float Mass = 1.f;
	/** Skater / boss / DF.Enemy.State.Phased: Tether never lands. */
	UPROPERTY(BlueprintReadWrite) bool bTetherImmune = false;
};

/** Everything one Apply() decided, so the component can mirror it into effects, cues and messages. */
USTRUCT(BlueprintType)
struct DFGAMEPLAY_API FDFStatusApplyOutcome
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) EDFStatusApplyResult Result = EDFStatusApplyResult::NoRow;
	UPROPERTY(BlueprintReadOnly) FName StatusId;
	UPROPERTY(BlueprintReadOnly) EDFStatusChannel Channel = EDFStatusChannel::Movement;
	UPROPERTY(BlueprintReadOnly) float Magnitude = 0.f;
	UPROPERTY(BlueprintReadOnly) float Duration = 0.f;
	/** Strongest-wins: the weaker status that left Channel (Applied only). */
	UPROPERTY(BlueprintReadOnly) FName ReplacedStatusId;

	// Reacted only.
	UPROPERTY(BlueprintReadOnly) FName ReactionId;
	UPROPERTY(BlueprintReadOnly) float BurstFraction = 0.f;
	UPROPERTY(BlueprintReadOnly) FName ConsumedStatusId;
	UPROPERTY(BlueprintReadOnly) EDFStatusChannel ConsumedChannel = EDFStatusChannel::Movement;
	UPROPERTY(BlueprintReadOnly) FName EmittedStatusId;
	UPROPERTY(BlueprintReadOnly) EDFStatusChannel EmittedChannel = EDFStatusChannel::Movement;
	UPROPERTY(BlueprintReadOnly) float EmittedMagnitude = 0.f;
	UPROPERTY(BlueprintReadOnly) float EmittedDuration = 0.f;
	/** The status the emitted output displaced in its channel (it is written straight in, never compared). */
	UPROPERTY(BlueprintReadOnly) FName EmitReplacedStatusId;
	/** The row named an EmitStatus but the cc-resist gate refused it (hard control at full gauge). */
	UPROPERTY(BlueprintReadOnly) bool bEmitRejected = false;
	/** The row named an EmitStatus but the burst killed the target first (Step.cs: `EmitStatus ... && !enemy.Dead`). */
	UPROPERTY(BlueprintReadOnly) bool bEmitSkippedDead = false;
};
