# C6 — Lane graph and socket asset

**Canonical:** `unreal/DeepField/Source/DFWorld/Public/LaneGraph/UDFLaneGraphAsset.h`, `ADFSocket.h`, `ADFLaneGate.h`, `ADFOperatedGate.h`, `ADFMutable.h`. Produced by `tools/ue-bridge/ue/build_level.py` from `unreal/content/levels/<map>.level.json` (+ `terrain.json`). **Owner:** WS-09. **Rule:** R; socket, node and edge ids are permanent — never renamed, only added.

```
UDFLaneGraphAsset
  Nodes[]        { Id, Position (cm, projected to terrain), Kind: Spawn|Junction|Core, Layer }
  Edges[]        { Id, From, To, Layer: Ground|Air, Kind: Walk|Warp, Waypoints[] (3D spline control points),
                   LengthMeters, MaxGradePercent, CostFactor, State: Open|Closed, ClosableBy: SocketId|MutableId|None,
                   AglMeters (air only), CorridorWidthMeters (3.4 default) }
  Itineraries[]  { Id, Layer, Via[] (NODE ids, in walk order) }  # the authored routes, derived exactly as LaneGraph.FromRoutes; EdgesOf() resolves node pairs to edges
  Gates[]        { EdgeId, SocketId }                         # barricade sockets that close an edge
  OperatedGates[]{ Id, EdgeId, At (cm), Label, CooldownSeconds (6), ReachMeters (9), BodyCheckMeters (4) }
  Mutables[]     { Id, Kind: Floodgate|Crusher|Wall|Cache|Nest|Barrel|Container, EdgeId?, At, Params{} }
  BossRoutes[]   { Id, Via[] }                                # ≤15 % grade, 8 m clearance, no warp legs
  Sockets[]      { SocketId, Tag: Ground|Wall|Trap|Barricade, Position, PadYaw, PadSlopePercent }
  Stations[]     { Id, Position }                             # hero stations
  Traversal[]    { Id, Kind: ladder|zipline|launcher|teleporter|elevator|nest|armory|mantle, Params{} }
  VehicleSpawns[]{ Id, VehicleId, Position, Yaw }
```

Rules the asset guarantees (checked by `DF.Map.Validate`, see `map-authoring-3d.md`): a closable set may seal a spawn from the core in combination — the validator reports every sealing combination, the runtime refuses the closure that would be the last (`WouldSeal`), and single-edge seals are authoring errors (RFC-0001: the rule follows the sim's harness gate 22c); ≤8 closable edges; a gate never closes on a body within 4 m; enemies mid-edge finish the edge and choose at the next junction; warp edges have zero length for every purpose; `RemainingToCore` is computed along the itinerary so targeting is comparable across routes and warps.

Runtime actors placed by the importer into `L_<Map>_Gameplay` keyed by stable ids (`SocketId` is the actor label and an `FName` property; re-import updates in place, never recreates): `ADFSocket`, `ADFLaneGate`, `ADFOperatedGate`, `ADFMutable*`, `ADFCore`, `ADFSpawnPortal`, `ADFWarpGate` (one actor per end, stable id `<node>@<edge>`; node ids therefore never contain `@`), traversal actors, `ADFVehicleSpawn`, `ADFHeroStation`.

## A stranded walker sorts last — and a sieging one is never stranded (INT, 2026-09-21)
`RemainingToCore` returns `TNumericLimits<float>::Max()` for a walker with nowhere to go, so it sorts
**last** in tower targeting. This is not a walker preference over WS-09's metric; it is what the sim
does, at `sim/Sim.Core/World.cs:116` — `float.IsPositiveInfinity(ahead) ? float.MaxValue : ...`, with
the comment "so it sorts last rather than first. Infinity would poison the comparison."

**The invariant that makes it safe, which is easy to get backwards.** The sim's condition is that the
node *ahead* has no open path to any core — a severed position. A walker's condition is that
`ChooseEdge` found nothing at a node — a severed step. They coincide only while siege routing works:
`Step.cs:1215-1224` routes a `StructureDps > 0` enemy against `DistToNodeOpen`/`DistToCoreOpen` (tables
that see *through* shut edges) with the wall priced as `BlockingHp / StructureDps × Speed ×
SiegeBreachBias`, so a sieging enemy always has an edge and is never stranded. A Ram standing at a
barricade is therefore on a blocked edge whose far node is still connected, reports a genuinely small
distance, and is targeted **first** — which is correct: the thing chewing your wall is the nearest
threat.

So: **a walker that is sieging must route with the see-through-gates tables, and must never be marked
stranded.** If siege routing is missing or disabled, a Ram at a barricade is flagged stranded, sorts
last, and every tower in range ignores the enemy breaking the player's wall — the exact inversion of
the problem the sentinel exists to prevent. WS-05 owns a test that pins it; WS-04 must treat the metric
as an opaque sort key and never do arithmetic on it (float max plus anything is still float max, and
subtracting two of them is zero).

