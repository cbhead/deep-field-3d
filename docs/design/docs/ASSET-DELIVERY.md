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
