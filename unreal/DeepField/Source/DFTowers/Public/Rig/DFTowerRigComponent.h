#pragma once

#include "Components/SceneComponent.h"
#include "Content/DFContentRows.h"
#include "CoreMinimal.h"
#include "Rig/DFTowerRig.h"
#include "DFTowerRigComponent.generated.h"

class UDFTowerDefinition;
class UStaticMeshComponent;

/** How a tower moves (rig.md). */
UENUM()
enum class EDFTowerRigStyle : uint8
{
	/** A barricade: it stands; its damage states are a geometry collection (GC_Barricade). */
	None,
	/** Foot -> Yaw -> Pitch -> Muzzle: bolt, mortar, flak, beam. */
	Turret,
	/** Foot -> Spin: aura, tesla, support. */
	Spin,
};

/**
 * C9 — the tower rig (rig.md). Cosmetic, on every machine that draws: the host decides what a tower
 * shoots (UDFTargetingComponent); each machine turns its own turret toward the replicated target.
 *
 * The component chain (Foot, Yaw, Pitch, Muzzle; or Foot, Spin) is built from the tower's
 * UDFTowerDefinition when it has one. Without one (no DA_Tower_<id> has been imported yet) the rig
 * still keeps its angles, so aim, settle and the tests work; there is just nothing to draw.
 */
UCLASS(ClassGroup = (DF), meta = (BlueprintSpawnableComponent))
class DFTOWERS_API UDFTowerRigComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UDFTowerRigComponent();

	/** Set the rig up for TowerId (a towers.json id) of Kind. Definition may be null. Rebuilds the parts. */
	void Configure(FName TowerId, EDFTowerKind Kind, const UDFTowerDefinition* Definition);

	/**
	 * Turret: slew toward TargetCm within the limits at their rates (the short way across the seam
	 * when yaw is unlimited). Spin: turn at the engaged rate. Returns true when settled on the aim
	 * (never while a search wobble runs). A barricade returns true.
	 */
	bool AimAt(const FVector& TargetCm, float DeltaSeconds);
	/** No target: a turret holds its last aim; a spinner turns at the idle rate. */
	void Idle(float DeltaSeconds);
	/** Night (rig.md): search this long, a small sweep either side of the aim, before locking on. */
	void StartSearchWobble(float Seconds = DFTowerRig::SearchWobbleSeconds);

	/** Attach the stage modules for these purchases (cumulative: path level N shows S02..SN). */
	void SetPathLevels(const FDFTowerRow& Row, TConstArrayView<int32> PathLevels);

	EDFTowerRigStyle GetStyle() const { return Style; }
	const FDFTowerRigLimits& GetLimits() const { return Limits; }
	float GetYawDeg() const { return YawDeg; }
	float GetPitchDeg() const { return PitchDeg; }
	float GetSpinDeg() const { return SpinDeg; }
	bool IsSearching() const { return WobbleRemaining > 0.f; }
	/** The point turret angles are measured from: the Yaw part, or 1.5 m above the tower (the sim's muzzle). */
	FVector GetPivotLocation() const;
	/** Where rounds and beams start: the Muzzle, or the pivot when there are no meshes. */
	FVector GetMuzzleLocation() const;
	/** Stage modules attached for PathId (tests, and the upgrade panel's preview). */
	int32 GetAttachedStageCount(FName PathId) const;

private:
	void DestroyParts();
	UStaticMeshComponent* MakePart(FName Name, const TSoftObjectPtr<UStaticMesh>& Mesh, USceneComponent* Parent, FName Socket);
	USceneComponent* PartByName(FName Part) const;
	void ApplyAngles();

	EDFTowerRigStyle Style = EDFTowerRigStyle::Turret;
	FDFTowerRigLimits Limits;
	float YawDeg = 0.f;
	float PitchDeg = 0.f;
	float SpinDeg = 0.f;
	float WobbleRemaining = 0.f;
	float WobbleTotal = 0.f;

	/** The asset the parts came from (the content subsystem keeps it loaded). */
	TWeakObjectPtr<const UDFTowerDefinition> Definition;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Foot;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Yaw;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Pitch;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Spin;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Muzzle;
	/** Attached stage modules per path id. */
	TMap<FName, TArray<TObjectPtr<UStaticMeshComponent>>> StageParts;
};
