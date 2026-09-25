---
ws: 19
slug: boss
title: Boss Frame01
state: claimed
owner: session-01EeMqPt-cloud
claimed_at: 2026-09-25T14:34:44Z
lease_expires: 2026-09-26T14:34:44Z
branch: ws/19-boss/core
last_commit: 
editor_heavy: true
phase: P4
size: XL
critical: false
blocked_on: 
---
# WS-19 — Boss Frame01

## Scope / DoD
**Scope.** 3 phases, sweep/slam/stomp, plates, brood vents, boss routes, boss HUD, horn; Source/DFEnemies/Boss/.

**Definition of done.** DF.Func.Boss.PhaseTransitions; kill-window gate 55–85% of walk on Crown/Foundry/Toaster.

**Spec.** B§2.4 (in `unreal/PLAN/PROGRAMME.md`). Size XL, phase P4.

## Contracts I consume
- C2
- C5
- C6
- C12

## Contracts / interfaces I provide
- FDFBossPhaseRow

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
This reaches the boss brood vents (B§2.4 P2: 4 motes every 15 s, max 12). INT recorded it on 2026-09-21 from PR #45; it is a one-line call, and finding it after
the fact costs a wave-clear bug that only shows up with that content in play.
