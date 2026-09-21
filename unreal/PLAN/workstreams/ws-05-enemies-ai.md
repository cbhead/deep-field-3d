---
ws: 05
slug: enemies-ai
title: Enemies & AI
state: active
owner: session-62767025
claimed_at: 2026-09-19T23:20:27Z
lease_expires: 2026-09-22T05:31:09Z
branch: ws/05-enemies-ai/waveplan
last_commit: 93cf845
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
- 2026-09-21 · new public API in `Source/DFEnemies/Public/Waves/DFWavePlan.h`, no contract touched — WS-27 and WS-17 will consume all three. `FDFWavePlan::OrdinalKey(FName)` is the one key an id sorts and hashes by (lower-cased: an FName is case-insensitive and outside the editor `ToString()` returns whichever casing reached the name table first, so `"escape"` reads back `"Escape"` once InputCore has registered its key — any new ordering or hashing of content ids must go through it). `FDFWavePlanTables::SetWavesFromRows(Rows, TotalWaves, OutError)` is the only supported way to fill `Waves` from wave-table rows: it bounds `TotalWaves` to `[1, MaxAuthoredWaves = 512]` and every `WaveIndex` to `[0, TotalWaves)` *before* sizing anything, and leaves `Waves` empty on failure. `FDFWavePlanDials::Unset` (quiet NaN) is the "not set" value for every dial — `Validate` needs finite, `> 0` for the four growth factors and `>= 0` for the per-player and bounty scales, so a dial of 0 is a balance choice rather than a missing row.

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-19 · session-62767025 · **session start:** claimed; first slice is code-only (no editor slot): `FDFWavePlan` port of `sim/Sim.Core/WavePlan.cs` + `DF.Unit.WavePlanBaseline` against `docs/gate-baseline.tsv`, in `Source/DFEnemies/{Public,Private}/Waves`. `ADFEnemy`, movement and StateTrees wait for WS-02's C4/C5 headers and WS-09's lane graph to land on `unreal/main`.
- 2026-09-21 · session-62767025 · **landed (PR 1, review round):** `93cf845` on `ws/05-enemies-ai/waveplan` — INT's three findings. Ids sort and hash by `FDFWavePlan::OrdinalKey` on both the C++ and the C# side (a Game build's `FName::ToString()` has no stable case; `groundShort` and spire's `escape` were the ids that would have drifted), golden regenerated. `SetWavesFromRows` bounds the wave index before sizing anything. Dials default to NaN so 0 stays a legitimate balance value. Plus `GGoldenRound` — `MathF.Round` vectors from the sim's own runtime, which caught that .NET returns -0 for `MathF.Round(-0.5f)` — and the FP pragmas extended over `DFDetRng.h`. 16 green. **Next:** the director core (`ws/05-enemies-ai/director`, PR #45, stacked); then `ADFEnemy` + `UDFEnemyMovement` against C6, now that WS-09 is on main.
