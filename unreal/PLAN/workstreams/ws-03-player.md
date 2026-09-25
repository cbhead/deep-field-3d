---
ws: 03
slug: player
title: Player
state: active
owner: session-01DTQRZ3-cloud
claimed_at: 2026-09-25T04:59:33Z
lease_expires: 2026-09-26T04:59:33Z
branch: ws/03-player/hero-movement
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
- 2026-09-25 · session-01DTQRZ3-cloud · **claim** — cloud session, on the user's ask (NEXT.md item 4). Read STATUS.md, NEXT.md, this file and INT's ADR-0024 note above. Plan for PR 1 on `ws/03-player/hero-movement`: `ADFHeroCharacter` + the hero movement component carrying B§1.1's parity numbers (walk 6.5, sprint 10, jump 4.8, crouch 3.0, ADS 3.5) through DFBalance dials, with DF.Unit tests for the 6.5/10/4.8 parity. Code-only, no editor slot. This session has no engine, so PR 1 will say it is unbuilt and can land only through `int-merge.sh` (CONTRACTS/ci.md).
