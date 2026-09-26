#pragma once

#include "CoreMinimal.h"
#include "Content/DFContentRows.h"
#include "GameplayTagContainer.h"
#include "World/DFWorldActor.h"
#include "DFSocket.generated.h"

class UBoxComponent;

// C6 ADFSocket{SocketId, Tag}: a build pad. Interact/build target only — a flat 1.1 m pad that
// blocks DF_Interact and DF_Build so the hold-E chain and the build ghost can find it, and
// deliberately ignores DF_Sight and DF_LaneSurface so a pad never occludes a tower's line of sight
// or catches the lane projection trace. No mesh: the pad art is a WS-33 binding, not a rule.
UCLASS()
class DFWORLD_API ADFSocket : public ADFWorldActor
{
	GENERATED_BODY()

public:
	ADFSocket();

	/** Permanent (C6): appears in save data and fixtures; never renamed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	FName SocketId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	EDFSocketTag Tag = EDFSocketTag::Ground;

	/** Pad orientation on a slope, degrees (validator rule 4). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	float PadYaw = 0.f;

	/** Residual slope of the levelled pad, percent (<= 5 after the cut). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DF")
	float PadSlopePercent = 0.f;

	/** 1.1 m pad radius (MAP-AUTHORING "socket pad 1.1 m radius visual, 1.2 m collision"). */
	static constexpr float PadRadiusCm = 110.f;
	static constexpr float PadHalfHeightCm = 10.f;

	virtual FName GetStableId() const override { return SocketId; }

	/** DF.Socket.Ground / Wall / Trap / Barricade for this pad. */
	FGameplayTag GetSocketTag() const;

	/** The centre of the pad's top face, where a tower stands. */
	FVector GetPadTop() const;

	UBoxComponent* GetPad() const { return Pad; }

protected:
	virtual void AssignStableId(FName NewId) override { SocketId = NewId; }

	UPROPERTY(VisibleAnywhere, Category = "DF")
	TObjectPtr<UBoxComponent> Pad;
};
