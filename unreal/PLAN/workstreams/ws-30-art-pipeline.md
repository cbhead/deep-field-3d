---
ws: 30
slug: art-pipeline
title: Art pipeline (Claude Design → Unreal, terrain lane, validators)
state: active
owner: session-75b58b1b/agent-ws30
claimed_at: 2026-09-19T15:16:34Z
lease_expires: 2026-09-20T15:16:34Z
branch: ws/30-art-pipeline/terrain-lane
last_commit: bd09a87
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
- 2026-09-19 · `unreal/.gitattributes`: appended `content/terrain/**/*.png filter=lfs …` so `unreal/content/terrain/out/` PNGs go through LFS (the existing `content/terrain/*.png` rule does not match the subfolder). Review/keep.
- 2026-09-19 · `unreal/Build/modules.json` documents DFEditor's private deps; `DFEditor.Build.cs` now also lists `ImageWrapper` (PNG heightmap decode) and `PhysicsCore` (PM_Grass). Please mirror in modules.json.
- 2026-09-19 · Binaries outside WS-30's globs, made by the terrain lane on INT's instruction (OWNERSHIP.md lists `tools/ue-bridge/terrain/**` + `unreal/content/terrain/**` under WS-09, `Content/DF/Maps/Foundry/L_Foundry*` under WS-10a, `Content/DF/Core/**` under WS-00): `unreal/content/terrain/out/foundry_*.png`, `Content/DF/Maps/Foundry/L_Foundry_Terrain.umap`, `Content/DF/Maps/Foundry/L_Foundry.umap` (minimal persistent level, created because WS-09 had not yet), `Content/DF/Core/PhysicalMaterials/PM_Grass.uasset`. Needs the `cross-owner-ok` label, or move the `tools/ue-bridge/terrain/**` glob to WS-30 (PROGRAMME.md §5.4 lists the terrain lane under WS-30).
- 2026-09-19 · Editor slot (EDITOR-SLOTS.md) not taken: this batch was told never to push to `unreal/main`; the commandlet ran under `editor-lock.sh` instead.
- 2026-09-19 · `DeepField.uproject` (INT-owned, one token): `DFEditor` `LoadingPhase` `PostEngineInit` → `Default`. Reason: `-run=<Commandlet>` is resolved in `FEngineLoop::PreInitPostStartupScreen` right after `LoadStartupModules` (PreDefault + Default) and before engine init, so a `PostEngineInit` editor module's commandlet classes do not exist yet — the first `DFTerrainImport` run died with `looked like a commandlet, but we could not find the class` (`Saved/Logs/terrain-import-foundry.log`). The `-run=Module.Commandlet` form does not help in 5.8 (the whole token is used as the class FName). Lyra's editor module loads at `Default` too; `DFEditor`'s StartupModule only logs. If INT prefers another mechanism (a `Default`-phase plugin module like WS-01's `DFContentPipeline`, or a `UDFTerrainImportLibrary` called from `-run=pythonscript`), the commandlet body moves unchanged.

- 2026-09-19 · `OWNERSHIP.md` has no row for `unreal/DeepField/Source/DFEditor/**` (`ownership-check.py --ws 30` reports every DFEditor file as "owned by nobody"). The terrain importer and its tests live there; suggest `Source/DFEditor/**` → WS-30 for the terrain/import lane files, or a shared-editor-module rule like `Source/DFCore`'s contract-append one.

