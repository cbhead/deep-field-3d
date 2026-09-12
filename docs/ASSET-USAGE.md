# Asset usage

**612 delivered · 434 consumed · 178 unused** — regenerate with `make usage`.

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

## What the 2026-09-12 drop added to the unused list

The drop (docs/ASSET-DELIVERY.md, plus the Toaster kit and vehicles it carried)
took the count from 129 to 178. The Toaster's forty-odd files are all consumed
— tiles in four variants, roads, the drive, trees and their billboards, four
shells and roofs, the interiors, the dressing, the live warp gate, the vehicles
— and the 49 that are not fall into six groups, none of them by mistake:

| Group | Count | Why |
|---|---|---|
| Off-hand reload poses (`hands_<set>_magout` / `_magin` / `_charge`) | 18 | **Requested by code** — the reload animation swaps them in for the platform's own left hand — but only during a reload, and a solo match never reloads: the blind spot above. `--shot foundry <txt> reload` exercises all eighteen. |
| Reload props (`weapon_<id>_magazine` ×3, `weapon_scattergun_shell`) | 4 | Same blind spot: they exist for the half-second a magazine is in the air. |
| `weapon_scattergun_magazine` | 1 | **Deliberately unused.** The same hull as `_shell`, shipped under the armoury's name so nothing fell back to a placeholder; `_shell` is the one the code loads. |
| Spire kit, first export | 20 | `BuildSpireStructures` is the graybox from before design's redesign and mounts the floor, facade, roof and terrain; the redesign's antenna, HVAC, water tank, lobby, stairwell, fire-escape flight, interior lane, parapet, boundary wall and dressed car have no placement yet. Adopting the authored Spire (`docs/design/models/levels.js`) is its own change — new socket ids, save data, the harness build order. |
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
| VFX | 27 | No status/ability/reaction VFX system yet: status particles, reaction bursts, ability effects, shield pop/regen, tower place/sell/upgrade, wave start/clear, the Detector pulse, Overclock link and lane wash. (Muzzle flashes, impacts and tracers are consumed now.) |
| `_s1` stage modules | 19 | **Correct and intentional.** Design's chassis *is* the level-1 state and `_s1` is an empty root, so sim level N asks for stage N+1 and `_s1` is never requested. |
| Map elements | 17 | Teleporter pad states, elevator, sniper nest, crusher, floodgate, operated gate, destructible wall, control-point and launcher-pad states, caches, physics props — M3/M4 map elements the sim does not drive yet (the pads and nest that *are* placed use the idle/neutral state). |
| M4/M5 enemies and states | 8 | Broodmother, Carapace (+ plate), Leaper (+ windup, airborne), the Ram's enraged state, the Shade's shimmer. Ram, Shade and Mender themselves are wired. |
| Projectile tier variants (`_t2`, `_t3`) | 6 | No tier escalation wiring; projectiles use the base model at every level. |
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
