#pragma once

#include "Content/DFContentRows.h"
#include "CoreMinimal.h"
#include "Damage/DFDamageMath.h"
#include "Status/DFReactionResolver.h"
#include "Status/DFStatusTypes.h"

// C5 — the status semantics with no world attached, so a unit test can pin every rule. The
// sim is the spec (ADR-0005): this is Step.cs ApplyStatus (1070-1131) and UpdateStatuses
// (1023-1060) line for line, and where status.md reads differently the sim wins (RFC'd):
//
//   Apply(id, row, now, source, target):
//     0. HERO GATE (C5 / B§1.6, not a sim rule — the sim's heroes carried no statuses): a target
//        with bHero accepts only Movement statuses and the hero-only `stagger` (Control, 0.25 s);
//        anything else is RejectedHero before the reaction scan, so chill + shock never becomes
//        flashFreeze on a hero. Stagger writes CcFillScale 0 and ignores the gauge: no cc-resist.
//     1. REACTION SCAN FIRST, across every active slot, before any gate. A match consumes the
//        ACTIVE partner (its slot is cleared), the INCOMING status is never applied, and Apply
//        returns. In between, OnReaction (bound by the component) applies the burst — through
//        the damage execution with the source at the target's own position, so no arc test but
//        mark, flat armor and shield all apply — and answers whether the target survived. The
//        row's EmitStatus is then written STRAIGHT INTO its channel slot: it overwrites whatever
//        held the slot outright (no strongest-wins, no refresh), takes the row's raw duration,
//        is gated only by cc-resist when it is hard control, and is skipped if the burst killed
//        the target. Outputs are never re-scanned: reactions cannot chain.
//        Consequences the tests pin: a shielded, chilled Warden hit by burn still detonates
//        thermalShock (the shield gate is below); shock into a chilled target at CcResist >= 1
//        still fires flashFreeze, whose emitted freeze is then refused by the gauge.
//     2. gates, on non-reaction paths only: Thermal on Shield > 0; hard control at
//        CcResist >= 1; Tether on Mass >= TetherImmuneMass or an immune target (B§1.6).
//     3. duration = row x DurationFactor. The factor is applied BEFORE the refresh /
//        strongest-wins branch, so an Ember refresh keeps the longer burn (Step.cs applies
//        EmberBurnDurationFactor by applier faction right here; the component computes it).
//     4. the channel slot: same id REFRESHES (duration and source reset, magnitude untouched,
//        and the component does not broadcast StatusApplied for it); otherwise
//        `active.Magnitude >= incoming.Magnitude` drops the incoming — a TIE keeps the active
//        status; a strictly stronger one replaces it. Never stacks.
//   Tick(now, dt): expires slots past EndTime; fills the cc-resist gauge at CcResistFillPerSecond
//     x the strongest active CcFillScale (hard control 1, Tether 0.5) or decays it at
//     CcResistDecayPerSecond.
//
// DoT cadence (decision for this port): Thermal / Toxin ticks are a periodic UDFDamageExecution
// at a FIXED 1/30 s — Balance("tickHz", 30), the sim's Balance.Dt — never the frame rate. Each
// tick is dps / tickHz through the full damage order, so mark amplifies it and the post-armor
// floor bites: a 0.2 burn tick (6 dps) becomes 0.5 on any target with FlatArmor > 0, which is
// why a burning Ram takes ~15 dps, not 6 (DF.Unit.Status.DotTickMatchesSim). Changing the
// period would change those numbers, so it is a balance dial, not a tuning knob.
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

	/** Fires inside Apply after the active partner is consumed and before the EmitStatus is written (the
	 *  Step.cs order: consume -> Damage() burst -> emit). The outcome carries ReactionId, BurstFraction and the
	 *  consumed slot. Return false when the burst killed the target: the emit is then skipped. Unbound = alive. */
	TFunction<bool(const FDFStatusApplyOutcome&)> OnReaction;

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
	/** The hero-only stagger's content id ("stagger", DF.Status.Stagger). */
	static FName HeroStaggerId();
	/** True when a hero may carry this status: any Movement row, or the stagger row in Control (C5). */
	static bool IsHeroStatus(FName StatusId, const FDFStatusRow& Row);

private:
	void Write(FDFStatusSlot& Target, FName StatusId, const FDFStatusRow& Row, float Magnitude, float Now, float Duration, int32 SourceId, bool bHero);
};
