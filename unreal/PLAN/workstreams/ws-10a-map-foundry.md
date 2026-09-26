---
ws: 10a
slug: map-foundry
title: Map redesign: Foundry
state: active
owner: session-01Bqjmob-cloud
claimed_at: 2026-09-26T00:24:17Z
lease_expires: 2026-09-27T01:01:55Z
branch: claude/ws-10a-work-ue4a6i
last_commit: 788db74
editor_heavy: true
phase: P2
size: L
critical: true
blocked_on: 
---
# WS-10a — Map redesign: Foundry

## Scope / DoD
**Scope.** terrain.json (works cut into a hillside: three slag terraces ~8 m each, deck overlook, vent tunnel through the spur, cinder-ridge belt) + new level.json using the old one as the brief; L_Foundry; navmesh; graybox kit; boss route; playtest layout iterated against the coverage report.

**Definition of done.** DF.Match.Solo.Foundry to victory; containment; validator green incl. LOS coverage; terrain proof for G2.

**Spec.** §3.2, C§1 Foundry landform, A1 foundry brief, B§1.15 (in `unreal/PLAN/PROGRAMME.md`). Size L, phase P2.

## Contracts I consume
- C6
- map-authoring-3d

## Contracts / interfaces I provide
- L_Foundry

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->
- 2026-09-26 · `unreal/content/schema/level.schema.json` **added** (the contract already names it canonical; no RFC). Two readings in it are mine and are the ones a WS-09 reader must follow: air routes declare `"agl": true` and carry `[x, agl, z]`; `bossRoute` is `{routeId}` naming a ground route. `areas.kind` gains `controlPoint` (see Needs INT). Dependents: WS-09 (loader/importer), WS-10b–f (their level files), WS-01 (owns `schema/`).
- 2026-09-26 · `unreal/Build/validate-content-json.py` (WS-15's) now validates levels and terrain, as INT asked; same CLI, a map id selects that map's files; prints a non-failing `warn` when `terrain/out/<map>_height.json` was built from a different spec.
- 2026-09-26 · `tools/ue-bridge/terrain/predict_level.py` **added** in WS-30's directory: offline prediction of DF.Map.Validate against a terrain spec. Reads only; imports `build_heightmap.build`.

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->
- **WS-09, before Foundry is imported:** `FDFLevelFile` must read an air route's `agl` and `DFLevelImport` must place each air waypoint at ground + y when it is set (today air y is kept as absolute, so Foundry's air lane, authored 9–13 m AGL, would run inside T1 at +16). Lifting the authored waypoints is enough: they are dense enough that straight lines between them stay within 0.9 m of the authored AGL (rule 3 allows 2).
- **WS-09:** read `bossRoute` into `UDFLaneGraphAsset::BossRoutes` (Via = the edges of the named route's itinerary). Until then rule 13 has no subject.
- **WS-09 / graybox:** nothing builds `volumes[]`, so the deck (+14) does not exist after import and the 11 wall pads float; the deck station projects to T2 below it. A graybox deck (box `[-4, 13.8, -14]`, size `[16, 0.4, 10]`) is needed before a playtest.
- **WS-09:** the importer reads `pad` and cuts nothing. The layout does not depend on it (every ground pad stands on ≤ 22.7 % ground, predicted), but rule 4's ≤ 5 % pad does.
- **WS-30 / INT:** `unreal/content/terrain/out/` is **stale**: it is WS-30's glob and git-lfs, which this session has neither, so the outputs were not regenerated. The validator warns about it. Regenerate and commit them on the GPU box (first command of the hand-over) under WS-30 or with `cross-owner-ok`.
- **OWNERSHIP.md row 28:** swap `L_Foundry.umap` to WS-10a now that it is claimed (the row says INT does this at claim time).
- **MAP-AUTHORING §2.5:** `controlPoint` is not one of the seven area kinds, and A1 gives Foundry a control point. The schema accepts it; whether it is a new kind or a mutable (C6 has no control-point kind either) is INT's call.
- **WS-27:** the ground lane is 169.5 m against the legacy 126 m (the user chose a lane the boss can walk over a shorter one; see Session log). Every Foundry wave walks ~35 % further; wave spacing and bounty pacing were tuned on the flat map.

## Open questions
- **Rule 14 (spawn shadow) is predicted to fail on terrain alone:** 10 ground pads can see the portal at (-50, 13, 28) down the notches the climb and the haul ramp cut in the pit rim (`predict_level.py` lists them under `spawnShadowTerrainOnly`). DF.Map.Validate does not implement rule 14. A portal mesh with side walls may supply the 2 m shadow; if not, the fix is a deeper pit (every metre deeper is 4 m more climb at 25 %) or a gate in an alcove. A dog-leg apron was tried and did not help (8 pads still saw it).
- **g3 (-31, 21) is load-bearing:** it sits on the island inside the haul loop and removing it leaves 19 segments dead (the climb, the landing, the ramp head). 37 of 84 live segments are covered by exactly 3 pads, so anything the terrain-only prediction cannot see (the deck, dressing, the portal) can open dead ground; the engine report decides.
- **Nova's projectile gravity** is not in `towers.json`; the predictor assumes 9.81 m/s² at 14 m/s. The G2 pair's 36.8° launch depends on it.
- `maps.json` has `lesson: ""` for Foundry while the level file carries `brief.lesson`; one of them should be the source (WS-09).

## Assignment from INT (2026-09-25) — written for a session with no engine

**This workstream is offered to the cloud session, and it is chosen deliberately.** A session with no
Unreal Engine, no GPU and no compiler cannot verify C++, and right now nothing else can either: since
`fcc1e1d` — the last commit anything actually compiled — **43 commits and roughly 9,130 lines of C++
have landed on `unreal/main` unverified**, across DFUI, DFPlayer, DFEnemies and DFCore. Zero
self-hosted runners are registered, so the `unreal-win` lane reports *skipped*; `unreal-checks` is
green but it is the Python lane and does not compile anything. Adding more unbuilt C++ to that pile is
borrowing against an account nobody can currently pay.

WS-10a is the opposite of that. ADR-0018 made terrain **text-authored on purpose** — a `terrain.json`
feature spec goes through `tools/ue-bridge/terrain/build_heightmap.py` to a 16-bit heightmap and layer
masks, deterministically, and hand sculpting is a later polish layer that is never the source. So the
authoring *and its verification* are JSON and Python. `build_heightmap.py` ships its own test suite
that needs no engine. And Foundry is on the critical path: G2 — the vertical slice, the first time this
is a game rather than a library — happens on this map.

### The hard constraint
**Deliver JSON, Python and ledger prose. Do not write C++ and do not run editor commands.** If the work
seems to need `-run=DFTerrainImport`, `-run=DFMapValidate`, a build or a test suite, that is the seam:
stop, write down exactly what you want run and what you expect it to say, and hand it over. A finished,
self-consistent pair of text files is worth far more than a half-finished import nobody can reproduce.

### Claim it yourself
Per §6.2, edit **only** this file's frontmatter (`state: active`, `owner:` your own session handle,
`claimed_at`, `lease_expires` = +24 h, `branch: ws/10a-map-foundry/<topic>`) and push that one-file
commit straight to `unreal/main`. Do not let another session claim on your behalf and do not adopt a
handle that is not yours.

