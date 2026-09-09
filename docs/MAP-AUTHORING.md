# Map authoring — the contract between Claude Design and the game

**Status:** proposal, written 2026-09-09. Nothing in the pipeline implements
this yet; the follow-on work is listed at the end.
**Audience:** Claude Design first, this repo second.
**Companions:** `docs/DESIGN-BRIEF.md` §3 (map briefs),
`docs/FORWARD-MANIFEST-switchyard.md` (the asks that came out of the last three
passes), `docs/design-system/README.md` (running gap list).

---

## 1. The problem, stated precisely

**No one designs the maps.** Not as an insult to anyone — structurally, there
is no step where a person or a model lays out a map and signs it off. There
are two halves, authored independently, and the map is whatever emerges when
they meet:

| Half | Lives in | Decides |
|---|---|---|
| The plan | `sim/Sim.Core/Content/Maps.cs`, 465 lines of C# | routes, socket positions and tags, spawn, armory, hero stations, wave count, weather schedule |
| The build | `game/scripts/GameRoot.cs`, ~750 lines across three per-map methods | every deck, wall, ladder, zipline, launcher, every prop position, every piece of decor |

Claude Design supplies neither. It supplies a **kit** — good props, correctly
drawn, with no say in where any of them go or what the space is.

That is the whole diagnosis. The props are not the problem; three passes over
Switchyard did not improve a single model. What was wrong every time was the
*plan*, and the plan had no author.

### The evidence

- **`docs/design/models/levels.js` exists, points the wrong way, and has drifted
  so far that design has never seen the maps that ship.** It already has
  exactly the right shape — `routes`, `sockets`, `heroSpawn`, `armory`,
  `stations`, and a `place` list of kit instances — and `Maps.html` renders it
  as a walkable assembly with route ribbons and socket markers. But its own
  header says it was *"lifted from `sim/Sim.Core/Content/Maps.cs` and
  `game/scripts/GameRoot.cs`"*. It is a **mirror of our code, not a source**,
  and mirrors go stale:

  | | in `levels.js` | in the game |
  |---|---|---|
  | Foundry sockets | 12 | **44** |
  | Switchyard sockets | 16 | **44** |
  | Switchyard railway | *nothing* — no lane modules, no cut channel, no retaining wall | a main line, three roads, a ladder, a freight cut, a depot road, two overbridges, three portals |

  When design previews Switchyard it is looking at a sparse yard with a
  quarter of its build pads and no railway at all. Every judgement made in that
  viewer has been made about a different map.

- **The Spire is broken in three places at once**, which is why it is the worst
  of the three:
  1. `docs/design/models/spire.js` defines a 15-piece kit — floor bays, facade,
     lobby, roof, parapet, fire escape, stairwell, skybox, HVAC, water tank,
     antenna. **`tools/design-export/export.html` does not import it.** It
     imports `FOUNDRY`, `SHARED` and `SWITCHYARD` and stops.
  2. So **zero `spire_*` files exist** in `game/assets/maps/`.
  3. And `spire_` is **not registered in `AssetLibrary.Routes`**, so even if
     they were delivered nothing would resolve them.

  The result is a 40 m tower with 66 sockets built entirely out of untinted
  coloured boxes, and a layout invented in 118 lines of C# by whoever wrote it.
  It is not a map that came out badly. It is a map that was never designed.

- **Every defect the last three passes found was a plan defect.** Eight phantom
  tracks baked into the ground tile. A cutting laid a quarter-turn wrong for
  its whole length. A running line through four tower pads. A depot road four
  metres from the spawn. Overbridges reaching eleven metres into the playfield.
  A ladder that topped out under the deck it served, taking a whole tier and
  the air lane with it. A marker post every four metres. A ladder angle sharper
  than any set of points ever laid. **Not one of those is visible in a
  screenshot, and not one of them is an art problem.**

### The shape of the fix

Reverse the arrow on `levels.js`.

```
  today      Maps.cs + GameRoot.cs  ──lifted──▶  levels.js  ──▶  Maps.html preview
                    ▲                                                (a mirror)
                    └── authored by whoever touched it last

  proposed   Claude Design ──authors──▶  <map>.level.json  ──▶  Maps.html (walk it)
                                              │
                                              ├──▶ generated Maps.cs   (the sim)
                                              └──▶ client layout       (the build)
```

One document per map, authored in Claude Design, is **the map**. Our two halves
become consumers of it. Design keeps the viewer it already has to walk the
space before anything is built.

This document specifies that file and the rules it must satisfy.

---

## 2. What Claude Design authors

One `<map>.level.json` per map — the same data `levels.js` already carries, plus
the three things it is missing (**volumes**, **areas**, **waves**) that today
only exist in C#.

