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
