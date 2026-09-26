---
ws: 10a
slug: map-foundry
title: Map redesign: Foundry
state: active
owner: session-01Bqjmob-cloud
claimed_at: 2026-09-26T00:24:17Z
lease_expires: 2026-09-27T00:24:17Z
branch: claude/ws-10a-work-ue4a6i
last_commit: 
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

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

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

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-26 · session-01Bqjmob-cloud · **claim** — cloud session, no engine, on the user's ask. Taking INT's brief and its corrections as written: JSON, Python and ledger prose only, no C++, no editor commands. Order: `level.schema.json` first and wired into `validate-content-json.py` (with the list of fields where the contract, `FDFLevelFile` and `docs/MAP-AUTHORING.md` disagree), then a fresh `foundry.terrain.json`, then the new `foundry.level.json`, then the hand-over. The session is bound to branch `claude/ws-10a-work-ue4a6i` rather than `ws/10a-map-foundry/<topic>`; PRs go to `unreal/main` titled `[WS-10a] …` as usual.