### What already exists — you are refining, not starting blank
- `unreal/content/terrain/foundry.terrain.json` and `unreal/content/terrain/out/` — WS-30 built the
  terrain lane against a Foundry landform and committed the generated heightmap, masks and preview.
  Read it before you write: your job is the **map's** landform, and some of it may already be right.
- `unreal/content/levels/legacy/foundry.level.json` — the Godot map. This is the **brief, not the
  output**: take its lessons, socket counts and route-length ratios, then re-lay everything on real
  terrain. B§1.15.
- `unreal/content/levels/reports/foundry.coverage.json` — WS-09's validator already produced a coverage
  report (54 segments, 0 dead ground, 379 traces) against the imported legacy map. Iterate against this
  artifact's shape; it is what "the coverage report drives the socket layout" means.
- `unreal/PLAN/CONTRACTS/map-authoring-3d.md` — the validator's rules, as a checklist to self-check
  against, including that every `routeId` in `waves_foundry.json` must resolve to an itinerary id.

### The landform (C§1, and this file's Scope)
A works cut into a hillside: **three slag terraces stepping ~8 m each** from the spawn gate down to the
core basin; the **deck a real overlook** on the middle terrace; the **vent tunnel bored through the
spur** between terraces; slag heaps and a **cinder ridge as the containment belt**; fog pools in the
basin, which is why W9's fog is a Foundry condition and not decoration. Dressing — materials, crucible
light, embers, Megascans — is **WS-37's**, not yours. You author relief and layout.

