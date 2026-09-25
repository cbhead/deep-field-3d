---
ws: 06
slug: economy-gunsmith
title: Economy, scrap, gunsmith
state: unclaimed
owner: 
claimed_at: 
lease_expires: 
branch: 
last_commit: 
editor_heavy: false
phase: P3
size: M
critical: false
blocked_on: 
---
# WS-06 — Economy, scrap, gunsmith

## Scope / DoD
**Scope.** Money/lives/bounty, team+personal scrap with physics pickups, Primecore, purchases, 7 slots/15 attachments/8 ammo, PaP, melee mastery in scrap, early-call bonus.

**Definition of done.** Balance test reproduces Balance.cs; armory buys/refusals as messages; view-model stats.

**Spec.** A1 weapons/economy, B§1.2, B§1.8, B§2.12 (in `unreal/PLAN/PROGRAMME.md`). Size M, phase P3.

## Contracts I consume
- C2
- C4
- C12

## Contracts / interfaces I provide
- UDFEconomySubsystem
- FDFWeaponBuild

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-25 · INT · ADR-0023 (ruling R1): money, lives, team scrap and bounty replicate from a **`UDFEconomyStateComponent`** (DFGameplay/Economy) that WS-28 attaches to `ADFMatchState`; personal scrap and weapon builds from a **`UDFLoadoutStateComponent`** on `ADFPlayerState`. WS-28 reads lives for Defeat and never writes them; leaks arrive as a message and your component takes the lives off. Scrap can draw from `DFCore/Public/Determinism/DFDetRng.h` (R2).
