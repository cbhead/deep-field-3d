#include "DFHeroCharacter.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "DFHeroCollision.h"
#include "DFPlayerModule.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
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
}

UDFHeroMovementComponent* ADFHeroCharacter::GetHeroMovement() const
{
	return Cast<UDFHeroMovementComponent>(GetCharacterMovement());
}

UCameraComponent* ADFHeroCharacter::GetFirstPersonCamera() const
{
	return FirstPersonCamera;
}

void ADFHeroCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

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
