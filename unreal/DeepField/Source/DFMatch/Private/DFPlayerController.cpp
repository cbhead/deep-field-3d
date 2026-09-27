#include "DFPlayerController.h"

#include "DFMatchState.h"
#include "DFPlayerState.h"
#include "DFWorldCollision.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "DFGameplayTags.h"
#include "Content/DFContentRows.h"
#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"
#include "World/DFSocket.h"
#include "Messages/DFMessageBus.h"
#include "Towers/DFBuildSubsystem.h"
#include "Towers/DFTower.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DFPlayerController)

DEFINE_LOG_CATEGORY_STATIC(LogDFPlayerController, Log, All);

ADFPlayerController::ADFPlayerController()
{
	// C16 assets (unreal/Build/make-default-input.py writes them); the mapping context the hero adds
	// on possession maps E and X to them.
	static ConstructorHelpers::FObjectFinder<UInputAction> Build(TEXT("/Game/DF/Core/Input/IA_Build.IA_Build"));
	static ConstructorHelpers::FObjectFinder<UInputAction> Sell(TEXT("/Game/DF/Core/Input/IA_Sell.IA_Sell"));
	static ConstructorHelpers::FObjectFinder<UInputAction> Upgrade(TEXT("/Game/DF/Core/Input/IA_Upgrade.IA_Upgrade"));
	BuildAction = Build.Object;
	SellAction = Sell.Object;
	UpgradeAction = Upgrade.Object;
}

void ADFPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Input)
	{
		return;
	}
	if (BuildAction)
	{
		Input->BindAction(BuildAction, ETriggerEvent::Triggered, this, &ADFPlayerController::HandleBuildInput);
	}
	if (SellAction)
	{
		Input->BindAction(SellAction, ETriggerEvent::Triggered, this, &ADFPlayerController::HandleSellInput);
	}
	if (UpgradeAction)
	{
		Input->BindAction(UpgradeAction, ETriggerEvent::Triggered, this, &ADFPlayerController::HandleUpgradeInput);
	}
}

ADFSocket* ADFPlayerController::FindAimedSocket() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector End = ViewLocation + ViewRotation.Vector() * QuickBuildRangeCm;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DFQuickBuildAim), /*bTraceComplex*/ false, GetPawn());

	// A pad blocks DF_Build: the crosshair on a pad means that pad.
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, ViewLocation, End, DFCollision::Build, Params))
	{
		if (ADFSocket* Socket = Cast<ADFSocket>(Hit.GetActor()))
		{
			return Socket;
		}
	}
	// Otherwise where the crosshair lands on the world, and the nearest pad to it.
	if (!Hit.bBlockingHit && !World->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_Visibility, Params))
	{
		return nullptr;
	}
	ADFSocket* Nearest = nullptr;
	double NearestDistSq = FMath::Square(static_cast<double>(QuickBuildReachCm));
	for (TActorIterator<ADFSocket> It(World); It; ++It)
	{
		const double DistSq = FVector::DistSquared2D(It->GetPadTop(), Hit.ImpactPoint);
		if (DistSq <= NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = *It;
		}
	}
	return Nearest;
}

ADFTower* ADFPlayerController::FindTowerOn(FName SocketId) const
{
	if (SocketId.IsNone() || !GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<ADFTower> It(GetWorld()); It; ++It)
	{
		if (It->GetSocketId() == SocketId && !It->IsActorBeingDestroyed())
		{
			return *It;
		}
	}
	return nullptr;
}

void ADFPlayerController::HandleBuildInput()
{
	const ADFSocket* Socket = FindAimedSocket();
	if (!Socket)
	{
		return;
	}
	UE_LOG(LogDFPlayerController, Log, TEXT("build %s on %s"), *QuickBuildTowerId.ToString(), *Socket->SocketId.ToString());
	Server_PlaceTower(QuickBuildTowerId, Socket->SocketId);
}