### The brief from A1, to hit or to beat with a reason
| | Foundry |
|---|---|
| Field | 110×80 m playable, plus the belt |
| Waves | 10 |
| Routes | ground; air **9→13 m** (authored **AGL**, not absolute) |
| Sockets | **24 ground / 11 wall / 11 trap** |
| Features | bridge ladder, launcher pad, zipline, control point |
| Conditions | W9 fog |
| Lesson | **"layers & armour"** — record it in the level file's `brief` field |

### The numbers the validator will hold you to (§3.2)
- Lane corridor area class `Lane`, **3.4 m**; **max lane grade 30 %** (enemy-walkable). Warp legs unchanged.
- Navmesh agent **slope 35°, step 45 cm**; player CMC **walkable 45°, step 45 cm**.
- Uphill **×0.85 above +15 %**, downhill **×1.1 below −15 %** — symmetric band, threshold is the WS-27
  dial `slopeGradeThresholdPercent` = 15, and grade is measured **per waypoint segment**, so a climb
  slows exactly the stretch that climbs. Design with it: a climb is a kill zone, for free.
- Tower sockets sit on a levelled **1.2 m pad**; the validator **refuses a socket on >25 % ungraded
  ground**. Air lanes **AGL 8–15 m**. Boss routes **≤15 %**. Vehicle roads **≤12 %**.
- Line of sight is **real** — `DF_Sight` traces terrain and static world. A ridge is a design tool, so
  place sockets knowing Nova and Flak fire indirect while Lance and beams need LOS.

### Definition of done, as G2 will actually test it
§3.2's last bullet is the acceptance test, and it is behavioural rather than cosmetic:
1. Foundry's terrain has **≥8 m of relief on the lane**.
2. **A Lance loses LOS behind a terrace and a Nova lobs over it.**
3. **Drifters slow on the climb.**
4. **The validator's coverage report drives the socket layout** — ≥3 covering sockets per lane segment
   under real traces, and the dead-ground report is what you iterate against, not taste.
Design so that someone with an engine can demonstrate all four. Write, in your session log, the
specific check you expect each one to produce.

### How to verify your own work without an engine
- `python3 -m pytest tools/ue-bridge/terrain/` — the terrain lane's own tests.
- `python3 tools/ue-bridge/terrain/build_heightmap.py` on your `terrain.json`, twice: the output must be
  **byte-identical** both times. Determinism is the property that makes text-authored terrain reviewable.
- `python3 unreal/Build/validate-content-json.py` — schema validation for the content and level files.
- A short throwaway Python script that cross-checks every `routeId` in `unreal/content/json/waves_foundry.json`
  against the itinerary ids in your new level file. WS-05 did exactly this for all five maps; a rename
  that misses the wave table is the failure the rule exists to catch.

### Hand-over, when you are done
Leave in the ws file: the exact commands you want run on a machine with the engine, in order, with what
each should print; every number you chose and why; and anything you could not verify. The next session
should be able to import, validate and playtest without re-deriving your reasoning.

### Corrections to the above, from a source audit (INT, 2026-09-25)

Five readers checked the brief against the tree. Most of it held; **these did not, and two of them
would have sent you down a dead end. They override anything above that contradicts them.**

