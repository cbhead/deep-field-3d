# The Toaster — commission for Claude Design

**Status:** commission, written 2026-09-11 against `main` at the M4 map pass.
Nothing here exists yet: no kit, no level file, and until this drop no
`toaster_` or `vehicle_` name resolved to anything.
**Audience:** Claude Design. This is a request list.
**Read first:** [docs/MAP-AUTHORING.md](MAP-AUTHORING.md) — how maps should be
authored at all. This is the first map delivered at a field size other than
110 × 80, and it should be the first delivered *with* its level file.
**Companion files:** `docs/FORWARD-MANIFEST-switchyard.md` (the shape of these
asks, and §5 there is why every repeating piece below carries a budget),
`docs/forward-manifest.json` (design's own — everything here wants to appear in
it next drop), `game/assets/structures/manifest.json` (this document asks for
four new fields in it), `docs/DESIGN-BRIEF.md` §2 (the integration contract:
metres, +Y up, −Z forward, origin at feet / footprint centre).

---

## Why this document exists

The Toaster is a real rural property in autumn — four buildings, a pond, a
paved road, a gravel drive, dry-grass fields and dense woodland round the whole
perimeter — at **320 × 160 m, six times the area of any map we have**. It is
also the first map with drivable vehicles and with gates that teleport a wave
across the property mid-walk. All three of those break assumptions the art
pipeline has been able to make until now.

The map is built and playable today, as grayboxes. It validates at zero
violations against every rule in MAP-AUTHORING §4, every ladder lands on its
roof, every vehicle drives, and the warp gates work. What it has no art for is
any of it. So this is not a list of fixes; it is the kit for a map that is
already standing up, and the numbers below are measured off the thing rather
than guessed at.

## The rule underneath almost all of it

> A file that will be instanced is drawn **once per part**, and its triangles
> are multiplied by every placement. Part count is draw calls. Put the detail
> in the texture.

Anything that repeats on this map is now drawn as one instanced mesh rather
than a copy of the model per placement, because it has to be. Switchyard's
ground is twenty-four copies of a 221-part tile: five thousand scene nodes and
a draw call per part per copy per shadow cascade, for one flat field. At six
times the area with the same mechanism that is twenty-eight thousand nodes
before a single one of this map's ~1,500 trees. Instanced, the same ground is
sixty draw calls, and the map builds 3,000 nodes on six times the area where
Switchyard builds 11,500 on one.

That only works if the repeating files are built for it, which is what the
budgets are: not taste, arithmetic. A file over its part budget is not refused
— it simply costs what it costs, and the perf probe is where that shows up.

Switchyard's other rule still holds, and this map is full of opportunities to
break it: **a tiling module may only contain features true at its own repeat
distance**. A 4 m road module carries no mailbox. A 40 m ground tile carries no
road, no fence and no tree.

Every entry below states: the file name, what it is, origin and axes, size in
metres, a triangle and part budget, `run`/`repeat`/`width` where it tiles, what
the code does with it, and what happens if it does not arrive.

---

## P0 — the instanced set: ground, roads, trees

These decide whether the map runs at all.

### A. Terrain tile — `toaster_terrain.glb`, `_v1`, `_v2`, `_v3`

**Ask:** a 20 m tile of dry autumn grass in four variants, four files, one
shared material set.

| | |
|---|---|
| Size | 20 × 20 m, origin at the tile centre on grade, +X east, +Z north |
| Relief | ≤ 0.15 m, and exactly 0 within 2 m of every edge so tiles seam and roads lie flat on them |
| Budget | **≤ 900 tris, ≤ 4 parts** per file; one 2048² albedo / roughness / normal set shared by all four |
| Must not contain | roads, paths, fences, trees, or grass-tuft geometry — 128 tiles at one tuft a metre is fifty thousand parts, and the tufts belong in **G** |
| Variants | v0 plain dry grass; v1 a mown strip and a bare-earth patch; v2 a leaf drift and a fallen branch; v3 tyre ruts running along +X |
| Code | 128 placements on an 8 × 16 grid, instanced, no collision (the ground slab underneath is the collider), shadows off |
| If it does not arrive | flat grey slab, which is what the map shows today |

Four variants matter more here than anywhere before: Switchyard has one tile
laid twenty-four times and we break the lattice by turning alternate copies,
which helps and is not a fix. A hundred and twenty-eight copies of one tile is
wallpaper at any rotation.

