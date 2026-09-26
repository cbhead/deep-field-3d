#include "Boss/DFBoss.h"

#include "DFGameplayTags.h"
#include "Damage/DFDamageMath.h"

namespace DFBossPrivate
{
	/** A plate is armour by definition. FDFDamageInput::IsArmored() reads RowFlatArmor, so a plate hit sets it
	 *  positive (hollow point gets no unarmoured bonus against a plate) while FlatArmor stays 0 (nothing is
	 *  subtracted from the hit). */
	constexpr float PlateRowArmor = 1.f;

	/** No real frame produces this many boss events; a step that would is a bad DeltaSeconds, not a fight. */
	constexpr int32 MaxEventsPerAdvance = 4096;

	// Written as the positive test so that NaN fails every rule.
	bool IsPositive(float V) { return V > 0.f; }
	bool IsNonNegative(float V) { return V >= 0.f; }

	void Emit(TArray<FDFBossEvent>& OutEvents, EDFBossEventKind Kind, int32 Phase, float SecondsIntoStep = 0.f)
	{
		FDFBossEvent& Event = OutEvents.AddDefaulted_GetRef();
		Event.Kind = Kind;
		Event.Phase = Phase;
		Event.SecondsIntoStep = SecondsIntoStep;
	}

	/** Enter every phase the body's hp has fallen to, in order. A phase change ends the old phase's
	 *  telegraph (the attack it warned of belongs to a phase that is over), restarts both clocks, and — when
	 *  the new phase holds no plates — sheds every plate still on. */
	void EnterPhasesReached(FDFBossState& State, const FDFBossTables& Tables, TArray<FDFBossEvent>& OutEvents)
	{
		const float Frac = State.HpFrac();
		while (Tables.Phases.IsValidIndex(State.PhaseIndex + 1) && Frac <= Tables.Phases[State.PhaseIndex + 1].HpFrom)
		{
			if (State.bTelegraphing)
			{
				Emit(OutEvents, EDFBossEventKind::AttackCancelled, State.PhaseIndex);
				OutEvents.Last().Detail = Tables.Phases[State.PhaseIndex].Attack.Id;
			}
			++State.PhaseIndex;
			State.AttackClock = 0.0;
			State.bTelegraphing = false;
			State.VentClock = 0.0;
			Emit(OutEvents, EDFBossEventKind::PhaseEntered, State.PhaseIndex);

			if (!Tables.Phases[State.PhaseIndex].bPlatesHeld)
			{
				const int32 Held = State.PlatesHeld();
				if (Held > 0)
				{
					for (float& Plate : State.PlateHp)
					{
						Plate = 0.f;
					}
					Emit(OutEvents, EDFBossEventKind::PlatesDropped, State.PhaseIndex);
					OutEvents.Last().Count = Held;
				}
			}
		}
	}
}

