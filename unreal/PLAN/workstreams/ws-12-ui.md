---
ws: 12
slug: ui
title: UI (Common UI + MVVM)
state: unclaimed
owner: 
claimed_at: 
lease_expires: 
branch: 
last_commit: 
editor_heavy: true
phase: P2-P5
size: XL
critical: true
blocked_on: 
---
# WS-12 — UI (Common UI + MVVM)

## Scope / DoD
**Scope.** 17 screens + teleport picker + world markers + coverage decal driver + boss bar + elite strip + ping UI + early-call vote + tiers; M_UI_Chamfer, styles from DA_UITokens, BP_ModelWell studio, icons registry, magenta-chip fallback.

**Definition of done.** Every screen reachable in L_Test_UI with a fake view model; no net branching (CI grep); settings persisted.

**Spec.** C12, A3 UI, C§6 (in `unreal/PLAN/PROGRAMME.md`). Size XL, phase P2-P5.

## Contracts I consume
- C1
- C8
- C15

## Contracts / interfaces I provide
- C12

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
