# Reload — forward manifest for Claude Design

**Status:** open asks, written 2026-09-10 against `main`.
**Audience:** Claude Design. This is a request list; nothing in the delivered
art has been altered.
**Scope note:** **no backend work is planned or needed.** The sim already has
the whole ammo and reload system. What is missing is the art a reload is made
of, and this document names it.
**Read first:** [docs/MAP-AUTHORING.md](MAP-AUTHORING.md) for how these
documents work; [FORWARD-MANIFEST-switchyard.md](FORWARD-MANIFEST-switchyard.md)
is the other one.

---

## 1. Where this actually stands

Two notes in the repo said the sim had no ammo pool and that the magazine
readout was "waiting on the ammo-quantity system". Both were stale and are
corrected in the same change as this document. The truth:

| | |
|---|---|
| Magazine size, reload duration | **done** — `WeaponDef.MagazineSize`, `WeaponDef.ReloadSeconds` |
| Rounds tracked per weapon | **done** — `PlayerState.RoundsIn(weaponId)` |
| Reload timer, hands-busy lockout | **done** — `player.ReloadTimer`, firing blocked while it runs |
| Auto-reload when the magazine runs dry | **done** |
| Events for a client to hang art on | **done** — `SimEvent.ReloadStarted(playerId, weaponId, seconds)` and `SimEvent.Reloaded` |
| Player command, bound to **R** | **done** — `Command.Reload` |
| **Anything visible when it happens** | **nothing at all** |

So a player presses R, the sim runs a 1.6-second reload during which they
cannot fire, and the screen does not acknowledge it. The gun does not move, the
hands do not move, and there is no round counter. That is the gap.

## 2. The constraint that shapes every ask below

**Not one of the 504 delivered GLBs carries an animation channel or a skin.**
Checked across the whole drop; the count is zero.

That is not a complaint — it is the established contract, and everything that
moves in this game moves because code moves a **named node**: a tower's
`_yaw` and `_pitch`, a scrap pickup's `_spin`, a barricade swapping to
`_damaged`. The reload will be animated the same way.

**So: please do not ship animation clips for this.** The exporter has never
emitted one, the client has no player for them, and a clip would arrive as dead
weight in the file. Ship **separable, correctly-pivoted parts** and **pose
variants**, which is what the kit already does well.

## 3. What already works — do not change it

The gun-side geometry is in better shape than I expected. Every delivered
viewmodel already separates its action:

| weapon | parts already named and separate |
|---|---|
| `sidearm` | `sidearm_magazine`, `sidearm_mag_release`, `sidearm_slide`, `sidearm_chamber_window` |
| `rifle` | `rifle_magazine`, `rifle_mag_release`, `rifle_bolt`, `rifle_bolt_catch`, `rifle_charging_handle` |
| `scattergun` | `scattergun_magazine`, `scattergun_bolt_carrier`, `scattergun_bolt_handle`, `scattergun_bolt_release` |
| `emberpistol` | `emberpistol_magazine`, `emberpistol_canister` |

All four also carry the full mount set including `<weapon>_mount_magazine`, so
the seat the magazine returns to is already a known point in the file. Between
them these cover a drop-the-mag, seat-the-mag, work-the-action reload almost
entirely. **Nothing in this section needs redrawing.**

---

## 4. Ask A — a magazine that exists off the gun (4 files)

`weapon_sidearm_magazine.glb` · `weapon_rifle_magazine.glb` ·
`weapon_scattergun_magazine.glb` · `weapon_emberpistol_magazine.glb`

Today a magazine exists **only as a child of its weapon**. A reload needs one in
two places the gun is not: falling away from the gun as a spent mag, and held in
the off-hand as the fresh one. Both need a model that stands alone.

- Same geometry and materials as the `<weapon>_magazine` node already in the
  viewmodel — this is that part, exported on its own, not a new design.
- Origin at the **seated top face** (the surface that meets `_mount_magazine`),
  +Y up, so dropping it is a translation down and a small tip about X rather
  than a swing about some arbitrary centre.
- Ship it at viewmodel scale. The spent mag is seen at arm's length for about
  half a second; it does not need world-model detail.

**This is the one ask that unblocks a readable reload on its own.** With just
these four, code can drop the spent magazine, seat a fresh one, and work the
slide or bolt using parts that already exist.

## 5. Ask B — off-hand poses (up to 18 files)

Hand sets today are `ember`, `firstperson`, `forge`, `glacier`, `specter`,
`tempest`, each in three poses: base, `_pistol`, `_tool`. The off-hand is the
half of a reload the gun cannot tell.

| pose | file | the off-hand is… |
|---|---|---|
| `magout` | `hands_<set>_magout.glb` | clear of the gun, having just pulled the spent magazine |
| `magin` | `hands_<set>_magin.glb` | at the magwell, seating the fresh magazine |
| `charge` | `hands_<set>_charge.glb` | on the charging handle or pump |

