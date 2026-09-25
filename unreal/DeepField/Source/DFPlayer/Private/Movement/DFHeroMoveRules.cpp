#include "Movement/DFHeroMoveRules.h"

#include "GameFramework/CharacterMovementComponent.h"

bool DFHeroMove::CanSprint(bool bCrouched, bool bAiming)
{
	return !bCrouched && !bAiming;
}

float DFHeroMove::MaxSpeed(const FDFHeroMoveSpeeds& Speeds, bool bCrouched, bool bAiming, bool bWantsSprint)
{
	if (bCrouched && bAiming)
	{
		return FMath::Min(Speeds.Crouch, Speeds.Aim);
	}
	if (bCrouched)
	{
		return Speeds.Crouch;
	}
	if (bAiming)
	{
		return Speeds.Aim;
	}
	return bWantsSprint ? Speeds.Sprint : Speeds.Walk;
}

float DFHeroMove::MaxSpeedForLife(EDFHeroLife Life, float StandingSpeed, float DownedSpeed)
{
	switch (Life)
	{
	case EDFHeroLife::Downed:
		return DownedSpeed;
	case EDFHeroLife::Respawning:
		return 0.f;
	case EDFHeroLife::Up:
		break;
	}
	return StandingSpeed;
}

float DFHeroMove::JumpApexCm(float JumpZ, float GravityZ)
{
	const float G = FMath::Abs(GravityZ);
	return G > KINDA_SMALL_NUMBER ? JumpZ * JumpZ / (2.f * G) : 0.f;
}

uint8 DFHeroMove::PackWants(bool bWantsSprint, bool bWantsAim)
{
	uint8 Flags = 0;
	if (bWantsSprint)
	{
		Flags |= FSavedMove_Character::FLAG_Custom_0;
	}
	if (bWantsAim)
	{
		Flags |= FSavedMove_Character::FLAG_Custom_1;
	}
	return Flags;
}

void DFHeroMove::UnpackWants(uint8 Flags, bool& bOutWantsSprint, bool& bOutWantsAim)
{
	bOutWantsSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bOutWantsAim = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
}
