#pragma once

#include "CoreMinimal.h"
#include "Content/DFContentRows.h"
#include "GameplayTagContainer.h"
#include "World/DFWorldActor.h"
#include "DFSocket.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/** What the local player's crosshair says about a pad (cosmetic, this machine only). */
UENUM()
enum class EDFPadHighlight : uint8
{
	None,
	/** Hold E would build here. */
	Free,
	/** A tower stands here: hold U upgrades it, hold X sells it. */
	Occupied,
	/** Hold E would be refused: the chosen tower does not fit this pad, or a trap stands on it. */
	Blocked,
};

// C6 ADFSocket{SocketId, Tag}: a build pad. Interact/build target only — a flat 1.1 m pad that
// blocks DF_Interact and DF_Build so the hold-E chain and the build ghost can find it, and
// deliberately ignores DF_Sight and DF_LaneSurface so a pad never occludes a tower's line of sight
// or catches the lane projection trace. The pad art is a WS-33 binding, not a rule; until it exists
// the pad draws a placeholder (DFWorldLook): a plate tinted by what it takes, and a glowing ring round
// it while it is the local player's build target (ADFPlayerController sets it). A Ground or Wall pad's
// plate fills the pad box (a tower stands on it; the validator keeps these off the lane); a Trap or
// Barricade pad lies in the lane, so its plate is flush with the ground and enemies walk over it, not
// through it. All of it is looks only: it collides with nothing.
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

	/** Local and cosmetic: show (or hide) the aim ring. The controller calls it every frame it aims. */
	void SetAimHighlight(EDFPadHighlight Highlight);
	EDFPadHighlight GetAimHighlight() const { return AimHighlight; }

	/** The plate's colour for Tag. */
	static FLinearColor PlateColourFor(EDFSocketTag InTag);

	/** Tint the plate for the current Tag (construction and BeginPlay; the tint is never saved). */
	void RefreshLook();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** The plate stands on the pad box: its top is the pad top, where a tower stands. */
	static constexpr float PlateRadiusCm = PadRadiusCm;
	/** A Trap or Barricade pad's plate: flush, so a body walking the lane crosses it. */
	static constexpr float FlushPlateHeightCm = 2.f;
	/** Trap and Barricade pads lie in the lane (their plate is flush). */
	static bool LiesInLane(EDFSocketTag InTag) { return InTag == EDFSocketTag::Trap || InTag == EDFSocketTag::Barricade; }
	/** The ring shows this far outside the plate. */
	static constexpr float AimRingMarginCm = 18.f;
	static constexpr float AimRingHeightCm = 4.f;

protected:
	virtual void BeginPlay() override;

	virtual void AssignStableId(FName NewId) override { SocketId = NewId; }

	UPROPERTY(VisibleAnywhere, Category = "DF")
	TObjectPtr<UBoxComponent> Pad;

	UPROPERTY(VisibleAnywhere, Category = "DF|Look")
	TObjectPtr<UStaticMeshComponent> Plate;

	/** Shown instead of Plate on a pad that lies in the lane. Two parts, toggled by visibility, because
	 *  a Static component may not be moved at play and a placed pad's Tag is only known then. */
	UPROPERTY(VisibleAnywhere, Category = "DF|Look")
	TObjectPtr<UStaticMeshComponent> FlushPlate;

	UPROPERTY(VisibleAnywhere, Category = "DF|Look")
	TObjectPtr<UStaticMeshComponent> AimRing;

	EDFPadHighlight AimHighlight = EDFPadHighlight::None;
};