**1. `build_level.py` does not exist.** §3.2 names it as the thing that imports the heightmap and
auto-cuts socket pads. There is no such file. The importers are C++ editor commandlets —
`-run=DFTerrainImport`, `-run=DFLevelImport`, `-run=DFMapValidate`. So **you cannot cut a pad, project a
route onto the surface, or test lane walkability by any means available to you.** Author the intent in
the level file and let the engine-side run do the cutting. Sample `unreal/content/terrain/out/foundry_height.{png,json}`
directly if you need to reason about heights — that is what WS-30's note suggests and it is pure Python.

**2. Do not use `pytest` — it is not installed.** `test_build_heightmap.py` is a standalone script with
its own `__main__` runner. Run `python3 tools/ue-bridge/terrain/test_build_heightmap.py`; it passes 9/9.
`numpy` and `PIL` are also absent, by design (`build_heightmap.py:13` says so) — the script ships its own
`read_png_gray`, so use that to inspect output rather than reaching for PIL.

**3. `validate-content-json.py` does not validate terrain or level files.** It covers
`unreal/content/json/`. `terrain.schema.json` exists with **no consumer**, and `unreal/content/levels/`
is validated by nothing in pure Python. So the self-check list above overstated what you can lean on.

**4. `level.schema.json` does not exist**, though `map-authoring-3d.md:3` names it as canonical.
**Write it first — this is now the highest-value single deliverable in the workstream and I am making it
an explicit ask.** It is pure JSON Schema, fully verifiable where you are, it turns the level half of
every future map from prose into something machine-checkable, and it closes the gap that let this
contradiction sit unnoticed. Derive it from `map-authoring-3d.md` §1, the C++ loader `FDFLevelFile`, and
`docs/MAP-AUTHORING.md` §2, and note in the ws file every field where those three disagree — that list
is itself worth having. Then wire it into `validate-content-json.py` so terrain and level files are
covered too.

**5. The socket numbers in §3.2 and in the contract disagree, and the contract governs your authoring.**
`map-authoring-3d.md:13`: pad **≥1.1 m** (ground/trap) or a wall face; **pad slope ≤5 % after the cut**;
surrounding ungraded slope **≤25 %**; sockets **≥3.5 m off any lane centreline**. §3.2's "1.2 m" is what
the importer cuts for `pad: true`, not a minimum you must clear. I did not include the ≤5 % or the 3.5 m
rule above; both are binding.

**6. The boss route has two conditions I omitted** (`map-authoring-3d.md:22`): no operated gate it cannot
break, and **at least one nest overlooking ≥50 % of it** — on top of 8 m clearance, ≤15 % grade and no
warp legs.

**7. Rulings on the questions the audit found genuinely open** — decide these my way or argue back:
- **WS-30's committed `foundry.terrain.json` is a reference, not your starting point.** Its geometry is
  pinned to the *legacy* socket and route positions and WS-30 itself calls it "legacy brief, not the
  redesign". **Author a fresh landform** in the same file, keeping its structure and its `note` discipline.
  Say in your first commit what you kept and what you replaced.
- **"Three slag terraces stepping ~8 m each" means two 8 m steps, ~16 m of total terrace drop** (T1 +16 /
  T2 +8 / T3 0), which is how the committed file reads it. G2 needs **≥8 m of relief on the lane**, so 16 m
  across the lane's descent clears it with margin. If you want 24 m, say why in the ws file.
- **A1 says 24 ground sockets; the legacy file has 23.** A1 is the brief: aim for 24, and if the terrain
  argues for a different count, record the number and the reason. The validator reads the file, not A1.
- The vent spur's side and the slag heaps' count and placement are **unspecified in C§1** — WS-30 put the
  spur north between T1 and T2 and three heaps in the belt. Yours to decide; write down the choice.

**8. Two things nobody can close yet, so do not promise them.** Contract rule 2 (the 3.4 m walkable
corridor) is a hard SKIP in the validator — it reports "no navmesh: not checked, not passed" — and ten of
the sixteen rules have no implementation at all. The committed `foundry.coverage.json` was produced from
the **legacy flat** level, so it is a shape to imitate, not a baseline your redesign can be scored
against until someone runs the validator on your file. Plan the hand-over knowing the engine-side pass
will be the first real measurement.

