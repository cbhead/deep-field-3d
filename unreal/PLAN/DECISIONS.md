# Decision log (ADRs)

Append-only. Format: `## ADR-NNNN <title>` · Date · Status (Proposed | Accepted | Superseded by ADR-NNNN) · Context · Decision · Consequences · Links. INT assigns numbers; anyone may append a `Proposed` entry. Every RFC that lands references or creates an ADR. Full rationale for 0001–0018 is in [PROGRAMME.md](PROGRAMME.md) Section 2 and Section 3.2.

## ADR-0001 No Lyra fork
Date 2026-09-17 · Accepted · Fresh C++ project; ~10 Lyra source files copied as patterns (AbilitySet, PawnExtension/Hero components, InputConfig, GameplayMessage usage, CommonUser session flow) with Epic headers. Consequence: no 15–25 GB Lyra content on an 8 GB / 16 GB-free Mac; we own every class.

## ADR-0002 CharacterMovementComponent, not Mover 2.0
Date 2026-09-17 · Accepted · `UDFCharacterMovement` with custom modes (Walk, Sprint, Crouch, ADS, Ladder, Zipline, Launch, Lift, Seated, Downed) behind `IDFMovementIntent`. Consequence: proven CMC+GAS prediction; a Mover swap is a module change.

## ADR-0003 Online Services (OSSv2) EOS behind a facade, with a spike gate
Date 2026-09-17 · Accepted · `UDFOnlineSubsystem` over `OnlineServicesEOS`; a 2-day spike must prove login + invite-only session + P2P relay NetDriver on the UE 5.8.2 launcher build; fallback is OSSv1 `OnlineSubsystemEOS` inside the facade. Record the verdict as a new ADR.

## ADR-0004 Server-authoritative on the listen host; messages for discrete state
Date 2026-09-17 · Accepted · Replicated properties (push model) for continuous state; `FDFMsg_*` gameplay messages mirroring `Events.cs` for discrete state via `ADFEventRelay`; GameplayCues for cosmetic bursts; Iris off. UI/audio/VFX never branch on authority (CI grep).

## ADR-0005 Content numbers live in text (JSON), bootstrapped once from Sim.Core
Date 2026-09-17 · Accepted · `tools/content-export` dumps `sim/Sim.Core/Content` to `unreal/content/json/*.json` once; from then on the JSON (schema-validated) is the hand-edited source of truth; a commandlet writes DataTables; asset bindings live in Primary Data Assets; the C# content files are frozen as the historical spec. Consequence: new content never requires C# edits; no `.uasset` is ever edited for a number.

## ADR-0006 No World Partition; External Actors on; one sublevel per owner
Date 2026-09-17 · Accepted · `L_<Map>` (persistent), `_Gameplay`, `_Terrain`, `_Art`, `_Lighting`, `_Audio`; OFPA on every level; legacy HLOD only where a belt needs it.

## ADR-0007 Tower rig = static-mesh component chain; skeletal reserved for characters/weapons/vehicles
Date 2026-09-17 · Accepted · `Foot → Yaw → Pitch → Muzzle` static meshes with sockets (`S_Yaw S_Pitch S_Muzzle S_Crown S_Hit S_Stage_<path>`), stage modules as static meshes on `S_Stage_<path>`, aim by `UDFTowerRigComponent` under `DA_Tower_<id>.Rig` limits, Nanite throughout. Skeletal + Control Rig for enemies, boss, heroes, FP arms, weapons, vehicles, animated traps/traversal.

## ADR-0008 Vehicles: Chaos wheeled, driver-client-authoritative with server clamp
Date 2026-09-17 · Accepted · Driver's transform + velocity replicated and clamped (`TopSpeed × 1.15`); seats and ram damage server-owned; non-drivers interpolate; off the critical path. Chaos suspension is required by ADR-0018; the only fallback is a simplified Chaos setup.

## ADR-0009 Native C++ GameplayTags for contracts; per-workstream tag ini for local tags
Date 2026-09-17 · Accepted · `DFGameplayTags.h/.cpp` for every contract tag; `Config/Tags/DF_<ws>.ini` append-only per workstream.