Six sets × three poses = 18 files. The exporter's `HAND_POSES` array is where
these get added; the naming follows the `<file>_<pose>.glb` scheme already in
use, so nothing else changes.

**If 18 is more than this is worth**, the ask degrades cleanly: `magin` alone,
six files, gets a reload that reads — the off-hand snaps to the magwell instead
of arcing through the motion. `charge` only matters for `rifle` and
`scattergun`, but any faction can carry those, so it is all six sets or none.

## 6. Ask C — pivots on the parts that already exist

No new geometry; this is a check on what is there, because a part with its
pivot at the model origin swings instead of sliding and there is no way to fix
that in code.

| part | motion code will apply | pivot must be |
|---|---|---|
| `<w>_magazine` | translate down and tip out | seated top face |
| `<w>_slide`, `<w>_bolt`, `<w>_bolt_carrier` | translate along the bore | anywhere on the bore axis |
| `<w>_charging_handle`, `<w>_bolt_handle` | translate along the bore | anywhere on the bore axis |
| `<w>_mag_release`, `<w>_bolt_catch`, `<w>_bolt_release` | press a few millimetres | on the press axis |

If a pivot is already right, say so and this row costs nothing.

## 7. Ask D — the two weapons with no viewmodel at all

`poisonStream` and `cryoSprayer` are in the sim's weapon table and have **no
`_vm` or `_world` model delivered**, and no row in `docs/asset-manifest.tsv`.
They are already M4 entries in `forward-manifest.json`, so this is not a new
ask — only a note that **when they land they should arrive with their action
parts and a standalone magazine or canister**, to the same contract as the four
above, rather than needing a second pass.

---

## 8. What code will do with these

Stated so the parts are built for the motion, not just for the still frame. The
client listens for `SimEvent.ReloadStarted`, which carries the duration, and
plays a sequence scaled to it — `ReloadSeconds` is per weapon, so the animation
has to stretch rather than assume 1.6 s:

1. hand to `magout`, `<w>_mag_release` presses, `<w>_magazine` hides
2. a `weapon_<id>_magazine` prop falls from `<w>_mount_magazine` under gravity
3. hand to `magin` carrying a second magazine prop up to the mount
4. `<w>_magazine` shows again, the prop is released
5. hand to `charge` for weapons with a bolt; `<w>_bolt` or `<w>_charging_handle`
   travels and returns

Everything there is a transform on a named node or a swap of a whole model.
Nothing needs a skin, an armature, or a clip.

## 8b. Turret elevation — one number, and it is visible in play

Not a reload ask, but it lives in the same file design already ships and it is
measured now, so it belongs here.

`game/assets/structures/manifest.json` carries a `rig.pitchLimits` per tower.
Filament's is **[-12, 48]**. A Skiff on the air strand passing near-overhead sits
at **74 degrees** from a pad beneath it, so the barrel parks at its 48 degree
ceiling and beams a target it is visibly pointing 26 degrees below. The sim
deals full damage throughout; only the model is wrong, which is exactly the kind
of thing that reads as the game being broken.

Measured by `--shot <map> <txt> aimair`, deterministic across runs:

| tower | ceiling | wanted | reached | error |
|---|---|---|---|---|
| skywatch | 82 | 46.3 | 43.2 | **3.1** (slew lag, fine) |
| filament | **48** | **74.0** | 48.0 | **26.0** |

**Ask:** raise Filament's pitch ceiling to at least **78 degrees**. Skywatch's 82
is the right shape for a tower that shoots upward and is the reason it measures
clean.

Two related notes:

- **Arc, Detector and Singularity have no `rig` row at all.** Detector and
  Singularity are auras with nothing to point and a Barricade has no weapon, so
  only Arc matters — and Arc targets air. It was inheriting a 26 degree fallback
  ceiling; the client now gives air-capable towers a usable fallback instead, so
  this one is handled our side. A real row would still be better than a default.
- Yaw is not the problem anywhere: every tower measured **0.0 degrees** of
  heading error once settled.

## 9. Not being asked for

- **Animation clips or skinned hands.** See §2.
- **A reserve-ammo model.** The reserve is unlimited by design — "a magazine
  runs dry and refilling it costs seconds you do not have" — so there is no
  pouch count to show.
- **Any backend.** It is done, and this document exists partly to stop the next
  person re-deriving that from two stale comments.

## 10. How we will know it landed

| Check | Command |
|---|---|
| Names match the brief | `make assets` |
| Nothing delivered goes unused | `make usage` |
| Nothing requested is missing | `--asset-audit` |
| The parts survive the gunsmith's rebuilds | `--shot <map> <png> vm-<weapon>` |
