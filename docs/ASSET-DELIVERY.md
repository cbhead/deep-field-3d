# Asset delivery — 2026-09-11

Regenerated from Claude Design (`Deep Field 3D - Asset Export.html`, source of truth
`models/*.js`). The zip unpacks onto the repo root as before.

**This drop answers [`docs/FORWARD-MANIFEST-reload.md`](FORWARD-MANIFEST-reload.md)** —
Asks A–D and §8b. It adds **35 GLB names** and renames **three nodes**; no existing
file is removed. The previous drop note (2026-09-07: hero weapon rebuild, shader
skyboxes) is superseded by this one but nothing it delivered has changed.

> One thing to read before the table: your §2 constraint holds completely. Nothing
> here ships an animation channel, a skin or an armature. Every new file is either a
> static part with a correct pivot or a static pose, and everything that moves moves
> because code moves a named node.

---

## Ask A — a magazine that exists off the gun · **done, 5 files**

| file | what it is |
|---|---|
| `weapon_sidearm_magazine.glb` | the `sidearm_magazine` node, exported alone |
| `weapon_rifle_magazine.glb` | the `rifle_magazine` node, exported alone |
| `weapon_emberpistol_magazine.glb` | the fuel canister (see below) |
| `weapon_scattergun_magazine.glb` | a loaded 12-gauge hull, under the name you asked for |
| `weapon_scattergun_shell.glb` | the same hull under the name that tells the truth |

Same geometry and materials as the node in the viewmodel — not a new design, as
requested — at viewmodel scale, origin on the seated face, +Y up.

**Two of the four are not magazines, and the models say so.**

- **Ember pistol** feeds from a pressure vessel, not a box. It is the part that
  detaches and it is what a reload replaces, so it now *also* carries the armoury's
  `_magazine` name (`emberpistol_magazine`, wrapping `emberpistol_canister` and its
  sight glass, bezel and hazard bands). The regulator valve stays on the frame — the
  vessel screws into it. Origin is the **forward dome**, and because it pulls off
  rearward its drop axis is **+Z**, not −Y.
- **Scattergun** is tube-fed. There is no detachable magazine on it and there should
  not be: its reload is shells through the loading port, one at a time. So the prop
  it needs is a **loaded hull** — brass head with rim and extractor groove, full
  length, folded six-point crimp cut into the dome as ridges. It ships under
  `weapon_scattergun_magazine.glb` so nothing falls back to a placeholder while the
  name is in your table, and under `weapon_scattergun_shell.glb`, which is the one to
  keep. Distinct from `scattergun_case`, which is *fired*: open mouth, no crimp.

Both weapons gained the control the reload presses: `emberpistol_mag_release` (a
brass canister latch at the regulator seat, on a brazed boss) and
`scattergun_mount_loadport` (the port under the receiver, not the tube cap).

---

## Ask B — off-hand poses · **done, 18 files — plus 12 that were missing**

`hands_<set>_magout`, `hands_<set>_magin`, `hands_<set>_charge`, for all six sets
(`firstperson`, `ember`, `forge`, `glacier`, `specter`, `tempest`).

**They carry the off-hand alone.** The firing hand never leaves the grip during a
reload, so a full two-hand set per pose would ship a firing hand that is wrong on
every platform but the one it was posed for. Instead each file is one posed left arm
rooted at `hand_l`, and `userData.offhand = { pose, side, replaces: 'hand_l' }` says
so in the file. **Client side that means: hide `hand_l` in the platform's own hands
file and parent the pose file under the same camera node.** One set of three then
serves pistol, rifle and tool platforms alike, which is why this is 18 files and not
54.

Each pose is the authored support hand with its digits closed by a curl angle and the
arm swung about its own wrist. The curl axis is derived from the hand's own geometry —
the MCP row crossed with the finger direction, signed toward the palm — so the fingers
close into a grip rather than hinging about a global axis.

Where they actually land, measured against the Sidearm:

| pose | off-hand is | `hands_mount_magazine` |
|---|---|---|
| `magout` | 7 cm left, 9 cm below, 13 cm behind the magwell — clear of the gun | present, carrying the spent mag |
| `magin` | at the magwell | **within 4 mm of `sidearm_mount_magwell`** |
| `charge` | palm over the receiver rear, fingers closed to a thumb-and-index pinch | — |

`magin` landing on the well within 4 mm is the number that matters: parent
`weapon_<id>_magazine` to `hands_mount_magazine` and the prop is where it belongs with
no offset in code.

**The 12 that were missing.** ART-INTEGRATION.md says "the export also emits
`hands_<faction>_pistol` and `_tool`". It did not — the exporter only ever wrote
`it.file`, so those 12 names were documented, requested and never delivered, and every
platform was falling back to the rifle pose. The exporter now emits one file per pose
for any hand set, so the six sets go **6 → 36 files**. That was the actual reason the
off-hand looked wrong before any reload work.