void ADFPlayerController::HandleUpgradeInput()
{
	const ADFSocket* Socket = FindAimedSocket();
	const ADFTower* Tower = Socket ? FindTowerOn(Socket->SocketId) : nullptr;
	const FDFTowerRow* Row = Tower ? Tower->GetRow() : nullptr;
	if (!Row || Row->UpgradePaths.Num() == 0)
	{
		return;
	}
	// The least-bought path (the first on a tie), so repeated holds raise every path in turn. A maxed or
	// unaffordable path comes back as DF.Message.UpgradeRejected, which the HUD shows.
	const TArray<int32>& Levels = Tower->GetPathLevels();
	int32 Path = 0;
	for (int32 i = 1; i < Row->UpgradePaths.Num(); ++i)
	{
		const int32 Level = Levels.IsValidIndex(i) ? Levels[i] : 0;
		const int32 Best = Levels.IsValidIndex(Path) ? Levels[Path] : 0;
		if (Level < Best)
		{
			Path = i;
		}
	}
	UE_LOG(LogDFPlayerController, Log, TEXT("upgrade %s (%d) path %s on %s"), *Tower->GetDefId().ToString(), Tower->GetStructureId(),
		*Row->UpgradePaths[Path].Id.ToString(), *Socket->SocketId.ToString());
	Server_UpgradeTower(Tower->GetStructureId(), Path);
}

void ADFPlayerController::HandleSellInput()
{
	const ADFSocket* Socket = FindAimedSocket();
	const ADFTower* Tower = Socket ? FindTowerOn(Socket->SocketId) : nullptr;
	if (!Tower)
	{
		return;
	}
	UE_LOG(LogDFPlayerController, Log, TEXT("sell %s (%d) on %s"), *Tower->GetDefId().ToString(), Tower->GetStructureId(), *Socket->SocketId.ToString());
	Server_SellTower(Tower->GetStructureId());
}

int32 ADFPlayerController::GetSeat() const
{
	const ADFPlayerState* Seated = GetPlayerState<ADFPlayerState>();
	return Seated ? Seated->GetSeat() : 0;
}

void ADFPlayerController::Server_Launch_Implementation()
{
	// As Step.cs ApplyLaunch: a launch from anyone but the launch seat, or outside the lobby, is
	// silently ignored — the lobby screen only offers the button to the seat that may press it.
	if (ADFMatchState* Match = GetWorld() ? GetWorld()->GetGameState<ADFMatchState>() : nullptr)
	{
		Match->ServerLaunch(GetSeat());
	}
}

void ADFPlayerController::Server_CallEarly_Implementation()
{
	if (ADFMatchState* Match = GetWorld() ? GetWorld()->GetGameState<ADFMatchState>() : nullptr)
	{
		Match->ServerCallEarly(GetSeat());
	}
}

void ADFPlayerController::Server_PlaceTower_Implementation(FName TowerId, FName SocketId)
{
	UDFBuildSubsystem* Build = UDFBuildSubsystem::Get(this);
	if (!Build)
	{
		return;
	}
	// The Forge discount waits on the builder's faction (WS-07's faction component on the player state).
	const FDFBuildResult Result = Build->PlaceTower(GetSeat(), TowerId, SocketId, /*bBuilderIsForge*/ false);
	if (!Result.Succeeded())
	{
		FDFMsg_Rejected Refusal;
		Refusal.PlayerId = GetSeat();
		Refusal.Reason = Result.Reason;
		Refusal.Subject = SocketId;
		Refusal.Detail = TowerId.ToString();
		Client_Refused(DFTags::Message_BuildRejected, Refusal);
	}
}

void ADFPlayerController::Server_UpgradeTower_Implementation(int32 StructureId, int32 PathIndex)
{
	UDFBuildSubsystem* Build = UDFBuildSubsystem::Get(this);
	if (!Build)
	{
		return;
	}
	const FDFBuildResult Result = Build->UpgradeTower(GetSeat(), StructureId, PathIndex);
	if (!Result.Succeeded())
	{
		FDFMsg_Rejected Refusal;
		Refusal.PlayerId = GetSeat();
		Refusal.Reason = Result.Reason;
		Refusal.Subject = Result.Tower ? Result.Tower->GetDefId() : NAME_None;
		Refusal.Detail = FString::FromInt(StructureId);
		Client_Refused(DFTags::Message_UpgradeRejected, Refusal);
	}
}

void ADFPlayerController::Server_SellTower_Implementation(int32 StructureId)
{
	if (UDFBuildSubsystem* Build = UDFBuildSubsystem::Get(this))
	{
		Build->SellTower(GetSeat(), StructureId);   // an unknown tower is ignored without a message (Step.cs)
	}
}

void ADFPlayerController::Client_Refused_Implementation(FGameplayTag Tag, const FDFMsg_Rejected& Payload)
{
	if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
	{
		Bus->Broadcast(Tag, Payload);
	}
}
