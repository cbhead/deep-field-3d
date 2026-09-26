---
ws: 24
slug: mutables
title: Mutable map set
state: unclaimed
owner: 
claimed_at: 
lease_expires: 
branch: 
last_commit: 
editor_heavy: true
phase: P4
size: L
critical: false
blocked_on: 
---
# WS-24 — Mutable map set

## Scope / DoD
**Scope.** floodgate/lanewash, destructible wall (GC + edge state), hidden cache, sniper nest, explosive barrel; container + crusher P2; lands in WS-09/WS-04.

**Definition of done.** DF.Func.Mutable.<kind>; validator rule 17.

**Spec.** B§2.11, B§3.1 (in `unreal/PLAN/PROGRAMME.md`). Size L, phase P4.

## Contracts I consume
- C6

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
This reaches a hidden nest or any mutable that releases bodies. INT recorded it on 2026-09-21 from PR #45; it is a one-line call, and finding it after
the fact costs a wave-clear bug that only shows up with that content in play.

## From INT (2026-09-21) — every closer asks WouldSeal, not just the lever gates
An authored lane gate cannot seal a spawn from a core: `WouldSeal` → `EverySpawnReachesCore` refuses it,
and warp arrival pads count as Spawn nodes (`DFLaneGraphBuilder.cpp:212`), so they are protected too.
That guarantee is only as good as the set of things that ask. **A floodgate, a crusher holding an edge
shut, a container dropped into a `containerGate`, or any scripted closure must go through the same
refusal** (RFC-0001: the closable set may seal in combination; the runtime refuses the closure that
would be the last). A destructible wall *opening* an edge cannot seal and needs no check.
The cost of skipping it is not an error message: it is a wave that stops walking, and — before #46
fixed the walker — a wave every tower ignored, because a stranded walker reports the cut-off sentinel.

