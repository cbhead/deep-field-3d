# The hero standard — forward manifest for Claude Design

**Status:** open asks, written 2026-09-12 against the drop of the same day
(`docs/ASSET-DELIVERY.md`, "Asset delivery — 2026-09-11", plus the Toaster kit
and vehicles that drop carried without saying so).
**Audience:** Claude Design. A request list, not a change log — nothing here
has been altered in the delivered art.
**Read first:** [docs/MAP-AUTHORING.md](MAP-AUTHORING.md), then
[docs/FORWARD-MANIFEST-toaster.md](FORWARD-MANIFEST-toaster.md) — the map this
drop answers, and the place its remaining asks live.

---

## Why this document exists

The drop is good. Every node the client depended on still resolves against the
rebuilt weapons; the four vehicles carry the rig contract exactly; the Toaster
kit came in under every budget it was given and the map runs on it at 5,041
nodes. The reload animation the reload manifest promised is built on the
delivered magazines and off-hand poses and passes its probe on all four
platforms.

What went wrong is weight, and it is the same thing three times: **every file
that uses the hero texture set embeds its own copy of it.** That is the whole
document.

---

## P0 — share the textures

### A. One texture set for the hands, referenced, not embedded

Measured from the files:

| | size | of which textures | geometry |
|---|---|---|---|
| `hands_<set>.glb` (rifle pose, old rig) | 1.0 MB | 0 | 1.0 MB |
| `hands_<set>_pistol.glb` | 6.6 MB | 4.85 MB | ~1.7 MB |
| `hands_<set>_magout` / `_magin` / `_charge` | 5.7 MB each | 4.85 MB | ~0.9 MB |

The 4.85 MB is five PNGs (a 1.5 MB normal map the largest) and it is
byte-identical in all twenty-four hero-hand files. That is **116 MB of one
texture set**, and the weapon viewmodels repeat the pattern: `weapon_sidearm_vm`
is 14.4 MB with 9.7 MB of PNG inside it, and every `_world` twin is a
byte-for-byte copy of its `_vm`. The drop took the tracked art from 205 MB to
630 MB, and the textures are 350 MB of the difference.

**Ask:** export the hero texture sets once each as loose PNGs beside the models
(`hands_hero_<n>.png`, `weapon_<id>_<n>.png`) and have the GLBs reference them by
relative `uri` instead of carrying a `bufferView` copy. Godot resolves relative
image URIs on import and nothing on our side changes. If the exporter cannot,
then at least: the `_world` twin references the `_vm`'s images, and the normal
map ships at 1024².

### B. A world model that is not the viewmodel

`weapon_<id>_world.glb` is what other players' avatars and the armoury bench
load, seen at five to forty metres, and it is the 143k–198k triangle viewmodel
under another name (`weapon_sidearm_vm` alone is 337 mesh instances, ~230 of
them single-glyph rollmark meshes). **Ask:** merge the rollmark glyphs into one
mesh per side, or bake them into the normal map the file already ships; and
deliver the `_world` file decimated, or as a `_lod1` beside it, the way the
Toaster's trees have one.

---

## P1 — finish the hero hands

### C. Rifle and tool poses to the hero standard

The pistol pose now uses the hero hands (`sidearm-hero.js`); the rifle base and
the tool pose are still the capsule-bone rig, and `sidearm-hero.js` says so.
Swapping sidearm to rifle therefore swaps the player's arms from one style to
the other. **Ask:** `hands_<set>.glb` and `hands_<set>_tool.glb` rebuilt on the
hero hands, referencing the shared set from A.

### D. Off-hand poses measured on the other three platforms

The `magout` / `magin` / `charge` poses were measured against the Sidearm and
land there within 6 mm. The client offsets each pose so its own
`hands_mount_magazine` sits on the platform's feed point, which puts the
Sidearm-shaped hand on the rifle's magwell (12 cm away from where it was
authored), the Ember's canister (8 cm, and it pulls off rearward) and the
Scattergun's loading port (6 cm). It works; it is not what a hand does on
those guns. **Ask, when convenient:** a `magin` pose per feed geometry —
`_magin_rifle`, `_magin_canister`, `_magin_port` — falling back to the Sidearm
one, which is what the client does today.

---

## P2 — the Toaster's last piece

### E. The enemy lane — `toaster_path_ground.glb`

Every map's enemy lane is dressed with `<map>_path_ground`; the commission
never asked for the Toaster's, so the client lays the gravel road module along
the lanes instead. It reads as a farm track and it is nearly right. **Ask:** a
4 m module of worn track — two tyre ruts in trodden grass, no kerb, 3.4 m wide
— `run: "+X", repeat: 4.0, width: 3.4`, instanced. Where the lane runs down the
county road it is drawn over the asphalt and should be a metre narrower than
the carriageway, as this is.

### F. Three notes, not asks

- The pond model is 34 m across; the commission said 44. Design's level file
  explains why (three routes pass within 22 m of the centre), the client's
  collider ring is 17 m, and the map validates at zero. The commission was
  wrong, not the model.
- `toaster_skybox` shows a vertical seam in the north face of the bake — a
  lighter strip from the horizon to the zenith, visible from the spawn yard
  (`--shot toaster <png> yard`). The other two domes do not; worth a look at
  the cube-to-equirect step for this sky's cloud deck.
- `docs/ASSET-DELIVERY.md` says `weapon_scattergun_vm` carries a hidden
  `scattergun_shell` template; it does not (128 nodes, none by that name). The
  client uses the standalone `weapon_scattergun_shell.glb` for the reload, which
  is the file to keep.

---

## Already right — please do not change

- The vehicle rig contract, all four files: seats, wheels at their axles,
  steer nodes, `userData.vehicle`, −Z forward. The vehicle probe drives every
  one of them.
- The Toaster's instanced set. Terrain 392 tris in one part; trees two parts
  each with a 4-triangle LOD1; every budget met with room. The map draws its
  ground in 40 multimeshes and its 709 trees in 168.
- `shared_warp_gate_active` and the `warp_membrane` contract: the client reads
  `extras.pulse` and pulses the emissive exactly as the file asks.
- The building shells: door openings at the offsets the client builds its
  colliders to, on all four, first time.
- The reload parts. Pivots on the seated face, `extras.drop`, `extras.recoil`,
  `hands_mount_magazine` — the animation is 350 lines of code moving what you
  named, and no numbers of ours.

---

## How we will know it landed

| Check | Command |
|---|---|
| Names match the brief | `make assets` |
| Nothing delivered goes unused | `make usage` |
| Textures are referenced, not embedded | `du -sh game/assets/weapons` — under 60 MB is the number |
| Every platform still reloads | `--shot foundry <txt> reload` |
| The lane is a track, not a road | `--shot toaster <png> gate` |
