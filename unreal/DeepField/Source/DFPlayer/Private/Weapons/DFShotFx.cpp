#include "Weapons/DFShotFx.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace DFShotLook
{
	// Seconds each piece takes to shrink away; B§1.2 wants fire to read at once and be gone as fast.
	constexpr float TracerSeconds = 0.07f;
	constexpr float MuzzleSeconds = 0.05f;
	constexpr float ImpactSeconds = 0.12f;
	constexpr float LifeSeconds = ImpactSeconds;

	// Sizes (cm). The engine's basic shapes are 100 cm across, pivot at the centre. The owner's muzzle is
	// on a half-size placeholder gun 40 cm from the eye (DFHeroCharacter.cpp), so the flash is small.
	constexpr float TracerThicknessCm = 1.2f;
	constexpr float MuzzleSizeCm = 3.f;
	constexpr float ImpactSizeCm = 20.f;
	constexpr float TargetImpactSizeCm = 28.f;

	// Emissive colours: above 1 they bloom, and kept warm so they do not wash out to white.
	const FLinearColor Tracer(2.f, 1.3f, 0.4f);
	const FLinearColor Muzzle(3.f, 1.8f, 0.5f);
	const FLinearColor ImpactWorld(2.2f, 1.2f, 0.4f);
	/** A hit on something that bleeds reads apart from a hit on the ground (toward M_Enemy's #C93B28). */
	const FLinearColor ImpactTarget(3.f, 0.6f, 0.3f);

	void MakeCosmetic(UStaticMeshComponent* Mesh)
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetCastShadow(false);
		Mesh->bReceivesDecals = false;
	}
}

ADFShotFx::ADFShotFx()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere"));
	// The basic shapes carry DefaultMaterial, which has no parameters. EmissiveMeshMaterial has a "Color"
	// and glows, so a shot reads in shade as well as in sun.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial"));

	// The cylinder's axis is its Z; pitched -90° it runs along the actor's X, which points at the end.
	Tracer = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tracer"));
	Tracer->SetupAttachment(Root);
	Tracer->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	MuzzleFlash = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MuzzleFlash"));
	MuzzleFlash->SetupAttachment(Root);
	Impact = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Impact"));
	Impact->SetupAttachment(Root);
	if (Cylinder.Succeeded())
	{
		Tracer->SetStaticMesh(Cylinder.Object);
	}
	if (Sphere.Succeeded())
	{
		MuzzleFlash->SetStaticMesh(Sphere.Object);
		Impact->SetStaticMesh(Sphere.Object);
	}
	for (UStaticMeshComponent* Mesh : { Tracer.Get(), MuzzleFlash.Get(), Impact.Get() })
	{
		DFShotLook::MakeCosmetic(Mesh);
		if (Glow.Succeeded())
		{
			Mesh->SetMaterial(0, Glow.Object);
		}
	}
	SetActorEnableCollision(false);
}

ADFShotFx* ADFShotFx::Spawn(UWorld* World, const FVector& Start, const FVector& End, bool bHit, bool bHitTarget)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;   // nobody to see it
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	const FRotator Facing = (End - Start).GetSafeNormal().Rotation();
	ADFShotFx* Fx = World->SpawnActor<ADFShotFx>(ADFShotFx::StaticClass(), Start, Facing, Params);
	if (Fx)
	{
		Fx->Configure(Start, End, bHit, bHitTarget);
	}
	return Fx;
}

void ADFShotFx::Tint(UStaticMeshComponent* Mesh, TObjectPtr<UMaterialInstanceDynamic>& Material, const FLinearColor& Colour)
{
	if (!Material)
	{
		Material = Mesh->CreateDynamicMaterialInstance(0);
	}
	if (Material)
	{
		Material->SetVectorParameterValue(TEXT("Color"), Colour);   // EmissiveMeshMaterial's colour
	}
}

void ADFShotFx::Configure(const FVector& Start, const FVector& End, bool bHit, bool bHitTarget)
{
	LengthCm = static_cast<float>(FVector::Dist(Start, End));
	bShowImpact = bHit;
	bImpactOnTarget = bHit && bHitTarget;

	Tint(Tracer, TracerMaterial, DFShotLook::Tracer);
	Tint(MuzzleFlash, MuzzleMaterial, DFShotLook::Muzzle);
	Tint(Impact, ImpactMaterial, bImpactOnTarget ? DFShotLook::ImpactTarget : DFShotLook::ImpactWorld);

	Impact->SetRelativeLocation(FVector(LengthCm, 0.f, 0.f));
	Impact->SetVisibility(bShowImpact);
	UpdateLook();   // this frame's sizes, before the first render
}

void ADFShotFx::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (Age >= DFShotLook::LifeSeconds)
	{
		Destroy();
		return;
	}
	UpdateLook();
}

void ADFShotFx::UpdateLook()
{
	// Each piece shrinks linearly over its own time, then hides (never to a zero scale: a degenerate
	// transform is not worth the trouble for a hidden mesh).
	auto Left = [this](float Seconds) { return FMath::Clamp(1.f - Age / Seconds, 0.01f, 1.f); };

	const float TracerLeft = Left(DFShotLook::TracerSeconds);
	const float Thickness = DFShotLook::TracerThicknessCm * TracerLeft / 100.f;
	Tracer->SetRelativeLocation(FVector(LengthCm * 0.5f, 0.f, 0.f));
	Tracer->SetRelativeScale3D(FVector(Thickness, Thickness, LengthCm / 100.f));
	Tracer->SetVisibility(Age < DFShotLook::TracerSeconds && LengthCm > 1.f);

	const float MuzzleLeft = Left(DFShotLook::MuzzleSeconds);
	MuzzleFlash->SetRelativeScale3D(FVector(DFShotLook::MuzzleSizeCm * MuzzleLeft / 100.f));
	MuzzleFlash->SetVisibility(Age < DFShotLook::MuzzleSeconds);

	if (bShowImpact)
	{
		const float Size = (bImpactOnTarget ? DFShotLook::TargetImpactSizeCm : DFShotLook::ImpactSizeCm) * Left(DFShotLook::ImpactSeconds);
		Impact->SetRelativeScale3D(FVector(Size / 100.f));
	}
}
