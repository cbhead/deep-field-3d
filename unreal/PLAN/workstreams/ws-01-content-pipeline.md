---
ws: 01
slug: content-pipeline
title: Content pipeline
state: blocked
owner: session-75b58b1b (INT)
claimed_at: 2026-09-17T07:18:45Z
lease_expires: 2026-09-18T07:18:45Z
branch: unreal/main
last_commit: 
editor_heavy: false
phase: P1
size: M
critical: true
blocked_on: uproject (external SSD, P0) for the commandlet half; JSON bootstrap done
---
# WS-01 — Content pipeline

## Scope / DoD
**Scope.** tools/content-export (.NET), JSON schemas, DFContentPipeline commandlet (JSON→DT), DT_ContentRegistry, DF.Content.RoundTrip/Bindings/TagCoverage, palette + UI tokens import.

**Definition of done.** Every Sim.Core content table exported, imported, round-trips; registry audit fails on any id without a row/binding or any Placeholder=true asset in a cook.

**Spec.** PROGRAMME.md §3.1 C2–C3, ADR-0005 (in `unreal/PLAN/PROGRAMME.md`). Size M, phase P1.

## Contracts I consume
- C1
- C2
- C3

## Contracts / interfaces I provide
- C2
- C3

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-17 · session-75b58b1b (INT) · Landed `tools/content-export` (.NET 8, reflection over Sim.Core records) and ran the one-time bootstrap: 20 tables in `unreal/content/json/` (towers 8, traps 3, enemies 11, statuses 8, reactions 3, factions 5, weapons 6, melee 4, meleeAttachments 8, attachments 11, ammo 7, conditions 2, vehicles 4, maps 5, waves_* ×5, balance 52 dials), `unreal/content/content-ids.json`, 16 JSON schemas in `unreal/content/schema/`, and the five legacy level briefs in `unreal/content/levels/legacy/`. `--diff` reports no drift. From here the JSON is authoritative (ADR-0005). **Next:** the `DFContentPipeline` commandlet (JSON → `DT_*`), `DF.Content.RoundTrip/Bindings/TagCoverage`, `DA_Palette`/`DA_UITokens` import — all need the .uproject, i.e. the external SSD (P0). A CI schema validator (`unreal/Build/validate-content-json.py`) can land before that.
