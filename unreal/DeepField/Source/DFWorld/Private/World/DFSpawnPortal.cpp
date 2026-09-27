#include "World/DFSpawnPortal.h"

#include "Components/StaticMeshComponent.h"
#include "DFWorldSubsystem.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "LaneGraph/DFLaneGraphAsset.h"
#include "World/DFLaneGraphInfo.h"
#include "World/DFWorldLook.h"

namespace DFPortalLook
{
	// The opening fits the widest scatter in the content (enemies.json scatterWidth 2.8 m: up to
	// 1.4 m either side of the spine) plus a body (ADFEnemy's placeholder is 0.8 m across), with room
	// to spare: nothing is seen to walk out through a post.
	constexpr float OpeningWidthCm = 420.f;
	constexpr float PostWidthCm = 40.f;
	constexpr float PostHeightCm = 360.f;
	constexpr float LintelHeightCm = 40.f;
	constexpr float LintelOverhangCm = 20.f;   // past each post
	constexpr float HeightCm = PostHeightCm + LintelHeightCm;                                   // 4 m
	constexpr float PostOffsetCm = (OpeningWidthCm + PostWidthCm) * 0.5f;
	constexpr float LintelWidthCm = OpeningWidthCm + 2.f * (PostWidthCm + LintelOverhangCm);   // 5.4 m
}

ADFSpawnPortal::ADFSpawnPortal()
{
	using namespace DFPortalLook;
	using DFWorldLook::EShape;

	LookRoot = CreateDefaultSubobject<USceneComponent>(TEXT("LookRoot"));
	LookRoot->SetupAttachment(RootComponent);
	LookRoot->SetMobility(EComponentMobility::Movable);

	// Local +X is the way through the opening; the posts stand either side of it on Y.
	PostLeft = DFWorldLook::CreatePart(*this, LookRoot, TEXT("PostLeft"), EShape::Cube,
		FVector(0.f, -PostOffsetCm, PostHeightCm * 0.5f), FVector(PostWidthCm, PostWidthCm, PostHeightCm), EComponentMobility::Movable);
	PostRight = DFWorldLook::CreatePart(*this, LookRoot, TEXT("PostRight"), EShape::Cube,
		FVector(0.f, PostOffsetCm, PostHeightCm * 0.5f), FVector(PostWidthCm, PostWidthCm, PostHeightCm), EComponentMobility::Movable);
	Lintel = DFWorldLook::CreatePart(*this, LookRoot, TEXT("Lintel"), EShape::Cube,
		FVector(0.f, 0.f, PostHeightCm + LintelHeightCm * 0.5f), FVector(PostWidthCm, LintelWidthCm, LintelHeightCm), EComponentMobility::Movable);
}

void ADFSpawnPortal::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshLook();
}

void ADFSpawnPortal::BeginPlay()
{
	Super::BeginPlay();
	// A level loaded into a game is not constructed again, and the tint is transient (never saved):
	// lay the gate out for play here.
	RefreshLook();
}

void ADFSpawnPortal::RefreshLook()
{
	if (!LookRoot)
	{
		return;
	}
	// Flyers leave at the node's height (AGL), so the opening is centred on the actor; walkers leave
	// from the ground the actor stands on, so the gate stands on it.
	const float Lift = (Layer == EDFEnemyLayer::Air) ? -DFPortalLook::HeightCm * 0.5f : 0.f;
	LookRoot->SetRelativeLocation(FVector(0.f, 0.f, Lift));

	float Yaw = 0.f;
	const UDFLaneGraphAsset* Graph = FindLaneGraph();
	if (Graph && LaneYawOutOf(*Graph, Id, Yaw))
	{
		LookRoot->SetWorldRotation(FRotator(0.f, Yaw, 0.f));
	}
	else
	{
		LookRoot->SetRelativeRotation(FRotator::ZeroRotator);
	}

	for (UStaticMeshComponent* Part : { PostLeft.Get(), PostRight.Get(), Lintel.Get() })
	{
		DFWorldLook::Tint(Part, DFWorldLook::PortalFrame());
	}
}

bool ADFSpawnPortal::LaneYawOutOf(const UDFLaneGraphAsset& Graph, FName NodeId, float& OutYawDegrees)
{
	if (NodeId.IsNone())
	{
		return false;
	}
	// The importer places the portal with no rotation (the level file has none for a node), so the
	// lane is what says which way the wave leaves. The first walk edge out of the node, and its first
	// leg with any horizontal length (a lane may start by climbing).
	for (const FDFLaneEdge& Edge : Graph.Edges)
	{
		if (Edge.From != NodeId || Edge.IsWarp())
		{
			continue;
		}
		for (int32 i = 0; i + 1 < Edge.Waypoints.Num(); ++i)
		{
			const FVector Leg = Edge.Waypoints[i + 1] - Edge.Waypoints[i];
			const FVector Flat(Leg.X, Leg.Y, 0.0);
			if (!Flat.IsNearlyZero(1.0))
			{
				OutYawDegrees = static_cast<float>(Flat.Rotation().Yaw);
				return true;
			}
		}
	}
	return false;
}

const UDFLaneGraphAsset* ADFSpawnPortal::FindLaneGraph() const
{
	if (const ULevel* Level = GetLevel())
	{
		for (const AActor* Actor : Level->Actors)
		{
			const ADFLaneGraphInfo* Info = Cast<ADFLaneGraphInfo>(Actor);
			if (Info && Info->LaneGraph)
			{
				return Info->LaneGraph;
			}
		}
	}
	// Only a game world asks the subsystem: in the editor a miss would be cached until a level
	// changes, and the level's own info is the whole answer there anyway.
	const UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		if (UDFWorldSubsystem* Subsystem = UDFWorldSubsystem::Get(this))
		{
			return Subsystem->FindLaneGraph();
		}
	}
	return nullptr;
}
