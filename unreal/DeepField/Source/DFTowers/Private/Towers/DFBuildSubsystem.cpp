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
#include "Towers/DFTrap.h"
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

	/** A quiet "is this id in the table": UDFContentSubsystem's lookups log a missing row as an Error
	 *  (content that should exist and does not), but a player asking for an id that is not a tower is
	 *  a refusal, not broken content. */
	bool HasRow(const UDFContentSubsystem* Content, const TCHAR* Table, FName Id)
	{
		return Content && !Id.IsNone() && Content->Ids(Table).Contains(Id);
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
		RemoveSpentTraps();
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

void UDFBuildSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;   // clients get the condition through each tower's replicated ActiveConditionId
	}
	if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
	{
		TWeakObjectPtr<UDFBuildSubsystem> WeakThis(this);
		WaveStartedHandle = Bus->Subscribe<FDFMsg_Wave>(DFTags::Message_WaveStarted,
			[WeakThis](const FGameplayTag&, const FDFMsg_Wave& Wave)
			{
				if (UDFBuildSubsystem* Self = WeakThis.Get())
				{
					Self->SetWaveCondition(ConditionIdFromTag(Wave.Condition));
				}
			});
	}
}

void UDFBuildSubsystem::Deinitialize()
{
	if (WaveStartedHandle.IsValid())
	{
		if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
		{
			Bus->Unsubscribe(WaveStartedHandle);
		}
		WaveStartedHandle = FDFMessageHandle();
	}
	Super::Deinitialize();
}

FName UDFBuildSubsystem::ConditionIdFromTag(const FGameplayTag& ConditionTag)
{
	if (!ConditionTag.IsValid())
	{
		return NAME_None;
	}
	FString Leaf = ConditionTag.GetTagName().ToString();
	int32 Dot = INDEX_NONE;
	if (Leaf.FindLastChar(TEXT('.'), Dot))
	{
		Leaf.RightChopInline(Dot + 1);
	}
	if (Leaf.IsEmpty())
	{
		return NAME_None;
	}
	Leaf[0] = FChar::ToLower(Leaf[0]);
	return FName(*Leaf);
}

void UDFBuildSubsystem::SetWaveCondition(FName ConditionId)
{
	if (!ConditionId.IsNone())
	{
		const UDFContentSubsystem* Content = UDFContentSubsystem::Get(this);
		if (Content && !HasRow(Content, TEXT("conditions"), ConditionId))
		{
			UE_LOG(LogDFBuild, Warning, TEXT("Wave condition '%s' has no conditions.json row; towers fight in clear weather."), *ConditionId.ToString());
			ConditionId = NAME_None;
		}
	}
	WaveConditionId = ConditionId;
	for (ADFTower* Tower : GetTowers())
	{
		Tower->SetActiveCondition(WaveConditionId);
	}
}

int32 UDFBuildSubsystem::RemoveSpentTraps()
{
	TArray<ADFTrap*> Spent;
	for (ADFTrap* Trap : GetTraps())
	{
		if (Trap->IsSpent())
		{
			Spent.Add(Trap);
		}
	}
	UDFMessageBus* Bus = UDFMessageBus::Get(this);
	for (ADFTrap* Trap : Spent)
	{
		FDFMsg_Structure Message;
		Message.StructureId = Trap->GetStructureId();
		Message.PlayerId = Trap->GetOwnerSeat();
		Message.DefId = Trap->GetDefId();
		Message.SocketId = Trap->GetSocketId();
		Message.HpFraction = 0.f;
		Message.State = TEXT("spent");
		Traps.RemoveAll([Trap](const TWeakObjectPtr<ADFTrap>& Weak) { return !Weak.IsValid() || Weak.Get() == Trap; });
		Trap->Destroy();
		if (Bus)
		{
			Bus->BroadcastTeam(DFTags::Message_TowerDestroyed, Message);
		}
	}
	return Spent.Num();
}

ADFTrap* UDFBuildSubsystem::FindTrapOnSocket(FName SocketId) const
{
	for (const TWeakObjectPtr<ADFTrap>& Weak : Traps)
	{
		ADFTrap* Trap = Weak.Get();
		if (IsValid(Trap) && Trap->GetSocketId() == SocketId)
		{
			return Trap;
		}
	}
	return nullptr;
}

