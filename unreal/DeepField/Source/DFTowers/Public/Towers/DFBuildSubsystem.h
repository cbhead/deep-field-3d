#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DFBuildSubsystem.generated.h"

class ADFSocket;
class ADFTower;
class IDFTeamWallet;

/** What a build command came to. Reason is DFTowerMath::Reasons::None when it went ahead. */
struct FDFBuildResult
{
	FName Reason;
	/** The tower placed, upgraded or (until the end of the frame) sold. */
	ADFTower* Tower = nullptr;
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
 * - Traps (the sim routes a trap def to ApplyPlaceTrap) arrive with ADFTrap; until then a trap id is
 *   refused as unknownTower, with a warning in the log.
 * - The Forge discount needs the builder's faction (WS-07); callers pass it.
 * - wouldSeal counts the edges that standing barricades close. Operated gates and mutables also close
 *   edges; WS-09's lane state component will report those, and this adds them when it exists.
 * - After a barricade goes up or is sold, the sim refreshes edge state (RefreshEdgeState). That is
 *   WS-09's lane state component, which listens for TowerPlaced / TowerSold.
 */
UCLASS()
class DFTOWERS_API UDFBuildSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDFBuildSubsystem* Get(const UObject* WorldContext);

	/** Command.PlaceTower: TowerId (a towers.json id) on SocketId, for PlayerId (the seat). */
	FDFBuildResult PlaceTower(int32 PlayerId, FName TowerId, FName SocketId, bool bBuilderIsForge = false);
	/** Command.UpgradeTower: one purchase on PathIndex of the tower with StructureId. */
	FDFBuildResult UpgradeTower(int32 PlayerId, int32 StructureId, int32 PathIndex);
	/** Command.SellTower: refund sellRefundPercent of everything spent on it, then remove it. An unknown
	 *  tower is ignored without a message, as the sim ignores it (Reason is unknownTower for the caller's log). */
	FDFBuildResult SellTower(int32 PlayerId, int32 StructureId);

	ADFTower* FindTower(int32 StructureId) const;
	ADFTower* FindTowerOnSocket(FName SocketId) const;
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

	TArray<TWeakObjectPtr<ADFTower>> Towers;
	TWeakObjectPtr<UObject> WalletOverride;
	mutable bool bWarnedNoWallet = false;
};
