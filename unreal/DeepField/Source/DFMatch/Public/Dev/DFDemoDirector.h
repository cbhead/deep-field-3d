#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DFDemoDirector.generated.h"

class ACameraActor;

/**
 * Attract mode for a solo host (dev builds only): start the game with `-DFDemo` and the match plays
 * itself for a recording or a showcase. ADFMatchState spawns one on the host when the flag is set.
 *
 * - **Builds** as the sim's player would, through UDFBuildSubsystem and WS-06's economy (no free
 *   money): every BuildIntervalSeconds it tries the next free pad, pads nearest the spawn portal
 *   first, a Nova every NovaEvery-th build and a Lance otherwise; a refusal (not enough money) just
 *   waits for bounty.
 * - **Films**: the first local player's view moves to a camera that orbits the lane's bounds (pads,
 *   portal, core) from above, slowly; its focus leans toward the live enemies (UDFTargetRegistry), so
 *   the fight stays in frame with the towers and the HUD.
 *
 * Pair it with `-benchmark -fps=20 -dumpmovie` to write every frame to Saved/Screenshots at a fixed
 * step (unreal/Build/record-demo.ps1 does that and encodes the video).
 */
UCLASS(NotBlueprintable, NotPlaceable)
class DFMATCH_API ADFDemoDirector : public AActor
{
	GENERATED_BODY()

public:
	ADFDemoDirector();

	/** `-DFDemo` on the command line, in a build that has dev tools. */
	static bool IsRequested();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "DF|Demo") float BuildIntervalSeconds = 1.f;
	UPROPERTY(EditAnywhere, Category = "DF|Demo") int32 NovaEvery = 3;
	UPROPERTY(EditAnywhere, Category = "DF|Demo") float OrbitDegreesPerSecond = 3.f;
	UPROPERTY(EditAnywhere, Category = "DF|Demo") float CameraFieldOfView = 60.f;
	/** How far the focus leans from the map's centre toward the enemies' centroid (0..1), and how fast. */
	UPROPERTY(EditAnywhere, Category = "DF|Demo") float FollowWeight = 0.6f;
	UPROPERTY(EditAnywhere, Category = "DF|Demo") float FollowSpeed = 0.8f;

private:
	void TryBuild();
	/** Pads nearest a spawn portal first, and the orbit's centre and size from what the map holds. */
	bool Survey();
	void PlaceCamera();

	UPROPERTY(Transient) TObjectPtr<ACameraActor> Camera;

	TArray<FName> BuildOrder;
	int32 Builds = 0;
	bool bSurveyed = false;
	FVector MapCentre = FVector::ZeroVector;
	FVector Focus = FVector::ZeroVector;
	float OrbitRadiusCm = 4000.f;
	float OrbitHeightCm = 2500.f;
	float OrbitDeg = 200.f;
	float BuildClock = 0.f;
};
