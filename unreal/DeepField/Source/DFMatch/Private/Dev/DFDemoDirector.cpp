#include "Dev/DFDemoDirector.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Combat/DFTargetable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"
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
#if !UE_BUILD_SHIPPING
	bCaptureFrames = FParse::Param(FCommandLine::Get(), TEXT("DFDemoCapture"));
#endif
	UE_LOG(LogDFDemo, Display, TEXT("demo: building from the economy and filming from an orbit (-DFDemo)%s"),
		bCaptureFrames ? TEXT(", a screenshot with the HUD every frame (-DFDemoCapture)") : TEXT(""));
}

bool ADFDemoDirector::Survey()
{
	UWorld* World = GetWorld();
	TArray<FVector> Cores;
	FBox Bounds(ForceInit);
	for (TActorIterator<ADFSpawnPortal> It(World); It; ++It)
	{
		Bounds += It->GetActorLocation();
	}
	for (TActorIterator<ADFCore> It(World); It; ++It)
	{
		Cores.Add(It->GetActorLocation());
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
	// Defend from the core outward: a first tower by the portal kills every body as it appears and
	// nothing else happens on screen; from the core end, the wave walks the lane under fire.
	auto NearestCore = [&Cores](const FVector& At)
	{
		double Best = TNumericLimits<double>::Max();
		for (const FVector& Core : Cores)
		{
			Best = FMath::Min(Best, FVector::DistSquared2D(Core, At));
		}
		return Best;
	};
	Sockets.Sort([&NearestCore](const ADFSocket& A, const ADFSocket& B) { return NearestCore(A.GetActorLocation()) < NearestCore(B.GetActorLocation()); });
	for (const ADFSocket* Socket : Sockets)
	{
		BuildOrder.Add(Socket->SocketId);
	}
	MapCentre = Bounds.GetCenter();
	Focus = MapCentre;
	const FVector Extent = Bounds.GetExtent();
	// Close enough that a 0.8 m body reads, far enough that the lane's ends stay in frame.
	OrbitRadiusCm = FMath::Clamp(static_cast<float>(FMath::Max(Extent.X, Extent.Y)) * 0.9f, 1800.f, 3600.f);
	OrbitHeightCm = OrbitRadiusCm * 0.6f;
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
	// Lean toward the fight: the centroid of every live body, smoothed so a death does not jerk the shot.
	FVector Desired = MapCentre;
	if (UDFTargetRegistry* Registry = UDFTargetRegistry::Get(this))
	{
		FVector Sum = FVector::ZeroVector;
		int32 Count = 0;
		for (AActor* Body : Registry->GetTargets())
		{
			const IDFTargetable* Target = Cast<IDFTargetable>(Body);
			if (Target && !Target->IsTargetDead())
			{
				Sum += Target->GetTargetPosition();
				++Count;
			}
		}
		if (Count > 0)
		{
			Desired = FMath::Lerp(MapCentre, Sum / Count, FollowWeight);
		}
	}
	Focus = FMath::VInterpTo(Focus, Desired, DeltaSeconds, FollowSpeed);
	PlaceCamera();
	if (bCaptureFrames)
	{
		// -dumpmovie leaves the UI out; this is `shot showui` once per frame (Saved/Screenshots/ScreenShotNNNNN.png).
		FScreenshotRequest::RequestScreenshot(FString(), /*bShowUI*/ true, /*bAddFilenameSuffix*/ true);
	}
	BuildClock += DeltaSeconds;
	if (BuildClock >= BuildIntervalSeconds)
	{
		BuildClock = 0.f;
		TryBuild();
	}
}