```jsonc
{
  "$schema": "deepfield-level/1",
  "id": "spire",
  "label": "The Spire",
  "kit": "spire",                  // asset prefix; must be registered our side
  "field": [110, 80],              // playable extent, metres (x, z) — fixed, see §3
  "totalWaves": 12,

  "routes": [ … ],                 // §2.1
  "sockets": [ … ],                // §2.2
  "anchors": { … },                // §2.3
  "volumes": [ … ],                // §2.4  ← new: the solid world
  "areas": [ … ],                  // §2.5  ← new: traversal
  "place": [ … ],                  // §2.6  decor, as levels.js already does
  "conditions": { "4": "fog", "9": "night" }
}
```

### 2.1 Routes — where enemies walk

```jsonc
{
  "id": "groundShort",
  "layer": "ground",               // "ground" | "air"
  "waypoints": [[-45,0,-10], [-20,0,-2], [5,0,0], [40,0,8]],
  "barricadeGate": "b1",           // optional: usable only while b1 is empty
  "fallbackRouteId": "ground"      // required if barricadeGate is set
}
```

A route is a polyline, walked at constant speed, corner to corner. Enemies
choose their route **at spawn and never re-path**, so a route is a commitment,
not a suggestion. Every ground route ends at the core.

A **gated route** is the shortcut/long-way decision that gives a map its
tactical spine: while the barricade slot is empty, enemies take the short way;
build a barricade and they take the fallback. Foundry and Switchyard each have
exactly one. Design owns whether a map has one and where it bites.

### 2.2 Sockets — where players build

```jsonc
["g8", [-38, 0, -1], "ground"]     // id, position, tag
```

| Tag | Meaning | Sits on |
|---|---|---|
| `ground` | ordinary tower pad | the ground plane |
| `wall` | tower pad on a deck, catwalk or ledge | a `volume` you authored |
| `trap` | path plate — laid **in** the lane, on the route line | the lane surface |
| `barricade` | the gate slot a shortcut route names | the lane, at the choke |

Socket ids are permanent. They appear in save data, in probe output and in the
brief; renaming one is a breaking change.

### 2.3 Anchors

```jsonc
"anchors": {
  "heroSpawn": [0, 0, -26],
  "armory":    [6, 0, -26],
  "stations":  [["yard",[0,0,-22]], ["midDeck",[-6,5,-17]], ["catwalk",[2,10,3]]]
}
```

Stations are the auto-hero traversal graph — the places a bot-controlled hero
will stand. There should be one per fightable tier, and each should be
somewhere a human would actually choose.

### 2.4 Volumes — the solid world *(new)*

Everything a player can stand on, walk into or be stopped by. Today these are
`AddStaticBox` calls in C# and design has no say in them, which is why decks and
sockets have to be hand-matched and why `w1` once hung in space.

```jsonc
{
  "id": "midDeck",
  "box": { "at": [-7, 4.8, -12], "size": [26, 0.4, 8] },
  "solid": true,                   // false = visual only, no collision
  "surface": "switchyard_middeck", // module tiled across it, optional
  "run": "x",                      // which axis the module tiles along
  "piece": 4.0
}
```

Rules:
- Every `wall` socket must sit on a `solid` volume, with its pad **inside** the
  footprint by at least 1 m. This is checked (§4).
- A volume with `surface` set has its graybox hidden and the module tiled
  across it. Without a delivered module the graybox shows — which is exactly
  what the whole Spire looks like today.

### 2.5 Areas — traversal *(new)*

The seven kinds the client understands. Nothing else is available; asking for
an eighth is a code change, and worth asking for if a map needs it.

| kind | parameters | behaviour |
|---|---|---|
| `ladder` | box | hold to climb; **must top out level with a surface** |
| `zipline` | box, `to: [x,y,z]` | hold to ride to the far anchor |
| `launcher` | box, `velocity: [x,y,z]` | hop pad; arcs the player somewhere specific |
| `teleporter` | box, `padId` | paired pads |
| `elevator` | box | cargo lift between two levels |
| `nest` | box | sniper perch marker |
| `armory` | box | the gunsmith kiosk trigger |

The single most expensive bug this project has had was a ladder whose rungs
ended against the **underside** of the deck it served. It looked correct from
every angle and from the map file. It made a whole tier unusable, and on
Switchyard that tier carried the only four sockets that cover the air lane — so
one wrong number made an entire enemy layer undefendable. **Every climb must be
authored to end level with something to step onto**, and §4 checks it.

### 2.6 Place — decor

Unchanged from what `levels.js` already does:

```jsonc
["switchyard_dress_railcar", [6, 0, 24.5], 0]      // model, position, yawRadians, scale?
```

Two rules, both learned expensively:

- **Nothing decorative may sit within 3 m of a route, a socket, the spawn, the
  armory or a station.** Decoration has no collision, so it does not block —
  it just looks like the map does not know where its own gameplay is.
