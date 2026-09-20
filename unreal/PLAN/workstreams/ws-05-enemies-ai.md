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
- `Source/DFEnemies/Public/Waves/DFWavePlan.h` — `FDFWavePlan::PlanWave / HpScale / BountyScale / ScrapScale / Lap`, `FDFWavePlanTables` (+ `FromContent`, `Validate`), `FDFSpawnEntry`
- `Source/DFEnemies/Public/Waves/DFDetRng.h`, `DFDetMath.h` — the sim's RNG streams and deterministic float helpers (header-only)

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->
- 2026-09-19 · new (no contract touched): `FDFWavePlan`, `FDFWavePlanTables`, `FDFSpawnEntry`, `FDFDetRng` / `FDFRngStreams`, `FDFDetMath`. `FDFSpawnEntry` carries `RouteId` (FName), not the sim's index into `MapDef.Routes` — routes are the lane graph's (C6, WS-09); the director resolves the id against the map's itineraries.
- 2026-09-21 · new public API in `Source/DFEnemies/Public/Waves/DFWavePlan.h`, no contract touched — WS-27 and WS-17 will consume all three. `FDFWavePlan::OrdinalKey(FName)` is the one key an id sorts and hashes by (lower-cased: an FName is case-insensitive and outside the editor `ToString()` returns whichever casing reached the name table first, so `"escape"` reads back `"Escape"` once InputCore has registered its key — any new ordering or hashing of content ids must go through it). `FDFWavePlanTables::SetWavesFromRows(Rows, TotalWaves, OutError)` is the only supported way to fill `Waves` from wave-table rows: it bounds `TotalWaves` to `[1, MaxAuthoredWaves = 512]` and every `WaveIndex` to `[0, TotalWaves)` *before* sizing anything, and leaves `Waves` empty on failure. `FDFWavePlanDials::Unset` (quiet NaN) is the "not set" value for every dial — `Validate` needs finite, `> 0` for the four growth factors and `>= 0` for the per-player and bounty scales, so a dial of 0 is a balance choice rather than a missing row.

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->
- `OWNERSHIP.md`: add `tools/waveplan-golden/**` → WS-05 (the C# dumper that regenerates `Source/DFEnemies/Private/Tests/DFWavePlanGolden.inl` from the frozen sim; text only, no binaries).

## Open questions
- **Where the RNG streams live.** `DFDetRng.h` / `DFDetMath.h` are header-only with no DF dependencies and sit in DFEnemies for now. WS-09's condition `HazardStream` (B§2.10) and WS-06's scrap need the same streams and cannot depend on DFEnemies (sibling / lower layer). Proposal: a `contract-append` moving both headers to `Source/DFCore/Public/Determinism/` when the second consumer shows up; nothing else changes.
- **Float determinism on Game targets.** UBT (5.8, BuildSettings V7) compiles Editor targets with precise FP but leaves Game/Client/Server at Default, which is `/fp:fast` on MSVC. Resume-under-a-new-host re-plans the current wave on another machine, so the plan must not depend on it: the two headers and `DFWavePlan.cpp` pin their arithmetic with pragmas (`float_control(precise)` on MSVC, `clang fp contract(off) reassociate(off)`). Unverified on Windows until the GPU box exists.
- **Route ids.** The plan passes `FDFWaveGroupRow::RouteId` through untouched (`ground`, `groundShort`, `air`, `stair`, …). WS-09: are `UDFLaneGraphAsset::Itineraries` keyed by exactly these ids after a map redesign? The director will refuse a wave whose route id has no itinerary.
- **Sim rules kept as they are, for WS-27 to know about:** (1) a stealth-weighted condition adds `round(authoredCount × (factor − 1))` bodies — off the authored count, not the player-scaled one, and ties round to even (5 shades at night → +2, not +3); (2) player scaling multiplies every group, including one authored with an `Elite` — `EliteCap` is WS-17's to enforce; (3) the sim's `List.Sort` is unstable, so where entries tie on (tick, def id) the sim's order is a .NET accident — here ties keep plan order.
- **The baseline file.** `docs/gate-baseline.tsv` has no hp column, and its `spawned` is plan bodies + 5 per killed Cluster. `foundry/4p` (203) is a two-player match (its ember/forge/ember/forge party has two factions refused); four seated players is the `~not-a-gate` row (356). `DF.Unit.WavePlanBaseline` reads live tables, so the first rebalance of `waves_*.json` or the growth dials fails it on purpose — re-baseline or retire it at G3.


## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-19 · session-62767025 · **session start:** claimed; first slice is code-only (no editor slot): `FDFWavePlan` port of `sim/Sim.Core/WavePlan.cs` + `DF.Unit.WavePlanBaseline` against `docs/gate-baseline.tsv`, in `Source/DFEnemies/{Public,Private}/Waves`. `ADFEnemy`, movement and StateTrees wait for WS-02's C4/C5 headers and WS-09's lane graph to land on `unreal/main`.
- 2026-09-19 · session-62767025 · **landed (PR 1):** `FDFWavePlan` — `WavePlan.cs` ported as a pure function of `(seed, tables, waveIndex, playerCount)` over plain `FDFWavePlanTables` (built by `FromContent` from the DataTables, or by hand in a test); wave stream and condition stream separate; endless laps; `Elite`/`bBoss` passed through from the row. `FDFDetRng`/`FDFRngStreams` (mulberry32 + FNV-1a) and `FDFDetMath` (`PowInt`, round-half-to-even) bit for bit. `tools/waveplan-golden` dumps golden vectors from the frozen sim. **Tests (11 green, headless, through the lock):** `DF.Unit.WavePlan.{RngMatchesSim, DetMathMatchesSim, ScalesMatchSim, PlansMatchSim (237 plans: 5 maps × authored+6 endless waves × 1/2/4 players, count + canonical hash), SpelledPlansMatchSim, IsPureFunction, ConditionNeverShiftsAuthoredSpawns, SortedAndTiesKeepPlanOrder, EndlessLaps, ValidateRejectsBadTables}` and `DF.Unit.WavePlanBaseline` (live tables vs `docs/gate-baseline.tsv`, all 15 rows, + hp/bounty/scrap scales vs the sim). **Next:** `ADFWaveDirector` (ticks a plan into spawn requests at `tickHz`, resolves `RouteId` against C6, injection-stream hooks) once WS-09's lane graph is on `unreal/main`; `ADFEnemy` + attribute init once WS-02's C4/C5 headers are.
- 2026-09-21 · session-62767025 · **landed (PR 1, review round):** `93cf845` on `ws/05-enemies-ai/waveplan` — INT's three findings. Ids sort and hash by `FDFWavePlan::OrdinalKey` on both the C++ and the C# side (a Game build's `FName::ToString()` has no stable case; `groundShort` and spire's `escape` were the ids that would have drifted), golden regenerated. `SetWavesFromRows` bounds the wave index before sizing anything. Dials default to NaN so 0 stays a legitimate balance value. Plus `GGoldenRound` — `MathF.Round` vectors from the sim's own runtime, which caught that .NET returns -0 for `MathF.Round(-0.5f)` — and the FP pragmas extended over `DFDetRng.h`. 16 green. **Next:** the director core (`ws/05-enemies-ai/director`, PR #45, stacked); then `ADFEnemy` + `UDFEnemyMovement` against C6, now that WS-09 is on main.
