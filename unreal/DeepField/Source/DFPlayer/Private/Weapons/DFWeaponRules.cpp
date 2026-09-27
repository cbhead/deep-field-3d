#include "Weapons/DFWeaponRules.h"

#include "Content/DFContentRows.h"

namespace DFWeapon
{
	FDFWeaponRules RulesFromRow(const FDFWeaponRow& Row)
	{
		FDFWeaponRules Rules;
		Rules.Damage = Row.Damage;
		Rules.ShotsPerSecond = Row.ShotsPerSecond;
		Rules.RangeCm = Row.RangeMeters * 100.f;
		Rules.MagazineSize = Row.MagazineSize;
		Rules.ReloadSeconds = Row.ReloadSeconds;
		Rules.bAutomatic = Row.bAutomatic;
		return Rules;
	}

	FDFWeaponState Loaded(const FDFWeaponRules& Rules)
	{
		FDFWeaponState State;
		State.Rounds = FMath::Max(0, Rules.MagazineSize);
		return State;
	}

	bool IsReloading(const FDFWeaponState& State)
	{
		return State.ReloadLeft > 0.f;
	}

	float ReloadFraction(const FDFWeaponState& State, const FDFWeaponRules& Rules)
	{
		if (!IsReloading(State) || Rules.ReloadSeconds <= 0.f)
		{
			return 0.f;
		}
		return FMath::Clamp(1.f - State.ReloadLeft / Rules.ReloadSeconds, 0.f, 1.f);
	}

	float ShotInterval(const FDFWeaponRules& Rules)
	{
		return Rules.ShotsPerSecond > 0.f ? 1.f / Rules.ShotsPerSecond : TNumericLimits<float>::Max();
	}

	bool BeginReload(FDFWeaponState& State, const FDFWeaponRules& Rules)
	{
		if (IsReloading(State) || State.Rounds >= Rules.MagazineSize)
		{
			return false;   // Step.cs BeginReload: already reloading, or already full
		}
		if (Rules.ReloadSeconds <= 0.f)
		{
			State.Rounds = Rules.MagazineSize;   // a gun with no reload time refills at once
			return true;
		}
		State.ReloadLeft = Rules.ReloadSeconds;
		return true;
	}

	bool AdvanceReload(FDFWeaponState& State, const FDFWeaponRules& Rules, float DeltaSeconds)
	{
		if (!IsReloading(State))
		{
			return false;
		}
		State.ReloadLeft = FMath::Max(0.f, State.ReloadLeft - DeltaSeconds);
		if (State.ReloadLeft > 0.f)
		{
			return false;
		}
		State.Rounds = Rules.MagazineSize;
		return true;
	}

	FDFWeaponStep Step(FDFWeaponState& State, const FDFWeaponRules& Rules, float DeltaSeconds, bool bTriggerHeld)
	{
		FDFWeaponStep Result;
		Result.bReloaded = AdvanceReload(State, Rules, DeltaSeconds);
		State.Cooldown -= DeltaSeconds;

		// Player.cs: `weapon.Automatic ? held : (held && !_triggerHeld)`.
		const bool bWants = bTriggerHeld && (Rules.bAutomatic || !State.bTriggerWasHeld);
		State.bTriggerWasHeld = bTriggerHeld;

		// Step.cs ApplyPlayerHit's order: the cooldown first, then busy hands, then the dry pull.
		if (bWants && State.Cooldown <= 0.f && !IsReloading(State))
		{
			if (State.Rounds <= 0)
			{
				Result.bReloadStarted = BeginReload(State, Rules);
			}
			else
			{
				--State.Rounds;
				State.Cooldown += ShotInterval(Rules);
				Result.bFired = true;
			}
		}
		// A cooldown spent while nothing fired is not credit for later shots.
		State.Cooldown = FMath::Max(0.f, State.Cooldown);
		return Result;
	}

	FDFShotBudget FullBudget()
	{
		FDFShotBudget Budget;
		Budget.Shots = BurstShots;
		return Budget;
	}

	void RefillBudget(FDFShotBudget& Budget, const FDFWeaponRules& Rules, float DeltaSeconds)
	{
		Budget.Shots = FMath::Min(BurstShots, Budget.Shots + FMath::Max(0.f, DeltaSeconds) * Rules.ShotsPerSecond * RateSlack);
	}

	EDFShotVerdict HostAcceptShot(FDFWeaponState& State, FDFShotBudget& Budget, const FDFWeaponRules& Rules)
	{
		if (IsReloading(State))
		{
			if (State.ReloadLeft > ReloadGraceSeconds)
			{
				return EDFShotVerdict::Reloading;
			}
			AdvanceReload(State, Rules, State.ReloadLeft);   // the owner's reload is already over
		}
		if (State.Rounds <= 0)
		{
			return EDFShotVerdict::Empty;
		}
		if (Budget.Shots < 1.f)
		{
			return EDFShotVerdict::TooFast;
		}
		Budget.Shots -= 1.f;
		--State.Rounds;
		return EDFShotVerdict::Accepted;
	}

	bool IsOriginPlausible(const FVector& ReportedOrigin, const FVector& HostEye)
	{
		return !ReportedOrigin.ContainsNaN() && FVector::DistSquared(ReportedOrigin, HostEye) <= FMath::Square(MaxOriginErrorCm);
	}
}