TArray<ADFTrap*> UDFBuildSubsystem::GetTraps() const
{
	TArray<ADFTrap*> Out;
	for (const TWeakObjectPtr<ADFTrap>& Weak : Traps)
	{
		if (ADFTrap* Trap = Weak.Get(); IsValid(Trap))
		{
			Out.Add(Trap);
		}
	}
	return Out;
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
	if (HasRow(Content, TEXT("traps"), TowerId))
	{
		if (const FDFTrapRow* TrapRow = Content->Trap(TowerId))
		{
			return PlaceTrap(PlayerId, TowerId, *TrapRow, *Socket);   // Step.cs: trap defs route to ApplyPlaceTrap
		}
	}
	const FDFTowerRow* Row = HasRow(Content, TEXT("towers"), TowerId) ? Content->Tower(TowerId) : nullptr;
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
	Tower->SetActiveCondition(WaveConditionId);   // built mid-wave: in this wave's weather from its first shot
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

FDFBuildResult UDFBuildSubsystem::PlaceTrap(int32 PlayerId, FName TrapId, const FDFTrapRow& Row, ADFSocket& Socket)
{
	using namespace DFTowerMath;
	// Step.cs ApplyPlaceTrap, in order: a Trap socket, not occupied, money, scrap. No Forge discount.
	if (Socket.Tag != EDFSocketTag::Trap)
	{
		return Refuse(Reasons::WrongSocketTag);
	}
	if (FindTrapOnSocket(Socket.SocketId))
	{
		return Refuse(Reasons::Occupied);
	}
	IDFTeamWallet* Wallet = FindWallet();
	if (!Wallet || Wallet->GetMoney() < Row.Cost)
	{
		return Refuse(Reasons::InsufficientFunds);
	}
	for (const TPair<EDFScrapType, int32>& Part : Row.ScrapCost.Amounts)
	{
		if (Wallet->GetTeamScrap().FindRef(Part.Key) < Part.Value)
		{
			return Refuse(Reasons::InsufficientScrap);
		}
	}
	if (!Wallet->TrySpend(Row.Cost, &Row.ScrapCost))
	{
		return Refuse(Reasons::InsufficientFunds);
	}

	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform At(FRotator(0.f, Socket.GetActorRotation().Yaw, 0.f), Socket.GetPadTop());
	ADFTrap* Trap = World ? World->SpawnActor<ADFTrap>(ADFTrap::StaticClass(), At, Params) : nullptr;
	if (!Trap || !Trap->InitializeTrap(TrapId, Socket.SocketId, PlayerId))
	{
		// Nothing was built: the money goes back. (Scrap too: the sim never gets here, so this is a guard, not a rule.)
		Wallet->AddMoney(Row.Cost);
		if (Trap)
		{
			Trap->Destroy();
		}
		UE_LOG(LogDFBuild, Error, TEXT("Could not spawn trap '%s' on '%s'; the money went back (scrap spent: %d lines)."),
			*TrapId.ToString(), *Socket.SocketId.ToString(), Row.ScrapCost.Amounts.Num());
		return Refuse(Reasons::UnknownTower);
	}
	Traps.Add(Trap);

	if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
	{
		FDFMsg_Structure Message;
		Message.StructureId = Trap->GetStructureId();
		Message.PlayerId = PlayerId;
		Message.DefId = TrapId;
		Message.SocketId = Socket.SocketId;
		Message.State = TEXT("armed");
		Bus->BroadcastTeam(DFTags::Message_TowerPlaced, Message);
	}
	FDFBuildResult Result;
	Result.Trap = Trap;
	return Result;
}

FDFBuildResult UDFBuildSubsystem::UpgradeTower(int32 PlayerId, int32 StructureId, int32 PathIndex)
{
	using namespace DFTowerMath;
	ADFTower* Tower = FindTower(StructureId);
	const FDFTowerRow* Row = (Tower && !Tower->IsBroken()) ? Tower->GetRow() : nullptr;
	if (!Row)
	{
		return Refuse(Reasons::UnknownTower);   // rubble awaiting removal is not a tower (Step.cs removes it the same tick)
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
	if (!Tower || Tower->IsBroken())
	{
		return Refuse(Reasons::UnknownTower);   // the sim ignores it (rubble included: it is gone by then); nothing is broadcast
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