bool FDFBossTables::Validate(FString& OutError) const
{
	using namespace DFBossPrivate;

	const int32 N = Phases.Num();
	if (N < 1 || N > MaxPhases)
	{
		OutError = FString::Printf(TEXT("boss: %d phases (1..%d)"), N, MaxPhases);
		return false;
	}
	if (PhaseIds.Num() != N)
	{
		OutError = FString::Printf(TEXT("boss: %d phase ids for %d phases"), PhaseIds.Num(), N);
		return false;
	}

	TSet<FName> Seen;
	for (int32 i = 0; i < N; ++i)
	{
		const FDFBossPhaseRow& P = Phases[i];
		const FString Where = FString::Printf(TEXT("boss phase %d (%s)"), i, *PhaseIds[i].ToString());
		auto Fail = [&OutError, &Where](const FString& What)
		{
			OutError = Where + TEXT(": ") + What;
			return false;
		};

		bool bDuplicate = false;
		Seen.Add(PhaseIds[i], &bDuplicate);
		if (PhaseIds[i].IsNone())
		{
			return Fail(TEXT("no id"));
		}
		if (bDuplicate)
		{
			return Fail(TEXT("duplicate id"));
		}

		// The hp bands: downward, inside [0, 1], contiguous, from 1 to 0.
		if (!(P.HpFrom > P.HpTo && P.HpTo >= 0.f && P.HpFrom <= 1.f))
		{
			return Fail(FString::Printf(TEXT("HpFrom %g .. HpTo %g must run downward inside [0, 1]"), P.HpFrom, P.HpTo));
		}
		if (i == 0 && P.HpFrom != 1.f)
		{
			return Fail(FString::Printf(TEXT("the first phase must start at full hp (HpFrom %g)"), P.HpFrom));
		}
		if (i == N - 1 && P.HpTo != 0.f)
		{
			return Fail(FString::Printf(TEXT("the last phase must end at 0 (HpTo %g)"), P.HpTo));
		}
		if (i + 1 < N && P.HpTo != Phases[i + 1].HpFrom)
		{
			return Fail(FString::Printf(TEXT("ends at %g but the next phase starts at %g"), P.HpTo, Phases[i + 1].HpFrom));
		}

		// The body this phase.
		if (!(P.FrontArcDeg >= 0.f && P.FrontArcDeg <= 360.f))
		{
			return Fail(FString::Printf(TEXT("FrontArcDeg %g outside [0, 360]"), P.FrontArcDeg));
		}
		if (!IsPositive(P.FrontArcFactor) || !IsPositive(P.SpeedFactor) || !IsPositive(P.WeakPointFactor))
		{
			return Fail(FString::Printf(TEXT("FrontArcFactor %g, SpeedFactor %g and WeakPointFactor %g must be positive"),
				P.FrontArcFactor, P.SpeedFactor, P.WeakPointFactor));
		}
		if (!IsNonNegative(P.FlatArmor) || !IsNonNegative(P.Siege.Dps) || !IsNonNegative(P.Siege.Reach))
		{
			return Fail(FString::Printf(TEXT("FlatArmor %g, Siege.Dps %g and Siege.Reach %g must be >= 0"), P.FlatArmor, P.Siege.Dps, P.Siege.Reach));
		}

		// The attack. Interval 0 is "none"; anything else needs an id and a telegraph that fits.
		const FDFBossAttackRow& A = P.Attack;
		if (!IsNonNegative(A.IntervalSeconds))
		{
			return Fail(FString::Printf(TEXT("Attack.IntervalSeconds %g must be >= 0"), A.IntervalSeconds));
		}
		if (A.IntervalSeconds > 0.f)
		{
			if (A.Id.IsNone())
			{
				return Fail(TEXT("an attack with no id"));
			}
			if (!(A.TelegraphSeconds >= 0.f && A.TelegraphSeconds <= A.IntervalSeconds))
			{
				return Fail(FString::Printf(TEXT("%s: telegraph %g s does not fit in its %g s interval"), *A.Id.ToString(), A.TelegraphSeconds, A.IntervalSeconds));
			}
			if (!IsNonNegative(A.Radius) || !IsNonNegative(A.Damage) || !IsNonNegative(A.KnockbackMeters) || !IsNonNegative(A.StaggerSeconds))
			{
				return Fail(FString::Printf(TEXT("%s: Radius, Damage, KnockbackMeters and StaggerSeconds must be >= 0"), *A.Id.ToString()));
			}
		}

		// The vent. Count 0 is "none"; a cap below one vent's worth is a typo, not a design.
		const FDFBossSpawnRow& S = P.Spawns;
		if (S.Count < 0)
		{
			return Fail(FString::Printf(TEXT("Spawns.Count %d must be >= 0"), S.Count));
		}
		if (S.Count > 0)
		{
			if (S.EnemyId.IsNone())
			{
				return Fail(TEXT("a vent with no enemy id"));
			}
			if (!IsPositive(S.IntervalSeconds))
			{
				return Fail(FString::Printf(TEXT("Spawns.IntervalSeconds %g must be positive"), S.IntervalSeconds));
			}
			if (S.MaxAlive < S.Count)
			{
				return Fail(FString::Printf(TEXT("Spawns.MaxAlive %d is less than one vent (%d)"), S.MaxAlive, S.Count));
			}
		}

		// Plates that have dropped do not come back.
		if (P.bPlatesHeld && i > 0 && !Phases[i - 1].bPlatesHeld)
		{
			return Fail(TEXT("holds plates after a phase that shed them"));
		}
	}

	if (!IsPositive(Body.RawHp))
	{
		OutError = FString::Printf(TEXT("boss: RawHp %g must be positive"), Body.RawHp);
		return false;
	}
	if (Body.PlateCount < 0 || Body.PlateCount > MaxPlates)
	{
		OutError = FString::Printf(TEXT("boss: %d plates (0..%d)"), Body.PlateCount, MaxPlates);
		return false;
	}
	if (Body.PlateCount > 0 && (!IsPositive(Body.PlateHp) || !IsPositive(Body.PlateShredFactor)))
	{
		OutError = FString::Printf(TEXT("boss: PlateHp %g and PlateShredFactor %g must be positive"), Body.PlateHp, Body.PlateShredFactor);
		return false;
	}
	return true;
}

