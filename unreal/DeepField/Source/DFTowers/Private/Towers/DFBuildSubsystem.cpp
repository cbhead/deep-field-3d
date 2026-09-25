#include "Towers/DFBuildSubsystem.h"

#include "Combat/DFStructure.h"
#include "Components/ActorComponent.h"
#include "Content/DFContentSubsystem.h"
#include "DFBalanceDial.h"
#include "DFGameplayTags.h"
#include "DFWorldSubsystem.h"
#include "Economy/DFEconomySeams.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "LaneGraph/DFLaneGraphAsset.h"
#include "Messages/DFMessageBus.h"
#include "Messages/DFMessages.h"
#include "Towers/DFTower.h"
#include "Towers/DFTowerMath.h"
#include "World/DFLaneGate.h"
#include "World/DFSocket.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DFBuildSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogDFBuild, Log, All);

namespace
{
	IDFTeamWallet* AsWallet(UObject* Object)
	{
		return (IsValid(Object) && Object->GetClass()->ImplementsInterface(UDFTeamWallet::StaticClass()))
			? Cast<IDFTeamWallet>(Object) : nullptr;
	}

	FDFBuildResult Refuse(FName Reason)
	{
		FDFBuildResult Result;
		Result.Reason = Reason;
		return Result;
	}
}

UDFBuildSubsystem* UDFBuildSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDFBuildSubsystem>() : nullptr;
}

void UDFBuildSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const UWorld* World = GetWorld();
	if (World && World->GetNetMode() != NM_Client)
	{
		RemoveBroken();
	}
}

TStatId UDFBuildSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDFBuildSubsystem, STATGROUP_Tickables);
}

int32 UDFBuildSubsystem::RemoveBroken()
{
	TArray<ADFTower*> Broken;
	for (ADFTower* Tower : GetTowers())
	{
		if (Tower->IsBroken())
		{
			Broken.Add(Tower);
		}
	}
	UDFMessageBus* Bus = UDFMessageBus::Get(this);
	for (ADFTower* Tower : Broken)
	{
		const FDFTowerRow* Row = Tower->GetRow();
		FDFMsg_Structure Message;
		Message.StructureId = Tower->GetStructureId();
		Message.PlayerId = Tower->GetOwnerSeat();
		Message.DefId = Tower->GetDefId();
		Message.SocketId = Tower->GetSocketId();
		Message.HpFraction = 0.f;
		if (Row && Row->Kind == EDFTowerKind::Barricade)
		{
			Message.State = TEXT("breached");   // Step.cs RefreshEdgeState("breached"): the lane it shut is open again
		}
		Forget(Tower);
		Tower->Destroy();
		if (Bus)
		{
			Bus->BroadcastTeam(DFTags::Message_TowerDestroyed, Message);
		}
	}
	return Broken.Num();
}

// ---- lookups ------------------------------------------------------------------------------------

ADFTower* UDFBuildSubsystem::FindTower(int32 StructureId) const
{
	for (const TWeakObjectPtr<ADFTower>& Weak : Towers)
	{
		ADFTower* Tower = Weak.Get();
		if (IsValid(Tower) && Tower->GetStructureId() == StructureId)
		{
			return Tower;
		}
	}
	return nullptr;
}

ADFTower* UDFBuildSubsystem::FindTowerOnSocket(FName SocketId) const
{
	for (const TWeakObjectPtr<ADFTower>& Weak : Towers)
	{
		ADFTower* Tower = Weak.Get();
		if (IsValid(Tower) && Tower->GetSocketId() == SocketId)
		{
			return Tower;
		}
	}
	return nullptr;
}

TArray<ADFTower*> UDFBuildSubsystem::GetTowers() const
{
	TArray<ADFTower*> Out;
	for (const TWeakObjectPtr<ADFTower>& Weak : Towers)
	{
		if (ADFTower* Tower = Weak.Get(); IsValid(Tower))
		{
			Out.Add(Tower);
		}
	}
	return Out;
}

