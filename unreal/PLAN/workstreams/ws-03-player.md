---
ws: 03
slug: player
title: Player
state: unclaimed
owner: 
claimed_at: 
lease_expires: 
branch: 
last_commit: 
editor_heavy: true
phase: P2
size: L
critical: true
blocked_on: 
---
# WS-03 — Player

## Scope / DoD
**Scope.** ADFHeroCharacter, CMC modes, input, server-traced weapons with magazines/spread/recoil knobs, reload montage driver scaled to ReloadSeconds, melee arcs, interaction precedence chain, build ghost, downed/revive/drag hooks.

**Definition of done.** 6.5/10/4.8 parity test; DF.Net.WeaponFeel at 150 ms; traversal modes pass DF.Func.Traversal.*.

**Spec.** B§1.1–1.3, B§1.13–1.14, §3.2 (player), §3.3 (in `unreal/PLAN/PROGRAMME.md`). Size L, phase P2.

## Contracts I consume
- C4
- C5
- C6
- C9
- C16

## Contracts / interfaces I provide
- ADFHeroCharacter
- UDFWeaponInstance

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-25 · INT · ADR-0024 (ruling R1): downed, revive progress and vehicle seat replicate from a **`UDFHeroStateComponent`** you write in DFPlayer and WS-28 attaches to `ADFPlayerState`; WS-28 respawns bleedout-expired players at the wave boundary through your hook.
