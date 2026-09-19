---
ws: 05
slug: enemies-ai
title: Enemies & AI
state: claimed
owner: session-62767025
claimed_at: 2026-09-19T23:20:27Z
lease_expires: 2026-09-20T23:20:27Z
branch: ws/05-enemies-ai/waveplan
last_commit: 
editor_heavy: true
phase: P2-P3
size: XL
critical: true
blocked_on: 
---
# WS-05 — Enemies & AI

## Scope / DoD
**Scope.** ADFEnemy, UDFEnemyMovement (terrain navmesh corridor per lane edge; air = AGL lane spline), StateTree per archetype (11), displacement-return + knockdown + slope-speed rules, ADFWaveDirector + FDFWavePlan port + endless + injection stream hooks, leash aggression.

**Definition of done.** DF.Unit.WavePlanBaseline matches docs/gate-baseline.tsv counts/hp; DF.Func.Enemy.<archetype> ×11, Siege, SlopeSpeed, FlyerAgl; containment.

**Spec.** A1 enemies/waves, B§1.5, B§1.11, §3.2 (enemies) (in `unreal/PLAN/PROGRAMME.md`). Size XL, phase P2-P3.

## Contracts I consume
- C2
- C4
- C5
- C6

## Contracts / interfaces I provide
- ADFEnemy
- ADFWaveDirector

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-19 · session-62767025 · **session start:** claimed; first slice is code-only (no editor slot): `FDFWavePlan` port of `sim/Sim.Core/WavePlan.cs` + `DF.Unit.WavePlanBaseline` against `docs/gate-baseline.tsv`, in `Source/DFEnemies/{Public,Private}/Waves`. `ADFEnemy`, movement and StateTrees wait for WS-02's C4/C5 headers and WS-09's lane graph to land on `unreal/main`.
