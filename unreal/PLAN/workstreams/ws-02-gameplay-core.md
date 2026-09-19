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

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
