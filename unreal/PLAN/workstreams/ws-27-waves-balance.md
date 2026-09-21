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
