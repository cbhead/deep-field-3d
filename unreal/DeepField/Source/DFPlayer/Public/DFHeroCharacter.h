#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Hero/DFHeroLife.h"
#include "Hero/DFHeroStateComponent.h"
#include "DFHeroCharacter.generated.h"

class UAbilitySystemComponent;
class UCameraComponent;
class UDFAbilitySystemComponent;
class UDFHealthSet;
class UDFHeroMovementComponent;
class UDFHeroSet;
class UInputAction;
class UInputMappingContext;
struct FGameplayEffectSpec;
struct FInputActionValue;

/**
 * The hero (WS-03, B§1.1): a first-person ACharacter on UDFHeroMovementComponent, with Godot's body
 * (0.4 m × 1.8 m capsule on DF_Hero, camera 1.6 m above the feet, 80° FOV, pitch clamped to ±1.5 rad).
 *
 * Input is Enhanced Input. The actions and the mapping context are C16 assets (IA_Move, IA_Look,
 * IA_Jump, IA_Sprint, IA_Crouch, IA_Aim in IMC_DF_Default) that do not exist yet; they are assigned on
 * the hero's Blueprint when they do, and until then the hero binds nothing and still spawns.
 *
 * Health (PROGRAMME.md §3.3 puts the hero's attributes on ADFHeroCharacter, so its ASC is here, Mixed
 * replication as C4 says for player-owned pawns): UDFHealthSet at playerMaxHp and UDFHeroSet at the
 * regen and bleedout dials. On the host, 0 health calls HostDeplete on the player's
 * UDFHeroStateComponent; a revive sets health to RevivedHpFraction of max and a respawn to max;
 * a standing hero regenerates after RegenDelay without damage (Step.cs). The hero follows its state
 * component on host and clients, which caps its movement while down.
 *
 * Weapons, melee, interaction, the build ghost and drag arrive in later PRs.
 */
UCLASS()
class DFPLAYER_API ADFHeroCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ADFHeroCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UDFHeroMovementComponent* GetHeroMovement() const;
	UCameraComponent* GetFirstPersonCamera() const;
	UDFAbilitySystemComponent* GetHeroAbilitySystem() const;
	UDFHealthSet* GetHealthSet() const;
	UDFHeroSet* GetHeroSet() const;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_PlayerState() override;
	virtual void PawnClientRestart() override;
	virtual void BecomeViewTarget(APlayerController* PC) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	UPROPERTY(VisibleAnywhere, Category = "DF|Abilities")
	TObjectPtr<UDFAbilitySystemComponent> AbilitySystem;

	/** Attribute sets as subobjects of the hero: the ASC registers them in InitializeComponent. */
	UPROPERTY()
	TObjectPtr<UDFHealthSet> HealthSet;

	UPROPERTY()
	TObjectPtr<UDFHeroSet> HeroSet;

	/** Host: playerMaxHp, playerRegenPerSecond, playerRegenDelaySeconds, bleedoutSeconds. */
	void InitHeroAttributes();

	/** Follow the player state's UDFHeroStateComponent, if it has one yet (host and clients). */
	void BindHeroState();
	void UnbindHeroState();

	void HandleHealthDepleted(AActor* Instigator, AActor* Causer, const FGameplayEffectSpec* Spec, float Magnitude, float OldValue, float NewValue);
	void HandleDamaged(AActor* Instigator, AActor* Causer, const FGameplayEffectSpec* Spec, float Magnitude, float OldValue, float NewValue);
	void HandleHeroLifeEvent(EDFHeroLifeEvent Event, UDFHeroStateComponent* Reviver);
	void HandleHeroStateChanged(UDFHeroStateComponent* State);

	TWeakObjectPtr<UDFHeroStateComponent> BoundHeroState;
	FDelegateHandle HeroStateChangedHandle;
	FDelegateHandle HeroLifeEventHandle;
	FDFHeroRegen Regen;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartSprint();
	void StopSprint();
	void StartCrouch();
	void StopCrouch();
	void StartAim();
	void StopAim();
};
