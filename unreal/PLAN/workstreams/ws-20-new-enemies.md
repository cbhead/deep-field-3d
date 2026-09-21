---
ws: 20
slug: new-enemies
title: New enemies ×5
state: unclaimed
owner: 
claimed_at: 
lease_expires: 
branch: 
last_commit: 
editor_heavy: false
phase: P4
size: L
critical: false
blocked_on: 
---
# WS-20 — New enemies ×5

## Scope / DoD
**Scope.** broodmother, leaper (Air while airborne), carapace (plates), skater (phase), nullifier (tether); lands in WS-05.

**Definition of done.** DF.Func.Enemy.<new> ×5.

**Spec.** B§2.5 (in `unreal/PLAN/PROGRAMME.md`). Size L, phase P4.

## Contracts I consume
- C2
- C5

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
This reaches broodmother egg-laying and death-split, and cluster splits. INT recorded it on 2026-09-21 from PR #45; it is a one-line call, and finding it after
the fact costs a wave-clear bug that only shows up with that content in play.
