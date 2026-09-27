#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Messages/DFMessages.h"
#include "DFPlayerController.generated.h"

/**
 * The Server RPC surface (WS-28, ADR-0024; PROGRAMME.md §3.3): every mutation a player asks for is a
 * Server RPC here, one per `Commands.cs` command, and every refusal goes back to the issuing client
 * only as `Client_Refused` → a `DF.Message.*Rejected` on that client's bus. Commands whose rules
 * belong to another domain (build, buy, revive, ...) arrive with that domain: the RPC is added here and
 * forwards to the domain's host-side code.
 *
 * Match flow: Launch (lobby → intermission, launch seat only) and StartWave (the early call).
 * Build (WS-04): PlaceTower, UpgradeTower, SellTower, forwarded to UDFBuildSubsystem.
 */
UCLASS()
class DFMATCH_API ADFPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Command.Launch. Ignored unless this player holds the launch seat and the party is in the lobby (as the sim). */
	UFUNCTION(Server, Reliable)
	void Server_Launch();

	/** Command.StartWave: call the next wave now. Ignored outside intermission and in the lobby (as the sim). */
	UFUNCTION(Server, Reliable)
	void Server_CallEarly();

	// ---- build (WS-04: the rules are UDFBuildSubsystem's, in DFTowers) --------------------------------

	/** Command.PlaceTower: TowerId (a towers.json id) on SocketId. A refusal comes back as DF.Message.BuildRejected
	 *  (Subject = the socket, Detail = the tower id); success is DF.Message.TowerPlaced to the whole team. */
	UFUNCTION(Server, Reliable)
	void Server_PlaceTower(FName TowerId, FName SocketId);

	/** Command.UpgradeTower: one purchase on PathIndex. A refusal comes back as DF.Message.UpgradeRejected
	 *  (Subject = the tower's def id, Detail = its structure id); success is DF.Message.TowerUpgraded to the team. */
	UFUNCTION(Server, Reliable)
	void Server_UpgradeTower(int32 StructureId, int32 PathIndex);

	/** Command.SellTower. An unknown tower is ignored, as the sim ignores it; success is DF.Message.TowerSold. */
	UFUNCTION(Server, Reliable)
	void Server_SellTower(int32 StructureId);

	/** A refusal for this client only: re-broadcast on its bus under Tag (a DF.Message.*Rejected). */
	UFUNCTION(Client, Reliable)
	void Client_Refused(FGameplayTag Tag, const FDFMsg_Rejected& Payload);

private:
	int32 GetSeat() const;
};
