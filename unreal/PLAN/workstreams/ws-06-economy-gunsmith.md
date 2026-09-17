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
