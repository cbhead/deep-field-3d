# Map authoring — the contract between Claude Design and the game

**Status:** proposal, written 2026-09-09; §3 and §4 revised 2026-09-12 for the
M5 lane graph, where they had come to describe a sim that no longer exists —
§3 listed re-routing mid-walk under what could not be modelled, and it is what
a barricade does now. §4's clearance rules were revised again the same day, for
the Spire: written for flat lanes, they were unsatisfiable by any lane that
climbs. §7's Spire entry is now a record of what was done rather than a list of
what is needed — it is the first map here designed rather than emerged, which
is the thing §1 says nobody does. The level-file pipeline itself is still a
proposal; the follow-on work is listed at the end.
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
  "teleportLegs": [11, 20]         // optional: legs crossed, not walked
}
```

A route is a polyline, walked at constant speed, corner to corner, and every
ground route ends at the core.

**What a route now means, since M5.** Routes are decomposed into a graph — the
junctions where they meet, split or share a span become nodes, and a route
becomes an *itinerary*: the ordered list of places it passes through. Enemies
follow that list rather than the polyline, heading for the next place they can
still reach and taking the cheapest open lane that gets there.

Two things follow, and they are why the decomposition happened. A route is
still a commitment, but at the granularity of a *span* rather than a whole
match: an enemy finishes the stretch it is on and decides at the far end. And
**where two routes overlap, that is now one lane rather than two copies of the
geometry** — the Toaster's `direct` shares every waypoint it has with `long`,
and the difference between the two routes is a single connection that was being
stored as nine duplicated coordinates.

**`barricadeGate` / `fallbackRouteId` are gone.** They said "while this socket
is occupied, *spawning* enemies pick a different polyline", which is not what a
wall does. A map declares `laneGates` instead — an edge and the barricade
socket that shuts it — and shutting one turns the wave at the fork, including
the enemies already walking down it. A map may have up to eight, not one, and
the rules they must satisfy are in §3.

A **teleport leg** is not walked. Leg *i* runs `waypoints[i]` to
`waypoints[i+1]`; naming it in `teleportLegs` means an enemy reaching the near
pad is standing on the far one the same tick and carries on from there. The leg
has length zero for every purpose — distance travelled, coverage sampling, lane
geometry — so the two pads can be anywhere and nothing downstream has to know
how far apart they are. Three rules, all gated in the harness: never the first
or last leg, never two in a row, never on an air route. A warp gate is drawn at
each end, and the eight metres after an arrival pad are spawn apron exactly as
the eight after the gate are (§4). The Toaster's three ground routes take
nought, one and two of them, which is the map's whole idea: the wave that takes
the long way is gone for three minutes and comes back somewhere else.

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
},
"vehicles": [
  ["buggy1", "buggy", [-120, 0, -14], -90]   // id, def, position, yaw degrees
],
"roads": [
  { "kind": "asphalt", "width": 6, "points": [[-54,0,68], [-54,0,45]] }
],
"pond": { "at": [19, 0, -36], "radius": 17 }
```

Vehicles are `buggy`, `dagator`, `grnmchn` and `vehickle`; a map may park any
of them anywhere, and the yaw matters — a vehicle nosed at the wall it is
parked against is one whose first press of W is a crash.

`roads` and `pond` are how a vehicle knows what it is driving on. There is no
physics query under the wheels: the surface is read from these lines, so where
a road runs is gameplay and what it is drawn with is not. Asphalt, gravel,
grass and water each bend a vehicle's grip, acceleration and top speed by its
own factors — a saloon is quick on tarmac and hopeless in a field, a quad the
other way round.

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
| `teleporter` | box, `padId`, `label` | one pad in the map's network |
| `elevator` | box | cargo lift between two levels |
| `nest` | box | sniper perch marker |
| `armory` | box | the gunsmith kiosk trigger |

A `teleporter` is a **network**, not a pair: every pad on a map is a
destination for every other, and a player holding E on one picks from a radial
of the rest by name and distance. The charge is a second and a half of standing
still, cancelled by stepping off, and the cooldown after it is twenty seconds
and personal. Label each pad with somewhere a player can recognise — four pads
inside four buildings are four identical rooms otherwise.

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
- Four socket tags.
- A per-wave weather schedule (`fog`, `night`, `storm`, `heatwave`, …).
- Tower range as a **3D radius**, 9–16 m at level 1, up to about 2.5× that at
  L10 on the range path.
- **A lane graph, and routing over it.** *(M5, and it replaces two entries that
  used to be in the list below.)* Routes are decomposed into nodes and the
  authored spans between them — which is the shape the content already had,
  stored as overlapping polylines with duplicate coordinates. An enemy heads
  for the next place its route names that it can still reach, taking the
  cheapest open edge that gets there. Decisions happen at junctions and nowhere
  else, so an edge closing moves nobody: what is on it finishes it and chooses
  at the far end.