## level.schema.json: where the three sources disagree (session-01Bqjmob-cloud, 2026-09-26)
INT asked for this list (correction 4). Sources: **C** = `CONTRACTS/map-authoring-3d.md` §1–3, **L** = the
loader `FDFLevelFile::Load` (`DFWorld/Private/LaneGraph/DFLevelFile.cpp`) and what `DFLevelImport` does with
it, **M** = `docs/MAP-AUTHORING.md` §2. The schema's choice is in the last column. Rows marked **engine** need
a WS-09 change before an authored 3D file imports correctly; the rest are settled by the schema alone.

| Field | C says | L does | M says | Schema |
|---|---|---|---|---|
| **Air route heights** (**engine**) | `agl`, 8–15 m above ground, "instead of absolute Y" | no `agl` field; the importer never projects air points and keeps y as absolute (its comment: "3D: AGL, which is the same thing over a flat floor") | y 8–15 absolute over decks, 9 on a flat map | air routes must carry `"agl": true` and every waypoint is `[x, agl, z]` with agl in [8, 15]. **Until the importer adds the ground height under each air point, an authored air lane lands inside the hill** (Foundry's T1 is at +16 m). |
| Socket pad | `pad: true` → importer cuts a level **1.2 m** pad; rule 4 says pad **≥ 1.1 m**, **≤ 5 %** after the cut | reads `pad` from the **object form only**; no pad size, yaw or slope field (C6 has `PadYaw`, `PadSlopePercent` with no level-file source) | 1.1 m radius visual, 1.2 m collision | `pad` on the object form only. Reading M: 1.1 and 1.2 are both radii, so C and §3.2 do not actually conflict. |
| Socket tag / route layer case | — | case-insensitive | lowercase | authored: lowercase; legacy: capitalised (as exported) |
| Anchors | — | top level **or** under `anchors` | under `anchors` (+ `stations`) | authored: `anchors{heroSpawn, armory, stations}`; legacy: top level |
| Conditions | — | reads **both** `conditionSchedule` and `conditions` and merges them | `conditions` | authored: `conditions`. Keys are **0-based wave indices** (`Conditions.cs:111`), so A1's "W9 fog" is `{"8": "fog"}`; M's example does not say. |
| `brief` | §1: "the lesson and the design intent, prose"; §3: `brief.lesson` + `brief.intent` | ignored | — | object: `lesson`, `intent` required; `legacy`, `decisions[]` optional. (`maps.json` also has a `lesson` field, currently `""` for Foundry — two homes for one sentence.) |
| `bossRoute` (**engine**) | singular, shape unspecified | ignored; `BuildFromLevel` resets `BossRoutes` | — | `{routeId, id?, clearanceMetres?}` naming a ground route, matching C6's `BossRoutes[] {Id, Via[]}` (the Via is that route's edges). |
| Vehicle roads | `vehicleRoads[]`, splines with `maxGradePercent` | ignored | `roads[] {kind, width, points}` (the surface a vehicle reads) | `vehicleRoads[] {id, kind, width, points, maxGradePercent ≤ 12}`; `roads` not accepted in authored files. |
| `pond` | — (water is `terrain.json` `water[]` now) | ignored | `pond {at, radius}` | dropped from authored files. |
| `mutables`, `containerGates`, `nests`, `caches`, `barrelSpawns` | named, no shapes | ignored | — | `mutables` from C6 `FDFMutableDef`; `nests {id, at, overlooks[]}`; the other three are **provisional** `{id, at}` with extra keys allowed until an owner specifies them. |
| `volumes`, `areas`, `place`, `label`, `kit` | — | ignored | §2.4–2.6 | accepted as M specifies. `areas.kind` adds **`controlPoint`**: A1 gives Foundry a control point and M's seven kinds have no way to say so (M calls an eighth kind a code change — flagged, not assumed). |
| Teleport legs | — | not checked | never first/last, never two in a row, never on air | enforced by the validator. |
| `field` | — | default 110×80 when absent | "fixed, see §3" | required; the validator checks it equals the terrain file's `bounds.playable`. |
| `$schema` | — | read, unused (`bLegacy` comes from the path) | `deepfield-level/1` | the discriminator: `deepfield-level/1` or `deepfield-level/legacy`. |

