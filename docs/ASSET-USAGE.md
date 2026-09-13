# Asset usage

**612 delivered · 486 consumed · 126 unused** — regenerate with `make usage`.

`asset-report.sh` answers *has design shipped it*. This answers the question that
rots silently: an asset can be delivered, imported, and never referenced by a
line of code. That is invisible from the outside, because the game looks
finished right up until you open the folder.

## How it is measured

Two sources, because neither alone is complete:

- `--asset-audit` walks the content tables and builds one view of every enemy,
  tower, upgrade stage, trap, projectile and hero, then requests every icon the
  UI derives from those tables.
- A real solo match on each map with `--dump-assets`, which is the only thing
  that builds a level and therefore the only thing that touches the map kit.

Both print `[asset-audit] requested <name>`, and the script diffs that against
the files on disk. **Requests are reported by the code, not scraped from
source** — icon ids are built by interpolation (`$"enemy_{defId}"`), so grepping
for string literals under-reports by more than half.

One blind spot, recorded rather than papered over: a solo match builds nothing
and nothing gets hurt, so state variants the code *does* request — a spent trap
plate, a half-demolished barricade — never come up in the measurement. They are
marked `wired=yes` in the manifest by reading the code, and show as unused here.

The effects layer would have fallen into that blind spot whole — a solo run
never sells a tower, never triggers a reaction, never gets anything frozen — so
`Vfx.Catalogue()` enumerates every name the client can ask for and the audit
asks for all of them. That keeps the measurement honest in the direction that
matters: the list is built from the switch statements that draw the effects, so
an effect that loses its caller stops being counted.

There is a second check for the half a name list cannot answer — whether the
code path that would draw an effect is reachable at all:

```sh
./play --headless -- --shot foundry /tmp/vfx.txt vfx        # one of everything
./play --headless -- --shot foundry /tmp/statuses.txt statuses   # on a live enemy
```

The first fires one of everything through the real entry points and reports how
many drew design's model. It catches what neither the delivery report nor this
measurement can: an effect correctly delivered, correctly imported, correctly
named, and wired to a method nothing calls.

The second covers the half that cannot: a status effect is held by three things
agreeing — the sim applying it, the snapshot's channel byte still saying so,
and the view sync asking for it again every frame; a beam is drawn by a tower
that never creates a projectile; a ramp brightens or it does not. It builds a
Filament, an Arc and a Singularity, walks a wave past them, and passes when all
three are true of a live enemy.

```sh
./play --headless -- --shot foundry /tmp/rounds.txt rounds
```

The third is about a round in flight rather than an effect, and measures the
three things a screenshot shows instantly and nothing else can: the angle
between where a round points and what it is flying at, the nearest any round
gets to the muzzle it left, and how far a **client's** copy of a round drifts
from the sim's own. Tower projectiles do not cross the wire — a client is
handed the shot and flies the rest, the way `Vfx.RemoteShot` already
reconstructs a teammate's tracer from the damage it did — so the probe flies a
real client round alongside each of the sim's and compares them. It runs the
actual client code rather than a copy of the arithmetic, because that branch is
otherwise run by nothing: it would compile, ship, and be wrong for everyone who
joined a friend's game and nobody who tested it. Worst gap measured: 0.5 m,
against a 1 m gate. A Lance upstream of a Nova, because the mortar shell is
the model both faults are unmistakable on and a Nova reaching sixteen metres
clears the wave before it ever walks into a Lance's twelve. Run windowed it
waits for a frame with a shell actually in the air, aims at it, and writes the
picture. All three run in CI.

## What the effects layer took off it (2026-09-13)

Thirty-one names in `game/assets/vfx/` came off this list in one change, and
all but two of the folder is now drawn. The old row read *"No status/ability/
reaction VFX system yet"*; there is one now
(`game/scripts/Vfx.cs`, `docs/FORWARD-MANIFEST-vfx.md`).