IDFTeamWallet* UDFBuildSubsystem::FindWallet() const
{
	if (IDFTeamWallet* Override = AsWallet(WalletOverride.Get()))
	{
		return Override;
	}
	const UWorld* World = GetWorld();
	AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (IDFTeamWallet* OnState = AsWallet(GameState))
	{
		return OnState;
	}
	if (GameState)
	{
		for (UActorComponent* Component : GameState->GetComponents())
		{
			if (IDFTeamWallet* OnComponent = AsWallet(Component))
			{
				return OnComponent;
			}
		}
	}
	if (!bWarnedNoWallet)
	{
		bWarnedNoWallet = true;
		UE_LOG(LogDFBuild, Warning, TEXT("No team wallet (IDFTeamWallet) on the game state: every build is refused as insufficientFunds until WS-06's economy component is attached."));
	}
	return nullptr;
}

void UDFBuildSubsystem::SetWalletOverride(UObject* Wallet)
{
	WalletOverride = Wallet;
}

void UDFBuildSubsystem::Forget(ADFTower* Tower)
{
	Towers.RemoveAll([Tower](const TWeakObjectPtr<ADFTower>& Weak) { return !Weak.IsValid() || Weak.Get() == Tower; });
}

bool UDFBuildSubsystem::WouldSealWithBarricadeOn(const ADFSocket& Socket) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	// The gates this socket closes, and the edges the barricades already standing close.
	TSet<FName> Closing;
	TSet<FName> Closed;
	for (TActorIterator<ADFLaneGate> It(World); It; ++It)
	{
		if (It->SocketId == Socket.SocketId)
		{
			Closing.Add(It->EdgeId);
		}
		else if (const ADFTower* Standing = FindTowerOnSocket(It->SocketId))
		{
			const FDFTowerRow* Row = Standing->GetRow();
			if (Row && Row->Kind == EDFTowerKind::Barricade)
			{
				Closed.Add(It->EdgeId);
			}
		}
	}
	if (Closing.Num() == 0)
	{
		return false;   // a barricade socket with no gate shapes nothing
	}
	UDFWorldSubsystem* WorldSubsystem = UDFWorldSubsystem::Get(this);
	const UDFLaneGraphAsset* Graph = WorldSubsystem ? WorldSubsystem->GetLaneGraph() : nullptr;
	if (!Graph)
	{
		return false;
	}
	// Step.cs checks each of the socket's gates on its own (w.WouldSeal(edge)); a socket has one gate.
	for (const FName& Edge : Closing)
	{
		TSet<FName> WithThis = Closed;
		WithThis.Add(Edge);
		if (Graph->WouldSeal(WithThis))
		{
			return true;
		}
	}
	return false;
}

// ---- commands -----------------------------------------------------------------------------------

FDFBuildResult UDFBuildSubsystem::PlaceTower(int32 PlayerId, FName TowerId, FName SocketId, bool bBuilderIsForge)
{
	using namespace DFTowerMath;
	UDFWorldSubsystem* WorldSubsystem = UDFWorldSubsystem::Get(this);
	ADFSocket* Socket = WorldSubsystem ? WorldSubsystem->FindSocket(SocketId) : nullptr;
	if (!Socket)
	{
		return Refuse(Reasons::UnknownSocket);
	}
	const UDFContentSubsystem* Content = UDFContentSubsystem::Get(this);
	if (Content && Content->Trap(TowerId))
	{
		UE_LOG(LogDFBuild, Warning, TEXT("'%s' is a trap; traps are placed by ADFTrap, which has not landed yet."), *TowerId.ToString());
		return Refuse(Reasons::UnknownTower);
	}
	const FDFTowerRow* Row = Content ? Content->Tower(TowerId) : nullptr;
	if (!Row)
	{
		return Refuse(Reasons::UnknownTower);
	}

	IDFTeamWallet* Wallet = FindWallet();
	FDFPlacementFacts Facts;
	Facts.SocketTag = Socket->Tag;
	Facts.bSocketOccupied = FindTowerOnSocket(SocketId) != nullptr;
	Facts.bWouldSeal = Row->Kind == EDFTowerKind::Barricade && !Facts.bSocketOccupied && WouldSealWithBarricadeOn(*Socket);
	Facts.Money = Wallet ? Wallet->GetMoney() : 0;
	const int32 Cost = BuildCost(*Row, bBuilderIsForge, DFBalance::Dial(this, TEXT("forgeBuildDiscount"), 0.9f));
	const FName Reason = CheckPlacement(*Row, Facts, Cost);
	if (!Reason.IsNone())
	{
		return Refuse(Reason);
	}
	if (!Wallet || !Wallet->TrySpend(Cost, nullptr))
	{
		return Refuse(Reasons::InsufficientFunds);
	}

	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform At(FRotator(0.f, Socket->GetActorRotation().Yaw, 0.f), Socket->GetPadTop());
	ADFTower* Tower = World ? World->SpawnActor<ADFTower>(ADFTower::StaticClass(), At, Params) : nullptr;
	if (!Tower || !Tower->InitializeTower(TowerId, SocketId, PlayerId, Cost))
	{
		// Nothing was built, so nothing was bought.
		Wallet->AddMoney(Cost);
		if (Tower)
		{
			Tower->Destroy();
		}
		UE_LOG(LogDFBuild, Error, TEXT("Could not spawn tower '%s' on '%s'; the money went back."), *TowerId.ToString(), *SocketId.ToString());
		return Refuse(Reasons::UnknownTower);
	}
	Towers.Add(Tower);
	// ADFTower::BeginPlay registers it too; a world that has not begun play (a test world) only gets this one.
	if (UDFStructureRegistry* Structures = UDFStructureRegistry::Get(this))
	{
		Structures->Register(Tower);
	}

	if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
	{
		FDFMsg_Structure Message;
		Message.StructureId = Tower->GetStructureId();
		Message.PlayerId = PlayerId;
		Message.DefId = TowerId;
		Message.SocketId = SocketId;
		Bus->BroadcastTeam(DFTags::Message_TowerPlaced, Message);
	}
	FDFBuildResult Result;
	Result.Tower = Tower;
	return Result;
}