int32 FDFBossState::PlatesHeld() const
{
	int32 Held = 0;
	for (const float Plate : PlateHp)
	{
		Held += Plate > 0.f ? 1 : 0;
	}
	return Held;
}

float FDFBossState::HpFrac() const
{
	return MaxHp > 0.f ? Hp / MaxHp : 0.f;
}

void FDFBoss::Begin(FDFBossState& State, const FDFBossTables& Tables, float HpScale)
{
	State = FDFBossState();
	if (!ensureMsgf(Tables.Phases.Num() > 0 && HpScale > 0.f, TEXT("FDFBoss::Begin: %d phases, HpScale %g"), Tables.Phases.Num(), HpScale))
	{
		return;
	}
	State.MaxHp = Tables.Body.RawHp * HpScale;
	State.Hp = State.MaxHp;
	State.PlateMaxHp = Tables.Body.PlateHp * HpScale;
	State.PlateHp.Init(Tables.Phases[0].bPlatesHeld ? State.PlateMaxHp : 0.f, Tables.Body.PlateCount);
	State.PhaseIndex = 0;
}

const FDFBossPhaseRow* FDFBoss::CurrentPhase(const FDFBossState& State, const FDFBossTables& Tables)
{
	return Tables.Phases.IsValidIndex(State.PhaseIndex) ? &Tables.Phases[State.PhaseIndex] : nullptr;
}

FGameplayTag FDFBoss::PhaseTag(const FDFBossTables& Tables, int32 PhaseIndex)
{
	return Tables.PhaseIds.IsValidIndex(PhaseIndex) ? DFTags::ForContentId(TEXT("DF.Boss.Phase"), Tables.PhaseIds[PhaseIndex]) : FGameplayTag();
}

FDFBossHitTarget FDFBoss::ResolveHit(const FDFBossState& State, const FDFBossTables& Tables, EDFBossZone Zone, int32 Index)
{
	FDFBossHitTarget Out;
	if (Zone == EDFBossZone::Body || !State.PlateHp.IsValidIndex(Index))
	{
		return Out;
	}
	if (State.PlateHp[Index] > 0.f)
	{
		Out.PlateIndex = Index;   // the plate, or the vent it still covers
		return Out;
	}
	const FDFBossPhaseRow* Phase = CurrentPhase(State, Tables);
	Out.WeakPointFactor = Phase ? Phase->WeakPointFactor : 1.f;
	return Out;
}

