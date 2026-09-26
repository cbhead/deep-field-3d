#pragma once

#include "Content/DFContentRows.h"
#include "CoreMinimal.h"
#include "Status/DFReactionResolver.h"
#include "Status/DFStatusResolver.h"

// Appendix A1 / Statuses.cs rows as literals, so DF.Unit.* never depends on WS-01's tables.
namespace DFTestRows
{
	inline FDFStatusRow Status(EDFStatusChannel Channel, float Duration, float SpeedFactor = 1.f, float Dps = 0.f, float TakenFactor = 1.f,
		float ArmorDelta = 0.f, bool bHardControl = false, bool bIgnoresArmor = false, bool bIgnoresShield = false, float PullSpeed = 0.f, float CcFill = 1.f)
	{
		FDFStatusRow Row;
		Row.Channel = Channel;
		Row.MaxDurationSeconds = Duration;
		Row.SpeedFactor = SpeedFactor;
		Row.DamagePerSecond = Dps;
		Row.DamageTakenFactor = TakenFactor;
		Row.ArmorDelta = ArmorDelta;
		Row.bHardControl = bHardControl;
		Row.bIgnoresArmor = bIgnoresArmor;
		Row.bIgnoresShield = bIgnoresShield;
		Row.PullSpeed = PullSpeed;
		Row.CcResistFill = CcFill;
		return Row;
	}

	inline FDFStatusRow Chill()   { return Status(EDFStatusChannel::Movement, 1.5f, 0.65f); }
	inline FDFStatusRow Burn()    { return Status(EDFStatusChannel::Thermal, 3.f, 1.f, 6.f); }
	inline FDFStatusRow Poison()  { return Status(EDFStatusChannel::Toxin, 6.f, 1.f, 3.f, 1.f, 0.f, false, true, true); }
	inline FDFStatusRow Shred()   { return Status(EDFStatusChannel::Defense, 4.f, 1.f, 0.f, 1.f, -2.f); }
	inline FDFStatusRow Mark()    { return Status(EDFStatusChannel::Vulnerability, 4.f, 1.f, 0.f, 1.25f); }
	inline FDFStatusRow Shock()   { return Status(EDFStatusChannel::Control, 0.25f, 0.f, 0.f, 1.f, 0.f, true); }
	inline FDFStatusRow Freeze()  { return Status(EDFStatusChannel::Control, 1.2f, 0.f, 0.f, 1.f, 0.f, true); }
	inline FDFStatusRow Reveal()  { return Status(EDFStatusChannel::Detection, 3.f); }
	/** B§1.6 rubble: Movement 0.8x, 3 s — weaker than chill. */
	inline FDFStatusRow Rubble()  { return Status(EDFStatusChannel::Movement, 3.f, 0.8f); }
	/** B§1.6 hero-only stagger: Control, 0.25 s, hard control; on a hero it never fills the gauge. */
	inline FDFStatusRow Stagger() { return Status(EDFStatusChannel::Control, 0.25f, 0.f, 0.f, 1.f, 0.f, true); }
	/** B§1.6 magnetize: Tether, pull 4 m/s, 1.5 s, fills cc-resist at 0.5x. */
	inline FDFStatusRow Magnetize() { return Status(EDFStatusChannel::Tether, 1.5f, 1.f, 0.f, 1.f, 0.f, false, false, false, 4.f, 0.5f); }

	inline FDFReactionRow Reaction(FName A, FName B, float Burst, FName Emit = NAME_None)
	{
		FDFReactionRow Row;
		Row.StatusA = A;
		Row.StatusB = B;
		Row.BurstFraction = Burst;
		Row.EmitStatus = Emit;
		return Row;
	}

	inline FDFReactionRow ThermalShock() { return Reaction(TEXT("chill"), TEXT("burn"), 0.12f); }
	inline FDFReactionRow FlashFreeze()  { return Reaction(TEXT("chill"), TEXT("shock"), 0.f, TEXT("freeze")); }
	inline FDFReactionRow Corrode()      { return Reaction(TEXT("poison"), TEXT("shred"), 0.10f); }

	/** The whole A1 table keyed by id. */
	inline TMap<FName, FDFStatusRow> AllStatuses()
	{
		TMap<FName, FDFStatusRow> Rows;
		Rows.Add(TEXT("chill"), Chill());
		Rows.Add(TEXT("burn"), Burn());
		Rows.Add(TEXT("poison"), Poison());
		Rows.Add(TEXT("shred"), Shred());
		Rows.Add(TEXT("mark"), Mark());
		Rows.Add(TEXT("shock"), Shock());
		Rows.Add(TEXT("freeze"), Freeze());
		Rows.Add(TEXT("reveal"), Reveal());
		Rows.Add(TEXT("rubble"), Rubble());
		Rows.Add(TEXT("magnetize"), Magnetize());
		Rows.Add(TEXT("stagger"), Stagger());
		return Rows;
	}

	/** A resolver bound to a private copy of the A1 table with the three reactions registered. */
	struct FResolverFixture
	{
		TMap<FName, FDFStatusRow> Rows = AllStatuses();
		FDFStatusResolver Resolver;
		FDFStatusTargetState Target;

		FResolverFixture()
		{
			Resolver.FindStatusRow = [this](FName Id) -> const FDFStatusRow* { return Rows.Find(Id); };
			Resolver.Reactions.Add(TEXT("thermalShock"), ThermalShock());
			Resolver.Reactions.Add(TEXT("flashFreeze"), FlashFreeze());
			Resolver.Reactions.Add(TEXT("corrode"), Corrode());
		}

		FDFStatusApplyOutcome Apply(FName Id, float Now = 0.f, int32 Source = 1, float MagnitudeOverride = -1.f)
		{
			return Resolver.Apply(Id, Rows.FindChecked(Id), Now, Source, Target, MagnitudeOverride);
		}
	};
}