### B. Asphalt road module — `toaster_road_asphalt.glb`

**Ask:** 4 m of two-lane country asphalt, 6 m wide, worn edges and a faded
centre line in the texture.

| | |
|---|---|
| Axes | `run: "+X", repeat: 4.0, width: 6.0`; origin at the module centre on grade; **top surface at y 0.05** |
| Budget | ≤ 200 tris, ≤ 2 parts; 1024² set shared with **C** |
| Must not contain | posts, drains, mailboxes, or potholes — all of them would repeat every four metres |
| Code | laid along the authored polylines, instanced; roughly 90 placements |
| Degradation | the graybox strip shows; vehicles still know it is asphalt |

The road is load-bearing in a way no map's scenery has been before: **vehicle
handling is read off the road centre lines**, not off the mesh. So where a road
runs is gameplay and what it is drawn with is not — which also means the art
must sit on the line rather than beside it.

### C. Gravel drive module — `toaster_road_gravel.glb`

Same contract, `width: 4.0`, top surface at **y 0.04** (asphalt is drawn a
centimetre proud so their junction is a crossing rather than a z-fight),
≤ 150 tris, 1 part. Seven placements, up to the barn.

### D. Asphalt arc — `toaster_road_arc.glb`

**Ask:** a 30° arc of the same 6 m asphalt, centreline radius 12 m, so twelve
make a circular drive.

| | |
|---|---|
| Origin | at the **centre of curvature** on grade; the arc starts on the +X axis and sweeps toward +Z |
| Manifest | `arcDeg: 30, radius: 12, width: 6` |
| Budget | ≤ 200 tris, 1 part |
| Degradation | the drive is laid from twelve straight **B** modules and reads as a dodecagon |

### E. Trees — six files

`toaster_tree_oak.glb`, `toaster_tree_maple.glb`, `toaster_tree_pine.glb`, and
`toaster_tree_oak_lod1.glb`, `_maple_lod1`, `_pine_lod1`.

**Ask:** three autumn species at full detail, and each again as a billboard
cross for distance. Between them they are about 780 placements today and will
be nearer 1,500 once the belt is dressed, so this is the most instanced thing
on the map by an order of magnitude.

| | LOD0 | LOD1 |
|---|---|---|
| Height / canopy | oak 14 m / r 5.5; maple 12 m / r 4.5; pine 16 m / r 3 | same silhouette within 5 % — the swap must not change the outline |
| Origin | root collar on grade, trunk up +Y | same |
| Budget | **≤ 600 tris, exactly 2 parts** (`<species>_trunk`, `<species>_canopy`) | **≤ 40 tris, 1 part** |
| Materials | one 1024² leaf-card atlas shared by all three canopies, glTF `alphaMode: MASK` — blended leaves sort badly per instance and cost a transparent pass; bark on its own 512² | a baked front/side image of LOD0, same MASK setup |
| Manifest | `lod1: "toaster_tree_oak_lod1", lodSwitch: 90` | — |
| Code | placed on a jittered 3.5 m lattice through a 12 m belt round the field edge, chunked into 40 m cells; LOD0 visible 0–90 m and casting shadows, LOD1 from 90 m and casting none, with an 8 m dissolve between |
| Degradation | without LOD1 the full tree draws at every distance — still instanced, so it runs, it just costs; without any tree the belt is bare ground and the horizon is a slab edge |

The chunking is why the part budget is exactly two: culling, visibility ranges
and mesh LOD all act on a multimesh's whole bounding box, so the belt is cut
into cells, and every part multiplies the number of them.

### F. Understory — `toaster_understory.glb`

A 4 m clump of shrubs and bracken for the inner edge of the belt. ≤ 300 tris,
1 part, MASK cards, origin at ground centre. Optional; degrades to nothing.

### G. Ground scatter — `toaster_terrain_scatter.glb`

Same contract as `switchyard_terrain_scatter`: a 4 m cluster — grass tufts, a
rock, a rotted post, a thistle. ≤ 400 tris, ≤ 4 parts. Instanced now, so the
placement cap has risen from 22 to 140.

---

## P1 — the four buildings

Each building is **two files**: a shell (exterior and interior in one, with the
door openings cut) and a roof (separate, because the roof is a surface players
stand on and the code puts a collider under it). Interior dressing is separate
props, placed by the level file, so rooms can be arranged around the teleport
pads and the sockets rather than baked around nothing.

**Shell contract, all four:**

