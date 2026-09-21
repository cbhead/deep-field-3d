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
