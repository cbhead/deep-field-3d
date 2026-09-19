---
ws: 09
slug: world
title: World: lanes, sockets, traversal, conditions, terrain import
state: active
owner: session-75b58b1b/agent-ws09
claimed_at: 2026-09-19T15:16:34Z
lease_expires: 2026-09-20T15:16:34Z
branch: ws/09-world/lanegraph-importer
last_commit: 
editor_heavy: true
phase: P1-P4
size: L
critical: true
blocked_on: 
---
# WS-09 — World: lanes, sockets, traversal, conditions, terrain import

## Scope / DoD
**Scope.** UDFLaneGraphAsset (3D, AGL air), level.json/terrain.json importer (build_level.py → L_<Map>_Gameplay + L_<Map>_Terrain), sockets/gates/core/portal, traversal set + mantle/vault volumes, teleporters, warp gates, UDFConditionSubsystem, physmats, mutables actor API, DF.Map.Validate 3D with LOS coverage + dead-ground report.

**Definition of done.** Importer round-trips all level + terrain files; validator green on Foundry with real relief; Traversal/Teleport/LosBlockedByTerrain tests.

**Spec.** §3.2, C6, A1 maps, B§1.10, B§1.15, B§2.10–2.11, CONTRACTS/map-authoring-3d.md (in `unreal/PLAN/PROGRAMME.md`). Size L, phase P1-P4.

## Contracts I consume
- C1
- C2
- C3

## Contracts / interfaces I provide
- C6
- UDFConditionSubsystem
- map-authoring-3d

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