Three things were needed beyond calling `Spawn` in more places, and each is
worth recording because none is visible from a file listing:

**Half the set has no fixed lifetime.** A muzzle flash lasts 70 ms and an
impact 220; a burn lasts until the burn ends and a revive column until the key
is let go, and nothing can know that in advance. Those are held rather than
fired: the frame that still wants one asks for it again by key, and anything
that went unasked-for is dropped at the end of the frame. The bookkeeping
inverts in the only direction that cannot leak — a status whose end the client
never hears about stops being drawn on the first frame the enemy's status bits
come back clear.

**A channel is not a status.** The snapshot's status byte says a channel is
occupied, which is enough to tint a silhouette and not enough to draw one:
Control holds either a 0.25 s stagger or a 1.2 s hard lock, and those are two
different pictures. `statusApplied` says which, the byte says for how long, and
both halves are needed. A client that joined mid-burn has the byte and not the
event, so each channel also has a default — the one status that channel holds,
except Control, where a stagger is the guess that ends before anyone reads it
wrong.

**Two facts about an enemy were never on the wire.** Shield and burrow are not
statuses and had no channel, so a networked Warden never showed its bubble *or*
its shield bar, and a burrowed Mole was inferred from hp reaching zero — which
is the reading for "dead". Protocol 5 adds a shield fraction and a state byte.
A fraction rather than a flag because the effect that matters is the one in
between: `vfx_shield_regen` is a window closing, and a bit cannot say that.

| came off the list | how it is driven |
|---|---|
| 8 `vfx_status_*` | held on the enemy while its channel is occupied, scaled to the body's own height — design authors for 1.8 m and a Monolith is two and a half times that |
| `vfx_reaction_thermalshock` · `_flashfreeze` | the reaction event, on the enemy it happened to |
| 3 `vfx_ability_*` | `abilityUsed`, which now carries the aim point — three of the five abilities happen somewhere other than at the hero, and only the player who pressed Q knew where they were looking |
| `vfx_tower_place` · `_sell` · `_upgrade` | their events, at the pad. Upgrade culls design's chevron tiers by level, which its note asks for |
| `vfx_wave_start` · `_clear` · `vfx_core_breach` | the wave beats, at the map's own gates and cores rather than at one of them |
| `vfx_shield_pop` · `_regen` · `vfx_burrow_spray` | transitions in the new snapshot fields, identical on both sides of the wire |
| `vfx_cluster_split` | the Cluster's death, not its five Motes' spawns — one thing happened, not five |
| `vfx_detector_pulse` | a 2 s sweep per Detector, staggered by tower id so a pair covering a junction sweeps rather than strobes. Not an event: an aura tower reapplies its status thirty times a second, and a pulse per tick is not a pulse |
| `vfx_revive_beam` | the sim's revive clock, now on the meta channel — so the teammate covering the door sees the same ring fill as the one crouched over the body |
| 6 `proj_*_t2` / `_t3` | design's rule from projectiles.js: the tier follows the damage path, L7 → T2, L10 → T3 |
| `proj_arc_beam` · `proj_filament_beam` | the two towers that fire no round — see below |

Two things were wrong on first play and are worth recording, because both are
the kind of failure that looks like working software:

**Two delivered projectiles had never been on screen at all.** The Arc and the
Filament never create a projectile in the sim — a tesla arc is instant and a
beam applies its damage where it stands — so the projectile view sync, which
follows sim projectiles, had nothing to follow for either. The models were
delivered, imported, correctly named, counted as consumed by this measurement
(the content audit instantiates one round per tower), and no player had ever
seen one: an Arc and a Filament dealt damage in silence. They are drawn now
from `TowerFired`, as a unit-length streak scaled to the hit — design's own
contract for them, and the same one the tracers use. The Arc's chain hop is
drawn too, the client mirroring the sim's nearest-other-target search the way
the turret aim already mirrors `PickTarget`. `filament_beam_ramp` is driven as
its note asks — emissive 0.3 → 2.0 as the beam's heat climbs — from a ramp
clock the client counts for itself: the sim adds one tick per tick it holds the
target and emits exactly one `TowerFired` for that tick, so counting events is
the same arithmetic rather than an approximation of it, and it works on a
client, which has no tower state at all. It is the only tell the Filament's
damage multiplier has ever had.

