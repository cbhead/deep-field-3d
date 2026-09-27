#include "Dev/DFDemoDirector.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Towers/DFBuildSubsystem.h"
#include "Towers/DFTower.h"
#include "World/DFCore.h"
#include "World/DFSocket.h"
#include "World/DFSpawnPortal.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DFDemoDirector)

DEFINE_LOG_CATEGORY_STATIC(LogDFDemo, Log, All);

namespace DFDemo
{
	const FName Lance(TEXT("lance"));
	const FName Nova(TEXT("nova"));
}

ADFDemoDirector::ADFDemoDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;   // host-only theatre: clients (if any) keep their own view
}

bool ADFDemoDirector::IsRequested()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return FParse::Param(FCommandLine::Get(), TEXT("DFDemo"));
#endif
}

void ADFDemoDirector::BeginPlay()
{
	Super::BeginPlay();
	FActorSpawnParameters Params;
	Params.Owner = this;
	Camera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
	if (Camera)
	{
		Camera->GetCameraComponent()->SetFieldOfView(CameraFieldOfView);
		Camera->GetCameraComponent()->bConstrainAspectRatio = false;
	}
	UE_LOG(LogDFDemo, Display, TEXT("demo: building from the economy and filming from an orbit (-DFDemo)"));
}

bool ADFDemoDirector::Survey()
{
	UWorld* World = GetWorld();
	TArray<FVector> Portals;
	FBox Bounds(ForceInit);
	for (TActorIterator<ADFSpawnPortal> It(World); It; ++It)
	{
		Portals.Add(It->GetActorLocation());
		Bounds += It->GetActorLocation();
	}
	for (TActorIterator<ADFCore> It(World); It; ++It)
	{
		Bounds += It->GetActorLocation();
	}
	TArray<ADFSocket*> Sockets;
	for (TActorIterator<ADFSocket> It(World); It; ++It)
	{
		if (It->Tag == EDFSocketTag::Ground)
		{
			Sockets.Add(*It);
		}
		Bounds += It->GetActorLocation();
	}
	if (!Bounds.IsValid)
	{
		return false;   // the level's actors are not in yet
	}
	auto NearestPortal = [&Portals](const FVector& At)
	{
		double Best = TNumericLimits<double>::Max();
		for (const FVector& Portal : Portals)
		{
			Best = FMath::Min(Best, FVector::DistSquared2D(Portal, At));
		}
		return Best;
	};
	Sockets.Sort([&NearestPortal](const ADFSocket& A, const ADFSocket& B) { return NearestPortal(A.GetActorLocation()) < NearestPortal(B.GetActorLocation()); });
	for (const ADFSocket* Socket : Sockets)
	{
		BuildOrder.Add(Socket->SocketId);
	}
	Focus = Bounds.GetCenter();
	const FVector Extent = Bounds.GetExtent();
	OrbitRadiusCm = FMath::Max(2500.f, static_cast<float>(FMath::Max(Extent.X, Extent.Y)) * 1.35f);
	OrbitHeightCm = OrbitRadiusCm * 0.75f;
	UE_LOG(LogDFDemo, Display, TEXT("demo: %d pads, orbit %.0f m around %s"), BuildOrder.Num(), OrbitRadiusCm / 100.f, *Focus.ToCompactString());
	return true;
}

void ADFDemoDirector::TryBuild()
{
	UDFBuildSubsystem* Builder = UDFBuildSubsystem::Get(this);
	if (!Builder)
	{
		return;
	}
	for (const FName& SocketId : BuildOrder)
	{
		if (Builder->FindTowerOnSocket(SocketId))
		{
			continue;
		}
		const FName TowerId = (NovaEvery > 0 && (Builds + 1) % NovaEvery == 0) ? DFDemo::Nova : DFDemo::Lance;
		const FDFBuildResult Result = Builder->PlaceTower(/*PlayerId*/ 1, TowerId, SocketId);
		if (Result.Succeeded())
		{
			++Builds;
			UE_LOG(LogDFDemo, Display, TEXT("demo: built %s on %s"), *TowerId.ToString(), *SocketId.ToString());
		}
		return;   // one pad at a time: the next free one waits for this one's money
	}
}

void ADFDemoDirector::PlaceCamera()
{
	if (!Camera)
	{
		return;
	}
	const float Rad = FMath::DegreesToRadians(OrbitDeg);
	const FVector Eye = Focus + FVector(FMath::Cos(Rad) * OrbitRadiusCm, FMath::Sin(Rad) * OrbitRadiusCm, OrbitHeightCm);
	Camera->SetActorLocationAndRotation(Eye, (Focus - Eye).Rotation());

	// The hero's possession hands the view back to the pawn; keep taking it.
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->GetViewTarget() != Camera)
		{
			PC->bAutoManageActiveCameraTarget = false;
			PC->SetViewTarget(Camera);
		}
	}
}

void ADFDemoDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bSurveyed)
	{
		bSurveyed = Survey();
		if (!bSurveyed)
		{
			return;
		}
	}
	OrbitDeg = FMath::Fmod(OrbitDeg + OrbitDegreesPerSecond * DeltaSeconds, 360.f);
	PlaceCamera();
	BuildClock += DeltaSeconds;
	if (BuildClock >= BuildIntervalSeconds)
	{
		BuildClock = 0.f;
		TryBuild();
	}
}
