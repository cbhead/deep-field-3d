#pragma once

#include "CoreMinimal.h"
#include "Messages/DFMessageBus.h"
#include "Subsystems/WorldSubsystem.h"
#include "DFBuildSubsystem.generated.h"

class ADFSocket;
class ADFTower;
class ADFTrap;
struct FDFTrapRow;
class IDFTeamWallet;

/** What a build command came to. Reason is DFTowerMath::Reasons::None when it went ahead. */
struct FDFBuildResult
{
	FName Reason;
	/** The tower placed, upgraded or (until the end of the frame) sold. */
	ADFTower* Tower = nullptr;
	/** A trap id placed: the trap. */
	ADFTrap* Trap = nullptr;
	/** Sell only: the money returned. */
	int32 Refund = 0;

	bool Succeeded() const { return Reason.IsNone(); }
};

/**
 * The host's build service (WS-04): Step.cs ApplyPlaceTower / ApplyUpgradeTower / ApplySellTower.
 * ADFPlayerController's Server RPCs call it; it checks the rules in the sim's order through
 * DFTowerMath, moves the team's money and scrap through IDFTeamWallet (WS-06's economy component),
 * spawns or removes the ADFTower, and announces the outcome to the team (DF.Message.TowerPlaced /
 * TowerUpgraded / TowerSold via UDFMessageBus::BroadcastTeam). A refusal is returned, not broadcast:
 * it belongs to the issuer only, and the controller sends it back (Client_Refused).
 *
 * Known gaps, each owned elsewhere:
 * - The Forge discount needs the builder's faction (WS-07); callers pass it.
 * - wouldSeal counts the edges that standing barricades close. Operated gates and mutables also close
 *   edges; WS-09's lane state component will report those, and this adds them when it exists.
 * - After a barricade goes up, is sold or is breached, the sim refreshes edge state (RefreshEdgeState).
 *   That is WS-09's lane state component, which listens for TowerPlaced / TowerSold / TowerDestroyed.
 */
UCLASS()
class DFTOWERS_API UDFBuildSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDFBuildSubsystem* Get(const UObject* WorldContext);

	// End of frame on the host: remove what siege broke this frame (Step.cs SiegeStructures' removal pass).
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/**
	 * The condition this wave is fought in (a conditions.json id, NAME_None for clear weather): every
	 * standing tower gets it now, and every tower built before the next wave starts gets it when placed
	 * (Step.cs reads Conditions.ForWave on every step, so a tower built mid-wave is in the weather too).
	 * Set from DF.Message.WaveStarted on the host; tests and dev tools call it directly.
	 */
	void SetWaveCondition(FName ConditionId);
	FName GetWaveCondition() const { return WaveConditionId; }
	/** DF.Condition.Coldsnap -> "coldsnap": the content id a condition tag names (ids are lower camelCase). */
	static FName ConditionIdFromTag(const FGameplayTag& ConditionTag);

	/**
	 * Remove every tower a siege broke (IDFStructure hp at 0): TowerDestroyed to the team (State
	 * "breached" for a barricade, which reopens its lane: WS-09's lane state hears it), then the actor
	 * goes. The Tick calls it; tests call it by hand. Returns how many went.
	 */
	int32 RemoveBroken();
	/** Remove every trap whose last charge is spent and whose rearm has run out (the sim's
	 *  `w.Traps.RemoveAll`), with TowerDestroyed (State "spent") so the pad reads free. */
	int32 RemoveSpentTraps();

	/** Command.PlaceTower: TowerId (a towers.json or traps.json id) on SocketId, for PlayerId (the seat).
	 *  A trap id takes Step.cs ApplyPlaceTrap's path: a Trap socket, unoccupied, money, then its scrap. */
	FDFBuildResult PlaceTower(int32 PlayerId, FName TowerId, FName SocketId, bool bBuilderIsForge = false);
	/** Command.UpgradeTower: one purchase on PathIndex of the tower with StructureId. */
	FDFBuildResult UpgradeTower(int32 PlayerId, int32 StructureId, int32 PathIndex);
	/** Command.SellTower: refund sellRefundPercent of everything spent on it, then remove it. An unknown
	 *  tower is ignored without a message, as the sim ignores it (Reason is unknownTower for the caller's log). */
	FDFBuildResult SellTower(int32 PlayerId, int32 StructureId);

	ADFTower* FindTower(int32 StructureId) const;
	ADFTower* FindTowerOnSocket(FName SocketId) const;
	ADFTrap* FindTrapOnSocket(FName SocketId) const;
	/** Every live trap, in build order. */
	TArray<ADFTrap*> GetTraps() const;
	/** Every standing tower, in build order. */
	TArray<ADFTower*> GetTowers() const;

	/** The purse builds spend from: the override when one is set, else the first IDFTeamWallet on the
	 *  game state or one of its components (WS-06's economy component). Null when there is none. */
	IDFTeamWallet* FindWallet() const;
	/** Tests and dev maps: spend from this object (it must implement IDFTeamWallet). Null clears it. */
	void SetWalletOverride(UObject* Wallet);

private:
	/** Would a barricade on Socket seal a spawn from the core, given the barricades already standing? */
	bool WouldSealWithBarricadeOn(const ADFSocket& Socket) const;
	void Forget(ADFTower* Tower);
	FDFBuildResult PlaceTrap(int32 PlayerId, FName TrapId, const FDFTrapRow& Row, ADFSocket& Socket);

	TArray<TWeakObjectPtr<ADFTower>> Towers;
	TArray<TWeakObjectPtr<ADFTrap>> Traps;
	TWeakObjectPtr<UObject> WalletOverride;
	FName WaveConditionId;
	FDFMessageHandle WaveStartedHandle;
	mutable bool bWarnedNoWallet = false;
};