- **A module that tiles may only contain features true at its repeat
  distance.** See §5.

---

## 3. The confines — what is actually possible

Design should know the walls of the room before drawing in it.

### The sim can model

- Ground and air routes as polylines; constant speed along them.
- One gated shortcut per map, with a named fallback.
- Four socket tags.
- A per-wave weather schedule (`fog`, `night`, `storm`, `heatwave`, …).
- Tower range as a **3D radius**, 9–16 m at level 1, up to about 2.5× that at
  L10 on the range path.

### The sim cannot model

- **Pathfinding.** There is no navmesh and no avoidance. Enemies walk their
  polyline through anything. A wall that is not on the route does not turn
  them; it just intersects them.
- **Re-routing mid-walk.** Route choice is made at spawn. A barricade built
  while enemies are walking does not redirect the ones already committed.
- **Moving or destructible geometry**, beyond what M4's map-morph entries will
  add.
- **Height as cover.** Line of sight is checked, but there is no partial cover,
  no elevation damage bonus, nothing that makes "high ground" mean anything
  except reach.

### The client can build

Solid and non-solid boxes; the seven area kinds; props mounted on volumes;
modules tiled along a run; a spanned piece between two points (ziplines); a
20 m terrain tile grid; scatter placed with clearance testing; a skybox dome.

### The client cannot build

- **Terrain relief.** The ground is one flat slab at y 0 with a tiled surface.
  There are no hills, no true cuttings, no ramps that are not authored volumes.
  Switchyard's "freight cut" is a channel *drawn* at grade for exactly this
  reason, and design's own note says so.
- **Curved routes or curved track.** Everything is straight segments.
- **Per-map field size.** The ground slab is **110 × 80 m for every map**
  (x ±55, z ±40), and terrain tiles cover x −50…50, z −30…30. Maps get taller,
  not wider. If a map needs a bigger field, that is a code change — ask.

### Fixed numbers to design against

| | |
|---|---|
| Field | 110 × 80 m, y 0 at grade |
| Ground lane width | 3.4 m |
| Air lane ribbon | 1.2 m, typically y 8–15 |
| Linear module repeat | 4 m |
| Socket pad | 1.1 m radius visual, 1.2 m collision |
| Tower range at L1 | 9–16 m depending on the tower |
| Player interact reach | 9 m |
| Deck heights in use | 5 m, 10 m (Foundry, Switchyard); 10/20/30/40 (Spire) |
| Structure clearance over a route | ≥ 5.5 m, so enemies pass under |

---

## 4. Rules a map must satisfy

These are not style notes. Each one is a defect this project actually shipped,
and each is a check that should run on the level file before anything is built.

**Traversal**
1. Every tier carrying a socket is reachable from the spawn by ladder, zipline,
   launcher, lift or pad.
2. Every `ladder` area tops out **level with a walkable surface**, not under it.
3. Every route of that chain is survivable — no drop the player cannot recover
   from without restarting the climb.

**Coverage** — the ones that decide whether a map is *playable*, and which
nothing currently checks
4. Every route segment is within reach of at least **three** ground sockets, so
   there is a choice rather than a forced build.
5. Every **air** route segment is within reach of at least **three** sockets
   that can target air. Skywatch, Arc and Filament damage flyers; Detector and
   Singularity affect them without damage; Lance, Nova and Barricade cannot
   touch them.
6. No socket covers nothing. A pad that reaches no route segment at any upgrade
   level is a pad that will never be built on.
7. A gated shortcut, once closed, leaves the fallback route covered by sockets
   the player plausibly already owns.

**Clearance**
8. No track, wall, prop or volume within **3 m** of a route centre line, a
   socket, the spawn, the armory or a station — except pieces the route is
   meant to run inside, which say so.
9. Every `wall` socket sits inside its volume's footprint by ≥ 1 m.
10. Anything spanning a route leaves ≥ 5.5 m of headroom.

**Readability**
11. One idea per map, stated in the brief, legible from the spawn.
12. Decoration reads as *where* you are, never as *what* to do. If a piece of
    decor could be mistaken for a route, a pad or a climb, it is wrong.

---

## 5. What three passes over Switchyard actually taught

Worth stating plainly, because these generalise to every map that follows.

**A module that tiles may only contain features true at its repeat distance.**
The 20 m terrain tile carried two sidings; tiled 6×4 that became eight
full-width tracks across the map, outnumbering the real railway and hiding it.
The 4 m lane module carried a mile-marker post; that became a post every four
metres, hundreds of them. The 4 m cut module carried a lamp; that became a lamp
every four metres. All three are *good details* at the wrong frequency.
Anything that should appear every 20–60 m is a **separate prop**, placed.

