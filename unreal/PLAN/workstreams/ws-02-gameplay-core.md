---
ws: 02
slug: gameplay-core
title: Gameplay core (GAS)
state: active
owner: session-75b58b1b/agent-ws02
claimed_at: 2026-09-19T15:16:34Z
lease_expires: 2026-09-20T15:16:34Z
branch: ws/02-gameplay-core/gas
last_commit: 
editor_heavy: false
phase: P1-P2
size: L
critical: true
blocked_on: 
---
# WS-02 — Gameplay core (GAS)

## Scope / DoD
**Scope.** ASC, attribute sets, damage execution (front arc, flat armor, shield, poison bypass, weak-point factor), 8 status channels, reactions, cc-resist, faction passive GEs, ability bases, cue bases, UDFTintComponent.

**Definition of done.** Unit tests reproduce Statuses.cs semantics and damage formulas; L_Test_Status shows chill+burn → thermalShock 12% via cue.

**Spec.** PROGRAMME.md §3.1 C4–C5, Appendix A1 statuses, B§1.6 (in `unreal/PLAN/PROGRAMME.md`). Size L, phase P1-P2.

## Contracts I consume
- C1
- C2
- C8

## Contracts / interfaces I provide
- C4
- C5

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->
- 2026-09-19 · C4 `UDFDamageExecution` / `FDFDamageMath` order corrected to Step.cs (arc → vulnerability → shred leak → flat armor floor → shield → hp); gas.md still reads "flat armor before vulnerability" — RFC text under Open questions. No API change; numbers change for every applier (WS-03/04/05/06): a marked, armored hit is `x1.25 - armor`, not `(x - armor) x1.25`.
- 2026-09-19 · C5 `FDFStatusResolver`: added `OnReaction` (bound by the component; burst between consume and emit) and `FDFStatusApplyOutcome::bEmitSkippedDead` (appends). `UDFStatusComponent::Apply(StatusTag, Source, MagnitudeOverride)` unchanged; Ember's burn factor and the fixed 1/30 s DoT period are now the component's, not the caller's. status.md corrections — RFC text under Open questions.
- 2026-09-19 · C8 contract-append: `Source/DFGameplay/Public/Tint/DFTintLayout.h` — the Custom Primitive Data index layout `UDFTintComponent` writes (WS-31 materials read it). Needs INT to append the table to palette.md.

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-19 · session-75b58b1b/agent-ws02 (continuation after a rate-limit stop) · **landed:** the GAS rules layer verified against Step.cs (the sim is the spec, ADR-0005) — damage order arc → vulnerability → shred leak → flat armor (floor) → shield → hp, the three Step.cs literals as Balance dials (`rearThresholdDegrees` / `shredFrontArcLeakFactor` / `postArmorDamageFloor`, sim defaults); `FDFStatusResolver` reaction-scan-first with the burst between consume and emit (`OnReaction`), emit overwrites its slot and is skipped on a kill, tie keeps the active status, silent same-id refresh; Thermal/Toxin ticks at a fixed `Balance("tickHz")` = 1/30 s; Ember's ×1.3 burn by applier faction before the refresh branch. C4 attribute sets, ASC, ability base/set, damage execution + context, 8 channel GEs, status component, tint component/palette/layout audited against C4/C5/C8. Test harness: a standalone world needs `AWorldSettings::NotifyBeginPlay()` for spawned actors' components to tick. 25 tests green through the lock: DF.Unit.Damage ×4 (incl. OrderMatchesSim), DF.Unit.Status ×20 (incl. ReactionScanPrecedesGates, ReactionConsumesActiveNotIncoming, BurstIsArmoredAndShielded, EmitOverwritesSlot, NoEmitOnKill, TieKeepsActive, RefreshIsSilent, DotTickMatchesSim, EmberDurationOnRefresh), DF.Unit.Tint ×1. **Left:** `L_Test_Status` map + the thermalShock cue (editor-heavy; cue assets are WS-14's), the CcResist attribute mirror needs `UDFControlSet` on enemies (WS-05 init), faction passive GEs (WS-07's `GA_Faction_*`; the hooks are `UDFCombatSet::ChilledBonus/WeakPointBonus`), hero-only `stagger`. **Needs INT:** see section above (palette.md append for `DFTintLayout.h`; three balance dials).
- 2026-09-21 · INT · **landing round 2 with one open minor, deliberately.** The review confirmed one
  finding and refuted four. `UDFStatusComponent::Now()` falls back to `World->GetTimeSeconds()` when the
  world has no game state, and that fallback is unconditional — so on a **joining client, in the window
  before the `AGameStateBase` channel opens**, a replicated `EndTimeServer` is subtracted from the
  client's own clock and `TimeRemaining` is wrong again, exactly as F2 was. It is narrow (one actor-
  channel window, cosmetic until something gameplay-facing reads it) and the fallback is *required* for
  the dev map and the unit-test world, which have no game state at all. Not landed-with-a-fix because
  the right behaviour on a client that cannot yet know the server clock is a display decision shared
  with WS-12 — "unknown" is not the same as "zero" or "full duration", and inventing that at a landing
  would be worse than leaving it named. **Fix in the next round**: make the fallback conditional on
  authority (or on `GetWorld()->GetNetMode() != NM_Client`), and decide with WS-12 what a client shows
  for a status whose clock it does not have yet. C4/C5 land now because six workstreams are waiting on
  them and this does not touch the rules.