## ADR-0010 Git LFS with single-owner directories and locks
Date 2026-09-17 · Accepted · LFS for `*.uasset *.umap *.ubulk *.uexp *.fbx *.glb *.png *.tga *.exr *.wav *.psd`; one owner directory per binary (`OWNERSHIP.md`, CI-enforced); `git lfs lock` on `.umap` and shared assets; `.lfsconfig fetchexclude` for Megascans and `L_*_Art*` so Mac sessions clone light; Megascans restored from Fab (`restore_fab.py`), never from LFS; 25 GB cap on tracked art.

## ADR-0011 Content root and naming
Date 2026-09-17 · Accepted · `unreal/DeepField/Content/DF/`; prefixes `SM_ SK_ SKEL_ PHYS_ ABP_ AS_ AM_ CR_ M_ MI_ MF_ MPC_ T_ NS_ NE_ BP_ DT_ DA_ WBP_ ST_ MS_ SC_ ATT_ L_ DL_ PCG_ GC_ RT_` (regex in `CONTRACTS/naming.md`).

## ADR-0012 Rendering stack
Date 2026-09-17 · Accepted · Nanite for every static mesh and for landscape; skeletal LOD chains until the GPU-box gate decides Nanite skinning; VSM everywhere (cascaded SM on Mac Low); software Lumen floor, hardware Lumen on Windows High; Sky Atmosphere + Volumetric Clouds replace skybox domes; Substrate off; Niagara Fluids P2.

## ADR-0013 Claude Design → Unreal lane
Date 2026-09-17 · Accepted · three.js export → glTF (+`extras.bone/socket/family/smooth`, `variants`) → headless Blender `tools/ue-bridge/blender/convert.py` (cm, Z-up, +X, sockets, rigid weights to family armature, auto-UV, AO/curvature/ID bakes, texture dedupe) → FBX + loose textures → Interchange pipelines via `tools/ue-bridge/ue/import_drop.py`.

## ADR-0014 Audio sources
Date 2026-09-17 · Accepted · MetaSounds; CC0/open-source libraries and generated audio only; music as procedural MetaSound stems or CC0 stems; every source logged in `Content/DF/Audio/LICENSES.md`.

## ADR-0015 Launch scope
Date 2026-09-17 · Accepted · 6 sectors (foundry, switchyard, spire, toaster redesigned + Sluice + Crown); full proposed roster (PROGRAMME.md B§4); Versus post-launch; Level 1 invite-only with L2/L3 seams (`IDFSessionBackend`, `IDFProgressionProvider`, `DF.Team.*`).

## ADR-0016 Machine plan
Date 2026-09-17 · Accepted · Mac M1 8 GB only until a Windows GPU workstation exists; external NVMe SSD is a P0 blocker; art sublevels fetch-excluded on the Mac; render/perf/Windows lanes recorded as unverified in the ledger until the box exists; two editor-heavy slots (Section 6.7).

## ADR-0017 Tint/palette is a parameter contract
Date 2026-09-17 · Accepted · `DA_Palette` from `docs/palette.json`; `UDFTintComponent` writes Custom Primitive Data / MID params by the fixed names in `CONTRACTS/palette.md`; no colour literals in code.

## ADR-0018 Real terrain everywhere; every map is a layout redesign
Date 2026-09-17 · Accepted · Every map is a Landscape with genuine relief, flat only where a place would be flat. Enemies, towers, vehicles, projectiles, scrap, VFX and validation are terrain-aware (PROGRAMME.md Section 3.2). Terrain is text-authored (`unreal/content/terrain/<map>.terrain.json` → `build_heightmap.py` → heightmap + masks); hand sculpting is a recorded polish delta. Line of sight is real (terrain and static world block it); the validator computes coverage with traces. The existing `level.json` is the brief, not the output. Supersedes the flat-world rules of `docs/MAP-AUTHORING.md` §4 where they conflict (see `CONTRACTS/map-authoring-3d.md`).
