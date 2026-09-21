---
ws: 17
slug: elites
title: Elite modifiers
state: unclaimed
owner: 
claimed_at: 
lease_expires: 
branch: 
last_commit: 
editor_heavy: false
phase: P4
size: M
critical: false
blocked_on: 
---
# WS-17 — Elite modifiers

## Scope / DoD
**Scope.** 5 modifiers, Dire pairs, injection stream, elite strip overheads, campaign debuts; lands in WS-05/WS-12/WS-01 paths.

**Definition of done.** DF.Func.Elite.<mod> ×5; injection determinism.

**Spec.** B§2.1, B§1.11 (in `unreal/PLAN/PROGRAMME.md`). Size M, phase P4.

## Contracts I consume
- C2
- C5

## Contracts / interfaces I provide
- FDFEliteRow

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
This reaches elite injection streams (an elite spawned into a running wave). INT recorded it on 2026-09-21 from PR #45; it is a one-line call, and finding it after
the fact costs a wave-clear bug that only shows up with that content in play.