**Every round in the game flew sideways, and none of them came out of a
barrel.** Design authors each round along −Z — the Nova shell's nose and the
Lance slug's tip are both at the −Z end, with the shell's lit fuse cap on the
back — and nothing in the client had ever turned a projectile view. Unmistakable
on a brass mortar shell; on a thin blue bolt it read as a slightly odd streak,
which is how it survived four milestones. Separately, the sim spawns a round at
the tower's centre because it has no barrel to spawn one at: where the muzzle is
depends on the rig's yaw and pitch, which are a client animation the sim knows
nothing about. So the view launches from the barrel — the same place the muzzle
flash already happens — and closes the gap over the first fifth of a second.
Measured by the rounds probe below: **60° and 0.93 m before, 3° and 0.07 m
after.**

**Half the effects were being laid on their backs.** `Spawn` turned a model's
−Z toward a direction, and passing `Vector3.Up` to mean "this one points
upward" does the opposite of what it reads as: it tips an upright effect
ninety degrees. The wave-clear beat rose sideways out of the core, and every
ground ring in the set stood up like a hoop. Anything authored the way it
stands now passes no direction at all, and only things that genuinely aim —
muzzle flashes, tracers, beams — pass one. `vfx_impact_nova` had the same bug
before any of this work, with a comment next to it correctly saying the ring is
authored flat on the ground.

Two are left, and neither is art:

| still unused | why |
|---|---|
| `vfx_overclock_link` | the Overclock tower does not exist in the sim. It is the conduit from the pylon to each fed tower, and it waits with the chassis and thirty stage modules already delivered for it |
| `vfx_lanewash` | the floodgate's panic button. `shared_floodgate` is one of the M5 mutable-map elements the sim does not drive |

Three names the code now asks for and design has not drawn —
`vfx_reaction_corrode`, `vfx_ability_cryofield`, `vfx_ability_revealpulse` —
are in the manifest as requested-and-missing, the same way the M3 tracers are,
and [FORWARD-MANIFEST-vfx.md](FORWARD-MANIFEST-vfx.md) is the ask.

## What the 2026-09-12 drop added to the unused list

The drop (docs/ASSET-DELIVERY.md, plus the Toaster kit and vehicles it carried)
took the count from 129 to 178; designing the Spire took it back to 158, and
every name that came off the list was a Spire one. The Toaster's forty-odd files are all consumed
— tiles in four variants, roads, the drive, trees and their billboards, four
shells and roofs, the interiors, the dressing, the live warp gate, the vehicles
— and the 49 that are not fall into six groups, none of them by mistake:

