#include "DFHeroCharacter.h"

#include "Abilities/DFAbilitySystemComponent.h"
#include "Attributes/DFHealthSet.h"
#include "Attributes/DFHeroSet.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "DFHeroCollision.h"
#include "DFPlayerModule.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "DFBalanceDial.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Hero/DFHeroStateComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Movement/DFHeroMovementComponent.h"
#include "Movement/DFHeroMoveRules.h"

ADFHeroCharacter::ADFHeroCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UDFHeroMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->InitCapsuleSize(DFHeroMove::CapsuleRadiusCm, DFHeroMove::CapsuleHalfHeightCm);
	Capsule->SetCollisionProfileName(DFHeroCollision::Profile());

	// First person: the controller's yaw turns the body, its pitch only the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	const float EyeAboveCentreCm = DFHeroMove::EyeHeightAboveFeetCm - DFHeroMove::CapsuleHalfHeightCm;
	BaseEyeHeight = EyeAboveCentreCm;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(Capsule);
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, EyeAboveCentreCm));
	FirstPersonCamera->SetFieldOfView(DFHeroMove::FieldOfViewDegrees);
	FirstPersonCamera->bUsePawnControlRotation = true;

	AbilitySystem = CreateDefaultSubobject<UDFAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	HealthSet = CreateDefaultSubobject<UDFHealthSet>(TEXT("HealthSet"));
	HeroSet = CreateDefaultSubobject<UDFHeroSet>(TEXT("HeroSet"));

	PrimaryActorTick.bCanEverTick = true;   // the host's regen
}

UDFHeroMovementComponent* ADFHeroCharacter::GetHeroMovement() const
{
	return Cast<UDFHeroMovementComponent>(GetCharacterMovement());
}

UCameraComponent* ADFHeroCharacter::GetFirstPersonCamera() const
{
	return FirstPersonCamera;
}

UDFAbilitySystemComponent* ADFHeroCharacter::GetHeroAbilitySystem() const
{
	return AbilitySystem;
}

UDFHealthSet* ADFHeroCharacter::GetHealthSet() const
{
	return HealthSet;
}

UDFHeroSet* ADFHeroCharacter::GetHeroSet() const
{
	return HeroSet;
}

UAbilitySystemComponent* ADFHeroCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ADFHeroCharacter::BeginPlay()
{
	Super::BeginPlay();

	AbilitySystem->InitAbilityActorInfo(this, this);
	if (HasAuthority())
	{
		InitHeroAttributes();
		HealthSet->OnHealthDepleted.AddUObject(this, &ADFHeroCharacter::HandleHealthDepleted);
		HealthSet->OnDamaged.AddUObject(this, &ADFHeroCharacter::HandleDamaged);
	}
	BindHeroState();
}

void ADFHeroCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindHeroState();
	Super::EndPlay(EndPlayReason);
}

void ADFHeroCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	AbilitySystem->RefreshAbilityActorInfo();   // now the ASC knows its player controller
	BindHeroState();
}

void ADFHeroCharacter::UnPossessed()
{
	UnbindHeroState();
	Super::UnPossessed();
}

void ADFHeroCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	BindHeroState();
}

void ADFHeroCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}
	const UDFHeroStateComponent* State = BoundHeroState.Get();
	if (State != nullptr && !State->IsUp())
	{
		return;   // Step.cs: no regen while down or waiting to respawn
	}
	const float Health = HealthSet->GetHealth();
	const float NewHealth = DFHeroLife::RegenStep(Regen, Health, HealthSet->GetMaxHealth(), HeroSet->GetRegenPerSecond(), DeltaSeconds);
	if (NewHealth != Health)
	{
		HealthSet->SetHealth(NewHealth);
	}
}

void ADFHeroCharacter::InitHeroAttributes()
{
	const float MaxHealth = DFBalance::Dial(this, TEXT("playerMaxHp"), 100.f);
	HealthSet->InitMaxHealth(MaxHealth);
	HealthSet->InitHealth(MaxHealth);
	HeroSet->InitRegenPerSecond(DFBalance::Dial(this, TEXT("playerRegenPerSecond"), 10.f));
	HeroSet->InitRegenDelay(DFBalance::Dial(this, TEXT("playerRegenDelaySeconds"), 6.f));
	HeroSet->InitBleedoutSeconds(DFBalance::Dial(this, TEXT("bleedoutSeconds"), 30.f));
	Regen = FDFHeroRegen();
}

void ADFHeroCharacter::BindHeroState()
{
	UDFHeroStateComponent* State = UDFHeroStateComponent::Find(GetPlayerState());
	if (State == BoundHeroState.Get())
	{
		return;
	}
	UnbindHeroState();
	if (State == nullptr)
	{
		return;   // nothing hosts the hero state yet (WS-28 attaches it to ADFPlayerState)
	}
	BoundHeroState = State;
	HeroStateChangedHandle = State->OnHeroStateChanged.AddUObject(this, &ADFHeroCharacter::HandleHeroStateChanged);
	if (HasAuthority())
	{
		HeroLifeEventHandle = State->OnHeroLifeEvent.AddUObject(this, &ADFHeroCharacter::HandleHeroLifeEvent);
	}
	HandleHeroStateChanged(State);
}

void ADFHeroCharacter::UnbindHeroState()
{
	if (UDFHeroStateComponent* State = BoundHeroState.Get())
	{
		State->OnHeroStateChanged.Remove(HeroStateChangedHandle);
		State->OnHeroLifeEvent.Remove(HeroLifeEventHandle);
	}
	HeroStateChangedHandle.Reset();
	HeroLifeEventHandle.Reset();
	BoundHeroState.Reset();
	HandleHeroStateChanged(nullptr);
}

