#pragma once

#include "Content/DFContentDefinition.h"
#include "CoreMinimal.h"
#include "Rig/DFTowerRig.h"
#include "DFTowerDefinition.generated.h"

class UStaticMesh;

/** One upgrade path's stage modules (rig.md): SM_Tower_<Id>_<Path>_S02..S10, mounted on MountPart. */
USTRUCT(BlueprintType)
struct DFTOWERS_API FDFTowerStageSet
{
	GENERATED_BODY()

	/** The part the modules attach to, at its S_Stage_<Path> socket: Foot, Yaw or Pitch (barrel-side
	 *  modules on Pitch swing with the barrel). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rig") FName MountPart = TEXT("Pitch");
	/** Modules[0] is S02 (S01 is the chassis). Cumulative: at path level N, S02..SN are attached. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rig") TArray<TSoftObjectPtr<UStaticMesh>> Modules;
};

/**
 * DA_Tower_<id> (C9, ADR-0007): a tower's binary bindings. The numbers are the DataTable row's
 * (FDFTowerRow); this carries the meshes, the rig limits and the stage modules. WS-30's importer
 * writes these from game/assets/structures/manifest.json; until one exists for a tower, the rig
 * falls back to DFTowerRig::ManifestLimitsFor and draws no meshes.
 */
UCLASS(BlueprintType)
class DFTOWERS_API UDFTowerDefinition : public UDFContentDefinition
{
	GENERATED_BODY()

public:
	UDFTowerDefinition();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig") TSoftObjectPtr<UStaticMesh> FootMesh;
	/** Turrets: the traversing part (on the Foot's S_Yaw) and the elevating part (on the Yaw's S_Pitch). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig") TSoftObjectPtr<UStaticMesh> YawMesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig") TSoftObjectPtr<UStaticMesh> PitchMesh;
	/** Aura / support: the rotating part on the Foot's S_Spin. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig") TSoftObjectPtr<UStaticMesh> SpinMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig") FDFTowerRigLimits Rig;

	/** Keyed by upgrade path id ("damage", "range", ...). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig") TMap<FName, FDFTowerStageSet> Stages;
};
