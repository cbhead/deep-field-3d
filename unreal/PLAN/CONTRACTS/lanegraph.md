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

Rules the asset guarantees (checked by `DF.Map.Validate`, see `map-authoring-3d.md`): closing any closable set never seals a spawn from the core (`WouldSeal`); ≤8 closable edges; a gate never closes on a body within 4 m; enemies mid-edge finish the edge and choose at the next junction; warp edges have zero length for every purpose; `RemainingToCore` is computed along the itinerary so targeting is comparable across routes and warps.

Runtime actors placed by the importer into `L_<Map>_Gameplay` keyed by stable ids (`SocketId` is the actor label and an `FName` property; re-import updates in place, never recreates): `ADFSocket`, `ADFLaneGate`, `ADFOperatedGate`, `ADFMutable*`, `ADFCore`, `ADFSpawnPortal`, `ADFWarpGate`, traversal actors, `ADFVehicleSpawn`, `ADFHeroStation`.
