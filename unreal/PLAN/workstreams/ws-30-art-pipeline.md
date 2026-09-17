---
ws: 30
slug: art-pipeline
title: Art pipeline (Claude Design → Unreal, terrain lane, validators)
state: unclaimed
owner: 
claimed_at: 
lease_expires: 
branch: 
last_commit: 
editor_heavy: true
phase: P1+
size: L
critical: false
blocked_on: 
---
# WS-30 — Art pipeline (Claude Design → Unreal, terrain lane, validators)

## Scope / DoD
**Scope.** Blender converter + family templates, generator extras, Interchange pipelines, import_drop.py, build_level.py, build_heightmap.py (terrain.json → heightmap + masks + sculpt-delta), validate_content.py (10 rules + ratchet), audit_registry.py, restore_fab.py, Content Audit tab.

**Definition of done.** Lance + 30 stages, Drifter, FP arms + Rifle, one foundry deck bay cook end-to-end and validate; a terrain.json regenerates the Foundry Landscape byte-identically.

**Spec.** C§7, ADR-0013, ADR-0018, §3.2 authoring (in `unreal/PLAN/PROGRAMME.md`). Size L, phase P1+.

## Contracts I consume
- C7
- C8
- C9
- C10

## Contracts / interfaces I provide
- import lane
- terrain lane
- validate_content
- audit_registry

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
