#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DFHeroCharacter.generated.h"

class UCameraComponent;
class UDFHeroMovementComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * The hero (WS-03, B§1.1): a first-person ACharacter on UDFHeroMovementComponent, with Godot's body
 * (0.4 m × 1.8 m capsule on DF_Hero, camera 1.6 m above the feet, 80° FOV, pitch clamped to ±1.5 rad).
 *
 * Input is Enhanced Input. The actions and the mapping context are C16 assets (IA_Move, IA_Look,
 * IA_Jump, IA_Sprint, IA_Crouch, IA_Aim in IMC_DF_Default) that do not exist yet; they are assigned on
 * the hero's Blueprint when they do, and until then the hero binds nothing and still spawns.
 *
 * Weapons, melee, interaction, the build ghost and downed/revive/drag (UDFHeroStateComponent,
 * ADR-0024) arrive in later PRs.
 */
UCLASS()
class DFPLAYER_API ADFHeroCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADFHeroCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UDFHeroMovementComponent* GetHeroMovement() const;
	UCameraComponent* GetFirstPersonCamera() const;

	virtual void PawnClientRestart() override;
	virtual void BecomeViewTarget(APlayerController* PC) override;

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** Added to the owning local player at priority 0 when this hero is possessed (C16: IMC_DF_Default). */
	UPROPERTY(EditDefaultsOnly, Category = "DF|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "DF|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "DF|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "DF|Input")
	TObjectPtr<UInputAction> JumpAction;

	/** Held, as Shift is in Godot. */
	UPROPERTY(EditDefaultsOnly, Category = "DF|Input")
	TObjectPtr<UInputAction> SprintAction;

	/** Held. */
	UPROPERTY(EditDefaultsOnly, Category = "DF|Input")
	TObjectPtr<UInputAction> CrouchAction;

	/** Held (RMB). */
	UPROPERTY(EditDefaultsOnly, Category = "DF|Input")
	TObjectPtr<UInputAction> AimAction;

private:
	UPROPERTY(VisibleAnywhere, Category = "DF|Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartSprint();
	void StopSprint();
	void StartCrouch();
	void StopCrouch();
	void StartAim();
	void StopAim();
};
