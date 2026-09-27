#include "World/DFLaneGraphInfo.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "LaneGraph/DFLaneGraphAsset.h"
#include "UObject/ConstructorHelpers.h"
#include "World/DFWorldLook.h"

namespace DFLaneStripLook
{
	constexpr double WidthCm = 120.0;      // the spine of the 3.4 m corridor, not the whole of it
	constexpr double ThicknessCm = 2.0;
	constexpr double LiftCm = 1.0;         // clear of the surface the waypoints were projected onto
}

ADFLaneGraphInfo::ADFLaneGraphInfo()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube"));
	StripMesh = Cube.Object;
}

void ADFLaneGraphInfo::BeginPlay()
{
	Super::BeginPlay();
	RebuildLaneStrip();
}

void ADFLaneGraphInfo::RebuildLaneStrip()
{
	using namespace DFLaneStripLook;
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || IsNetMode(NM_DedicatedServer) || !StripMesh)
	{
		return;
	}
	if (!LaneStrip)
	{
		LaneStrip = NewObject<UInstancedStaticMeshComponent>(this, TEXT("LaneStrip"), RF_Transient);
		// Movable: an editor build's root is AInfo's sprite, which is, and a static child of a
		// movable parent is refused.
		LaneStrip->SetMobility(EComponentMobility::Movable);
		LaneStrip->SetStaticMesh(StripMesh);
		LaneStrip->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		LaneStrip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		LaneStrip->SetCollisionResponseToAllChannels(ECR_Ignore);   // the profile leaves the project's trace channels at Block
		LaneStrip->SetGenerateOverlapEvents(false);
		LaneStrip->SetCanEverAffectNavigation(false);
		LaneStrip->SetCastShadow(false);
		if (RootComponent)
		{
			LaneStrip->SetupAttachment(RootComponent);
		}
		else
		{
			SetRootComponent(LaneStrip);   // a cooked AInfo has no root (its sprite is editor-only)
		}
		LaneStrip->RegisterComponent();
		AddInstanceComponent(LaneStrip);
		DFWorldLook::Tint(LaneStrip, DFWorldLook::LaneStrip());
		// AInfo is hidden, and a hidden actor draws none of its components: the strip is the one
		// thing of this actor that is meant to be seen (the sprite stays hidden in game on its own).
		SetActorHiddenInGame(false);
	}

	LaneStrip->ClearInstances();
	if (!LaneGraph)
	{
		return;
	}
	TArray<FTransform> Legs;
	for (const FDFLaneEdge& Edge : LaneGraph->Edges)
	{
		// Ground walk edges only: an air lane is not on the ground, and a warp is not walked.
		if (Edge.IsWarp() || Edge.Layer != EDFEnemyLayer::Ground)
		{
			continue;
		}
		for (int32 i = 0; i + 1 < Edge.Waypoints.Num(); ++i)
		{
			const FVector Leg = Edge.Waypoints[i + 1] - Edge.Waypoints[i];
			const double Length = Leg.Size();
			if (Length < 1.0)
			{
				continue;
			}
			// Each leg runs half a width past both its ends, so a corner is filled, not notched (the
			// overlap is one colour at one height: nothing to see). Pitched with the leg on a slope.
			const FVector Centre = (Edge.Waypoints[i] + Edge.Waypoints[i + 1]) * 0.5 + FVector(0.0, 0.0, LiftCm + ThicknessCm * 0.5);
			const FVector Scale((Length + WidthCm) / 100.0, WidthCm / 100.0, ThicknessCm / 100.0);
			Legs.Emplace(Leg.Rotation(), Centre, Scale);
		}
	}
	LaneStrip->AddInstances(Legs, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ true, /*bUpdateNavigation*/ false);
}