---

## Ask C — pivots · **audited; one row needed changing, four did not**

The distinction that decides this: **translation does not care where a pivot is.**
Moving a group along the bore puts every vertex in the same place whether the pivot is
on the bore axis or at the weapon origin. Only *rotation* cares. So:

| part | motion | verdict |
|---|---|---|
| `<w>_magazine` | translate down **and tip out** | **was wrong — fixed.** Pivot was at the weapon origin, so a tip swung the magazine through the frame. Now on the seated face. |
| `<w>_slide`, `<w>_bolt_carrier` | translate along the bore | **already right; no change needed.** Pure translation. |
| `<w>_charging_handle`, `<w>_bolt_handle` | translate along the bore | **already right.** Same reason. |
| `<w>_mag_release`, `<w>_bolt_catch`, `<w>_bolt_release` | press a few mm | **already right.** Same reason. |

I deliberately did **not** re-pivot the translating parts: the offset would be noise in
the file and one more number to keep in step with nothing.

The magazine fix is `repivot()` in `models/gun-kit.js` — it moves the group to the seat
and counter-translates its children, so not a vertex moves and the world transform is
identical; only the centre of rotation changes. Seats used:

- `sidearm_magazine` → the seated top face, `[0, −0.0060, 0.0018 + 6 mm·tan(rake)]`
- `rifle_magazine` → the magwell mouth plane where the 25 mm curve crosses it, 1.9 mm
  forward of the well centre
- `emberpistol_magazine` → the forward dome

Each group also carries `userData.drop = { axis, tip, clear }` so the drop direction,
tip axis and the clearance before the tip starts come out of the file instead of being
guessed per weapon.

**New node: `<w>_mount_magwell`.** `_mount_magazine` is the *attachment* origin for a
drum, and on the Sidearm it hangs 36 mm below the face the magazine actually seats
against — a prop spawned there would fall out of the air below the gun. The three
magazine-fed platforms (Sidearm, Rifle, Ember pistol) now also expose
`<w>_mount_magwell`, which is the seat; §8 step 2 should use it. **The Scattergun has
no magwell by design** — it is tube-fed, so its feed point is
`scattergun_mount_loadport` (the port under the receiver) and the shell prop is pushed
in along +Z from there.

---

## Ask D — `poisonStream` and `cryoSprayer`

Confirmed, no change: still M4 entries in `forward-manifest.json`, still unmodelled.
When they land they will arrive with their action parts, a standalone canister and a
`_mount_magwell`, to the same contract as the four above — noted so it does not need a
second pass.

---

## §8b — turret elevation

**Filament's pitch ceiling is raised 48 → 78.** `rig.pitchLimits` for `filament` is now
`[-12, 78]`, which clears your measured 74° near-overhead case with margin, and the
emitter head still cannot fold back into its own radiator stack. `make design-export`
rewrites `game/assets/structures/manifest.json`, so nothing needs editing by hand.

**Arc: keep your fallback — the missing rig row is deliberate.** Arc is an aura tower.
It has no `_yaw`, `_pitch` or `_muzzle` node because it has nothing to point: the field
emits from the whole ring, and what turns is `arc_spin`. Giving it a `rig` row would
name three nodes that do not exist and make `TowerRig.cs` drive a tower that should
never traverse. Your air-capable fallback is the right fix and I would rather not paper
over it with a row. Same for Detector, Singularity and Overclock. If you want Arc to
*look* like it tracks, the honest version is a cosmetic `extras.role = cosmeticSpin`
bias on `arc_spin`, which I can add — say the word.

---

## Regressions this surfaced, and fixed

The hero rebuild (2026-09-07) replaced four weapons wholesale, and three names in your
§3 table went with them. Your document could not have known — it was written against
the vendored `docs/design/`, which predates that drop.

| name in §3 | reality | now |
|---|---|---|
| `scattergun_magazine` | never existed post-rebuild; the gun is tube-fed (`scattergun_mag_tube`) | unchanged — see Ask A |
| `scattergun_bolt_handle` | was split into `_bolt_handle_shaft` + `_bolt_knob`, two nodes to keep in step | **grouped** under `scattergun_bolt_handle` |
| `rifle_bolt_catch` | was `rifle_bolt_catch_pin` | **renamed** to `rifle_bolt_catch` |
| `emberpistol_magazine` | was `_canister` only, so the viewer's magazine-slot hide found nothing | **added** as the group name |
| `rifle_bolt` | no such mass — on this action the reciprocating group *is* the carrier | use `rifle_bolt_carrier` (has `userData.recoil`) |
| `sidearm_chamber_window` | no such node by design — the ejection pocket is displacement in the slide profile, not a part glued to it | nothing to toggle; slide travel exposes the chamber on its own |

