#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Messages/DFMessages.h"
#include "DFPlayerController.generated.h"

class ADFSocket;
class ADFTower;
class UInputAction;

/**
 * The Server RPC surface (WS-28, ADR-0024; PROGRAMME.md §3.3): every mutation a player asks for is a
 * Server RPC here, one per `Commands.cs` command, and every refusal goes back to the issuing client
 * only as `Client_Refused` → a `DF.Message.*Rejected` on that client's bus. Commands whose rules
 * belong to another domain (build, buy, revive, ...) arrive with that domain: the RPC is added here and
 * forwards to the domain's host-side code.
 *
 * Match flow: Launch (lobby → intermission, launch seat only) and StartWave (the early call).
 * Build (WS-04): PlaceTower, UpgradeTower, SellTower, forwarded to UDFBuildSubsystem.
 *
 * Until WS-12's build wheel exists, the controller also binds C16's hold-to-build pair itself:
 * IA_Build (hold E) on a free socket builds QuickBuildTowerId there, IA_Sell (hold X) on a built
 * socket sells its tower. "On a socket" is the pad under the crosshair (a DF_Build trace from the view)
 * or, when the crosshair is on the ground, the nearest pad within QuickBuildReachCm of where it lands.
 */
UCLASS()
class DFMATCH_API ADFPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADFPlayerController();

	/** The socket the crosshair means, or null (local; see the class comment). */
	ADFSocket* FindAimedSocket() const;
	/** The standing tower on SocketId, as this machine sees it (towers replicate their socket), or null. */
	ADFTower* FindTowerOn(FName SocketId) const;

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

protected:
	virtual void SetupInputComponent() override;

	/** C16 IA_Build: hold E. */
	UPROPERTY(EditDefaultsOnly, Category = "DF|Input")
	TObjectPtr<UInputAction> BuildAction;

	/** C16 IA_Sell: hold X 0.7 s. */
	UPROPERTY(EditDefaultsOnly, Category = "DF|Input")
	TObjectPtr<UInputAction> SellAction;

	/** What hold-E builds until the build wheel lets the player choose (a towers.json id). */
	UPROPERTY(EditDefaultsOnly, Category = "DF|Build")
	FName QuickBuildTowerId = TEXT("lance");

	/** How far the crosshair reaches for a pad, and how near its landing point a pad must be. */
	UPROPERTY(EditDefaultsOnly, Category = "DF|Build")
	float QuickBuildRangeCm = 2500.f;

	UPROPERTY(EditDefaultsOnly, Category = "DF|Build")
	float QuickBuildReachCm = 250.f;

private:
	int32 GetSeat() const;
	void HandleBuildInput();
	void HandleSellInput();
};
