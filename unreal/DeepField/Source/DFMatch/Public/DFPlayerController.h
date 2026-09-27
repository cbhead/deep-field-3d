#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Messages/DFMessages.h"
#include "DFPlayerController.generated.h"

class ADFSocket;
class ADFTower;
class UInputAction;
struct FInputActionValue;

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
 * IA_Wheel picks a tower from QuickBuildChoices, IA_Build (hold E) on a free socket builds it there, IA_Upgrade (hold U) on a built
 * socket buys one level on its tower's least-bought path, IA_Sell (hold X) sells it. "On a socket" is the pad under the crosshair (a DF_Build trace from the view)
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

	/** What hold-E builds now (a towers.json id): QuickBuildChoices at the wheel's index. Local. */
	FName GetQuickBuildTowerId() const;
	/** Step the choice by Steps (wraps). The mouse wheel and the D-pad call it through IA_Wheel. */
	void CycleQuickBuild(int32 Steps);

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

	/** C16 IA_Upgrade: hold U. One purchase on the tower's least-bought path. */
	UPROPERTY(EditDefaultsOnly, Category = "DF|Input")
	TObjectPtr<UInputAction> UpgradeAction;

	/** C16 IA_Wheel: the mouse wheel (or the D-pad left/right) picks what hold-E builds. */
	UPROPERTY(EditDefaultsOnly, Category = "DF|Input")
	TObjectPtr<UInputAction> WheelAction;

	/** What the wheel steps through, until WS-12's radial build wheel (towers.json ids that fight on the
	 *  ground: the first playable's maps have no air waves yet). */
	UPROPERTY(EditDefaultsOnly, Category = "DF|Build")
	TArray<FName> QuickBuildChoices = { TEXT("lance"), TEXT("nova"), TEXT("arc"), TEXT("filament") };

	/** How far the crosshair reaches for a pad, and how near its landing point a pad must be. */
	UPROPERTY(EditDefaultsOnly, Category = "DF|Build")
	float QuickBuildRangeCm = 2500.f;

	UPROPERTY(EditDefaultsOnly, Category = "DF|Build")
	float QuickBuildReachCm = 250.f;

private:
	int32 GetSeat() const;
	void HandleBuildInput();
	void HandleSellInput();
	void HandleUpgradeInput();
	void HandleWheelInput(const FInputActionValue& Value);
	int32 QuickBuildIndex = 0;
};
