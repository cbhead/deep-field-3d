---
ws: 04
slug: towers
title: Towers & build
state: unclaimed
owner: 
claimed_at: 
lease_expires: 
branch: 
last_commit: 
editor_heavy: true
phase: P2-P3
size: L
critical: true
blocked_on: 
---
# WS-04 — Towers & build

## Scope / DoD
**Scope.** ADFStructure/Tower/Trap/Barricade, UDFTowerRigComponent, UDFTargetingComponent with real LOS + registered blockers, kinds Bolt/Mortar(indirect)/Aura/Flak/Tesla/Beam/Barricade/Support, stage attachment, sell/upgrade/breakpoints, UDFBuildSubsystem, tower ownership.

**Definition of done.** Every kind fires at a test enemy; DF.Func.Tower.Aim/AirTracking/Rounds/LosBlockedByTerrain/IndirectOverRidge; stage cumulativeness; refusals as messages.

**Spec.** A1 towers, B§1.4, §3.2 (towers) (in `unreal/PLAN/PROGRAMME.md`). Size L, phase P2-P3.

## Contracts I consume
- C2
- C4
- C5
- C6
- C8

## Contracts / interfaces I provide
- C9
- UDFBuildSubsystem

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->

## From INT (2026-09-21) — two things targeting must know before you write `UDFTargetingComponent`
1. **`RemainingToCore` is an opaque sort key, not a distance.** A walker with nowhere to go returns
   `TNumericLimits<float>::Max()` so it sorts last (sim parity, `World.cs:116`). Never do arithmetic on
   it — float max plus anything is float max, and the difference of two of them is zero. Compare only.
2. **A sieging enemy is NOT stranded and must be targeted first.** It reports a genuinely small distance
   because siege routing sees through the shut edge it is chewing (`Step.cs:1215-1224`). The sim gets
   "the Ram breaking your barricade is the priority target" for free from one comparison, with no special
   case — do not add one. See `CONTRACTS/lanegraph.md`, "A stranded walker sorts last".
Targeting otherwise follows `Step.cs:1620-1653` exactly: layer filter, not burrowed, stealth needs the
Detection channel, range and min-range, sight, then lowest `RemainingToCore` wins.