- Origin at the footprint centre on grade; local +X along the stated width, +Z
  along the stated depth.
- Walls 0.3 m thick; interior floor top at y 0.15; **interior clear height
  ≥ 3.0 m** everywhere a player walks.
- **Door openings only, no doors.** A door that closes is a wall, and the sim
  cannot model one that opens. Openings at the stated positions, measured along
  the wall from its centre.
- Windows in the texture (emissive at night), not glass geometry — no
  transparency anywhere in this kit.
- Budget **≤ 6,000 tris, ≤ 12 parts, one 2048² set** per shell; **≤ 1,500 tris,
  ≤ 3 parts** per roof.
- The roof carries **one flat area ≥ 4 × 4 m at the stated height**. That is
  where the roof collider, the wall socket and the ladder head are. Pitched
  slopes elsewhere are fine at ≤ 30°.
- The shell is hollow and double-sided: it is read from inside as often as from
  out.

The code hangs the shell over the graybox and hides the boxes; the colliders,
the ladder area and the roof surface are unchanged by what arrives. So a shell
that is the wrong size does not break the map, it just does not line up — the
dimensions below are the ones the volumes are built at.

### H1. Barn — `toaster_barn_shell.glb`, `toaster_barn_roof.glb`

A pole barn and equipment shed. Footprint **30 (X) × 21 (Z)**, centre
(−108, 54), eaves **6.0 m**, roof surface at **6.3 m**.

| Opening | Where | Size |
|---|---|---|
| Sliding door | **+Z wall**, centred | 5 w × 4 h |
| Person door | **−Z wall**, 8 m left of centre | 2.4 × 2.6 |

The big door is 5 × 4 because the Gator is driven through it — 2.9 m long,
1.5 wide, 1.85 tall. Ladder to the roof on the **+X wall**, 8.5 m toward −Z.
This is the one building with no teleport pad: it is the vehicle shed.

### H2. Buggy house — `toaster_house_buggy_shell.glb`, `_roof.glb`

A single-storey ranch house. Footprint **19 × 28**, centre (−134, −7), eaves
**3.4 m**, roof surface **3.7 m**. Openings: **+X wall** 6 m toward +Z (front
door), **−X wall** 8 m toward −Z (back). Ladder on the **−X wall**, 12 m toward
+Z. Teleport pad inside at local (0, 0, −5).

### H3. Vehickle house — `toaster_house_vehickle_shell.glb`, `_roof.glb`

The large main house, with the circular drive to its north. Footprint
**34 × 26**, centre (30, 31), eaves **3.4 m**, roof surface **3.7 m**.
Openings: **−Z wall** 10 m toward −X (front, onto the drive), **+Z wall** 12 m
toward +X (back), **−X wall** centred, 4.5 w × 2.8 h (garage — the quad lives
in it). Ladder on the **+X wall**, 11 m toward −Z. Pad inside at local
(−7, 0, 0).

One storey, deliberately: a second floor needs a climb, and a climb the sim
cannot see is a tier the coverage rules cannot reason about.

### H4. Grnmchn house — `toaster_house_grnmchn_shell.glb`, `_roof.glb`

The second house, south-east, facing the pond. Footprint **24 × 30**, centre
(90, −50), eaves **3.4 m**, roof surface **3.7 m**. Openings: **−X wall**
centred (front, toward the pond), **+X wall** 8 m toward +Z. Ladder on the
**−X wall**, 13 m toward +Z. Pad inside at local (−4, 0, 2).

### H5. Interior dressing — four props

`toaster_dress_workbench.glb` (3 × 1 × 0.9, barn and garage),
`toaster_dress_shelving.glb` (4 m run, 2.2 high),
`toaster_dress_furniture_living.glb` (sofa, table, lamp as one 4 × 3 cluster),
`toaster_dress_furniture_kitchen.glb` (4 m counter run with cabinets).
Each ≤ 1,500 tris, ≤ 8 parts, origin at footprint centre on the floor.

---

## P2 — the four vehicles

New prefix `vehicle_` → `game/assets/vehicles/`, registered our side already.

**Rig contract, all four.** The same idea as the tower `rig` block: the code
moves named nodes and nothing is animated in the file.