**Scenery drawn into the ground plane becomes the map.** The sidings were
scenery. Once the ground tiles, scenery tiles with it and stops being scenery.

**Real-world proportion is not decoration, it is legibility.** A classification
ladder's angle *is* its turnout angle; ours was 1:4.9, sharper than any points
ever laid, and the throat read as tracks merging rather than as a junction.
At 1:6 it reads. Nobody could have told you the number was wrong; everybody
could tell you the yard looked wrong.

**A linear module must declare its run axis.** The cut channel runs along its
local **+X**; it was placed with a helper that aligns **+Z**, so every module
sat a quarter-turn wrong and was stepped by its width instead of its length —
and it survived review because it is plausible art placed wrongly. `run` and
`repeat` are not recoverable from the file. See the forward manifest, §Conventions.

**If it is only checked by looking at it, it is not checked.** Every defect
above passed a screenshot review. The probes found them.

---

## 6. Process

1. **Brief.** One paragraph: what this map is *about*, and the one decision it
   asks the player to make. `DESIGN-BRIEF.md` §3.
2. **Author** the level file in Claude Design, alongside the kit.
3. **Walk it** in `Maps.html` — it already assembles the level, draws route
   ribbons and socket markers, and has a first-person camera. If it does not
   read from the spawn, it will not read in game.
4. **Validate.** The §4 rules, as a pass/fail report. Iterate here, not in a PR.
5. **Export.** Kit GLBs plus the level file, in the same drop.
6. **Build.** The level file drives both halves; no map layout is written in C#.
7. **Review shots.** `--shot <map> <png> top|iso|lane|cut|yardline` plus the
   probes, on every drop.

Steps 4 and 6 do not exist yet. See §8.

---

## 7. Where each map stands

### Foundry — retro-author
The reference map: it works, and it should be the baseline the other two are
measured against. But `levels.js` carries 12 of its 44 sockets, so even the map
that works is not the map design has been looking at. Regenerate it, bring it
up to the schema (volumes, areas, waves), and hand ownership to design.

### Switchyard — retro-author, then improve
`levels.js` carries 16 of its 44 sockets and **none of its railway**. Regenerate
it from the current `GameRoot.cs`/`Maps.cs` so design is at least looking at
the shipped map, then work the outstanding art asks in
`FORWARD-MANIFEST-switchyard.md`. This one is the proof that a mirror cannot be
kept in step by hand: it fell three passes behind in a fortnight.

### Spire — design from scratch
The current Spire is a graybox that no one designed, and it should be thrown
away rather than repaired. What is needed, in order:

1. **Export the kit.** `tools/design-export/export.html` imports `FOUNDRY`,
   `SHARED` and `SWITCHYARD`; add `SPIRE`. Fifteen pieces are already written in
   `docs/design/models/spire.js` and have never been built. This is a one-line
   import, and it is the single cheapest improvement available to this project.
2. **Register the prefix.** `spire_` is missing from `AssetLibrary.Routes` —
   our side, one line.
3. **Author the layout.** A vertical map is a different design problem from a
   flat one and the current file does not attempt it. It needs, at minimum:
   which floors are fightable; how the ground route climbs or whether enemies
   arrive at height; where the air lane sits relative to the floors; how a
   player gets up and — the part the current map fails hardest — how they get
   *down* without a fall that costs the wave. Its 66 sockets should be
   redesigned with it, not preserved.
4. **Keep** the wave count, weather schedule and balance; those pass the gate
   suite and are not what is wrong.

The Spire's kit notes already imply a layout — a lobby bay used twice, a fire
escape on the east face, a roof at y 40 with the core on it, an atrium the
stair reads as a well through. That is a design that was written down and never
built. Building *that* is the ask.

---

## 8. What this repo has to build

Listed so the document is honest about what it is asking for. None of it exists
yet, and none of it blocks design starting on the Spire.

| | Work | Why |
|---|---|---|
| 1 | **Level validator** — reads a level file, reports every §4 violation | So a bad map fails in design, not in a PR |
| 2 | **Coverage probe** — rules 4–7, which nothing checks today | The rules that decide whether a map is playable at all |
| 3 | **Level loader** — client builds volumes, areas and place from the file | Removes ~750 lines of per-map C# |
| 4 | **`Maps.cs` codegen** — generated from the same file | Keeps `Sim.Core` pure C# and deterministic; no runtime parsing in the sim |
| 5 | **Export change** — ship the level file in the drop | Design's file has to reach us as data |
| 6 | **`spire_` in `AssetLibrary.Routes`** | One line, unblocks the Spire kit the moment it is exported |

Order matters: **1 and 2 are worth more than 3 and 4**. A validator on a
hand-maintained file catches the whole class of defect this document is about.
The loader is a refactor; the validator is the fix.