void ADFHeroCharacter::HandleHealthDepleted(AActor* /*Instigator*/, AActor* /*Causer*/, const FGameplayEffectSpec* /*Spec*/, float /*Magnitude*/, float /*OldValue*/, float /*NewValue*/)
{
	BindHeroState();
	UDFHeroStateComponent* State = BoundHeroState.Get();
	if (State == nullptr)
	{
		UE_LOG(LogDFPlayer, Warning, TEXT("%s reached 0 health with no UDFHeroStateComponent on its player state; it stays at 0 until one is attached (WS-28)"), *GetName());
		return;
	}
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
	const int32 Players = GameState != nullptr ? GameState->PlayerArray.Num() : 1;
	State->HostDeplete(Players, HeroSet->GetBleedoutSeconds());
}

void ADFHeroCharacter::HandleDamaged(AActor* /*Instigator*/, AActor* /*Causer*/, const FGameplayEffectSpec* /*Spec*/, float /*Magnitude*/, float /*OldValue*/, float /*NewValue*/)
{
	DFHeroLife::NoteDamaged(Regen, HeroSet->GetRegenDelay());
}

void ADFHeroCharacter::HandleHeroLifeEvent(EDFHeroLifeEvent Event, UDFHeroStateComponent* /*Reviver*/)
{
	const UDFHeroStateComponent* State = BoundHeroState.Get();
	const float MaxHealth = HealthSet->GetMaxHealth();
	switch (Event)
	{
	case EDFHeroLifeEvent::Revived:
		HealthSet->SetHealth(MaxHealth * (State != nullptr ? State->GetRules().RevivedHpFraction : 0.5f));
		Regen = FDFHeroRegen();
		break;
	case EDFHeroLifeEvent::Respawned:
		HealthSet->SetHealth(MaxHealth);
		Regen = FDFHeroRegen();
		break;
	case EDFHeroLifeEvent::Downed:
	case EDFHeroLifeEvent::SoloDowned:
		break;
	}
}

void ADFHeroCharacter::HandleHeroStateChanged(UDFHeroStateComponent* State)
{
	const EDFHeroLife Life = State != nullptr ? State->GetLife() : EDFHeroLife::Up;
	if (UDFHeroMovementComponent* HeroMove = GetHeroMovement())
	{
		HeroMove->SetHeroLife(Life);
	}
	if (Life != EDFHeroLife::Up && bIsCrouched)
	{
		UnCrouch();
	}
}

void ADFHeroCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	AbilitySystem->RefreshAbilityActorInfo();

	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC == nullptr || DefaultMappingContext == nullptr)
	{
		return;
	}
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}
}

void ADFHeroCharacter::BecomeViewTarget(APlayerController* PC)
{
	Super::BecomeViewTarget(PC);

	if (PC != nullptr && PC->PlayerCameraManager != nullptr)
	{
		PC->PlayerCameraManager->ViewPitchMin = -DFHeroMove::PitchLimitDegrees;
		PC->PlayerCameraManager->ViewPitchMax = DFHeroMove::PitchLimitDegrees;
	}
}

void ADFHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (Input == nullptr)
	{
		UE_LOG(LogDFPlayer, Error, TEXT("%s: the input component is not a UEnhancedInputComponent; the hero binds nothing (C16 is Enhanced Input)"), *GetName());
		return;
	}

	if (MoveAction != nullptr)
	{
		Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADFHeroCharacter::Move);
	}
	if (LookAction != nullptr)
	{
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADFHeroCharacter::Look);
	}
	if (JumpAction != nullptr)
	{
		Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
	if (SprintAction != nullptr)
	{
		Input->BindAction(SprintAction, ETriggerEvent::Started, this, &ADFHeroCharacter::StartSprint);
		Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &ADFHeroCharacter::StopSprint);
	}
	if (CrouchAction != nullptr)
	{
		Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &ADFHeroCharacter::StartCrouch);
		Input->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ADFHeroCharacter::StopCrouch);
	}
	if (AimAction != nullptr)
	{
		Input->BindAction(AimAction, ETriggerEvent::Started, this, &ADFHeroCharacter::StartAim);
		Input->BindAction(AimAction, ETriggerEvent::Completed, this, &ADFHeroCharacter::StopAim);
	}
}

void ADFHeroCharacter::Move(const FInputActionValue& Value)
{
	if (Controller == nullptr)
	{
		return;
	}
	// X strafes, Y walks forward; the CMC clamps a diagonal to length 1, as Godot normalises it.
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotationMatrix Yaw(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f));
	AddMovementInput(Yaw.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(Yaw.GetUnitAxis(EAxis::Y), Axis.X);
}

void ADFHeroCharacter::Look(const FInputActionValue& Value)
{
	// Sensitivity and inversion are IA_Look's modifiers, not code.
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void ADFHeroCharacter::StartSprint()
{
	if (UDFHeroMovementComponent* HeroMove = GetHeroMovement())
	{
		HeroMove->SetWantsToSprint(true);
	}
}

void ADFHeroCharacter::StopSprint()
{
	if (UDFHeroMovementComponent* HeroMove = GetHeroMovement())
	{
		HeroMove->SetWantsToSprint(false);
	}
}

void ADFHeroCharacter::StartCrouch()
{
	Crouch();
}

void ADFHeroCharacter::StopCrouch()
{
	UnCrouch();
}

void ADFHeroCharacter::StartAim()
{
	if (UDFHeroMovementComponent* HeroMove = GetHeroMovement())
	{
		HeroMove->SetWantsToAim(true);
	}
}

void ADFHeroCharacter::StopAim()
{
	if (UDFHeroMovementComponent* HeroMove = GetHeroMovement())
	{
		HeroMove->SetWantsToAim(false);
	}
}