| Group | Count | Why |
|---|---|---|
| Off-hand reload poses (`hands_<set>_magout` / `_magin` / `_charge`) | 18 | **Requested by code** — the reload animation swaps them in for the platform's own left hand — but only during a reload, and a solo match never reloads: the blind spot above. `--shot foundry <txt> reload` exercises all eighteen. |
| Reload props (`weapon_<id>_magazine` ×3, `weapon_scattergun_shell`) | 4 | Same blind spot: they exist for the half-second a magazine is in the air. |
| `weapon_scattergun_magazine` | 1 | **Deliberately unused.** The same hull as `_shell`, shipped under the armoury's name so nothing fell back to a placeholder; `_shell` is the one the code loads. |
| ~~Spire kit, first export~~ | ~~20~~ **0** | **The Spire was designed in M5 and every one of its 47 files is consumed.** Fifteen of those twenty were unplaced because the map was a graybox authored before the kit existed; the other five were variants nobody cycled. Two things beyond placement were needed and both are worth recording, because neither is visible from a file listing: **fifteen `spire_*` files had no `.import`** — the drop landed and Godot had never been run over it, so `AssetLibrary.Has` said no and the boundary wall, the parked cars, the skylight bay and every terrain variant silently did not exist, which is what made the plaza render as flat white; and the roof, boundary and terrain variants needed *cycling* rather than placing, or four of them stay on the shelf while one piece repeats on a lattice. |
| `shared_ladder_250` / `_500` / `_540` / `_1000` | 4 | Height variants of the ladder; the client scales the one it has. |
| `shared_warp_gate_idle` | 1 | The gate is live at both ends; idle is the fallback for a kit without the active one. |
| `foundry_gantry_walk` | 1 | A 4 m grating bridge design added so the Foundry gantry could clear its lane (MAP-AUTHORING §4.10); the gantry still uses a deck bay. |

## What the unused 129 were before it

The 2026-09-07 drop (427 models, 65 icons) replaced the first delivery's
geometry and added 47 names beyond the brief — M4/M5 enemies, the Glacier and
Specter heroes, map elements, status VFX. The armory redesign and the
first-person pass then put the weapon models, attachments, ammo, hands (now in
three poses, +12 files), tracers, muzzle flashes and impacts to work, and the
content audit builds all of them so the measurement sees it. What is left
waits on the system it is for.

| Group | Count | Why |
|---|---|---|
| Overclock tower (chassis + 30 stage modules) | 31 | Overclock does not exist as a tower yet (M4). Delivered early on purpose — art lead time is the schedule risk. |
| ~~VFX~~ | ~~27~~ **2** | **The effects layer landed on 2026-09-13 and draws 25 of the 27** — see the section above. The two left are the Overclock link and the lane wash, and both wait on a system rather than on art. |
| `_s1` stage modules | 19 | **Correct and intentional.** Design's chassis *is* the level-1 state and `_s1` is an empty root, so sim level N asks for stage N+1 and `_s1` is never requested. |
| Map elements | 13 | Crusher, floodgate, destructible wall (+ broken + debris), control-point capturing/held, caches, launcher charging/fired, physics props — M4 map elements the sim does not drive yet. **`shared_gate_operated` came off this list on 2026-09-12**: it is the lane lever, and Switchyard places two. The elevator and sniper nest *are* placed but inert: both have an `Area3D` and **no handler**, and the elevator is credited as a traversal exit by validator §4.1 without working. All of these are the subject of the M5 mutable-map work. |
| ~~Spire kit, unmounted pieces~~ | ~~8~~ **0** | All eight are placed by the Spire redesign: the antenna, HVAC and water tank as roof plant, the fire-escape flight and stairwell as the flights the routes actually climb, the lobby as the two doorways the kit names by z coordinate, the parapet on the roof's X ends and the scatter at the plaza margins. |
| Teleporter pad charged/cooldown | 2 | **Requested by code** (`SetPadArt`/`RefreshPadArt` follow charge and cooldown) — the blind spot above: a solo match never stands on a pad long enough to charge one. |
| M4/M5 enemies and states | 8 | Broodmother, Carapace (+ plate), Leaper (+ windup, airborne), the Ram's enraged state, the Shade's shimmer. Ram, Shade and Mender themselves are wired. |
| ~~Projectile tier variants (`_t2`, `_t3`)~~ | ~~6~~ **0** | **All six are drawn.** The rule is design's own, from projectiles.js: the tier follows the damage path, level 7 buys T2 and level 10 buys T3. Sim path levels count purchases rather than levels, so the thresholds in the client are 6 and 9 — the same off-by-one that makes a stage module ask for level+1. |
| Hero revive poses | 5 | Downed poses are wired; the revive-crouch pose is not. |
| Trap spent/triggered/rearming states | 5 | **Requested by code** (`RefreshTrapArt` follows `ChargesLeft`) — the blind spot above. |
| Faction-neutral hands, wrench viewmodel | 4 | Every player has a faction, so `hands_firstperson` (all three poses) is only a fallback; the wrench has no first-person view yet. |
| Barricade damaged/broken | 2 | **Requested by code** (`RefreshBarricadeArt` follows structure health) — the blind spot above: nothing gets hurt in a solo match. |
| Icons | 2 | `icon_tower_overclock` (no tower) and `icon_weapon_wrench` (melee has no armory card). |
| Socket base plates | 2 | The occupied-socket art is the tower's own foot. |
| `ui_nameplate` | 1 | No world-space nameplates yet. |