- **Closable lanes, and any number of them.** A `LaneGateDef` ties an edge to a
  barricade socket. Closing it turns the wave at the fork — including the
  enemies already walking. Up to eight closable edges per map, which is a
  legibility cap as much as a computational one.
- **A wall a siege enemy prices.** Chewing through costs `hp / StructureDps`
  seconds, which at the enemy's speed is that many metres it could have walked
  instead. So the same wall is worth breaking in front of a long detour and
  worth walking round in front of a short one, and your own barricade is what
  makes the detour long.

### The sim cannot model

- **Free pathfinding.** There is still no navmesh and no avoidance, and enemies
  still walk their authored geometry: the graph gives them a choice of *lanes*,
  not a choice of ground. A wall that is not an authored gate on an authored
  edge does not turn anything; it just intersects them.
- **Moving geometry.** A gate opens and closes; nothing slides, rises or falls.
- **Height as cover.** Line of sight is checked, but there is no partial cover,
  no elevation damage bonus, nothing that makes "high ground" mean anything
  except reach.

### One rule the sim enforces, and you should design against

**The map can be shaped and it cannot be sealed.** Any closure that would leave
a spawn unable to reach a core is refused — every spawn, not only the ones the
current wave uses, so the property is provable before a match starts rather
than discovered during one. The player sees *"that would leave the wave nowhere
to walk"* and is not charged for the attempt.

That is a safety net, and the harness proves the same thing at author time:
every combination of a map's gates is enumerated and checked, along with three
properties that keep the design honest — closing more never connects more (so
you can always undo what you shut), every gate must change where something
actually walks (a gate that moves nothing is inert content), and the
enumeration must agree with what the runtime would allow.

Design two ways round before you design a door. A map with one lane cannot have
a gate on it.

### The client can build

Solid and non-solid boxes; the seven area kinds; props mounted on volumes;
modules tiled along a run; a spanned piece between two points (ziplines); a
20 m terrain tile grid; scatter placed with clearance testing; a skybox dome.

### The client cannot build

- **Terrain relief.** The ground is one flat slab at y 0 with a tiled surface.
  There are no hills, no true cuttings, no ramps that are not authored volumes.
  Switchyard's "freight cut" is a channel *drawn* at grade for exactly this
  reason, and design's own note says so.
- **Curved routes, track or road.** Everything is straight segments; an arc is
  built from arc modules laid round a centre.

### The client can now build, since the Toaster

- **A field of any size.** `"field": [x, z]` is honoured: the slab, the terrain
  grid, the boundary, the scatter and the shadow distance all read it, and the
  edge of the playable area is an invisible wall rather than a drop. 110 × 80
  stays the default, and the three maps that were that size are laid exactly as
  they were. The Toaster is 320 × 160.
- **Buildings with insides.** A floor, walls with openings, and a roof that is
  a walkable surface rather than a lid — so a wall socket can sit on one and a
  ladder can serve it. Openings only; a door that closes is a wall.
- **Vehicles**, per §2.3.

### Fixed numbers to design against

| | |
|---|---|
| Field | per map; 110 × 80 m unless the level file says otherwise, y 0 at grade |
| Ground lane width | 3.4 m |
| Air lane ribbon | 1.2 m; y 8–15 over a map with decks, y 9 over one without |
| Linear module repeat | 4 m |
| Socket pad | 1.1 m radius visual, 1.2 m collision |
| Tower range at L1 | 9–16 m depending on the tower |
| Player interact reach | 9 m |
| Deck heights in use | 5 m, 10 m (Foundry, Switchyard); 10/20/30/40 (Spire) |
| Structure clearance over a route | ≥ 3 m gameplay minimum (5.5 m is the railway convention over track) |

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

**Coverage** — the ones that decide whether a map is *playable*

These apply to the **lane, not the gate**: the first 8 m of a route is spawn
apron and is exempt. An enemy standing on its spawn point has the whole walk
still ahead of it and nothing is lost by not shooting it there, while demanding
three pads within reach of the map edge would force every map to grow a cluster
of build pads at the mouth — which pays players to turtle on the entrance and
is worse design than the hole it closes.

4. Every route segment is within reach of at least **three** ground sockets, so
   there is a choice rather than a forced build.