Also: the magazine-slot hide in `ui-weapon-view.js` and the Gunsmith now hides
`<id>_mag_tube` as well as `<id>_magazine`, so fitting a drum to the Scattergun removes
the tube it replaces instead of leaving both.

---

## Where to look at this

`Deep Field 3D - Gunsmith.html` has a new **Reload** tab: the four props and the three
off-hand poses, each with its origin, drop axis and source node, so the pivots can be
eyeballed before the drop rather than after.

## Integration

1. Unzip over the repo root (`unzip -o deepfield-3d-assets.zip -d .`).
2. `godot --headless --path game --import`.
3. `./tools/prepare-icons.sh` — unchanged, icons still use `currentColor`.
4. `make assets` — expect **+35 names**: 5 reload props, 30 hand poses. Two renames
   (`rifle_bolt_catch`, `scattergun_bolt_handle`) are *node* names inside existing
   files, so no filename changes.
5. `--asset-audit` will now resolve `weapon_scattergun_magazine`; add
   `weapon_scattergun_shell` and the 30 `hands_*_<pose>` names to
   `docs/asset-manifest.tsv` or CI will flag them as delivered-but-unnamed.

**New nodes inside existing files** (no new filenames, nothing for `make assets` to
report — listed so lookups can be written against them):

| node | on | is |
|---|---|---|
| `<w>_mount_magwell` | `sidearm`, `rifle`, `emberpistol` | the face the magazine seats against — spawn point for the reload prop |
| `scattergun_mount_loadport` | `scattergun` only | the loading port under the receiver; the Scattergun has no magwell |
| `hands_mount_magazine` | `hands_<set>_magout`, `hands_<set>_magin` | where the magazine prop rides in the off-hand |
| `scattergun_shell` | `scattergun` viewmodel | hidden loaded-hull template, cloned per shell fed |
| `emberpistol_mag_release`, `emberpistol_mag_release_boss` | `emberpistol` | the canister latch the reload presses |

---

## Integration record — 2026-09-07 (code side)

What actually landed when this drop was generated from the project sources
(`make design-export`), against the note above:

- **427 GLB names, not 380.** The sources had moved past this note: 47 names are
  new — the M4/M5 enemies and their states (Broodmother, Carapace + plate,
  Leaper + windup/airborne, Ram + enraged, Shade + shimmer, Mender), the Glacier
  and Specter heroes with downed/revive poses and hands, the scrap pickups, the
  Filament beam, Detector pulse, Overclock link and lane-wash VFX, poison /
  reveal / stun status VFX, and the M3–M4 map elements (teleporter pad states,
  elevator + shaft, sniper nest, crusher, floodgate, operated gate, destructible
  wall states, caches, physics props). Fifteen of those are not in the brief;
  they are listed at the end of `docs/asset-manifest.tsv`.
- **Requested-and-missing is shorter than the note says.** `hero_glacier`,
  `hero_specter` (+ `_downed`) shipped. Still missing: the Spire kit
  (`spire_floor`, `spire_facade`, `spire_roof`, `spire_fireescape`,
  `spire_path_ground`, `spire_skybox`) and `icon_cond_fog` / `icon_cond_night`,
  plus the fourteen M3 icons listed in `docs/design-system/README.md`.
- **The `wired` column was refreshed** from a measured run (`make usage`)
  rather than flipped by hand; 57 rows changed.
- **Turrets now run on the rig** (`game/scripts/TowerRig.cs`): yaw/pitch/spin
  nodes, limits and slew rates from `manifest.json`. The muzzle node is found
  but not yet used as the projectile origin.
- **Barricade states are wired** to structure health (damaged below ⅔,
  broken below ⅓).
- **Camera far planes** on the review-shot cameras were raised past the 700 m
  dome. The player camera already used Godot's default.

---

## Integration record — 2026-09-12 (code side)

**Six Singularity upgrades were invisible.** `pruneToPath` in the export page
matched `^<id>_up_<path>_l\d+$`, and the Singularity is the only tower that
names increments per part rather than as one group — `perArm` builds
`singularity_up_<path>_l<N>_arm<i>` and `cue()` mirrors the proxy's visibility
onto each copy (`models/singularity.js`). The `$` dropped every arm copy, so
range s2, s3, s6, s8, s10 and rate s9 exported identical to the stage below
them: an upgrade you paid for and could not see. The pattern now matches the
level and then a boundary, `l\d+(?:_|$)`, so a suffixed child counts as part of
its increment. Range went from `[0, 0, 0, 3624, 4544, 4544, 6836, 6836, 14068,
14068]` triangles to a ladder that climbs at every rung. Eleven files changed;
no other tower uses `perArm`.

