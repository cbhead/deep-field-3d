#pragma once

#include "CoreMinimal.h"
#include "Content/DFContentRows.h"
#include "GameplayTagContainer.h"
#include "World/DFWorldActor.h"
#include "DFSocket.generated.h"

class UBoxComponent;
class UInstancedStaticMeshComponent;
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
// it while it is the local player's build target (ADFPlayerController sets it), with a dashed range ring
// on the ground showing how far the tower hold E would build (or the one standing there) reaches. A
// Ground or Wall pad's plate fills the pad box (a tower stands on it; the validator keeps these off the
// lane); a Trap or Barricade pad lies in the lane, so its plate is flush with the ground and enemies walk
// over it, not through it. All of it is looks only: it collides with nothing.
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

	/**
	 * Local and cosmetic: a dashed ring on the ground round the pad at RadiusCm from its centre (a tower's
	 * reach), in Tone's aim-ring colour, and a second one at MinRadiusCm when the tower cannot fire that
	 * close (a mortar's dead zone). RadiusCm 0 (or Tone None) hides it. The controller calls it every
	 * frame it aims, so a call that changes nothing does nothing, and the segments are laid again only
	 * when a radius changes: hiding keeps them, so aiming back at the pad shows the same ring for free.
	 * The ring is level at the socket's origin height, whatever the pad's own tilt (an in-lane pad on a
	 * slope; the reach it shows is a distance, not a surface).
	 */
	void SetRangePreview(float RadiusCm, EDFPadHighlight Tone, float MinRadiusCm = 0.f);
	/** The reach the ring shows, cm; 0 while it is hidden. */
	float GetRangePreviewRadius() const { return RangeTone != EDFPadHighlight::None ? RangeRingRadiusCm : 0.f; }
	/** The inner ring's radius, cm; 0 while hidden or when there is no inner ring. */
	float GetRangePreviewMinRadius() const { return RangeTone != EDFPadHighlight::None ? RangeRingMinRadiusCm : 0.f; }
	EDFPadHighlight GetRangePreviewTone() const { return RangeTone; }
	/** How many times the segments have been laid (tests: an unchanged radius must not lay them again). */
	int32 GetRangeRingLayCount() const { return RangeRingLays; }
	UInstancedStaticMeshComponent* GetRangeRing() const { return RangeRing; }

	/** Segments in one circle of RadiusCm: one per RangeSegmentSpacingCm of circumference, so a 20 m
	 *  reach still reads as a circle, clamped so a small one stays round and a huge one stays cheap. */
	static int32 RangeSegmentsFor(float RadiusCm);
	static constexpr float RangeSegmentSpacingCm = 75.f;
	static constexpr int32 MinRangeSegments = 24;
	static constexpr int32 MaxRangeSegments = 256;
	/** Each segment covers this share of its arc: a dashed line reads as "would reach", not a wall. */
	static constexpr float RangeDashFraction = 0.6f;
	static constexpr float RangeRingWidthCm = 10.f;
	static constexpr float RangeRingHeightCm = 2.f;
	/** Segment centre above the socket's origin: its top (5 cm) clears the lane strip (top at 3 cm) it crosses. */
	static constexpr float RangeRingLiftCm = 4.f;

	/** The plate's colour for Tag. */
	static FLinearColor PlateColourFor(EDFSocketTag InTag);
	/** The aim ring's (and range ring's) colour for a highlight: green Free, gold Occupied, red Blocked. */
	static FLinearColor AimColourFor(EDFPadHighlight Highlight);

	/** Tint the plate for the current Tag (construction and BeginPlay; the tint is never saved). */
	void RefreshLook();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** The plate stands on the pad box: its top is the pad top, where a tower stands. */
	static constexpr float PlateRadiusCm = PadRadiusCm;
	/** A Trap or Barricade pad's plate: flush, so a body walking the lane crosses it. Its top clears
	 *  the lane strip (ADFLaneGraphInfo: 2 cm thick, lifted 1 cm), which would otherwise paint over it. */
	static constexpr float FlushPlateHeightCm = 4.f;
	/** Trap and Barricade pads lie in the lane (their plate is flush). */
	static bool LiesInLane(EDFSocketTag InTag) { return InTag == EDFSocketTag::Trap || InTag == EDFSocketTag::Barricade; }
	/** The ring shows this far outside the plate. */
	static constexpr float AimRingMarginCm = 18.f;
	/** Above a flush plate's top (so over an in-lane pad the whole disc lights), below a full plate's. */
	static constexpr float AimRingHeightCm = 5.f;

	/**
	 * An in-lane pad lies on the lane, and lanes climb (Foundry's trap pads sit on 13-25 % grades): tilt
	 * the flush plate and the ring to the lane surface under the pad (a DF_LaneSurface trace, the channel
	 * lane projection uses), so neither stands out of a slope for the wave to walk through. Game worlds
	 * call it at BeginPlay; a no-op for pads that are not in the lane or with no surface under them.
	 * Returns whether it found a surface.
	 */
	bool AlignToLaneSurface();

protected:
	virtual void BeginPlay() override;

	virtual void AssignStableId(FName NewId) override { SocketId = NewId; }

	UPROPERTY(VisibleAnywhere, Category = "DF")
	TObjectPtr<UBoxComponent> Pad;

	UPROPERTY(VisibleAnywhere, Category = "DF|Look")
	TObjectPtr<UStaticMeshComponent> Plate;

	/** Shown instead of Plate on a pad that lies in the lane (toggled by visibility: Plate is Static and a
	 *  placed pad's Tag is only known at play). Movable, like AimRing, so it can lie along a sloped lane. */
	UPROPERTY(VisibleAnywhere, Category = "DF|Look")
	TObjectPtr<UStaticMeshComponent> FlushPlate;

	UPROPERTY(VisibleAnywhere, Category = "DF|Look")
	TObjectPtr<UStaticMeshComponent> AimRing;

	/** The range ring's segments: engine cubes, one instanced draw. Absolute rotation and scale, so it
	 *  stays a level, true circle whatever the actor's transform. Empty and hidden until aimed at. */
	UPROPERTY(VisibleAnywhere, Category = "DF|Look")
	TObjectPtr<UInstancedStaticMeshComponent> RangeRing;

	EDFPadHighlight AimHighlight = EDFPadHighlight::None;
	FTimerHandle AlignRetry;

private:
	/** Replace the ring's segments with circles of RadiusCm and (if > 0) MinRadiusCm. */
	void LayRangeRing(float RadiusCm, float MinRadiusCm);

	/** What the segments were laid for (kept while hidden), and the tone they show in (None: hidden). */
	float RangeRingRadiusCm = 0.f;
	float RangeRingMinRadiusCm = 0.f;
	EDFPadHighlight RangeTone = EDFPadHighlight::None;
	int32 RangeRingLays = 0;
};
