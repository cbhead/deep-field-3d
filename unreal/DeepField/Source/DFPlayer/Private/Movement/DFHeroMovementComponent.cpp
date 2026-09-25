#include "Movement/DFHeroMovementComponent.h"

#include "GameFramework/Character.h"

/** A saved move that also remembers the hero's sprint and ADS wants, so a replay moves at the same speed. */
class FDFSavedMove_Hero : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	virtual void Clear() override
	{
		Super::Clear();
		bSavedWantsToSprint = false;
		bSavedWantsToAim = false;
	}

	virtual uint8 GetCompressedFlags() const override
	{
		return static_cast<uint8>(Super::GetCompressedFlags() | DFHeroMove::PackWants(bSavedWantsToSprint, bSavedWantsToAim));
	}

	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override
	{
		const FDFSavedMove_Hero* Other = static_cast<const FDFSavedMove_Hero*>(NewMove.Get());
		if (bSavedWantsToSprint != Other->bSavedWantsToSprint || bSavedWantsToAim != Other->bSavedWantsToAim)
		{
			return false;
		}
		return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
	}

	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override
	{
		Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
		if (const UDFHeroMovementComponent* Move = Cast<UDFHeroMovementComponent>(C->GetCharacterMovement()))
		{
			bSavedWantsToSprint = Move->bWantsToSprint;
			bSavedWantsToAim = Move->bWantsToAim;
		}
	}

	virtual void PrepMoveFor(ACharacter* C) override
	{
		Super::PrepMoveFor(C);
		if (UDFHeroMovementComponent* Move = Cast<UDFHeroMovementComponent>(C->GetCharacterMovement()))
		{
			Move->bWantsToSprint = bSavedWantsToSprint;
			Move->bWantsToAim = bSavedWantsToAim;
		}
	}

private:
	bool bSavedWantsToSprint = false;
	bool bSavedWantsToAim = false;
};

/** The client's move buffer, allocating FDFSavedMove_Hero instead of the engine's saved move. */
class FDFNetworkPredictionData_Client_Hero : public FNetworkPredictionData_Client_Character
{
public:
	explicit FDFNetworkPredictionData_Client_Hero(const UCharacterMovementComponent& ClientMovement)
		: FNetworkPredictionData_Client_Character(ClientMovement)
	{
	}

	virtual FSavedMovePtr AllocateNewMove() override
	{
		return FSavedMovePtr(new FDFSavedMove_Hero());
	}
};

UDFHeroMovementComponent::UDFHeroMovementComponent()
{
	MaxWalkSpeed = DFHeroMove::WalkCmPerSec;
	MaxWalkSpeedCrouched = DFHeroMove::CrouchCmPerSec;
	JumpZVelocity = DFHeroMove::JumpZCmPerSec;
	GravityScale = 1.f;   // with DefaultGravityZ -980 this is Godot's 9.8 m/s²

	// Godot's direct velocity writes: near-instant start, stop and turn, full control in the air.
	MaxAcceleration = DFHeroMove::ParityAccelerationCmPerSec2;
	BrakingDecelerationWalking = DFHeroMove::ParityAccelerationCmPerSec2;
	BrakingDecelerationFalling = DFHeroMove::ParityAccelerationCmPerSec2;
	AirControl = 1.f;

	SetWalkableFloorAngle(DFHeroMove::WalkableFloorDegrees);
	bOrientRotationToMovement = false;   // first person: the controller's yaw turns the body
	GetNavAgentPropertiesRef().bCanCrouch = true;
}

bool UDFHeroMovementComponent::IsSprinting() const
{
	return HeroLife == EDFHeroLife::Up && bWantsToSprint && DFHeroMove::CanSprint(IsCrouching(), bWantsToAim);
}

FDFHeroMoveSpeeds UDFHeroMovementComponent::GetHeroSpeeds() const
{
	FDFHeroMoveSpeeds Speeds;
	Speeds.Walk = MaxWalkSpeed;
	Speeds.Sprint = MaxSprintSpeed;
	Speeds.Crouch = MaxWalkSpeedCrouched;
	Speeds.Aim = MaxAimSpeed;
	return Speeds;
}

float UDFHeroMovementComponent::GetMaxSpeed() const
{
	switch (MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
	case MOVE_Falling:
		return DFHeroMove::MaxSpeedForLife(HeroLife,
			DFHeroMove::MaxSpeed(GetHeroSpeeds(), IsCrouching(), bWantsToAim, bWantsToSprint), MaxDownedSpeed);
	default:
		return Super::GetMaxSpeed();
	}
}

bool UDFHeroMovementComponent::CanAttemptJump() const
{
	return HeroLife == EDFHeroLife::Up && Super::CanAttemptJump();
}

bool UDFHeroMovementComponent::CanCrouchInCurrentState() const
{
	return HeroLife == EDFHeroLife::Up && Super::CanCrouchInCurrentState();
}

FNetworkPredictionData_Client* UDFHeroMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UDFHeroMovementComponent* MutableThis = const_cast<UDFHeroMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FDFNetworkPredictionData_Client_Hero(*this);
	}
	return ClientPredictionData;
}

void UDFHeroMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	DFHeroMove::UnpackWants(Flags, bWantsToSprint, bWantsToAim);
}
