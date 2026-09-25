#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "DFTrap.generated.h"

struct FDFTrapRow;

/**
 * A trap on a Trap socket (WS-04): Step.cs's Trap and TriggerTraps, one trap at a time.
 *
 * While armed it watches its radius. Any live, surfaced Ground body inside it sets it off, and
 * everything inside is hit at once: the row's damage, then (if still alive) its status, then (if still
 * alive) a knockback of KnockbackMeters / max(Mass, 0.25) along the lane. One charge per trigger;
 * it then waits RearmSeconds. When the last charge is spent and the rearm has run out it is done, and
 * UDFBuildSubsystem removes it (the sim's `w.Traps.RemoveAll`).
 *
 * Traps are not structures: nothing sieges them (Step.cs SiegeStructures walks towers only).
 */
UCLASS()
class DFTOWERS_API ADFTrap : public AActor
{
	GENERATED_BODY()

public:
	ADFTrap();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Host: become DefId (a traps.json id) on SocketId, built by OwnerSeat. False if the def is unknown. */
	bool InitializeTrap(FName InDefId, FName InSocketId, int32 InOwnerSeat);

	/** Host: one step of Step.cs TriggerTraps for this trap. Tick calls it; tests call it by hand.
	 *  Returns true when it went off this step. */
	bool StepTrap(float DeltaSeconds);

	/** The last charge is gone and the rearm has run out: remove me. */
	bool IsSpent() const { return ChargesLeft <= 0 && RearmTimer <= 0.f; }
	/** Charges left and not rearming: it will go off on the next body in range. */
	bool IsArmed() const { return ChargesLeft > 0 && RearmTimer <= 0.f; }

	const FDFTrapRow* GetRow() const;
	FName GetDefId() const { return DefId; }
	FName GetSocketId() const { return SocketId; }
	int32 GetStructureId() const { return StructureId; }
	int32 GetOwnerSeat() const { return OwnerSeat; }
	int32 GetChargesLeft() const { return ChargesLeft; }
	float GetRearmTimer() const { return RearmTimer; }

private:
	void Announce(const FGameplayTag& Tag, FName State) const;

	UPROPERTY(Replicated) FName DefId;
	UPROPERTY(Replicated) FName SocketId;
	UPROPERTY(Replicated) int32 StructureId = 0;
	UPROPERTY(Replicated) int32 OwnerSeat = 0;
	UPROPERTY(Replicated) int32 ChargesLeft = 0;
	/** Host-only (clients learn armed / triggered from the messages). */
	float RearmTimer = 0.f;
};
