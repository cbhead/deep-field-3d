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
#include "InputActionValue.h"
#include "UObject/ConstructorHelpers.h"
#include "World/DFSocket.h"
#include "Messages/DFMessageBus.h"
#include "Towers/DFBuildSubsystem.h"
#include "Content/DFContentSubsystem.h"
#include "Towers/DFTower.h"
#include "Towers/DFTowerMath.h"
#include "Towers/DFTrap.h"

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
	static ConstructorHelpers::FObjectFinder<UInputAction> Wheel(TEXT("/Game/DF/Core/Input/IA_Wheel.IA_Wheel"));
	WheelAction = Wheel.Object;
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
	if (WheelAction)
	{
		// Started: one step per notch or press, however long the axis stays non-zero.
		Input->BindAction(WheelAction, ETriggerEvent::Started, this, &ADFPlayerController::HandleWheelInput);
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
	const FName TowerId = GetQuickBuildTowerId();
	if (TowerId.IsNone())
	{
		return;
	}
	UE_LOG(LogDFPlayerController, Log, TEXT("build %s on %s"), *TowerId.ToString(), *Socket->SocketId.ToString());
	Server_PlaceTower(TowerId, Socket->SocketId);
}

void ADFPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);   // PlayerTick runs for local controllers only
	UpdatePadHighlight();
}

void ADFPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ADFSocket* Old = HighlightedSocket.Get())
	{
		ClearPadHighlight(*Old);
	}
	HighlightedSocket.Reset();
	Super::EndPlay(EndPlayReason);
}

void ADFPlayerController::ClearPadHighlight(ADFSocket& Socket)
{
	Socket.SetAimHighlight(EDFPadHighlight::None);
	Socket.SetRangePreview(0.f, EDFPadHighlight::None);
}

void ADFPlayerController::UpdatePadHighlight()
{
	// Only while the player looks through their hero: a spectator or demo camera is not aiming at a pad.
	ADFSocket* Aimed = (GetPawn() && GetViewTarget() == GetPawn()) ? FindAimedSocket() : nullptr;
	ADFSocket* Old = HighlightedSocket.Get();
	if (Old && Old != Aimed)
	{
		ClearPadHighlight(*Old);
	}
	HighlightedSocket = Aimed;
	if (Aimed)
	{
		const UDFContentSubsystem* Content = UDFContentSubsystem::Get(this);
		const FName ChoiceId = GetQuickBuildTowerId();
		const FDFTowerRow* Choice = Content ? Content->Tower(ChoiceId) : nullptr;
		const ADFTower* Tower = FindTowerOn(Aimed->SocketId);
		const EDFPadHighlight Highlight = DecidePadHighlight(Aimed->Tag, Choice, Tower != nullptr, FindTrapOn(Aimed->SocketId) != nullptr);
		Aimed->SetAimHighlight(Highlight);
		// Both setters ignore a frame that changes nothing, so asking every frame costs a table lookup.
		const FDFRangePreview Reach = DecideRangePreview(Highlight, Choice, ChoiceId,
			Highlight == EDFPadHighlight::Free ? GetBuildCondition() : nullptr, Tower);
		Aimed->SetRangePreview(Reach.RangeMeters * 100.f, Highlight, Reach.MinRangeMeters * 100.f);
	}
}

FDFRangePreview ADFPlayerController::DecideRangePreview(EDFPadHighlight Highlight, const FDFTowerRow* Choice, FName ChoiceId,
	const FDFConditionRow* Condition, const ADFTower* Tower)
{
	FDFRangePreview Reach;
	if (Highlight == EDFPadHighlight::Free && Choice)
	{
		Reach.RangeMeters = DFTowerMath::RangeMeters(*Choice, ChoiceId, /*PathLevels*/ {}, Condition);
		Reach.MinRangeMeters = Choice->MinRangeMeters;
	}
	else if (Highlight == EDFPadHighlight::Occupied && Tower)
	{
		const FDFTowerRow* Row = Tower->GetRow();
		Reach.RangeMeters = Tower->GetRangeMeters();
		Reach.MinRangeMeters = Row ? Row->MinRangeMeters : 0.f;
	}
	return Reach;
}

const FDFConditionRow* ADFPlayerController::GetBuildCondition() const
{
	UWorld* World = GetWorld();
	const UDFContentSubsystem* Content = UDFContentSubsystem::Get(this);
	if (!World || !Content)
	{
		return nullptr;
	}
	if (GetNetMode() != NM_Client)
	{
		const UDFBuildSubsystem* Build = UDFBuildSubsystem::Get(this);
		const FName ConditionId = Build ? Build->GetWaveCondition() : NAME_None;
		return ConditionId.IsNone() ? nullptr : Content->Condition(ConditionId);
	}
	for (TActorIterator<ADFTower> It(World); It; ++It)
	{
		if (!It->IsActorBeingDestroyed() && !It->GetDefId().IsNone())
		{
			return It->GetActiveCondition();
		}
	}
	return nullptr;
}

EDFPadHighlight ADFPlayerController::DecidePadHighlight(EDFSocketTag PadTag, const FDFTowerRow* Choice, bool bTowerOn, bool bTrapOn)
{
	if (bTowerOn)
	{
		return EDFPadHighlight::Occupied;
	}
	if (bTrapOn || !Choice)
	{
		return EDFPadHighlight::Blocked;
	}
	// The host's rule, asked with everything but the tag out of the way: only a tag refusal comes back.
	DFTowerMath::FDFPlacementFacts Facts;
	Facts.SocketTag = PadTag;
	Facts.Money = TNumericLimits<int32>::Max();
	return DFTowerMath::CheckPlacement(*Choice, Facts, /*Cost*/ 0).IsNone() ? EDFPadHighlight::Free : EDFPadHighlight::Blocked;
}

ADFTrap* ADFPlayerController::FindTrapOn(FName SocketId) const
{
	if (SocketId.IsNone() || !GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<ADFTrap> It(GetWorld()); It; ++It)
	{
		if (It->GetSocketId() == SocketId && !It->IsActorBeingDestroyed())
		{
			return *It;
		}
	}
	return nullptr;
}

FName ADFPlayerController::GetQuickBuildTowerId() const
{
	return QuickBuildChoices.IsValidIndex(QuickBuildIndex) ? QuickBuildChoices[QuickBuildIndex] : NAME_None;
}

void ADFPlayerController::CycleQuickBuild(int32 Steps)
{
	const int32 Count = QuickBuildChoices.Num();
	if (Count == 0 || Steps == 0)
	{
		return;
	}
	QuickBuildIndex = ((QuickBuildIndex + Steps) % Count + Count) % Count;
	UE_LOG(LogDFPlayerController, Log, TEXT("hold E builds %s"), *GetQuickBuildTowerId().ToString());
}

void ADFPlayerController::HandleWheelInput(const FInputActionValue& Value)
{
	// Wheel up / D-pad right is the next tower (the list reads left to right).
	const float Axis = Value.Get<float>();
	CycleQuickBuild(Axis > 0.f ? 1 : Axis < 0.f ? -1 : 0);
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