## Hand-over (session-01Bqjmob-cloud, 2026-09-26)

**What landed** (branch `claude/ws-10a-work-ue4a6i`; the session was bound to that name): `level.schema.json` and the validator (above); a fresh `unreal/content/terrain/foundry.terrain.json`; the first authored `unreal/content/levels/foundry.level.json` (`deepfield-level/1`); `tools/ue-bridge/terrain/predict_level.py`; its report `unreal/content/levels/reports/foundry.predicted.json`. No C++, no editor commands, no binaries.

**The design in one paragraph.** T1 +16 (west: the tapping pit round the spawn gate, a spur tongue and a north shoulder), T2 +8 (middle: the deck and the hero yard south of the spur, the control point north of it), T3 0 (the south shelf and the east), basin −2.5 at the core (43, 0). The lane is one haul road: an 8 m apron on the pit floor, **one climb** (3 m over 12 m, 25 %), a level landing, then a constant **13.5 % descent** for 137 m through the shoulder and the spur, round the deck's corner, along a bench under T2's south lip and into the basin. The boss walks the same road but goes round the climb on the pit's haul ramp (10 %); `DFLaneGraphBuilder` step 2b promotes `climbTop` and `haulBend` to nodes, so the ground itinerary is pinned to the climb and the boss's to the ramp. Every number and its reason is in the level file's `brief.decisions` and each terrain feature's `note`.

**Predicted, from the terrain alone** (`python3 tools/ue-bridge/terrain/predict_level.py foundry`): 88 segments (4 apron), **0 dead**; socketOffset, spawnApron, routeIds clean; lane max 25 % (the climb), everything else ≤ 13.6 %; boss route 13.57 %, no warps, `deckRoof` overlooks 83 %; every ground pad on ≤ 22.7 % ground; air lane 9 m minimum AGL, ≤ 0.9 m off the authored AGL; relief along the lane 18.5 m; ground:air 169.5:124.3 = 1.36 (legacy 1.48, allowed 1.19–1.78).

