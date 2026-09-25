---
ws: 10a
slug: map-foundry
title: Map redesign: Foundry
state: unclaimed
owner: 
claimed_at: 
lease_expires: 
branch: 
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

## Session log
<!-- append-only: date · session · what landed · what's next -->