## Terrain lane notes (for build_level.py and the sculpt-delta workflow)
- **What exists.** `tools/ue-bridge/terrain/build_heightmap.py <map>` renders `unreal/content/terrain/<map>.terrain.json` (schema `unreal/content/schema/terrain.schema.json`) to `unreal/content/terrain/out/<map>_height.png` (16-bit, 0 = minZ, 65535 = maxZ), `<map>_height.json` (minZ/maxZ/metresPerSample/originX/originZ/width/height, frame `sim-metres`, per-roadbed grade stats), one 8-bit mask per feature kind + `_mask_steep` (+ `_mask_water` when `water[]` is set) and `_preview.png`. `UnrealEditor-Cmd … -run=DFTerrainImport -map=<map>` (DFEditor) builds `/Game/DF/Maps/<Map>/L_<Map>_Terrain` from that pair (100 cm quads, sim (0,0) at the Unreal origin, Z scale from minZ/maxZ, PM_Grass, Nanite flagged, collision on), creates a minimal `L_<Map>` if none exists, adds the sublevel as always-loaded, then reloads and checks bounds + three probe samples. `-verifyonly` just runs the check.
- **Frames.** Images are sim metres: column = +x, row = +z (toward the spawn yard on the legacy maps). The importer transposes and flips (landscape local X = sim −z, local Y = sim +x) — `FDFTerrainLandscapeLayout` in `Source/DFEditor/Public/DFTerrainHeightmap.h` is the one place the mapping lives; `DF.Editor.Terrain.*` pins it.
- **For build_level.py (WS-09's lane projection).** Sample the same `<map>_height.png`/json (`FDFTerrainHeightmap` from C++, or `build_heightmap.read_png_gray` from Python) rather than tracing the Landscape: it is exact, RHI-free and identical to what the commandlet imported. The `roadbed` feature already flattens and re-grades the lane corridor (`width` 5 m + `falloff` shoulders, `maxGrade` 30, `gradeMode` balanced = cut volume equals fill volume, ramp centred on each terrace edge), so a projected 3D lane on the bed is ≤ 30 % by construction; `_mask_roadbed.png` is the corridor. Socket pads are NOT cut by the heightmap tool (they belong with `sockets[].pad` in build_level.py): use `_mask_steep.png` (0 at ≤ 30 % grade, 255 at ≥ 70 %) to refuse a pad on a face. Air lanes: AGL over the sampled height. Vehicle roads: add a second `roadbed` with `maxGrade` 12.
- **Foundry today (legacy brief, not the redesign).** All ground sockets sit on terrace tops (T1 ≈ 16 m: g7 g8 g9 g1 g11 g23; T2 ≈ 8 m: g10 g2 g12 g13 g3 g14 g4 g15 g16 g5 g17; T3 ≈ 0 … −1.9 m in the basin: g18 g6 g19 g20 g21 g22). Lane relief 16 → −2.5 m (18.5 m, ≥ 8 m for G2). Known consequences for the redesign: traps t1 t5 t6 t9 t3 lie on the two 30 % ramps (a trap pad on a ramp has 0 % cross-slope but 30 % along-slope — move them or accept graded trap pads); g1 (−24,6) is 4 m from the lane on the ramp's shoulder (33 %); the deck's east end (w7 at x=14) hangs over the T2/T3 face — that is the overlook, WS-37 builds it as such.
- **Sculpt-delta workflow (recorded polish, ADR-0018).** 1) Sculpt `L_<Map>_Terrain` in the editor. 2) Export the heightmap (Landscape mode → Export, 16-bit PNG, same size as `_height.png`; the export is in landscape layout, so rotate it back: `sim[row][col] = export[col][H-1-row]`). 3) Diff against the generated map: delta_m = (exported − generated) × (maxZ − minZ) / 65535; write `unreal/content/terrain/<map>_sculpt_delta.png` as 16-bit with value = 32768 + round(delta_m × 100) (1 unit = 1 cm, ±327 m). 4) Re-run `build_heightmap.py` (it applies the delta after flats and reports `sculptDeltaApplied: true`) and `DFTerrainImport`. A `record_sculpt_delta.py` that does steps 2–3 is the natural next tool; until then the delta is hand-made from the two PNGs. The generated map + delta is always the source; the Landscape never is.
- **Next.** Landscape material (`M_Landscape_Default` two-layer master — left on the engine default for now, WS-31's masters are the real answer), weightmap import from the kind masks as paint layers, `record_sculpt_delta.py`, the RVT blend, and the other five maps' terrain.json.

## Open questions
- OWNERSHIP.md puts `tools/ue-bridge/terrain/**` under WS-09 while PROGRAMME.md §5.4 lists the terrain lane under WS-30 — which glob is right?
- `unreal/Build/test.sh` (WS-15) greps for `Result=\{Failed\}` but UE 5.8's controller logs `Result={Fail}` / `Result={Success}`, so a failing run prints no verdict line and exits 1 only because the final `grep Passed` finds nothing under `pipefail` (seen on `DF.Editor.Terrain` this session; the truth was in `Saved/Logs/test-DF_Editor_Terrain.log`). Suggest matching `Result=\{(Fail|Failed)\}` and `Result=\{(Success|Passed)\}`.
- `terrain.schema.json` is not validated by `unreal/Build/validate-content-json.py` (it only walks `content/json/`); should the validator learn `content/terrain/*.terrain.json` (WS-01/WS-15)?
- Edit layers: in 5.8 they cannot be turned off (`ALandscapeProxy::CanHaveLayersContent()` returns `true` unconditionally; `bCanHaveLayersContent` is `_DEPRECATED`). `Import` writes the final heightmap directly and mirrors it into the default edit layer "Layer"; the GPU merge is skipped under `-nullrhi` (`CanUpdateLayersContent` needs `FApp::CanEverRender()`), so the saved final heightmap equals the import and the first editor open re-renders the same data. Nothing to decide — recorded so nobody tries to flag layers off.
- OFPA (ADR-0006 "OFPA on every level"): `L_<Map>_Terrain` and the placeholder `L_<Map>` are saved with actors *inside* the `.umap` (like WS-00's `L_Dev_Empty`). Reasons: the terrain sublevel is a one-actor product regenerated wholesale (a new actor GUID per run would churn `Content/__ExternalActors__/…` files); `OWNERSHIP.md` has no `__ExternalActors__`/`__ExternalObjects__` globs yet, so the first external-actor tree would fail `ownership-check.py` for every owner; and `SaveWorld`'s stale-external-package cleanup opens a dialog path a `-unattended` commandlet cannot answer. If INT wants OFPA on the generated sublevel, it is one call (`World->PersistentLevel->SetUseExternalActors(true)` before the first save) plus the ownership globs.
- Byte-identical regeneration (WS-30 DoD): the heightmap *data* is byte-identical (`build_heightmap.py` output and the imported samples; the landscape GUID is now `FGuid::NewDeterministicGuid("/DF/Terrain/<map>")`), but the `.umap` bytes are not — the actor and heightmap-texture GUIDs are fresh per run because the commandlet builds a new world each time. A reimport-in-place path (`FLandscapeEditDataInterface::SetHeightData` on the existing actor when the layout is unchanged) would make the package stable; proposed as the next step rather than done, since it changes the "fresh world" contract.

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-19 · session-75b58b1b/agent-ws30 (continuation after the rate-limit stop) · **landed**: rebased on `unreal/main` 9fc6e61 (editor-lock fix); `test_build_heightmap.py` 9/9 (python3 3.13); committed `unreal/content/terrain/out/` is byte-identical to a fresh `build_heightmap.py foundry` (10/10 files, 2.6 s); first successful `DFTerrainImport` run — the previous run had died at class lookup because `DFEditor` loaded at `PostEngineInit` (uproject → `Default`, Needs INT above); landscape GUID now deterministic per map; verify logs/checks collision, physmat, Nanite. Produced and committed via LFS: `/Game/DF/Maps/Foundry/L_Foundry_Terrain` (`Landscape_Foundry`: 3×4 components of 63 quads, 100 cm quads, 12/12 collision components, `PM_Grass`, Nanite on with the Nanite mesh built in-commandlet, bounds X [-8000, 10900] Y [-9500, 15700] Z [-279.2, 3576.6] cm = y [-2.792, 35.766] m == `foundry_height.json`; probes sim (0,0)=8.000 m, (30,10)=−1.710 m, (−40,−4)=15.873 m all equal the PNG), minimal `/Game/DF/Maps/Foundry/L_Foundry` streaming it always-loaded, `/Game/DF/Core/PhysicalMaterials/PM_Grass` (SurfaceType3). Second run idempotent ("already streams"); `-verifyonly` green; `DF.Editor.Terrain` 4/4 through `editor-lock.sh test.sh` (the Layout test's 126-quad expectation corrected to the engine's one-section-first policy — the previous session never ran it). `layering-check` OK; `ownership-check --ws 30` = the 9 PNG violations + warnings already listed under Needs INT. **left**: reimport-in-place for a byte-stable `.umap`; weightmaps/paint layers from the kind masks; `record_sculpt_delta.py`; landscape material + RVT (WS-31); the other five maps' `terrain.json`. **next**: pushed `ws/30-art-pipeline/terrain-lane`; PR to `unreal/main` needs the `cross-owner-ok` label and the Needs INT items.
