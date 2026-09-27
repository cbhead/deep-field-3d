#include "Enemies/DFEnemy.h"

#include "Components/StaticMeshComponent.h"
#include "Content/DFContentRows.h"
#include "Content/DFContentSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Movement/DFEnemyMovement.h"
#include "Movement/DFLaneWalker.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Waves/DFWaveDirector.h"
#include "LaneGraph/DFLaneGraphAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFEnemy, Log, All);

namespace DFEnemyLook
{
	// The placeholder body: the engine's 1 m cylinder (pivot at its centre), scaled to the sim's enemy.
	constexpr float WidthCm = 80.f;
	constexpr float HeightCm = 160.f;
}

ADFEnemy::ADFEnemy()
{
	bReplicates = true;
	SetReplicatingMovement(true);
	PrimaryActorTick.bCanEverTick = false;   // UDFEnemyMovement ticks; the actor has nothing of its own

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder"));
	if (Cylinder.Succeeded())
	{
		Body->SetStaticMesh(Cylinder.Object);
	}
	// The lane puts the actor's origin on the ground; the cylinder's pivot is its centre.
	Body->SetRelativeLocation(FVector(0.f, 0.f, DFEnemyLook::HeightCm * 0.5f));
	Body->SetRelativeScale3D(FVector(DFEnemyLook::WidthCm / 100.f, DFEnemyLook::WidthCm / 100.f, DFEnemyLook::HeightCm / 100.f));
	// Walking bodies do not shove the hero or each other around; targeting and shots come with WS-04.
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	Movement = CreateDefaultSubobject<UDFEnemyMovement>(TEXT("Movement"));
}

void ADFEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADFEnemy, Entry, COND_InitialOnly);
}

ADFEnemy* ADFEnemy::SpawnFromEntry(UWorld* World, const FDFSpawnEntry& InEntry, const UDFLaneGraphAsset* Graph,
	TSharedPtr<const FDFLaneRouting> Routing, ADFWaveDirector* InDirector, FString& OutError)
{
	// A body that never makes it onto the lane still has to leave the wave's count, or the wave waits
	// for it forever.
	auto Refuse = [&OutError, InDirector](const FString& Why) -> ADFEnemy*
	{
		OutError = Why;
		if (InDirector)
		{
			InDirector->NotifyEnemyRemoved();
		}
		return nullptr;
	};

	if (!World || !Graph)
	{
		return Refuse(TEXT("no world or no lane graph"));
	}
	const UDFContentSubsystem* Content = UDFContentSubsystem::Get(World);
	const FDFEnemyRow* Row = Content ? Content->Enemy(InEntry.DefId) : nullptr;
	if (!Row)
	{
		return Refuse(FString::Printf(TEXT("the content has no enemy '%s'"), *InEntry.DefId.ToString()));
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	ADFEnemy* Enemy = World->SpawnActor<ADFEnemy>(ADFEnemy::StaticClass(), FTransform::Identity, Params);
	if (!Enemy)
	{
		return Refuse(TEXT("SpawnActor failed"));
	}
	Enemy->Entry = InEntry;
	Enemy->Director = InDirector;
	Enemy->FinishSpawning(FTransform::Identity);

	// Listen before Configure: it relays the walker's first events.
	Enemy->Movement->OnWalkEvent.AddUObject(Enemy, &ADFEnemy::HandleWalkEvent);
	FString Why;
	if (!Enemy->Movement->Configure(Graph, InEntry.RouteId, InEntry.LateralOffset * 100.f, *Row, MoveTemp(Routing), Why))
	{
		OutError = Why;
		Enemy->Destroy();   // EndPlay tells the director
		return nullptr;
	}
	const FDFLaneWalkerState& Walker = Enemy->Movement->GetWalkerState();
	Enemy->SetActorLocationAndRotation(FDFLaneWalker::LocationOf(*Graph, Walker), FDFLaneWalker::FacingOf(*Graph, Walker).Rotation());
	Enemy->ApplyPlaceholderLook();
	return Enemy;
}

void ADFEnemy::HandleWalkEvent(const FDFWalkEvent& Event)
{
	if (Event.Kind != EDFWalkEventKind::ReachedCore)
	{
		return;
	}
	UE_LOG(LogDFEnemy, Log, TEXT("%s (%s) reached the core"), *GetName(), *Entry.DefId.ToString());
	OnLeaked.Broadcast(this);
	Destroy();
}

void ADFEnemy::EndPlay(const EEndPlayReason::Type Reason)
{
	// Once, on the host, and only when the body leaves a running game: a level being torn down is not
	// the wave losing an enemy.
	if (Reason == EEndPlayReason::Destroyed && HasAuthority() && !bReportedGone)
	{
		bReportedGone = true;
		if (ADFWaveDirector* D = Director.Get())
		{
			D->NotifyEnemyRemoved();
		}
	}
	Super::EndPlay(Reason);
}

void ADFEnemy::ApplyPlaceholderLook()
{
	if (!Body || Entry.DefId.IsNone())
	{
		return;
	}
	// One stable colour per enemy id, so a mixed wave reads at a glance.
	const uint32 Hash = GetTypeHash(Entry.DefId.ToString());
	const FLinearColor Colour = FLinearColor::MakeFromHSV8(static_cast<uint8>(Hash & 0xFF), 200, 230);
	UMaterialInstanceDynamic* Mid = Body->CreateDynamicMaterialInstance(0);
	if (Mid)
	{
		Mid->SetVectorParameterValue(TEXT("Color"), Colour);
	}
}
