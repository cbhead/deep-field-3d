#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "DFPlayerState.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FDFOnPlayerStateChanged, class ADFPlayerState* /*PlayerState*/);

/**
 * One seat in the match (WS-28, ADR-0024). WS-28's own fields are the seat and the match stats (the
 * host match record WS-11 persists). Every other per-player field arrives as a component its domain
 * writes and attaches here: faction and level (WS-07), personal scrap and builds (WS-06), downed,
 * revive and vehicle seat (WS-03).
 *
 * CopyProperties carries the seat and stats across a seamless travel. Holding them for a player who
 * disconnects and rejoins (World.cs holds the seat) arrives with WS-11's rejoin flow: AGameModeBase
 * keeps no inactive player states.
 */
UCLASS()
class DFMATCH_API ADFPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ADFPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;

	/** 1-based seat (World.cs Player.Id); 0 until the game mode seats this player. */
	int32 GetSeat() const { return Seat; }
	int32 GetKills() const { return Kills; }
	float GetDamageDealt() const { return DamageDealt; }
	int32 GetTowersBuilt() const { return TowersBuilt; }
	int32 GetRevives() const { return Revives; }
	int32 GetMatchXp() const { return MatchXp; }

	// ---- host only ------------------------------------------------------------------------------
	void SetSeat(int32 InSeat);
	void AddKill();
	void AddDamageDealt(float Amount);
	void AddTowerBuilt();
	void AddRevive();
	/** XP events for the host match record (B§1.7: combo +3, elite +1, boss phase +10, cache +4). The
	 *  per-match cap (60) is applied where the record is folded into the profile (WS-11). */
	void AddMatchXp(int32 Amount);

	/** Fired when a replicated field changes: on the host when it is written, on clients on receipt. */
	FDFOnPlayerStateChanged OnPlayerStateChanged;

private:
	UFUNCTION()
	void OnRep_Match();

	void Changed();

	UPROPERTY(ReplicatedUsing = OnRep_Match) int32 Seat = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Match) int32 Kills = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Match) float DamageDealt = 0.f;
	UPROPERTY(ReplicatedUsing = OnRep_Match) int32 TowersBuilt = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Match) int32 Revives = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Match) int32 MatchXp = 0;
};
