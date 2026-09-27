#include "Look/DFShapeLook.h"

#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/StrongObjectPtr.h"

namespace DFShapeLook
{
	UMaterialInterface* Material()
	{
		// Rooted for the process: one engine material, shared by every placeholder.
		static TStrongObjectPtr<UMaterialInterface> Cached;
		if (!Cached.IsValid())
		{
			Cached.Reset(LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));
		}
		return Cached.Get();
	}

	UMaterialInstanceDynamic* Tint(UMeshComponent* Mesh, const FLinearColor& Colour, int32 Slot)
	{
		UMaterialInterface* Base = Material();
		if (!Mesh || !Base)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(Slot));
		if (!Mid || Mid->Parent != Base)
		{
			Mid = UMaterialInstanceDynamic::Create(Base, Mesh);
			Mid->SetFlags(RF_Transient);   // never saved into a map or an asset
			Mesh->SetMaterial(Slot, Mid);
		}
		Mid->SetVectorParameterValue(TEXT("Color"), Colour);
		return Mid;
	}
}
