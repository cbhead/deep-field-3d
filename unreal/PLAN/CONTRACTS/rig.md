# C9 — Tower rig (ADR-0007)

**Canonical:** `unreal/DeepField/Source/DFTowers/Public/Rig/DFTowerRigComponent.h` (the component), `Rig/DFTowerRig.h` (`FDFTowerRigLimits` and the pure aim math), `Rig/DFTowerDefinition.h` (`UDFTowerDefinition`, the class of `DA_Tower_<id>`: meshes, `.Rig`, stage sets), the socket names in `naming.md`. **Owner:** WS-04. **Rule:** R.

A tower is a **static-mesh component chain** on `ADFTower`:

```
RootComponent
└─ Foot   (UStaticMeshComponent, SM_Tower_<Id>_Foot)        socket S_Yaw, S_Stage_<path> (foot-mounted paths), S_Hit
   └─ Yaw    (UStaticMeshComponent, SM_Tower_<Id>_Yaw, attached to S_Yaw)   socket S_Pitch, S_Stage_<path>
      └─ Pitch (UStaticMeshComponent, SM_Tower_<Id>_Pitch, attached to S_Pitch)  socket S_Muzzle, S_Crown, S_Stage_<path>
         └─ Muzzle (USceneComponent at S_Muzzle)   # projectile/beam origin; +X is the bore
```
Aura/support towers (singularity, arc, detector, overclock): `Foot → Spin` (rotating component on `S_Spin`, 25°/s idle, 110°/s engaged); no Yaw/Pitch; overclock carries `S_Link_1..6` on Foot. Barricade: `GC_Barricade` geometry collection with damage thresholds at hp 100 % / 50 % / 0 %.

**Stage modules** `SM_Tower_<Id>_<Path>_S02..S10` are static meshes attached to `S_Stage_<Path>` on whichever part the manifest names for that path (so barrel-side modules swing with Pitch). Cumulative: at path level N the modules S02..SN are attached; S01 is the chassis. The importer (WS-30) preserves the design rule that s4/s7/s10 are silhouette jumps; the validator checks source-tri cumulativeness.

**Rig limits** live in `DA_Tower_<id>.Rig { YawMinDeg, YawMaxDeg, PitchMinDeg, PitchMaxDeg, TraverseDegPerSec, ElevateDegPerSec, bUnlimitedYaw }`, copied from `game/assets/structures/manifest.json` `rig` blocks (lance ±180 / −8..26 / 90 / 60; nova ±180 / 22..70 / 45 / 30; skywatch ±180 / 6..82 / 200 / 140; filament ±180 / −12..78 / 220 / 160). `UDFTowerRigComponent::AimAt(TargetLocation, DeltaTime)` slews Yaw and Pitch within limits at those rates (shortest way across the ±180 seam when unlimited), returns `bSettled`; at Night adds a 0.2 s search wobble before lock. **On terrain the limits are design levers**: a socket on a ridge must depress to the valley; nova's 22° floor is its 5 m minimum range in geometry.

**Firing**: bolt/flak rounds spawn at `Muzzle` along +X and rejoin the server's simulated round over 0.2 s (visual lead); **Nova and flak fire indirect ballistics** (`Projectile{Speed,Gravity}`) solved server-side against the target's predicted position; beams/tesla are Niagara beams from `Muzzle` to the hit with `Heat` (filament) or hop targets (arc); every shot passes the targeting component's LOS test (`map-authoring-3d.md`).

**Replicated per tower:** `DefTag, SocketId, OwnerPlayerId, PathLevels[3], HpFraction (uint8), TargetId, Heat (uint8), ChargesLeft, FedBy (overclock)`. Clients drive the rig from `TargetId` locally.

**Implementation notes (WS-04, 2026-09-25).** The manifest's `pitchSign` is -1 in every block: that is Godot's X-rotation convention. Unreal's pitch is + up, so `FDFTowerRigLimits::PitchSign` is +1 for all four, and stays for a mesh authored the other way round. Until WS-30 imports `DA_Tower_<id>`, `DFTowerRig::ManifestLimitsFor` supplies the four manifest blocks and the component draws no meshes (angles, settle and stage bookkeeping still run). A missing `DA_Tower_<id>` is not logged by the tower: reporting missing bindings is the registry audit's (WS-01). The night search is a single sweep of ±6° over `SearchWobbleSeconds` (0.2 s), during which `AimAt` never reports settled.
