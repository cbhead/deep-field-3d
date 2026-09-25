# Ownership: path glob → workstream

CODEOWNERS-style. A PR titled `[WS-NN]` may add or modify **binary** files only under WS-NN's globs unless INT applies the `cross-owner-ok` label. Text files outside your globs go through the owner (PR to them) or an RFC for contracts. `unreal/Build/ownership-check.py` enforces this in CI. First match wins; later rows are more specific.

| Glob | Owner |
|---|---|
| `unreal/PLAN/**` | INT (workstream sessions may edit only their own `workstreams/ws-NN-*.md`, `EDITOR-SLOTS.md`, and append to `DECISIONS.md` / add `rfcs/`) |
| `unreal/DeepField/DeepField.uproject`, `unreal/DeepField/Config/Default*.ini`, `unreal/DeepField/Source/*.Target.cs`, `.gitattributes`, `.lfsconfig` | INT |
| `unreal/DeepField/Config/Tags/DF_<ws>.ini` | the named workstream (append-only) |
| `unreal/DeepField/Source/DFCore/**`, `unreal/DeepField/Source/DFMatch/**`, `unreal/DeepField/Content/DF/Core/**` | WS-00 (contract-append PRs from anyone; changes by RFC) |
| `tools/content-export/**`, `unreal/content/schema/**` (except `terrain.schema.json`), `unreal/DeepField/Source/DFContentPipeline/**`, `unreal/DeepField/Content/DF/Data/Tables/**`, `unreal/DeepField/Source/DFCore/Public/Content/DFContentRows.h`, `unreal/content/README.md` | WS-01 |
| `unreal/content/json/towers.json`, `traps.json` | WS-04 |
| `tools/waveplan-golden/**`, `unreal/content/json/enemies.json`, `elites.json`, `boss.json`, `waves_*.json` | WS-05 (WS-17 elites, WS-19 boss, WS-27 waves by delegation) |
| `unreal/content/json/weapons.json`, `melee.json`, `attachments.json`, `ammo.json`, `balance.json` | WS-06 (WS-27 balance) |
| `unreal/content/json/statuses.json`, `reactions.json` | WS-02 |
| `unreal/content/json/factions.json` | WS-07 |
| `unreal/content/json/maps.json`, `conditions.json`, `vehicles.json`, `unreal/content/levels/**`, `unreal/map-validation-baseline.tsv` | WS-09 (per-map level files by the WS-10x owner once claimed) |
| `unreal/content/terrain/**`, `unreal/content/schema/terrain.schema.json`, `tools/ue-bridge/terrain/**`, `unreal/DeepField/Content/DF/Maps/*/L_*_Terrain*`, `unreal/DeepField/Source/DFEditor/**/DFTerrain*` | WS-30 (terrain lane; per-map `terrain.json` by the WS-10x owner once claimed) |
| `unreal/DeepField/Source/DFGameplay/**` (except `Economy/`, `Gunsmith/`, `Factions/`), `unreal/DeepField/Content/DF/Gameplay/{GE,GA_Base,Cues}/**` | WS-02 |
| `unreal/DeepField/Source/DFGameplay/Economy/**`, `Source/DFGameplay/Gunsmith/**`, `Content/DF/Data/Defs/{Weapons,Melee,Ammo}/**`, `Content/DF/Weapons/Attachments/**` | WS-06 |
| `unreal/DeepField/Source/DFGameplay/Factions/**`, `Content/DF/Gameplay/GA_Faction_*`, `Content/DF/Data/Defs/Factions/**` | WS-07 |
| `unreal/DeepField/Source/DFPlayer/**`, `Content/DF/Heroes/**` (bindings), `Content/DF/Weapons/**` (bindings), `Content/DF/Gameplay/GA_Player_*` | WS-03 |
| `unreal/DeepField/Source/DFTowers/**`, `Content/DF/Towers/**` (bindings), `Content/DF/Data/Defs/Towers/**` | WS-04 |
| `unreal/DeepField/Source/DFEnemies/**`, `Content/DF/Enemies/**` (bindings), `Content/DF/Data/Defs/Enemies/**` | WS-05 |
| `unreal/DeepField/Source/DFVehicles/**`, `Content/DF/Vehicles/**` (bindings) | WS-08 |
| `unreal/DeepField/Source/DFWorld/**`, `Content/DF/World/**` (bindings), `Content/DF/Data/Defs/{Maps,LaneGraphs,Conditions}/**`, `Content/DF/Maps/*/L_*_Gameplay*`, `tools/ue-bridge/terrain/**` | WS-09 |
| `unreal/DeepField/Source/DFEditor/**/DFLevelImport*`, `unreal/DeepField/Source/DFEditor/**/DFMapValidate*` | WS-09 (the level importer and `DF.Map.Validate` commandlets; the rest of DFEditor has no owner yet — add a row with the first file) |
| `unreal/DeepField/Content/DF/Maps/<Map>/L_<Map>.umap` | WS-09 (legacy import) until the map's WS-10a…f is claimed, then that workstream — INT swaps this row at claim time |
| `unreal/DeepField/Content/DF/Core/PhysicalMaterials/**` | WS-09 (physical materials per §5.2; a terrain-lane physmat a landscape layer needs is a PR to WS-09 — new files only, never a rewrite) |
| `unreal/DeepField/Source/DFOnline/**`, `Content/DF/Online/**` | WS-11 |
| `unreal/DeepField/Source/DFUI/**`, `Content/DF/UI/**` (except `Icons/`, `Styles/`, `Textures/`, `Studio/` → WS-45) | WS-12 |
| `unreal/DeepField/Source/DFAudio/**`, `Content/DF/Audio/**`, `Content/DF/Maps/*/L_*_Audio*` | WS-13 |
| `unreal/DeepField/Source/DFVfx/**`, `Content/DF/VFX/**` | WS-14 |
| `unreal/DeepField/Plugins/DFAutomation/**`, `Source/DFTests/**`, `unreal/Build/**`, `.github/workflows/unreal-*.yml`, `Content/DF/Dev/**` | WS-15 |
| `tools/ue-bridge/blender/**`, `tools/ue-bridge/ue/**`, `docs/design/models/**` (extras only), `Content/DF/Env/Kits/**`, `unreal/validation-baseline.tsv` | WS-30 |
| `unreal/DeepField/Content/DF/Materials/**`, `Content/DF/Textures/Shared/**`, `Content/DF/Core/DA_Palette*` | WS-31 |
| `unreal/DeepField/Content/DF/Characters/_Shared/**`, `Content/DF/Enemies/_Shared/**`, `tools/ue-bridge/blender/templates/**` | WS-32 |
| `unreal/DeepField/Content/DF/Towers/**` (meshes/MIs), `Content/DF/World/Sockets/**`, `Content/DF/World/Traps/**` (meshes) | WS-33 |
| `unreal/DeepField/Content/DF/Enemies/<id>/**` (meshes/anims/MIs), `Content/DF/Characters/Boss/**`, `Content/DF/Enemies/Elites/**` | WS-34 |
| `unreal/DeepField/Content/DF/Heroes/**` (meshes/anims), `Content/DF/Characters/FPArms/**`, `Content/DF/Weapons/**` (meshes/anims) | WS-35 |
| `unreal/DeepField/Content/DF/Vehicles/**` (meshes/physics/ABP) | WS-36 |
| `unreal/DeepField/Content/DF/Maps/Foundry/L_Foundry_Art*`, `L_Foundry_Lighting*`, `Content/DF/Env/Foundry/**` | WS-37 |
| `unreal/DeepField/Content/DF/Maps/Switchyard/L_Switchyard_Art*`, `L_Switchyard_Lighting*`, `Content/DF/Env/Switchyard/**` | WS-38 |
| `unreal/DeepField/Content/DF/Maps/Spire/L_Spire_Art*`, `L_Spire_Lighting*`, `Content/DF/Env/Spire/**` | WS-39 |
| `unreal/DeepField/Content/DF/Maps/Toaster/L_Toaster_Art*`, `L_Toaster_Lighting*`, `Content/DF/Env/Toaster/**`, `Content/DF/Env/Toaster/PCG/**` | WS-40 |
| `unreal/DeepField/Content/DF/Maps/Sluice/L_Sluice_Art*`, `L_Sluice_Lighting*`, `Content/DF/Env/Sluice/**` | WS-41 |
| `unreal/DeepField/Content/DF/Maps/Crown/L_Crown_Art*`, `L_Crown_Lighting*`, `Content/DF/Env/Crown/**` | WS-42 |
| `unreal/DeepField/Content/DF/World/**` (meshes: traversal set, destructibles, props, pickups) | WS-43 |
| `unreal/DeepField/Config/DefaultDeviceProfiles.ini` (via INT), `unreal/Build/perf/**` | WS-44 |
| `unreal/DeepField/Content/DF/UI/{Icons,Styles,Textures,Studio}/**`, `Content/DF/Maps/L_MainMenu*` | WS-45 |
| `unreal/DeepField/Content/Megascans/**` | nobody (Fab-restored, never edited, never committed) |