| Node | What the code does with it |
|---|---|
| `seat_driver`, `seat_passenger` | empty at the seat cushion; the rider's eye sits at the stated height above the vehicle's origin, and their body is carried by this node |
| `wheel_fl`, `wheel_fr`, `wheel_rl`, `wheel_rr` (trike: `wheel_0` front, `wheel_1`, `wheel_2`) | separate meshes, **pivot at the axle centre, axle along local X**; spun about X at speed ÷ radius |
| `steer_wheel` / `steer_bars` / `steer_lever_l`, `steer_lever_r` | the thing the driver holds, turned with the steering input |

Origin at the ground contact point under the chassis centre. **Nose along −Z**
— the integration contract's forward, and the direction this controller drives
on. Every other kit piece with a front faces +Z and is turned by a yaw helper;
a vehicle is not, because forward here is a direction of travel rather than a
thing to point at something. Budget **≤ 8,000 tris, ≤ 24 parts, one 2048² set**
each. These four are the only pieces on the map that cannot be instanced —
named nodes have to stay nodes — so 24 parts is a ceiling, not a target.
Windows opaque-tinted or MASK; never blended.

**Manifest block, per file:**

```json
"vehicle": {
  "seats": ["seat_driver", "seat_passenger"],
  "wheels": [{ "node": "wheel_fl", "radius": 0.29, "steers": true },
             { "node": "wheel_rl", "radius": 0.29, "steers": false }],
  "steer": "steer_wheel",
  "steerMaxDeg": 32,
  "hull": [1.55, 1.5, 4.1]
}
```

`hull` is the collision box the code already builds; a delivered model that
does not fit inside it will clip the world. The dimensions below are those
boxes.

### V1. `vehicle_buggy.glb` — rear-engined saloon, 2 seats

**1.55 w × 1.5 h × 4.1 long**, wheelbase ~2.4, wheel radius 0.29,
`steer_wheel`. Rounded, friendly, faded paint, one dented wing. Seats abreast
at eye height 1.15. Farm-kept rather than restored: it is the thing that lives
under the carport and gets driven to the end of the road.

### V2. `vehicle_dagator.glb` — utility vehicle, 2 seats and a bed

**1.5 w × 1.85 h × 2.9 long**, wheel radius 0.31. Bench seat, roll cage, open
cargo bed at the back with a strapped tarp, knobbly tyres, a light bar. Eye
height 1.0 — low and workmanlike. `steer_wheel`. Lives in the barn and is
driven through its big door.

### V3. `vehicle_grnmchn.glb` — motorised drift trike, 1 seat

**0.9 w × 0.9 h × 1.9 long**. Front `wheel_0` radius 0.25 on a fork that is
also the steering node; rear `wheel_1` and `wheel_2` radius ~0.13 wearing
**plastic drift sleeves**, which is why it slides. **Two lever handles**,
`steer_lever_l` and `steer_lever_r`, pivoting about Y at the fork — it steers
by pulling levers, not by turning a wheel, and that is why it can spin on the
spot. Seat height 0.3 and eye height **0.75**, the lowest on the map, which is
most of the joke. Green.

### V4. `vehicle_vehickle.glb` — sport quad, 1 seat

**1.16 w × 1.13 h × 1.85 long**, wheel radius 0.28, `steer_bars`. Saddle at
eye height 1.05, knobbly tyres, plastic bodywork, a number board. The fastest
thing on the map at 22 m/s and it handles like it.

### V5. Hands — `hands_<set>_wheel.glb`, `hands_<set>_bars.glb`

Two poses through the existing `HAND_POSES` mechanism, six sets each.
`wheel`: both hands at ten-and-two on a 0.38 m rim, origin at the hub, in the
viewmodel frame. `bars`: grips 0.7 m apart, used by the quad and the trike's
levers.

**Not blocking, and `wheel` first if only one is affordable.** Without them the
driver sees no hands while the wheel turns, which reads as a car rather than as
a bug.

---

## P3 — gates, pond, sky, dressing

### W. Warp gate — `shared_warp_gate_idle.glb`, `shared_warp_gate_active.glb`

**Ask: a new silhouette, not a reuse, and specifically not a flat pad.**

The sim moves an enemy from one gate to the other in a single tick. The player
has, on this same map, been taught that a flat 3 m hex pad means *stand here
and hold E*. A flat enemy gate would say the same thing and mean the opposite,
which MAP-AUTHORING §4.12 forbids outright: decoration must never read as a
control.