**Commands for a machine with the engine, in order, with what each should print.** Do the WS-09 air-AGL item in Needs INT first, or step 3's air coverage is measuring a lane inside the hill.
1. `python tools\ue-bridge\terrain\build_heightmap.py foundry` → `foundry: 191x161 @ 1.0 m, y in [-2.5, 30.5175] m, origin (-95.0, -80.0), sculpt delta none`, `roadbed bossHaul: 43.849 m, max grade 10.09 %`, `roadbed laneBed: 167.929 m, max grade 25.0 %`. Run it twice: `out\` must be byte-identical. Commit `out\` (LFS). `python unreal\Build\validate-content-json.py foundry` then stops printing the `warn` line.
2. `git lfs lock` both Foundry umaps, then `"%UE%" "%PROJ%" -run=DFEditor.DFTerrainImport -map=foundry -nullrhi -unattended -nop4 -nosplash -NoSound` → bounds Z [−250.0, 3051.8] cm (= y [−2.5, 30.5175] m, as `foundry_height.json`), X/Y unchanged from WS-30's run (same 191×161 grid); probes sim (0, 0) = 8.007 m, (30, 10) = 1.928 m, (−40, −4) = 15.875 m.
3. `"%UE%" "%PROJ%" -run=DFLevelImport -map=foundry -nullrhi -unattended -nop4 -nosplash -NoSound` (no `-legacy`: the primary file now exists and wins) → `foundry: projected N points onto DF_LaneSurface (0 had nothing below them)`. A non-zero second number means the Landscape is not on the LaneSurface channel.
4. `"%UE%" "%PROJ%" -run=DFMapValidate -map=foundry -nullrhi -unattended -nop4 -nosplash -NoSound` → sealing PASS, spawnApron PASS, socketOffset PASS (closest pad g3 at 4.0 m), corridor SKIP (no navmesh), coverage PASS with about 88 segments (4 apron) and 0 dead, routeIds PASS. Commit `reports/foundry.coverage.json`. **Any dead segment: diff it against `foundry.predicted.json`** (same segment keys, `edge@t`), and `predict_level.py foundry --suggest 10` lists where a pad would fix it. A difference there is either static world the prediction cannot see or a difference in how the engine segments an edge; both are worth writing down here.
5. G2, as the vertical-slice checks will run them:
   - **Relief ≥ 8 m on the lane:** the lane graph's westGate node is at 13 m, climbTop and landing at 16 m, core at −2.5 m (18.5 m).
   - **Lance loses LOS behind a terrace, Nova lobs over it:** build a Lance on **g14** (16, 0, −37), on the south shelf below the bench. For an enemy on the bench at about (8.2, 4.9, −29) the Lance is 11.2 m away and within its pitch (24.5°), but the bench edge blocks its sight line, so it does not fire; a Nova on g14 hits the same enemy with a ~37° lob. Predicted, g14 has no direct sight of any lane segment at all, so a Lance there should stay silent all wave.
   - **Drifters slow on the climb:** edge `pitFloor-climbTop`, 12 m at +25 %, the only stretch above +15 %: a drifter takes 12 / (2.5 × 0.85) = 5.6 s instead of 4.8 s. Nothing else on the map is outside the ±15 % band, so no stretch is sped up.
   - **The coverage report drives the layout:** the 24 ground pads were placed greedily against the predicted dead-ground report and pruned to A1's count while every segment kept ≥ 3; step 4's report is the first real measurement of that.
6. Playtest (DoD: `DF.Match.Solo.Foundry` to victory) needs the deck graybox, the pad cut and the rest of Needs INT; not attempted.

**Not verified by anything:** rule 2 (the corridor; navmesh), rule 7 (reachability on foot: the stations are on T1, T2, the deck and T3, all joined by the lane itself), rule 8 (containment: the cinder ridge's inner foot is the playable edge), rules 10–12 and 15, and everything a terrain-only trace cannot see.

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-26 · session-01Bqjmob-cloud · **claim** — cloud session, no engine, on the user's ask. Taking INT's brief and its corrections as written: JSON, Python and ledger prose only, no C++, no editor commands. Order: `level.schema.json` first and wired into `validate-content-json.py` (with the list of fields where the contract, `FDFLevelFile` and `docs/MAP-AUTHORING.md` disagree), then a fresh `foundry.terrain.json`, then the new `foundry.level.json`, then the hand-over. The session is bound to branch `claude/ws-10a-work-ue4a6i` rather than `ws/10a-map-foundry/<topic>`; PRs go to `unreal/main` titled `[WS-10a] …` as usual.
- 2026-09-26 · session-01Bqjmob-cloud · **`level.schema.json` landed** — `unreal/content/schema/level.schema.json` (authored `deepfield-level/1` and legacy branches) and `validate-content-json.py` extended to levels and terrain: `$ref`, `oneOf`/`anyOf`, `minimum`/`maximum`, `pattern`, `minLength`, `prefixItems`, plus the cross-file rules (unique ids, teleport legs, gate sockets, condition ids, boss route, waves routeIds, field vs terrain bounds). All five legacy files and WS-30's `foundry.terrain.json` pass; 18 hand-made broken files each fail with the right message. The disagreement table above is the other half of the ask. Next: the Foundry landform.
- 2026-09-26 · session-01Bqjmob-cloud · **Foundry landform and level landed** — the user chose the long gentle lane with the boss on it over a legacy-length lane with no boss route (asked: the boss rule and 18.5 m of drop cannot both be met by a 126 m lane). WS-30's `foundry.terrain.json` replaced (kept/replaced list in its `note`); first `deepfield-level/1` file; `predict_level.py` built to iterate the socket layout against the terrain, since nothing else here can; its report committed. `build_heightmap.py` twice byte-identical; `test_build_heightmap.py` 9/9; validator green; layering OK; ownership 0 violations. Not done: `terrain/out/` (LFS, WS-30's glob), rule 14, everything engine-side. Next: the Hand-over's commands on the GPU box, after WS-09's air-AGL change.