FDFBuildResult UDFBuildSubsystem::UpgradeTower(int32 PlayerId, int32 StructureId, int32 PathIndex)
{
	using namespace DFTowerMath;
	ADFTower* Tower = FindTower(StructureId);
	const FDFTowerRow* Row = Tower ? Tower->GetRow() : nullptr;
	if (!Row)
	{
		return Refuse(Reasons::UnknownTower);
	}
	IDFTeamWallet* Wallet = FindWallet();
	static const TMap<EDFScrapType, int32> NoScrap;
	const FDFUpgradeQuote Quote = QuoteUpgrade(*Row, Tower->GetPathLevels(), PathIndex,
		Wallet ? Wallet->GetMoney() : 0, Wallet ? Wallet->GetTeamScrap() : NoScrap);
	if (!Quote.IsAllowed())
	{
		FDFBuildResult Refused = Refuse(Quote.Reason);
		Refused.Tower = Tower;
		return Refused;
	}
	if (!Wallet || !Wallet->TrySpend(Quote.MoneyCost, Quote.Recipe))
	{
		return Refuse(Reasons::InsufficientFunds);
	}
	Tower->ApplyUpgrade(PathIndex, Quote.MoneyCost);

	if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
	{
		FDFMsg_Upgrade Message;
		Message.StructureId = StructureId;
		Message.PlayerId = PlayerId;
		Message.PathId = Row->UpgradePaths[PathIndex].Id;
		Message.NewLevel = Quote.NextLevel;              // the level the player sees (purchases + 1)
		Message.bBreakpoint = Quote.Recipe != nullptr;   // 4, 7, 10: the levels that cost scrap
		Bus->BroadcastTeam(DFTags::Message_TowerUpgraded, Message);
	}
	FDFBuildResult Result;
	Result.Tower = Tower;
	return Result;
}

FDFBuildResult UDFBuildSubsystem::SellTower(int32 PlayerId, int32 StructureId)
{
	using namespace DFTowerMath;
	ADFTower* Tower = FindTower(StructureId);
	if (!Tower)
	{
		return Refuse(Reasons::UnknownTower);   // the sim ignores it; nothing is broadcast
	}
	const int32 Refund = SellRefund(Tower->GetSpent(), FMath::RoundToInt(DFBalance::Dial(this, TEXT("sellRefundPercent"), 70.f)));
	if (IDFTeamWallet* Wallet = FindWallet())
	{
		Wallet->AddMoney(Refund);
	}

	FDFMsg_Structure Message;
	Message.StructureId = StructureId;
	Message.PlayerId = PlayerId;
	Message.DefId = Tower->GetDefId();
	Message.SocketId = Tower->GetSocketId();
	Message.Refund = Refund;

	Forget(Tower);
	Tower->Destroy();
	if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
	{
		Bus->BroadcastTeam(DFTags::Message_TowerSold, Message);
	}
	FDFBuildResult Result;
	Result.Tower = Tower;
	Result.Refund = Refund;
	return Result;
}