So make it **vertical**: a 4.5 m ring or arch on a 0.3 m plinth, 3 m clear
opening, origin at ground centre, opening facing ±Z (the code yaws it along the
route). `_active` carries a part named `warp_membrane` — emissive, MASK edge —
that the code can pulse; on `_idle` the ring is dark. ≤ 2,500 tris, ≤ 12 parts.
Six placements: three gates, two ends each.

Degradation today is `shared_spawn_portal`, which reads as three extra spawn
points — the reason for the ask.

### X. Pond — `toaster_pond.glb`

34 m across, origin at the pond centre on grade. The ground is one flat slab
and always will be, so **the pond is drawn upward, not dug**: a riprap bank
ring from about r 15 to r 18 rising to 0.5 m (the hidden collider the code
rings it with sits at r 17), an opaque dark water disc at y 0.15
inside it (roughness ~0.1, no transparency, no refraction), and a small island
at the centre left bare — the trees on it come from the tree set. **≤ 2,500
tris, ≤ 3 parts** named `pond_bank`, `pond_water`, `pond_island`. No collision:
the code rings it with a hidden bank collider, which is what stops a quad doing
twenty across the surface of the water.

`toaster_dock.glb` (optional): a 2 × 6 m plank jetty, ≤ 300 tris, origin at the
landward end.

### Y. Skybox — `toaster_skybox.glb`

Same 700 m dome and the same bake as the other two. **Overcast late-autumn
afternoon**: low warm sun in the south-west at about 24° elevation, a grey-gold
cloud deck, and — the part that matters on a map this wide — **a treeline
silhouette baked into the horizon band**, so the belt meets the sky instead of
stopping. **Bake at 2048 × 1024**; the existing two are 4096 × 2048 and 9 MB
each, and an overcast sky does not need it.

### Z. Dressing

| File | What | Size (m) | Budget | Placed |
|---|---|---|---|---|
| `toaster_fence_wood` | post-and-rail run, `run: "+X", repeat: 4.0, width: 0.2`, 1.2 high | 4 × 1.2 | ≤ 120 tris, 1 part | instanced, along the drive and field edges |
| `toaster_mailbox` | post box at the road head | 0.5 × 1.2 | ≤ 200, 2 | 3 |
| `toaster_woodpile` | split logs under a tarp edge | 3 × 1.4 × 1.2 | ≤ 600, 2 | 2 |
| `toaster_hay_bale` | round bale on its side | 1.5 dia × 1.2 | ≤ 150, 1 | instanced, ~25 in the east field |
| `toaster_propane_tank` | horizontal tank on legs | 3.2 × 1.0 × 1.4 | ≤ 500, 3 | 3, one per house |
| `toaster_wreck_pickup` | rusted pickup on flat tyres, hood up | 5.2 × 2.0 × 1.8 | ≤ 4,000, 6 | 1, behind the barn — **dressing, not a vehicle; no rig** |
| `toaster_leaf_pile` | a drift of leaves | 2 × 0.3 | ≤ 60, 1, MASK | instanced, under the belt edge |

All: origin at footprint centre on grade. Nothing in this table may sit within
3 m of a route, a socket, a pad, the spawn, the armory or a hero station — the
code refuses those placements and logs them, so a bad coordinate is a line in
the output rather than a girder in the roadway.

### VFX

`vfx_teleport_burst` — a short column of light for a player arriving or
leaving on a pad. One file, tinted by the code (teal for a player, red for an
enemy through a warp gate), authored standing on the ground, ≤ 400 tris.
Stand-in today is the Nova impact ring, which is flat.

---

## Conventions we need stated on delivery

Four fields in `game/assets/structures/manifest.json`, generated by the export:

1. **`run` / `repeat` / `width`** on every linear module (**B**, **C**, the
   fence), and `arcDeg` / `radius` on **D**. This was the Switchyard ask and it
   is unchanged: neither is recoverable from the file, and a module placed a
   quarter-turn out is plausible art placed perfectly wrongly. It cost that map
   a whole pass, and it cost this one a pond whose bank reached three metres
   into a lane forty metres away — same mistake, found by a probe rather than
   by looking.
2. **`budgetTris` / `budgetParts`** beside the measured `tris` / `parts`, taken
   from the kit entry, plus **`instanced: true`** on anything meant for the
   multimesh path. Then the export itself can print a line when a file is over,
   and `make assets` can fail on it, instead of the cost turning up as a frame
   rate three weeks later.
3. **`lod1` / `lodSwitch`** on the tree LOD0 entries.
4. **`vehicle`** block (P2), in the same spirit as `rig`.

