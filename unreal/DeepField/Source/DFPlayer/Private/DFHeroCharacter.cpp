#include "DFHeroCharacter.h"

#include "Abilities/DFAbilitySystemComponent.h"
#include "Attributes/DFHealthSet.h"
#include "Attributes/DFHeroSet.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DFHeroCollision.h"
#include "DFPlayerModule.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
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
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Movement/DFHeroMovementComponent.h"
#include "Movement/DFHeroMoveRules.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/DFHeroWeaponComponent.h"

namespace DFHeroGun
{
	// The placeholder gun (cm, relative to the camera: +X ahead, +Y right, +Z up), until WS-35's arms and
	// weapon art. The engine's basic shapes are 100 cm across with the pivot at the centre.
	// A half-size gun at half the distance looks the same as a full-size one (perspective), and this one
	// ends about 40 cm ahead of the eye, the capsule's radius, so it hardly pokes through a wall the hero
	// stands against. Full size it would be a 20 cm receiver 45 cm ahead, 18 cm right and 16 cm down,
	// with a 26 cm barrel.
	const FVector HipOffset(22.5f, 9.f, -8.f);
	/** Aiming brings it under the crosshair. */
	const FVector AimOffset(20.f, 0.f, -4.5f);
	const FVector BodyScale(0.10f, 0.02f, 0.03f);           // 10 × 2 × 3 cm
	const FVector BarrelScale(0.012f, 0.012f, 0.13f);       // 1.2 cm across, 13 cm long
	const FVector BarrelOffset(5.f + 6.5f, 0.f, 0.6f);      // from the block's front face
	const FVector MuzzleOffset(5.f + 13.f, 0.f, 0.6f);
	const FLinearColor BodyColour(0.045f, 0.047f, 0.055f);  // gunmetal
	const FLinearColor BarrelColour(0.1f, 0.1f, 0.11f);

	void MakeViewModelPart(UStaticMeshComponent* Mesh)
	{
		// Only the owner sees it (a first-person prop), it casts no shadow, and it never blocks anything:
		// not the hero's own shots, not movement, not another player's trace.
		Mesh->SetOnlyOwnerSee(true);
		Mesh->SetCastShadow(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->bReceivesDecals = false;
	}
}

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

	Weapon = CreateDefaultSubobject<UDFHeroWeaponComponent>(TEXT("Weapon"));

	// The placeholder gun rides on the camera, so it follows the view's pitch and the crouch easing.
	ViewModel = CreateDefaultSubobject<USceneComponent>(TEXT("ViewModel"));
	ViewModel->SetupAttachment(FirstPersonCamera);
	ViewModel->SetRelativeLocation(DFHeroGun::HipOffset);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder"));
	// The basic shapes carry DefaultMaterial, which has no parameters; BasicShapeMaterial has "Color".
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	GunBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunBody"));
	GunBody->SetupAttachment(ViewModel);
	GunBody->SetRelativeScale3D(DFHeroGun::BodyScale);
	GunBarrel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunBarrel"));
	GunBarrel->SetupAttachment(ViewModel);
	GunBarrel->SetRelativeLocation(DFHeroGun::BarrelOffset);
	GunBarrel->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));   // the cylinder's axis (Z) along the view
	GunBarrel->SetRelativeScale3D(DFHeroGun::BarrelScale);
	if (CubeMesh.Succeeded())
	{
		GunBody->SetStaticMesh(CubeMesh.Object);
	}
	if (CylinderMesh.Succeeded())
	{
		GunBarrel->SetStaticMesh(CylinderMesh.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		GunBody->SetMaterial(0, ShapeMaterial.Object);
		GunBarrel->SetMaterial(0, ShapeMaterial.Object);
	}
	DFHeroGun::MakeViewModelPart(GunBody);
	DFHeroGun::MakeViewModelPart(GunBarrel);
	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(ViewModel);
	Muzzle->SetRelativeLocation(DFHeroGun::MuzzleOffset);

	PrimaryActorTick.bCanEverTick = true;   // the host's regen

	// The default input (C16: Content/DF/Core/Input). Hard references, so the cook takes them with the
	// hero; a subclass or a later DA_InputConfig can still replace any of them.
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultContextAsset(TEXT("/Game/DF/Core/Input/IMC_DF_Default"));
	static ConstructorHelpers::FObjectFinder<UInputAction> MoveAsset(TEXT("/Game/DF/Core/Input/IA_Move"));
	static ConstructorHelpers::FObjectFinder<UInputAction> LookAsset(TEXT("/Game/DF/Core/Input/IA_Look"));
	static ConstructorHelpers::FObjectFinder<UInputAction> JumpAsset(TEXT("/Game/DF/Core/Input/IA_Jump"));
	static ConstructorHelpers::FObjectFinder<UInputAction> SprintAsset(TEXT("/Game/DF/Core/Input/IA_Sprint"));
	static ConstructorHelpers::FObjectFinder<UInputAction> CrouchAsset(TEXT("/Game/DF/Core/Input/IA_Crouch"));
	static ConstructorHelpers::FObjectFinder<UInputAction> AimAsset(TEXT("/Game/DF/Core/Input/IA_Aim"));
	static ConstructorHelpers::FObjectFinder<UInputAction> FireAsset(TEXT("/Game/DF/Core/Input/IA_Fire"));
	static ConstructorHelpers::FObjectFinder<UInputAction> ReloadAsset(TEXT("/Game/DF/Core/Input/IA_Reload"));
	DefaultMappingContext = DefaultContextAsset.Object;
	MoveAction = MoveAsset.Object;
	LookAction = LookAsset.Object;
	JumpAction = JumpAsset.Object;
	SprintAction = SprintAsset.Object;
	CrouchAction = CrouchAsset.Object;
	AimAction = AimAsset.Object;
	FireAction = FireAsset.Object;
	ReloadAction = ReloadAsset.Object;
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

UDFHeroWeaponComponent* ADFHeroCharacter::GetWeapon() const
{
	return Weapon;
}

int32 ADFHeroCharacter::GetAmmoInMagazine() const
{
	return Weapon != nullptr ? Weapon->GetAmmoInMagazine() : 0;
}

int32 ADFHeroCharacter::GetMagazineSize() const
{
	return Weapon != nullptr ? Weapon->GetMagazineSize() : 0;
}

bool ADFHeroCharacter::IsReloading() const
{
	return Weapon != nullptr && Weapon->IsReloading();
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
	Weapon->SetMuzzle(Muzzle);
	ApplyGunLook();
}

void ADFHeroCharacter::ApplyGunLook()
{
	auto Tint = [](UStaticMeshComponent* Part, const FLinearColor& Colour)
	{
		if (Part == nullptr || Part->GetStaticMesh() == nullptr)
		{
			return;
		}
		if (UMaterialInstanceDynamic* Material = Part->CreateDynamicMaterialInstance(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Colour);   // BasicShapeMaterial's colour (set on the slot in the constructor)
		}
	};
	Tint(GunBody, DFHeroGun::BodyColour);
	Tint(GunBarrel, DFHeroGun::BarrelColour);
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
	Weapon->SetTriggerHeld(false);   // a trigger held as control left is not held by nobody
	Super::UnPossessed();
}

void ADFHeroCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	BindHeroState();
}

void ADFHeroCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	// The capsule (and the camera on it) just dropped by ScaledHalfHeightAdjust in one frame: hold the
	// eye where it was and let it glide down, rather than snapping.
	CrouchEyeOffsetCm += ScaledHalfHeightAdjust;
}

void ADFHeroCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	CrouchEyeOffsetCm -= ScaledHalfHeightAdjust;
}

void ADFHeroCharacter::UpdateCameraEasing(float DeltaSeconds)
{
	if (FirstPersonCamera == nullptr)
	{
		return;
	}
	CrouchEyeOffsetCm = FMath::FInterpTo(CrouchEyeOffsetCm, 0.f, DeltaSeconds, DFHeroMove::CrouchEyeEaseRate);
	const float EyeAboveCentreCm = DFHeroMove::EyeHeightAboveFeetCm - DFHeroMove::CapsuleHalfHeightCm;
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, EyeAboveCentreCm + CrouchEyeOffsetCm));

	const UDFHeroMovementComponent* HeroMove = GetHeroMovement();
	const bool bAiming = HeroMove != nullptr && HeroMove->WantsToAim();
	const float TargetFov = bAiming ? DFHeroMove::AimFieldOfViewDegrees : DFHeroMove::FieldOfViewDegrees;
	FirstPersonCamera->SetFieldOfView(FMath::FInterpTo(FirstPersonCamera->FieldOfView, TargetFov, DeltaSeconds, DFHeroMove::AimFovEaseRate));

	// The placeholder gun comes under the crosshair while aiming, at the field of view's pace.
	if (ViewModel != nullptr)
	{
		const FVector TargetGun = bAiming ? DFHeroGun::AimOffset : DFHeroGun::HipOffset;
		ViewModel->SetRelativeLocation(FMath::VInterpTo(ViewModel->GetRelativeLocation(), TargetGun, DeltaSeconds, DFHeroMove::AimFovEaseRate));
	}
}

void ADFHeroCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsLocallyControlled())
	{
		UpdateCameraEasing(DeltaSeconds);
	}

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
	if (FireAction != nullptr)
	{
		Input->BindAction(FireAction, ETriggerEvent::Started, this, &ADFHeroCharacter::StartFire);
		Input->BindAction(FireAction, ETriggerEvent::Completed, this, &ADFHeroCharacter::StopFire);
		Input->BindAction(FireAction, ETriggerEvent::Canceled, this, &ADFHeroCharacter::StopFire);
	}
	if (ReloadAction != nullptr)
	{
		Input->BindAction(ReloadAction, ETriggerEvent::Started, this, &ADFHeroCharacter::Reload);
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

void ADFHeroCharacter::StartFire()
{
	Weapon->SetTriggerHeld(true);
}

void ADFHeroCharacter::StopFire()
{
	Weapon->SetTriggerHeld(false);
}

void ADFHeroCharacter::Reload()
{
	Weapon->RequestReload();
}
