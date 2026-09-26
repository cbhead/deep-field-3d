---
ws: 27
slug: waves-balance
title: Waves, tiers, endless, balance
state: unclaimed
owner: 
claimed_at: 
lease_expires: 
branch: 
last_commit: 
editor_heavy: false
phase: P4 (last)
size: L
critical: false
blocked_on: 
---
# WS-27 — Waves, tiers, endless, balance

## Scope / DoD
**Scope.** 74 authored waves incl. debuts, Standard/Hardened/Assault tiers, endless boss/elite-pack laps, Gauntlet mid-band bot sweep per map/tier, boss kill-window gate.

**Definition of done.** Every map/tier clears with the mid-band bot within the intended margin; boss window 55–85%.

**Spec.** B§1.11, B§4 (in `unreal/PLAN/PROGRAMME.md`). Size L, phase P4 (last).

## Contracts I consume
- C2
- C3

## Contracts / interfaces I provide
- (none)

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->

## Invariant from WS-05 (wave director) — read before you write a spawner
**Anything that puts an enemy into the world outside the wave plan MUST call
`ADFWaveDirector::NotifyEnemyAdded` for it**, or the wave clears while your bodies are still alive
(the director owns "what spawns when" and "when the wave is over", and counts only what it knows about).
This reaches endless elite packs and PCG wave variants. INT recorded it on 2026-09-21 from PR #45; it is a one-line call, and finding it after
the fact costs a wave-clear bug that only shows up with that content in play.
- 2026-09-21 · INT · **hardening follow-up from the PR #42 review** (refuted as a defect there, real as a
  gap): `FDFWaveGroupRow::Count` is still an unbounded content number. `SetWavesFromRows` now bounds
  `totalWaves` and every `waveIndex`, but `PlanWave` multiplies `Count` by the co-op and lap scales and
  casts to int32 with no bound — faithful to `WavePlan.cs:31-33`, which does the same, so it is not a
  port defect; the sim would blow up identically. The gap is that no schema in `unreal/content/schema/`
  carries a numeric `maximum` for any field. When this workstream owns the dials and the 74 authored
  waves, add a `MaxAuthoredGroupCount` beside `MaxAuthoredWaves=512` (enforced in `SetWavesFromRows` or
  `Validate`, not in `PlanWave` — a clamp there would be a deliberate divergence from the sim), and
  consider `maximum` bounds in `waves.schema.json` and `balance.json`'s schema so the importer refuses a
  fat-fingered number before a host ever allocates by it. Largest authored count today is 14.
- 2026-09-21 · INT · **three damage dials are missing from `balance.json`** and live as `constexpr`
  defaults in `DFDamageMath.h`: `shredFrontArcLeakFactor` (1.35), `postArmorDamageFloor` (0.5),
  `rearThresholdDegrees` (150). Verified missing from the shipped `balance.json` (its single `default`
  row has none of the three). Add them with the schema, and C4's "every number comes from the content
  rows" becomes true rather than nearly true. See RFC-0002.

