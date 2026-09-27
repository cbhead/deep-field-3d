#pragma once

#include "CoreMinimal.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;

// Placeholder looks from the engine's basic shapes (contract-append, 2026-09-27): every body, tower,
// portal and core drawn before its art exists is a /Engine/BasicShapes mesh with a colour. In UE 5.8
// those meshes carry /Engine/EngineMaterials/DefaultMaterial, which has no parameters, so a dynamic
// instance of the mesh's own material takes no colour and everything renders grey. The tintable
// material is BasicShapeMaterial (vector "Color", scalar "Roughness"); Tint puts it on the slot first.
// The project cooks /Engine/BasicShapes (DefaultGame.ini DirectoriesToAlwaysCook), so a packaged
// build finds it too.
namespace DFShapeLook
{
	/** The engine material whose "Color" parameter tints a placeholder. Loaded once. */
	DFCORE_API UMaterialInterface* Material();

	/** Make Slot a dynamic instance of Material() (unless it already is one) and set its Color.
	 *  Returns the instance, or null without a mesh or the material. Every machine that draws. */
	DFCORE_API UMaterialInstanceDynamic* Tint(UMeshComponent* Mesh, const FLinearColor& Colour, int32 Slot = 0);
}