⚠ **The same fix is needed upstream.** It is applied here to
`tools/design-export/export.html` (which a drop does not overwrite) and to the
vendored `docs/design/Deep Field 3D - Asset Export.html` (which a drop *does*
overwrite). A zip built on design's side still carries the old pattern, so the
six stages come back unless design takes the change. `make model-validate`
fails if they do.

**A gate for all of it.** `make model-validate` (`tools/model-validate.py`)
reads every delivered `.glb` and checks what the client actually consumes: the
rig chain `TowerRig` resolves, the empties `MergeRig` merges onto, the mount
nodes `WeaponAssembly` hangs modules from, manifest against disk, and the rule
that caught this — a stage the manifest says *adds* something must add
triangles. No Godot, no browser, no dependencies, about two seconds for the
whole drop, so it runs in CI's fast lane and in `make check`. Everything is
ratcheted against `docs/model-validation-baseline.tsv`, the way `map-validate.sh`
works, because some of today's exceptions are correct as they stand — and a
contract row there means a defect somebody chose to carry, restated on every run
rather than passing quietly. Pointed at the pre-fix drop it reports all six
invisible upgrades and the twelve missing manifest rows; pointed at this one it
reports `toaster_skybox`.

**Partial rebuilds.** `make design-export ONLY=<sel>` rebuilds one tower, one
model, or one category instead of the whole drop — see
[ART-INTEGRATION.md](ART-INTEGRATION.md). A filtered run merges its rows into
`game/assets/structures/manifest.json` rather than replacing it; replacing it
would drop the `rig` blocks `TowerRig` reads and silently reset every tower to
default limits.

⚠ **`toaster_skybox` in this drop disagrees with its own manifest row**, and the
gate above is what found it: the row says 0 triangles and 0 parts, the file has
2208 and 1. `triCount` / `countMeshes` skip any mesh flagged `userData.gizmo`,
and `bakeSkies()` only clears that flag on a sky it re-bakes. The other three
skyboxes are shader skies, so they get baked, cleared and counted. The toaster's
ships a pre-baked `MeshBasicMaterial`, never goes through `bakeSkies`, keeps the
flag — and the exporter writes the mesh anyway. Carried in the baseline with a
note rather than fixed here, because it is this drop's to fix: clear the flag on
the toaster sky, or stop skipping a mesh that is actually exported.

**Other things this pass found and fixed:**

- A full export overwrote this file with design's copy and deleted the
  integration record above. It now carries every `## Integration record`
  section onto the fresh note. A zip drop unpacked by hand still clobbers it —
  re-append by hand after one.
- The 12 `hands_<faction>_pistol` / `_tool` files had no manifest rows: the
  pose re-export skipped the manifest entirely. This drop replaced that
  mechanism with a per-item `poses` list, which writes rows for every pose, so
  the gap is closed on both paths.
- Nineteen GLBs re-exported with identical geometry — same triangle count, part
  count and bounds — but a few bytes' difference from what was committed. Sixteen
  were a one-time convergence onto the pinned toolchain (glTF float formatting);
  those bytes are committed and a full run no longer touches them. The other
  three — `foundry_terrain`, `switchyard_terrain`, `foundry_wall_boundary` — are
  the canvas-drawn floor textures from `models/floor-textures.js`, and they
  alternate between two variants of *identical size* with identical geometry:
  the browser's 2D rasteriser anti-aliases the grain rects a little differently
  between runs. Harmless, and below the level this repo can reach — so expect a
  full `make design-export` to leave up to three textured map files dirty with no
  real change, and `git checkout` them. `make model-validate` compares geometry,
  not bytes, so it is unaffected.
- `head_height` (overhead bars) was `1.9 × EnemyScale`, calibrated against the
  graybox. It now reads the delivered model's own height from the manifest, so
  a Ram's bar stops sitting inside its head and a Monolith's stops floating.
- **Left alone on purpose:** the rig muzzle nodes sit at 1.52 m (Lance), 1.62 m
  (Nova), 1.86 m (Skywatch) and 2.03 m (Filament), while the sim spawns every
  round at a flat 1.5 m (`sim/Sim.Core/Step.cs`). Flashes already come from the
  node (`GameRoot.OnTowerFired`); it is the round itself that starts below the
  barrel, by half a metre on a Filament. Closing it means either moving the sim's
  origin — which changes ballistics, hit timing and the gates that measure them —
  or a view-side offset that decays over the first frames. Both are calls to
  make deliberately, not defects to quietly patch.
