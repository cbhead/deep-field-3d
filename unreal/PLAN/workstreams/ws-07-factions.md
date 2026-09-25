---
ws: 07
slug: factions
title: Factions & abilities
state: unclaimed
owner: 
claimed_at: 
lease_expires: 
branch: 
last_commit: 
editor_heavy: false
phase: P2-P4
size: M
critical: true
blocked_on: 
---
# WS-07 — Factions & abilities

## Scope / DoD
**Scope.** 5 predicted GAs, passives, level curve, lobby uniqueness, XP from host match record (cap 60), level-5 signatures, Specter weak-point highlight, resonance combos.

**Definition of done.** Abilities land at aim point; per-level factors match Factions.cs; combo ICD shared with Overclock Surge.

**Spec.** A1 factions, B§1.7, B§2.3, B§3.10 (in `unreal/PLAN/PROGRAMME.md`). Size M, phase P2-P4.

## Contracts I consume
- C4
- C5
- C8

## Contracts / interfaces I provide
- GA_Faction_*

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-25 · INT · Unblocked by today's rulings: `FDFDetMath::PowInt` is in DFCore (`Determinism/DFDetMath.h`, R2), so the level curve (`CooldownFactor = PowInt(0.94, L-1)`, `RadiusFactor`, `MagnitudeFactor`, `LevelForXp`) can be ported bit for bit into DFGameplay/Factions and replace WS-11's placeholder `UDFLocalProgressionProvider::LevelForXp`. Faction and level replicate from a **`UDFFactionStateComponent`** you write and WS-28 attaches to `ADFPlayerState` (ADR-0024).
