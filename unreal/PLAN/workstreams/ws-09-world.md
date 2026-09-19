---
ws: 09
slug: world
title: World: lanes, sockets, traversal, conditions, terrain import
state: active
owner: session-75b58b1b/agent-ws09
claimed_at: 2026-09-19T15:16:34Z
lease_expires: 2026-09-20T15:16:34Z
branch: ws/09-world/lanegraph-importer
last_commit: 8d0c45f
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
- `DeepField.uproject`: DFEditor `LoadingPhase` PostEngineInit -> Default. The engine resolves `-run=` before PostEngineInit modules load (LaunchEngineLoop.cpp: `LoadStartupModules()` precedes the commandlet lookup; PostEngineInit modules load after), so the bare `-run=DFLevelImport` / `-run=DFMapValidate` from the DoD cannot find the class. Until then they are `-run=DFEditor.DFLevelImport` and `-run=DFEditor.DFMapValidate` (the "Module.Commandlet" spelling the engine supports).
- `unreal/Build/test.sh` (WS-15): it greps `Result={Passed}` but UE 5.8 logs `Result={Success}` (and `Result={Fail}`), so every run exits 1 with no output under `pipefail` even when every test passes. Verdicts in this log were read from `Saved/Logs/test-*.log` directly.
- `OWNERSHIP.md` / `ownership-check.py`: the brace glob `Content/DF/Data/Defs/{Maps,LaneGraphs,Conditions}/**` is not expanded by the checker, so `DA_LaneGraph_*.uasset` reads as "owned by nobody". And the five persistent `L_<Map>.umap` seeds (floor, PlayerStart, sun, sky, fog) are created once by the importer per the WS-09 DoD but the table gives `L_<Map>.umap` to WS-10a-f: `cross-owner-ok` on this PR, or a row for importer-seeded persistent levels.

## Open questions
- C6 shape vs the doc: `Edges[].ClosableBy` is implemented as an enum `None|Socket|Mutable|Lever` plus `ClosableById` (the doc has one field `SocketId|MutableId|None` and does not list levers as closers, though `OperatedGates[]` are). `Traversal[].Position/Label`, `Nodes[].Layer` and the asset's `NearMisses[]` are optional-with-default additions (contract-append); `ADFLaneGraphInfo` (the one actor in `L_<Map>_Gameplay` that references the asset) is an extra actor class. Append, or RFC for the ClosableBy split?
- `L_<Map>_Gameplay` is saved with External Actors off (one `.umap` per sublevel) because the ownership globs do not cover `__ExternalActors__/DF/Maps/<Map>/L_<Map>_Gameplay/**` and the LFS lock model assumes a file per level; ADR-0006 says OFPA on. Keep, or add the glob and flip?
- Coverage (rule 5) counts every Ground/Wall pad as air-capable until the towers table's `TargetLayers` is read (WS-04); traps and barricade slots cover nothing.
- Legacy air lanes are absolute-Y over a flat floor, so `AglMeters` is the edge's mean height; authored 3D files carry `agl` and will set it directly. Legacy points at y=0 are projected onto `DF_LaneSurface`; anything above grade (decks, wall sockets) keeps its height.

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-19 · session-75b58b1b/agent-ws09 · **Landed** on `ws/09-world/lanegraph-importer` (rebased on unreal/main 9fc6e61; `DeepFieldEditor Mac Development` builds): C6 `UDFLaneGraphAsset` + `DFLaneGraphTypes.h` and `UDFLaneGraphBuilder`, a faithful port of `LaneGraph.FromRoutes` (exact-equality coalescing, parallel-span mid-point promotion, shared prefixes -> shared edges, zero-length Warp edges, `laneNodeNames`, `<from>-<to>[@air][#n]` ids, near-miss report) with the routing helpers (`DistanceToCore/ToNode`, `ChooseEdge`, `PathFor`, `RemainingToCore` along the itinerary, `EverySpawnReachesCore`, `WouldSeal`, `ClosableEdges`, `Rebuild`); `FDFLevelFile` (legacy + authored spellings, sim->Unreal frame applied once: X=-Z*100, Y=X*100, Z=Y*100); `ADFWorldActor` + `ADFSocket/ADFLaneGate/ADFOperatedGate/ADFCore/ADFSpawnPortal/ADFHeroStation/ADFVehicleSpawn/ADFWarpGate/ADFLaneGraphInfo`, `UDFWorldSubsystem`, `DFWorldCollision.h`; `-run=DFEditor.DFLevelImport` (L_<Map> created once with a 400 m DF_Graybox floor, PlayerStart at heroSpawn, sun/sky/fog; L_<Map>_Gameplay actors keyed by stable id, re-import moves in place: foundry spawned 0 / moved 53 / removed 0) and `-run=DFEditor.DFMapValidate` (sealing over every closable subset + the 8-gate cap, spawnApron, socketOffset >= 3.5 m, corridor SKIP until a navmesh exists, trace-based coverage with the dead-ground report at `unreal/content/levels/reports/<map>.coverage.json`, ratchet `unreal/map-validation-baseline.tsv` seeded with Spire's two legacy failures). **All five legacy maps imported and committed via LFS** (`DA_LaneGraph_<Map>`, `L_<Map>`, `L_<Map>_Gameplay`, 15 objects); foundry validates 4 pass / 0 fail / 1 skip (54 segments, 0 dead, 379 sight traces). **Tests green through the lock:** `DF.Unit.LaneGraph.FoundryDerivation`, `.SwitchyardJunctions`, `.ToasterWarpEdges`, `.RemainingToCoreMonotonic`, `.WouldSeal`. `layering-check.py` OK; `ownership-check.py` flags the two items under Needs INT. **Next:** terrain rules (grade, AGL, pad slope, containment, reachability, water) once WS-30's terrain import lands; navmesh corridor walk; `ADFMutable*` + traversal actors and teleporters; `UDFConditionSubsystem` + physmats; wire lever / lane-gate closure into match state with WS-02/05 (`OnRep_Closed` is a stub); authored `<map>.level.json` 3D files. No editor slot taken (headless only).
