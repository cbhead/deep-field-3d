---
ws: 01
slug: content-pipeline
title: Content pipeline
state: active
owner: session-75b58b1b/agent-ws01
claimed_at: 2026-09-19T15:16:34Z
lease_expires: 2026-09-20T15:21:30Z
branch: ws/01-content-pipeline/commandlet
last_commit: 
editor_heavy: false
phase: P1
size: M
critical: true
blocked_on: 
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
- 2026-09-19 · session-75b58b1b/agent-ws01 · **session start** — continuing the INT-assigned batch claim; lease renewed. Worktree `/Volumes/Toshiba/Deepfield-Unreal/wt-ws-01` on `ws/01-content-pipeline/commandlet` (SSD, ADR-0021). Read PROGRAMME §3/§5/§6, STATUS, ADR-0001..0021, C1–C3, digest 2026-09-17; no RFCs open, no contract hits since the bootstrap. Plan for this session: PR 1 = `Plugins/DFContentPipeline` (`-run=DFContentImport` JSON → `DT_*`, `-run=DFContentExport`) + `DF.Content.RoundTrip`.
- 2026-09-19 · session-fae2d0c5 (**not the owner**) · **correction** — the 2026-09-19 "session start" line above and the lease renewal in `db41492` were written by a separate desktop session that mistook this INT-assigned claim for its own. They do not describe agent-ws01's work; in particular the `Plugins/DFContentPipeline` layout was that session's plan, not the owner's. That session has withdrawn: it created `wt-ws-01` (the owner adopted it and keeps it), has removed its own files from it (nothing of the owner's was touched), and parked them on the local-only branch `claude/ws01-importer-parked` (`ec18609`, internal-disk clone) — never compiled, not for merge as-is. Ownership, state and branch of WS-01 are unchanged. Two findings for the owner: (1) `conditions.json` and its schema say `stealthWeightFactor` (= `sim/Sim.Core/Content/Conditions.cs:35`) but `FDFConditionRow` says `StealthWeight` — the only header/schema name mismatch across the 15 tables, and a strict importer trips on it; (2) the schemas are `additionalProperties: false` yet omit every Unreal-era optional field the header already has (`indirect`, `energyHue`, `weakPoints`, `elite`, `boss`, …) and `Primecore` in the scrap-bundle key enums, so authors cannot use those fields until the schemas are extended.
