#pragma once

#include "Content/DFContentRows.h"
#include "CoreMinimal.h"
#include "Damage/DFDamageMath.h"
#include "Status/DFReactionResolver.h"
#include "Status/DFStatusTypes.h"

// C5 — the status semantics with no world attached, so a unit test can pin every rule
// (Statuses.cs + Step.cs ApplyStatus/TickStatuses):
//
//   Apply(id, row, now, source, target):
//     1. reaction scan over every active channel — a match consumes the active half, drops the
//        incoming one, reports the burst fraction and writes the row's EmitStatus straight into
//        its slot (gated only by cc-resist for hard control). Outputs are never re-scanned:
//        reactions cannot chain. This runs BEFORE the gates, as the sim did.
//     2. gates: Thermal on Shield > 0; hard control at CcResist >= 1; Tether on Mass >=
//        TetherImmuneMass or an immune target.
//     3. the channel slot: same id refreshes; a magnitude <= the active one is dropped; a
//        stronger one replaces it. Never stacks.
//   Tick(now, dt): expires slots past EndTime; fills the cc-resist gauge at CcResistFillPerSecond
//     x the strongest active CcFillScale (hard control 1, Tether 0.5) or decays it at
//     CcResistDecayPerSecond.
//
// Rows are looked up through FindStatusRow (the component binds UDFContentSubsystem; a test
// binds a map). Rates default to Balance.cs and are overwritten from the balance table.
struct DFGAMEPLAY_API FDFStatusResolver
{
	static constexpr int32 NumChannels = 8;

	FDFStatusSlot Slots[NumChannels];

	/** 0..1; hard control is refused at 1. */
	float CcResist = 0.f;
	/** Balance ccResistFillPerSecond / ccResistDecayPerSecond (Balance.cs values until content overrides). */
	float CcResistFillPerSecond = 0.45f;
	float CcResistDecayPerSecond = 0.125f;
	/** B§1.6 Tether immunity threshold (Balance tetherImmuneMass when present). */
	float TetherImmuneMass = DFDamageDefaults::TetherImmuneMass;

	FDFReactionResolver Reactions;

	/** Row source for EmitStatus lookups; unbound = reactions cannot emit. */
	TFunction<const FDFStatusRow*(FName)> FindStatusRow;

	FDFStatusResolver();

	/** DurationFactor multiplies the row duration (conditions, Ember's burn passive); MagnitudeOverride < 0 = row magnitude. */
	FDFStatusApplyOutcome Apply(FName StatusId, const FDFStatusRow& Row, float Now, int32 SourceId, const FDFStatusTargetState& Target,
		float MagnitudeOverride = -1.f, float DurationFactor = 1.f);

	/** Expires slots and moves the gauge. Returns the number expired; OutExpired receives copies of them. */
	int32 Tick(float Now, float DeltaSeconds, TArray<FDFStatusSlot>* OutExpired = nullptr);

	const FDFStatusSlot& Slot(EDFStatusChannel Channel) const { return Slots[static_cast<int32>(Channel)]; }
	FDFStatusSlot& Slot(EDFStatusChannel Channel) { return Slots[static_cast<int32>(Channel)]; }
	bool IsActive(EDFStatusChannel Channel) const { return Slot(Channel).IsActive(); }
	/** Any hard-control status active. */
	bool IsControlled() const;
	/** The strongest CcFillScale among active slots (0 = the gauge decays). */
	float ActiveCcFillScale() const;
	void ClearAll();
	void ClearChannel(EDFStatusChannel Channel) { Slot(Channel).Clear(); }

	/** Row magnitude, or the override when >= 0. */
	static float MagnitudeFor(const FDFStatusRow& Row, float MagnitudeOverride);
	/** Row duration x factor. */
	static float DurationFor(const FDFStatusRow& Row, float DurationFactor);
	/** CcResistFill for hard control and Tether rows, 0 otherwise. */
	static float CcFillScaleFor(const FDFStatusRow& Row);

private:
	void Write(FDFStatusSlot& Target, FName StatusId, const FDFStatusRow& Row, float Magnitude, float Now, float Duration, int32 SourceId);
};
