#include "World/DFWorldLook.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Look/DFShapeLook.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Tint/DFPaletteSettings.h"
#include "UObject/ConstructorHelpers.h"

namespace DFWorldLook
{
	namespace
	{
		// Constructor-time finders (the CDOs are built once, at module load): each shape is found once
		// and kept alive by the parts that reference it.
		UStaticMesh* ShapeMesh(EShape Shape)
		{
			switch (Shape)
			{
			case EShape::Cylinder:
			{
				static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cylinder"));
				return Mesh.Object;
			}
			case EShape::Sphere:
			{
				static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Sphere"));
				return Mesh.Object;
			}
			case EShape::Cube:
			default:
			{
				static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cube"));
				return Mesh.Object;
			}
			}
		}

		void SetUpPart(UPrimitiveComponent& Part, USceneComponent* Parent, EComponentMobility::Type Mobility)
		{
			Part.SetupAttachment(Parent);
			Part.SetMobility(Mobility);
			Part.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			Part.SetCollisionEnabled(ECollisionEnabled::NoCollision);
			// The NoCollision profile names only the engine channels; the project's trace channels would keep their
			// ini default (Block). Say it for every channel, so no query can ever be told this part is there.
			Part.SetCollisionResponseToAllChannels(ECR_Ignore);
			Part.SetGenerateOverlapEvents(false);
			Part.SetCanEverAffectNavigation(false);
		}
	}

	UStaticMeshComponent* CreatePart(AActor& Owner, USceneComponent* Parent, FName Name, EShape Shape,
		const FVector& CentreCm, const FVector& SizeCm, EComponentMobility::Type Mobility)
	{
		UStaticMeshComponent* Part = Owner.CreateDefaultSubobject<UStaticMeshComponent>(Name);
		SetUpPart(*Part, Parent, Mobility);
		if (UStaticMesh* Mesh = ShapeMesh(Shape))
		{
			Part->SetStaticMesh(Mesh);
		}
		Part->SetRelativeLocation(CentreCm);
		Part->SetRelativeScale3D(SizeCm / 100.f);
		return Part;
	}

	UProceduralMeshComponent* CreateLaidPart(AActor& Owner, USceneComponent* Parent, FName Name)
	{
		UProceduralMeshComponent* Part = Owner.CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		SetUpPart(*Part, Parent, EComponentMobility::Movable);
		return Part;
	}

	void Tint(UStaticMeshComponent* Part, const FLinearColor& Colour)
	{
		// DFShapeLook starts from BasicShapeMaterial: the shapes' own DefaultMaterial takes no colour.
		DFShapeLook::Tint(Part, Colour);
	}

	void Glow(UMeshComponent* Part, const FLinearColor& Colour)
	{
		DFShapeLook::Glow(Part, Colour);
	}

	FLinearColor PadGround()      { return UDFPaletteSettings::FromHex(TEXT("#B08A3E")); }
	FLinearColor PadTrap()        { return UDFPaletteSettings::FromHex(TEXT("#D8A13A")); }
	FLinearColor PadBarricade()   { return UDFPaletteSettings::FromHex(TEXT("#7A7F88")); }
	FLinearColor PadWall()        { return UDFPaletteSettings::FromHex(TEXT("#9AA6B7")); }
	FLinearColor PadAimFree()     { return UDFPaletteSettings::FromHex(TEXT("#7FE65A")) * 3.f; }
	FLinearColor PadAimOccupied() { return UDFPaletteSettings::FromHex(TEXT("#E3BC66")) * 3.f; }
	FLinearColor PadAimBlocked()  { return UDFPaletteSettings::FromHex(TEXT("#E9614C")) * 2.f; }

	FLinearColor PortalFrame() { return UDFPaletteSettings::FromHex(TEXT("#FF2E4A")); }
	FLinearColor CoreEnergy()  { return UDFPaletteSettings::FromHex(TEXT("#22D3EE")); }
	FLinearColor CoreCap()     { return UDFPaletteSettings::FromHex(TEXT("#A8F0F4")); }
	FLinearColor CoreBase()    { return UDFPaletteSettings::FromHex(TEXT("#242E43")); }
	FLinearColor LaneStrip()   { return UDFPaletteSettings::FromHex(TEXT("#5A6880")); }
}
