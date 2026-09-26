#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DFContentDefinition.generated.h"

// The binding half of content (ADR-0005): numbers live in DataTables (DFContentRows.h), binary
// bindings (meshes, animation, materials, Niagara, sounds, icons, rig limits) live in a
// UDFContentDefinition subclass per domain — DA_Tower_<id>, DA_Enemy_<id>, DA_Weapon_<id>, ...
// The Asset Manager scans them by PrimaryAssetType (DefaultGame.ini); gameplay resolves content
// only through UDFContentSubsystem, never by asset path (C§7.5).
UCLASS(Abstract, BlueprintType)
class DFCORE_API UDFContentDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The row name in the domain's DataTable and the JSON id (lower camelCase). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content")
	FName ContentId;

	/** Tower / Enemy / Weapon / Melee / Faction / Vehicle / Trap / Map / Condition — matches DefaultGame.ini. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content")
	FPrimaryAssetType PrimaryType;

	/** Resolved at load from PrimaryType + ContentId (e.g. DF.Tower.Lance). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Content")
	FGameplayTag ContentTag;

	/** Set on a stand-in asset; the registry audit fails a cook that contains one (C§7.5). */
	UPROPERTY(EditDefaultsOnly, AssetRegistrySearchable, Category = "Content")
	bool bPlaceholder = false;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(PrimaryType, ContentId);
	}

	virtual void PostLoad() override;
};
