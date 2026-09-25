#include "Hero/DFHeroLife.h"

bool DFHeroLife::IsUp(const FDFHeroLifeState& State)
{
	return State.Life == EDFHeroLife::Up;
}

bool DFHeroLife::IsBleeding(const FDFHeroLifeState& State)
{
	return State.Life == EDFHeroLife::Downed && State.BleedoutLeft > 0.f;
}

EDFHeroDown DFHeroLife::Deplete(FDFHeroLifeState& State, const FDFHeroLifeRules& Rules, int32 ConnectedPlayers)
{
	if (State.Life != EDFHeroLife::Up)
	{
		return EDFHeroDown::Ignored;
	}
	State.ReviveProgress = 0.f;
	if (ConnectedPlayers <= 1)
	{
		State.Life = EDFHeroLife::Respawning;
		State.RespawnLeft = Rules.SoloRespawnSeconds;
		State.BleedoutLeft = 0.f;
		return EDFHeroDown::SoloRespawn;
	}
	State.Life = EDFHeroLife::Downed;
	State.BleedoutLeft = Rules.BleedoutSeconds;
	State.RespawnLeft = 0.f;
	return EDFHeroDown::Downed;
}

DFHeroLife::FTickResult DFHeroLife::Tick(FDFHeroLifeState& State, const FDFHeroLifeRules& Rules, float DeltaSeconds, bool bReviveHeld)
{
	FTickResult Result;
	switch (State.Life)
	{
	case EDFHeroLife::Respawning:
		State.RespawnLeft -= DeltaSeconds;
		if (State.RespawnLeft <= 0.f)
		{
			Respawn(State);
			Result.bRespawned = true;
		}
		break;

	case EDFHeroLife::Downed:
		State.BleedoutLeft = FMath::Max(0.f, State.BleedoutLeft - DeltaSeconds);
		if (bReviveHeld)
		{
			State.ReviveProgress += DeltaSeconds;
			if (State.ReviveProgress >= Rules.ReviveSeconds)
			{
				Respawn(State);
				Result.bRevived = true;
			}
		}
		else
		{
			State.ReviveProgress = FMath::Max(0.f, State.ReviveProgress - DeltaSeconds * Rules.ReviveDecayRate);
		}
		break;

	case EDFHeroLife::Up:
		break;
	}
	return Result;
}

bool DFHeroLife::InReviveRange(float DistanceCm, const FDFHeroLifeRules& Rules)
{
	return DistanceCm <= Rules.ReviveRangeCm;
}

bool DFHeroLife::ShouldRespawnAtWaveBoundary(const FDFHeroLifeState& State)
{
	return State.Life == EDFHeroLife::Downed && State.BleedoutLeft <= 0.f;
}

void DFHeroLife::Respawn(FDFHeroLifeState& State)
{
	State.Life = EDFHeroLife::Up;
	State.BleedoutLeft = 0.f;
	State.RespawnLeft = 0.f;
	State.ReviveProgress = 0.f;
}