And two things about the files: canopy and leaf materials are glTF
**`alphaMode: MASK`** (Godot imports it as alpha-scissor), and every instanced
file names its parts `<id>_<part>` so a placement can leave one out.

---

## The map, so pieces are drawn to fit

Metres, +X east, +Z north, y 0 at grade. Taken from the built map rather than
proposed.

| | |
|---|---|
| Field | **320 × 160**, x ±160, z ±80 |
| Invisible wall | x ±148, z ±68 — the inner edge of the treeline |
| Tree belt | 12 m deep, straddling that edge |
| Barn | centre (−108, 54), 30 × 21, eaves 6.0, roof 6.3, big door on +Z |
| Buggy house | centre (−134, −7), 19 × 28, eaves 3.4, roof 3.7 |
| Vehickle house | centre (30, 31), 34 × 26, eaves 3.4, roof 3.7 |
| Grnmchn house | centre (90, −50), 24 × 30, eaves 3.4, roof 3.7 |
| Pond | centre (19, −36), 34 m across, island at its centre |
| County road | (−54, 68) → (−54, 45) → (−36, 54) → (0, 54) → (10, 22) → (35, 13), 6 m |
| South road | (−45, −66) → (97, −66), 6 m |
| Barn drive | (−89, 45) → (−61, 49), 4 m gravel |
| Player pads | inside three houses, and one at the core (−52, 0, −14) |
| Warp gates | (72, −40) ⇒ (−137, 11), and (−44, 60) ⇒ (52, 29) |
| Spawn gate / core | (72, 0, 24) / (−57, 0, −10) |
| Hero spawn / armory | (−60, 0, −22) / (−66, 0, −22) |
| Vehicles at match start | Buggy (−120, −14) facing east; Gator (−110, 40) facing south; trike (76, −30) facing north; quad (58, 38) facing east |
| Sun | elevation 24°, azimuth 220°, warm, overcast ambient |

Handling reads surface off the road lines: asphalt, gravel, grass, water, each
with its own grip, acceleration and top-speed multiplier per vehicle. So the
road *routes* above are gameplay; the road *art* is not.

---

## Already right — reuse, please do not change

- `shared_teleporter_pad_idle` / `_charged` / `_cooldown` — the player's 3 m
  hex, used at four pads here and finally working on the Spire too.
- `shared_spawn_portal`, `shared_core` / `_hit`, `shared_armory_kiosk`,
  `shared_airlane_pylon`, the six `socket_*` markers — as they are.
- `shared_ladder` — four times, once per roof. It is 14,056 tris, which is fine
  at four and would not be at forty.
- `prop_barrel_explosive`, `prop_container_droppable` — placed as the
  interactive dressing they already are.
- The `hands_<set>_<pose>` mechanism: **V5** adds two strings to `HAND_POSES`
  and nothing else.

---

## Also in this drop

- **`toaster.level.json`**, per MAP-AUTHORING §2, with `"field": [320, 160]`.
  This map is the code change §3 said to ask for, and it has been made: a map
  now carries its own field size and the client reads it. The file wants
  `routes` (with `teleportLegs`), `sockets`, `anchors`, `volumes`, `areas`,
  `place`, `vehicles`, `roads`, `pond` and `conditions`.
- **`export.html` must import `TOASTER`, `VEHICLES` — and `SPIRE`.** The
  Spire's fifteen-piece kit has been written since M3 and has never once been
  built, because that page imported three map modules and stopped. Both the
  import and the `spire_` prefix are now in place on our side; the export is
  the last thing between that map and having any art at all.

---

## How we will know it landed

| Check | Command |
|---|---|
| Names match the brief | `make assets` |
| Nothing delivered goes unused | `make usage` |
| Nothing requested is missing | `--asset-audit` |
| The map still builds inside its node budget | `--solo toaster`, the `[map] built N nodes` line |
| §4 rules, with a `toaster` row in the ratchet | `make map-validate` |
| Every roof ladder lands on its roof | `--shot toaster <txt> traversal` |
| Each pad sends you to another and then makes you wait | `--shot toaster <txt> teleport` |
| Each vehicle drives, turns, stops at the edge and lets you out | `--shot toaster <txt> vehicle` |
| Eye-level read of the property | `--shot toaster <png> top`, `iso` |

The last one is the one that matters for this document. Every other check on
this list passes today, on a map made entirely of grey boxes.
