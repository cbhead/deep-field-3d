---
ws: 08
slug: vehicles
title: Vehicles
state: unclaimed
owner: 
claimed_at: 
lease_expires: 
branch: 
last_commit: 
editor_heavy: true
phase: P3-P4
size: L
critical: false
blocked_on: 
---
# WS-08 — Vehicles

## Scope / DoD
**Scope.** Chaos wheeled ×4 on real terrain (grades, roll-over recovery), seats, enter/exit, driver authority + clamp, physmat surfaces, non-driver interpolation, passenger shooting, hp/wreck/respawn, ramming, tow hook.

**Definition of done.** DF.Func.Vehicle.ReachesClaimedSpeed per vehicle × surface; ClimbsRatedGrade; AutoRights; 2-client seat contention; snap >20 m; ram damage host-computed.

**Spec.** A1 vehicles, B§1.12, §3.2 (vehicles), ADR-0008 (in `unreal/PLAN/PROGRAMME.md`). Size L, phase P3-P4.

## Contracts I consume
- C4
- C6
- C16

## Contracts / interfaces I provide
- ADFVehicle

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