5. Every **air** route segment is within reach of at least **three** sockets
   that can target air.

   Mind *which* sockets. Only three towers damage flyers — Skywatch (15 m), Arc
   (11 m) and Filament (12 m) — and a tower on a ground pad stands at y 0, so
   its whole range budget goes on climbing. A strand cruising at 13–15 m is
   above what Arc and Filament can reach from the ground **on every map in the
   campaign**; they are deck weapons against air, and nothing in the def table
   says so. A strand answerable only from decks is a legitimate design — it is
   what Switchyard's catwalk is *for* — but it has to be a decision, not an
   accident, and §4.1 then carries the whole air layer on that climb being
   reachable. The validator prints the ground/deck split per tower.

   A map with no deck has to make the opposite decision, and the Toaster does:
   its strand flies at 9 m, down the same line the walkers take, so the pads
   that cover the road cover it too. That is a real cost — a mixed wave there
   is a question of volume rather than of position, and the towers-only floor
   holds two waves longer than it did when the strand had a line of its own —
   and it is the right cost, because the alternative on flat ground is a lane
   nothing in the game can shoot at. Height is how an air lane asks for a
   second position; where there is no second position to ask for, do not ask. Skywatch, Arc and Filament damage flyers; Detector and
   Singularity affect them without damage; Lance, Nova and Barricade cannot
   touch them.
6. No socket covers nothing. A pad that reaches no route segment at any upgrade
   level is a pad that will never be built on.
7. ~~A gated shortcut, once closed, leaves the fallback route covered by
   sockets the player plausibly already owns.~~ **Subsumed by 4, since M5.** On
   a graph the fallback is not a separate route, it is another edge — one that
   some configuration walks — so rule 4 already demands three pads on every
   metre of it. Deleting a rule is the best outcome a rewrite of this section
   can have, and this one was only ever rule 4 restated for the one case the
   old model could not express.

**Clearance**
8. No track, wall, prop or volume within **3 m** of a route centre line, a
   socket, the spawn, the armory or a station — except pieces the route is
   meant to run inside, which say so.
9. Every `wall` socket sits inside its volume's footprint by ≥ 1 m.
10. Anything spanning a route leaves ≥ 3 m of headroom, so a player and an
    enemy both pass under it. (The 5.5 m figure elsewhere is the railway
    convention for structures over track, not a gameplay minimum — Switchyard's
    mid deck clears its lane by 4.6 m and is correct.)

**Both of these are measured off the lane's own pitch, and neither counts the
lane's own floor** — two clauses that are invisible on a flat map and decide
whether a map that climbs can exist at all. Written for flat lanes they said
something else than they meant:

- *Off the lane, not off the horizontal.* A 3.4 m clearance box centred 1.1 m
  **vertically** above a climbing lane reaches 1.7 m along the run, where the
  stair carrying that lane has already risen 1.7 m. So the flight intersects
  the box and the rule reports it as an obstruction of the route it exists to
  carry. Unsatisfiable by any ramp over about 25°: as written, rule 8 did not
  forbid walls in lanes, it forbade lanes that climb.
- *A lane's own floor is not an obstruction of it.* Even pitched, at the corner
  where a level leg meets a flight the flight is the lane a metre and a half
  ahead and inside the corridor's own cross-section, so any volume test wide
  enough to be a lane catches the stair. The probe now asks what each sample is
  standing on and exempts those bodies, plus anything whose highest point is
  under the clearance band — floor beside the lane, whoever's lane it belongs
  to. Neither exemption can hide a wall: a wall is above the lane and holds
  nothing up.

Three maps' lanes are flat, so on Foundry, Switchyard and the Toaster both
clauses are the identity and those maps report exactly what they reported
before. The fourth had never been built.

**Readability**
**Configurations** — the rules a mutable map adds. All four are checked by the
harness, in the engine-free lane, because connectivity is a sim invariant and
does not want a Godot boot to answer.

11. **No configuration seals the map.** Every combination of gates a player can
    reach leaves every spawn able to reach a core. The runtime refuses the
    closure that would break it; this proves the refusal never has to fire in
    shipped content.
12. **No configuration is a trap.** From anything you can shut, you can get
    back to neutral. Free today because closing is monotonic — opening a gate
    can only reconnect — and asserted anyway, because it stops being free the
    day something irreversible lands.
13. **Every gate moves something.** Shut it alone and at least one itinerary
    must walk a different set of lanes. A gate that changes no path is inert
    content: the mutable analogue of rule 6's dead pad, and the defect that let
    a control point sit on Foundry as decoration for two milestones. Measured on
    paths, not distances — Switchyard's switchback gate changes no distance to
    the core, because the cut was already shorter, while changing which lane
    half the waves walk down.
14. **At most eight gates.** 256 configurations is the enumeration's budget, and
    it is a legibility cap first: no map that teaches one idea needs nine levers.

**Legibility**

15. One idea per map, stated in the brief, legible from the spawn.
16. Decoration reads as *where* you are, never as *what* to do. If a piece of
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

### Spire — **done, M5**
Designed rather than repaired, and at 0 violations from 84. All four points
below were the ask; all four are met.

1. ~~**Export the kit.**~~ Done in M5 phase 0. `export.html` imported
   `FOUNDRY`, `SHARED` and `SWITCHYARD` and threw on an unvendored import
   before it reached anything, so `make design-export` had been producing
   *nothing at all, for every asset*. Forty-seven `spire_*` files now exist and
   every one of them is consumed.