void FDFBoss::ProfileHit(const FDFBossState& State, const FDFBossTables& Tables, const FDFBossHitTarget& Target, FDFDamageInput& InOut)
{
	if (Target.IsPlate())
	{
		InOut.FrontArmorArcDegrees = 0.f;
		InOut.FrontArmorFactor = 1.f;
		InOut.RearWeakFactor = 1.f;
		InOut.FlatArmor = 0.f;
		InOut.RowFlatArmor = DFBossPrivate::PlateRowArmor;
		InOut.WeakPointFactor = 1.f;
		return;
	}
	const FDFArmorProfile Arc = ArmorProfile(State, Tables);
	InOut.FrontArmorArcDegrees = Arc.FrontArmorArcDegrees;
	InOut.FrontArmorFactor = Arc.FrontArmorFactor;
	InOut.RearWeakFactor = Arc.RearWeakFactor;
	InOut.RowFlatArmor = FlatArmor(State, Tables);
	InOut.WeakPointFactor = Target.WeakPointFactor;
}

FDFArmorProfile FDFBoss::ArmorProfile(const FDFBossState& State, const FDFBossTables& Tables)
{
	FDFArmorProfile Out;
	if (const FDFBossPhaseRow* Phase = CurrentPhase(State, Tables))
	{
		Out.FrontArmorArcDegrees = Phase->FrontArcDeg;
		Out.FrontArmorFactor = Phase->FrontArcFactor;
	}
	return Out;
}

float FDFBoss::FlatArmor(const FDFBossState& State, const FDFBossTables& Tables)
{
	const FDFBossPhaseRow* Phase = CurrentPhase(State, Tables);
	return Phase ? Phase->FlatArmor : 0.f;
}

float FDFBoss::SpeedFactor(const FDFBossState& State, const FDFBossTables& Tables)
{
	const FDFBossPhaseRow* Phase = CurrentPhase(State, Tables);
	return Phase ? Phase->SpeedFactor : 1.f;
}

float FDFBoss::ApplyDamage(FDFBossState& State, const FDFBossTables& Tables, const FDFBossHitTarget& Target, float Amount,
	bool bTargetShredded, TArray<FDFBossEvent>& OutEvents)
{
	using namespace DFBossPrivate;

	if (!State.IsActive() || !(Amount > 0.f))
	{
		return 0.f;
	}

	if (Target.IsPlate())
	{
		if (!State.PlateHp.IsValidIndex(Target.PlateIndex) || State.PlateHp[Target.PlateIndex] <= 0.f)
		{
			return 0.f;
		}
		float& Plate = State.PlateHp[Target.PlateIndex];
		const float Dealt = Amount * (bTargetShredded ? Tables.Body.PlateShredFactor : 1.f);
		const float Taken = FMath::Min(Plate, Dealt);
		Plate -= Taken;
		if (Plate <= 0.f)
		{
			Plate = 0.f;
			Emit(OutEvents, EDFBossEventKind::PlateRemoved, State.PhaseIndex);
			OutEvents.Last().Index = Target.PlateIndex;
		}
		return Taken;
	}

	const float Taken = FMath::Min(State.Hp, Amount);
	State.Hp -= Taken;
	EnterPhasesReached(State, Tables, OutEvents);
	if (State.Hp <= 0.f)
	{
		// A warning on screen for an attack that will never land ends here, as it would on a phase change.
		if (State.bTelegraphing)
		{
			Emit(OutEvents, EDFBossEventKind::AttackCancelled, State.PhaseIndex);
			OutEvents.Last().Detail = Tables.Phases[State.PhaseIndex].Attack.Id;
			State.bTelegraphing = false;
		}
		State.Hp = 0.f;
		State.bDead = true;
		Emit(OutEvents, EDFBossEventKind::Died, State.PhaseIndex);
	}
	return Taken;
}

float FDFBoss::Heal(FDFBossState& State, float Amount)
{
	if (!State.IsActive() || !(Amount > 0.f))
	{
		return 0.f;
	}
	const float Before = State.Hp;
	State.Hp = FMath::Min(State.MaxHp, State.Hp + Amount);
	return State.Hp - Before;
}

