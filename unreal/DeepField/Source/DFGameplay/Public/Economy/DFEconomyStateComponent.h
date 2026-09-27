#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Economy/DFEconomySeams.h"
#include "Match/DFMatchSeams.h"
#include "Messages/DFMessageBus.h"
#include "DFEconomyStateComponent.generated.h"

struct FDFMsg_Enemy;
struct FDFMsg_Kill;

DECLARE_MULTICAST_DELEGATE_OneParam(FDFOnEconomyChanged, class UDFEconomyStateComponent* /*Economy*/);

/**
 * The team's money and the core's lives (WS-06, ADR-0024): a replicated component WS-28 attaches to
 * ADFMatchState. The host writes it; every machine reads it (the HUD, the build wheel's prices).
 *
 * - **Lives** start at the balance dial startingLives. The host takes a leak's lives when it hears
 *   DF.Message.EnemyLeaked (FDFMsg_Enemy.Phase carries the leak damage), which ADFEnemy sends BEFORE
 *   the body is reported to the wave director, so a last enemy that leaks the core to zero is a Defeat
 *   (DFMatchSeams.h). Lives never go below 0 (Step.cs CheckEndState clamps them there).
 * - **Money** starts at startingMoney. The host pays a kill's bounty when it hears DF.Message.EnemyKilled
 *   (FDFMsg_Kill.Bounty, already scaled for the wave the body spawned in, Step.cs ScaledBounty), and
 *   spends through IDFTeamWallet (UDFBuildSubsystem finds it on the game state).
 * - **Team scrap** is the wallet's pool; nothing earns scrap yet, so it stays empty and host-only until
 *   the scrap half of WS-06 replicates it.
 *
 * Not here yet (the rest of WS-06): scrap, personal loadouts, purchases, the early-call bonus.
 */
UCLASS(ClassGroup = (DF), meta = (BlueprintSpawnableComponent))
class DFGAMEPLAY_API UDFEconomyStateComponent : public UActorComponent, public IDFTeamWallet, public IDFMatchLivesSource
{
	GENERATED_BODY()

public:
	UDFEconomyStateComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	// ---- IDFTeamWallet (host) ----------------------------------------------------------------------
	virtual int32 GetMoney() const override { return Money; }
	virtual const TMap<EDFScrapType, int32>& GetTeamScrap() const override { return TeamScrap; }
	virtual bool TrySpend(int32 InMoney, const FDFScrapBundle* Scrap) override;
	virtual void AddMoney(int32 InMoney) override;

	// ---- IDFMatchLivesSource -----------------------------------------------------------------------
	virtual int32 GetLives() const override { return Lives; }

	// ---- host --------------------------------------------------------------------------------------
	/** A new match: the balance dials startingMoney and startingLives (BeginPlay calls it on the host). */
	void ResetFromBalance();
	void Reset(int32 InMoney, int32 InLives);
	/** A leak: Amount lives off, never below 0. */
	void TakeLives(int32 Amount);

	/** Money, lives: on the host when written, on clients when they arrive. */
	FDFOnEconomyChanged OnEconomyChanged;

	/** The sim's defaults, until the balance table answers (the dials say the same). */
	static constexpr int32 DefaultStartingMoney = 250;
	static constexpr int32 DefaultStartingLives = 20;

private:
	UFUNCTION()
	void OnRep_Economy();

	void HandleLeaked(const FDFMsg_Enemy& Msg);
	void HandleKilled(const FDFMsg_Kill& Msg);
	void Changed();

	UPROPERTY(ReplicatedUsing = OnRep_Economy)
	int32 Money = DefaultStartingMoney;

	UPROPERTY(ReplicatedUsing = OnRep_Economy)
	int32 Lives = DefaultStartingLives;

	TMap<EDFScrapType, int32> TeamScrap;

	FDFMessageHandle LeakedHandle;
	FDFMessageHandle KilledHandle;
};