Nothing in that list is unused *by mistake*.

## What this evaluation found

Three real gaps, all fixed:

**Content ids are camelCase, design's filenames are not.** `emberPistol`,
`longBarrel`, `ignitionWave` and `chainSurge` resolved to
`weapon_emberPistol` / `attach_longBarrel` / `ability_ignitionWave`, none of
which exist — design ships `weapon_emberpistol`, `attach_longbarrel`,
`icon_ability_ignitionwave`. All of them silently fell back to placeholder
chips. This is the exact failure the seam was built to prevent, and it survived
because **the placeholder is designed to look deliberate**. Both loaders now
lower-case the id before lookup.

**Trap states were never asked for.** Design ships armed / triggered / spent for
every trap because charges and rearm are gameplay information, and the client
only ever requested the armed model. A spent plate that still looks armed is a
lie the player pays for. Trap art now follows `ChargesLeft`.

**`shared_core_hit` was never asked for.** A leak costs a life wherever it
happened; the core now flashes to its struck state for a beat on every leak,
on both the host and client paths.

Found by the 2026-09-07 re-measurement:

**The manifest's `wired` column had rotted.** Fifty-seven rows said `no` for
names a played match requests every time — the whole Foundry and Switchyard
kits, every socket pad, the core and its struck state, the Detector and
Filament towers. The column now follows this measurement (a delivered name is
`yes` when the game asked for it), plus the code-read exceptions above.

**Stage modules never followed the barrel.** Godot's glTF import wraps each
file's root node in an extra scene root, so the merge that reparents a
module's parts onto the chassis rig nodes (`MergeRig`) compared two wrappers
with different names and reparented the whole module instead. Nothing showed,
because the whole tower turned as one node. It surfaced the moment the yaw and
pitch nodes started being driven (`TowerRig`); the merge now starts below the
wrapper.

**Scrap had no floor to land on.** The five `pickup_*` models were delivered
and unused because a kill credited scrap straight into the wallet. Enemies drop
the personal half of it now and a player walks over to collect it, so all five
are consumed. Nothing automated kills anything in a `--solo` run, so the
content audit asks for the models directly rather than waiting for a match that
never shoots.

**Five M3 names had no manifest row.** The armory and the first-person view
now ask for `ammo_shock`, `ammo_toxin`, `attach_toxinfeed`, `vfx_tracer_shock`
and `vfx_tracer_toxin`; design has not modelled them and the brief never named
them, so `--verify` failed the moment the code was honest about wanting them.
They are in the manifest as requested-and-missing now.

**One icon name drifted.** The brief and the drop name the cryo ammo icon
`icon_ammo_cryo`; the sim's M3 ammo rows are `cryoRounds`, `shockRounds`,
`toxinRounds`, so the armory asks for `icon_ammo_cryorounds` and draws a chip
beside a delivered icon. Recorded in the design-system README with the other
missing icons; the fix is on the code side (the brief is the contract).

## Keeping it honest

```sh
make usage          # counts
make usage-list     # counts plus every unused name
```

When a system lands that should consume a group above, the count moves and the
row leaves this table. If a number goes *up* without a delivery, something
stopped being referenced.