2. ~~**Register the prefix.**~~ Done.
3. ~~**Author the layout.**~~ Done. See `Maps.Spire` and GameRoot's Spire
   section for the design and the reasoning; the short version is that the
   diagnosis in §1 was exactly right and the fix was not subtle. **The routes
   climbed a building that had been built out of solid slabs.** No door where
   either ground route enters, no well where the stair pierces a plate, no
   opening where it reaches the roof, and the cargo lift's shaft rising through
   four storeys of concrete — which is why it had been marked out of service
   rather than fixed. The four flights the routes climb had never been built at
   all, so the stair was forty metres of open air and the pads generated six
   metres off it hung beside nothing: 26 sockets with no way up and 32 of 43
   with nothing under them were **one** defect, seen twice.

   What the layout now answers: all four floors are fightable and pads are on
   every one; the ground routes climb, one inside and one out, converging at
   floor two; the air lane spirals outside the building's own volume and lands
   on the roof; a player goes up by the lift or by ladders staggered round the
   atrium, and gets *down* by stepping into the atrium — free, instant, and
   paid for by the forty metres you then have to climb again. That asymmetry is
   the map: **the defence is mobile and the attack is not.**

   The 66 sockets were redesigned with it and not one survived. The 68 that
   replaced them were generated against the routes *and against the building* —
   a clear metre of plate, landing or roof on every side, inside a traversal
   exit's reach, and chosen until every stretch of every lane, air included,
   has three pads that can answer it.
4. ~~**Keep the wave count, weather schedule and balance.**~~ Kept, unchanged,
   and they still hold: 12 waves clear for the mid-band bot with 16 lives and
   for towers alone with 12.

The kit's notes did imply a layout — a lobby bay used twice, a fire escape on
the east face, a roof at y 40 with the core on it, an atrium the stair reads as
a well through — and design had gone further than that: `spire_stairwell` is
authored at a 10 m rise over a 10 m run with a note saying to stretch it for
"the 18 m and 14 m legs", which are exactly the four climbing legs in
`Maps.cs`. **The kit had always known the shape of this stair.** Nothing had
ever been built to it.

---

## 8. What this repo has to build

Listed so the document is honest about what it is asking for. None of it exists
yet, and none of it blocks design starting on the Spire.

| | Work | Status |
|---|---|---|
| 1 | **Validator** — every §4 rule this can measure, on the assembled map | **done** — `make map-validate` |
| 2 | **Coverage probe** — rules 4–7, which nothing checked before | **done** — folded into the same probe |
| 3 | **Level loader** — client builds volumes, areas and place from the file | not started |
| 4 | **`Maps.cs` codegen** — generated from the same file | not started |
| 5 | **Export change** — ship the level file in the drop | design's side |
| 6 | **`spire_` in `AssetLibrary.Routes`** | one line, waiting on the kit being exported |

Order mattered: **1 and 2 were worth more than 3 and 4.** The loader is a
refactor; the validator is the fix.

### What the validator found

Run on all three maps the day it was written, against rules that had never been
checked:

| | Foundry | Switchyard | Spire |
|---|---|---|---|
| §4.1 sockets with no way up | 0 | 0 | **26** |
| §4.4 ground lanes under-covered | 0 of 36 | 3 of 67 | 2 of 91 |
| §4.5 air lane under-covered | 6 of 23 | 10 of 23 | **37 of 43** |
| §4.6 pads that reach nothing | 0 | 0 | 0 |
| §4.9 deck pads properly footed | 7 of 11 | 9 of 9 | **11 of 43** |
| §4.8 lane blocked by solid geometry | 0 | 1 | **23** |
| **Total violations** | **5** | **9** | **90** |

Foundry and Switchyard are both at **0** now: three deck columns came out of
Switchyard's lanes, four Foundry pads came back from a deck edge, and the thin
stretches of both air lanes got the pads they needed. The Spire still has all
of its, because they belong to the redesign in §7 rather than to patching.

Read that Spire column against §7. Twenty-six sockets with no way to reach
them, thirty-two deck pads with nothing under them, and an air lane that
thirty-seven of its forty-three sample points have **no tower in the game able
to reach at all**. It is not a map that needs tuning.

Foundry — the map that works — has a thin air lane at the west mouth and four
catwalk pads within a metre of an edge. Switchyard adds a catwalk column
standing in the long route's lane.

Because every map fails today, the check is a **ratchet**, not a gate:
`docs/map-validation-baseline.tsv` records what each map is currently allowed
and CI fails only when a map gets *worse*. Lower a baseline in the same commit
that improves a map. A gate nobody can turn green gets switched off within a
week; a ratchet only ever tightens.