void FDFBoss::Advance(FDFBossState& State, const FDFBossTables& Tables, float DeltaSeconds, int32 BroodAlive, TArray<FDFBossEvent>& OutEvents)
{
	using namespace DFBossPrivate;

	const FDFBossPhaseRow* Phase = CurrentPhase(State, Tables);
	if (!State.IsActive() || !Phase || !(DeltaSeconds > 0.f))
	{
		return;
	}

	// Nothing inside a step changes the phase (only damage does), so its rows hold for the whole step.
	const FDFBossAttackRow& Attack = Phase->Attack;
	const FDFBossSpawnRow& Vent = Phase->Spawns;
	const bool bAttacks = Attack.IntervalSeconds > 0.f;
	const bool bVents = Vent.Count > 0 && Vent.IntervalSeconds > 0.f;
	const double TelegraphAt = static_cast<double>(Attack.IntervalSeconds) - Attack.TelegraphSeconds;
	const double Step = DeltaSeconds;
	const int32 AliveBefore = FMath::Max(0, BroodAlive);
	int32 VentedThisStep = 0;
	double Now = 0.0;

	for (int32 Guard = 0; Guard < MaxEventsPerAdvance; ++Guard)
	{
		constexpr double Never = TNumericLimits<double>::Max();
		const double UntilAttack = bAttacks
			? FMath::Max(0.0, (State.bTelegraphing ? static_cast<double>(Attack.IntervalSeconds) : TelegraphAt) - State.AttackClock)
			: Never;
		const double UntilVent = bVents ? FMath::Max(0.0, static_cast<double>(Vent.IntervalSeconds) - State.VentClock) : Never;
		const double Until = FMath::Min(UntilAttack, UntilVent);

		if (Now + Until > Step)
		{
			const double Rest = Step - Now;
			State.AttackClock += Rest;
			State.VentClock += Rest;
			return;
		}
		Now += Until;
		State.AttackClock += Until;
		State.VentClock += Until;

		// At a tie the attack goes first: a telegraph and a landing due together come out in that order,
		// and a vent due at the same instant follows them.
		if (UntilAttack <= UntilVent)
		{
			const EDFBossEventKind Kind = State.bTelegraphing ? EDFBossEventKind::AttackLanded : EDFBossEventKind::AttackTelegraphed;
			State.AttackClock = State.bTelegraphing ? 0.0 : TelegraphAt;
			State.bTelegraphing = !State.bTelegraphing;
			Emit(OutEvents, Kind, State.PhaseIndex, static_cast<float>(Now));
			OutEvents.Last().Detail = Attack.Id;
		}
		else
		{
			State.VentClock = 0.0;
			const int32 Room = Vent.MaxAlive - (AliveBefore + VentedThisStep);
			const int32 Count = FMath::Clamp(Room, 0, Vent.Count);
			if (Count > 0)
			{
				VentedThisStep += Count;
				Emit(OutEvents, EDFBossEventKind::BroodVented, State.PhaseIndex, static_cast<float>(Now));
				OutEvents.Last().Count = Count;
				OutEvents.Last().Detail = Vent.EnemyId;
			}
		}
	}
	ensureMsgf(false, TEXT("FDFBoss::Advance: %d events in one %g s step; stopped"), MaxEventsPerAdvance, DeltaSeconds);
}

float FDFBossKillWindow::WalkFraction(float WalkedMeters, float RouteMeters)
{
	if (!(RouteMeters > 0.f))
	{
		return 1.f;
	}
	const float Fraction = WalkedMeters / RouteMeters;
	if (FMath::IsNaN(Fraction))
	{
		return 1.f;   // a broken measurement fails the gate rather than passing it
	}
	return FMath::Clamp(Fraction, 0.f, 1.f);
}
