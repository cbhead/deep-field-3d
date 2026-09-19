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
- **`cross-owner-ok` on this PR for the five persistent seeds** `Content/DF/Maps/<Map>/L_<Map>.umap` (Testlane, Foundry, Switchyard, Spire, Toaster; the DoD asks the importer to create them once and commit them; OWNERSHIP.md gives `L_<Map>.umap` to WS-10a-f). They are isolated in ONE commit, the last binary commit on the branch (`[WS-09] cross-owner-ok: ...`), so the alternative is `git rebase --onto <it>^ <it>` — the WS-10x owner then runs `-run=DFEditor.DFLevelImport -map=<map> -legacy` once (it seeds `L_<Map>` when missing: 400 m `DF_Graybox` floor, PlayerStart, sun/sky/fog, `L_<Map>_Gameplay` as an always-loaded sublevel) and commits it under their own row. Note `-run=DFEditor.DFMapValidate -map=foundry` loads `L_Foundry`, so CI needs one or the other.
- **`OWNERSHIP.md` row 25** `Content/DF/Data/Defs/{Maps,LaneGraphs,Conditions}/**` is a brace glob and `unreal/Build/ownership-check.py` (WS-15) matches with plain `fnmatch`, which never expands braces, so `DA_LaneGraph_*.uasset` reads as "binary owned by nobody". Either split the row into three globs (`Content/DF/Data/Defs/Maps/**`, `.../LaneGraphs/**`, `.../Conditions/**`) or expand braces in the checker — one line before the `rules.append`: `for g in (re.sub(r"\{[^}]*\}", alt, g) for alt in (m.group(1).split(",") if (m := re.search(r"\{([^}]*)\}", g)) else [""]))` (or `braceexpand`). Same fix covers the WS-06 `{Weapons,Melee,Ammo}` and WS-02 `{GE,GA_Base,Cues}` rows, which will hit the same wall.
- **`OWNERSHIP.md` rows for two text paths the contract names but nobody owns:** `unreal/map-validation-baseline.tsv` (the `DF.Map.Validate` ratchet, CONTRACTS/map-authoring-3d.md §2 -> WS-09, like `unreal/validation-baseline.tsv` -> WS-30) and `unreal/DeepField/Source/DFEditor/**` (no row at all: `Private/Commandlets/DFLevelImport*`, `DFMapValidate*` -> WS-09; WS-30's `DFTerrainImport*` -> WS-30; or one shared row "DFEditor: the commandlet's workstream").
- `DeepField.uproject`: DFEditor `LoadingPhase` PostEngineInit -> Default. The engine resolves `-run=` before PostEngineInit modules load (LaunchEngineLoop.cpp: `LoadStartupModules()` precedes the commandlet lookup; PostEngineInit modules load after), so the bare `-run=DFLevelImport` / `-run=DFMapValidate` from the DoD cannot find the class. Until then they are `-run=DFEditor.DFLevelImport` and `-run=DFEditor.DFMapValidate` (the "Module.Commandlet" spelling the engine supports).
- `unreal/Build/test.sh` (WS-15): it greps `Result={Passed}` but UE 5.8 logs `Result={Success}` (and `Result={Fail}`), so every run exits 1 with no output under `pipefail` even when every test passes. Verdicts in this log were read from `Saved/Logs/test-*.log` directly.
- ADR-0006 ruling: `L_<Map>_Gameplay` is saved with `bUseExternalActors=false` (one `.umap` per sublevel) because no ownership glob covers `Content/__ExternalActors__/DF/Maps/<Map>/L_<Map>_Gameplay/**` and the LFS lock model (ADR-0010) assumes a file per level. Keep the deviation for importer-written sublevels, or add the `__ExternalActors__` glob to the WS-09 row and WS-09 flips the flag (one line in `DFLevelImportCommandlet.cpp`, one re-import).

## Open questions
- **C6 text vs code — `Itineraries[].Via`:** CONTRACTS/lanegraph.md writes `Via[] (edge ids)`, but the same line says "derived exactly as LaneGraph.FromRoutes", and `LaneGraph.cs ItineraryDef.Via` is the ordered list of NODE ids (the junction sequence the route passes through; `EdgesOf()` resolves each pair to an edge, `EdgeBetween(Via[i], Via[i+1], Layer)`). The port follows the C# — `FDFLaneItinerary.Via` holds node ids — so the fix is one word in the doc (`Via[] (node ids)`), which is a contract text change: RFC if INT wants one, or an editorial correction by the contract owner (WS-09 owns C6 — say which and it lands in the same PR).
- **Rule 11 reading in `CheckSealing`:** the contract says "closing any closable set never seals (`WouldSeal`)". Read literally, Switchyard FAILS: b1+b2 seals the west gate (and `DF.Unit.LaneGraph.SwitchyardJunctions` asserts exactly that, from Sim.Harness gate 22c "you may shut either way through, and never both"). The validator implements the harness reading instead: every subset is enumerated, a sealing combination is REPORTED (the runtime refuses its last closure, as the sim does), and what FAILS is a single edge that seals on its own (an inert gate) or a monotonicity break (opening a gate from a legal set must never reduce connectivity). If the contract owner wants the literal reading, it is a one-line flip (`R.Status = EStatus::Fail` on `!Legal[Mask]`) plus a baseline line for Switchyard; decide before `-map=switchyard` goes into CI.
- C6 shape vs the doc: `Edges[].ClosableBy` is implemented as an enum `None|Socket|Mutable|Lever` plus `ClosableById` (the doc has one field `SocketId|MutableId|None` and does not list levers as closers, though `OperatedGates[]` are). `Traversal[].Position/Label`, `Nodes[].Layer` and the asset's `NearMisses[]` are optional-with-default additions (contract-append); `ADFLaneGraphInfo` (the one actor in `L_<Map>_Gameplay` that references the asset) is an extra actor class. Append, or RFC for the ClosableBy split?
- Coverage (rule 5) counts every Ground/Wall pad as air-capable until the towers table's `TargetLayers` is read (WS-04); traps and barricade slots cover nothing. On the flat `DF_Graybox` floor no `DF_Sight` trace is ever blocked, so the rule cannot fail on a legacy map yet — it is the seed of the check, real once WS-30's Landscape (and the registered blockers) is in the assembled level.
- Legacy air lanes are absolute-Y over a flat floor, so `AglMeters` is the edge's mean height; authored 3D files carry `agl` and will set it directly. Legacy points at y=0 are projected onto `DF_LaneSurface`; anything above grade (decks, wall sockets) keeps its height.

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-19 · session-75b58b1b/agent-ws09 · **Landed** on `ws/09-world/lanegraph-importer` (rebased on unreal/main 9fc6e61; `DeepFieldEditor Mac Development` builds): C6 `UDFLaneGraphAsset` + `DFLaneGraphTypes.h` and `UDFLaneGraphBuilder`, a faithful port of `LaneGraph.FromRoutes` (exact-equality coalescing, parallel-span mid-point promotion, shared prefixes -> shared edges, zero-length Warp edges, `laneNodeNames`, `<from>-<to>[@air][#n]` ids, near-miss report) with the routing helpers (`DistanceToCore/ToNode`, `ChooseEdge`, `PathFor`, `RemainingToCore` along the itinerary, `EverySpawnReachesCore`, `WouldSeal`, `ClosableEdges`, `Rebuild`); `FDFLevelFile` (legacy + authored spellings, sim->Unreal frame applied once: X=-Z*100, Y=X*100, Z=Y*100); `ADFWorldActor` + `ADFSocket/ADFLaneGate/ADFOperatedGate/ADFCore/ADFSpawnPortal/ADFHeroStation/ADFVehicleSpawn/ADFWarpGate/ADFLaneGraphInfo`, `UDFWorldSubsystem`, `DFWorldCollision.h`; `-run=DFEditor.DFLevelImport` (L_<Map> created once with a 400 m DF_Graybox floor, PlayerStart at heroSpawn, sun/sky/fog; L_<Map>_Gameplay actors keyed by stable id, re-import moves in place: foundry spawned 0 / moved 53 / removed 0) and `-run=DFEditor.DFMapValidate` (sealing over every closable subset + the 8-gate cap, spawnApron, socketOffset >= 3.5 m, corridor SKIP until a navmesh exists, trace-based coverage with the dead-ground report at `unreal/content/levels/reports/<map>.coverage.json`, ratchet `unreal/map-validation-baseline.tsv` seeded with Spire's two legacy failures). **All five legacy maps imported and committed via LFS** (`DA_LaneGraph_<Map>`, `L_<Map>`, `L_<Map>_Gameplay`, 15 objects); foundry validates 4 pass / 0 fail / 1 skip (54 segments, 0 dead, 379 sight traces). **Tests green through the lock:** `DF.Unit.LaneGraph.FoundryDerivation`, `.SwitchyardJunctions`, `.ToasterWarpEdges`, `.RemainingToCoreMonotonic`, `.WouldSeal`. `layering-check.py` OK; `ownership-check.py` flags the two items under Needs INT. **Next:** terrain rules (grade, AGL, pad slope, containment, reachability, water) once WS-30's terrain import lands; navmesh corridor walk; `ADFMutable*` + traversal actors and teleporters; `UDFConditionSubsystem` + physmats; wire lever / lane-gate closure into match state with WS-02/05 (`OnRep_Closed` is a stub); authored `<map>.level.json` 3D files. No editor slot taken (headless only).
