#pragma once

#include "CoreMinimal.h"
#include "World/DFWorldActor.h"
#include "DFOperatedGate.generated.h"

// C6 OperatedGates[]: a lever and the lane it shuts. Free, instant, reversible; stops walkers and
// not sieges (nothing to break, so a Ram walks through); refuses to shut on a body within
// BodyCheckMeters; CooldownSeconds between flips so a lever is not a lane you toggle every time a
// tower is about to fire. The numbers come from the lane graph asset (6 s / 9 m / 4 m by default).
UCLASS()
class DFWORLD_API ADFOperatedGate : public ADFWorldActor
{
	GENERATED_BODY()

public:
	ADFOperatedGate();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	FName EdgeId;

	/** Shown on the lever prompt ("FREIGHT CUT"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	FString Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	float CooldownSeconds = 6.f;

	/** How far a player may stand from the lever and still pull it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	float ReachMeters = 9.f;

	/** An enemy inside this radius denies the close. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	float BodyCheckMeters = 4.f;

	/** Replicated: whether the lane is currently shut by this lever. */
	UPROPERTY(ReplicatedUsing = OnRep_Closed, BlueprintReadOnly, Category = "DF")
	bool bClosed = false;

	virtual FName GetStableId() const override { return Id; }

	/** Seconds until the lever can be pulled again (0 = now). */
	float CooldownRemaining() const;

	/** True when an enemy body (DF_Enemy) is within BodyCheckMeters of the gateway. */
	bool HasBodyWithin() const;

	/** Whether a pull is allowed right now; the reason is one of "cooldown", "bodyInGateway", "tooFar". */
	bool CanToggle(const FVector& FromLocation, FName* OutReason = nullptr) const;

	/** Authority only. Flips the lane and restarts the cooldown; returns false with the reason when refused. */
	bool TryToggle(const FVector& FromLocation, FName* OutReason = nullptr);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void AssignStableId(FName NewId) override { Id = NewId; }

	UFUNCTION()
	void OnRep_Closed();

	/** World time of the last flip (authority). */
	float LastToggleTime = -1000.f;
};
