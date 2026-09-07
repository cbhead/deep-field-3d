# Asset delivery — 2026-09-07

Regenerated from Claude Design (`Deep Field 3D - Asset Export.html`, source of truth `models/*.js`).
The zip unpacks onto the repo root: `game/assets/<folder>/` follows `AssetLibrary.Routes`,
`docs/` carries palette, forward manifest and the vendored design system. **Same 380 GLB names
as the previous drop — nothing renamed, nothing added, nothing removed** — so `make assets`
and `make usage` should report the same shape; only the geometry changed.

## What changed in this drop

**Weapons (`game/assets/weapons/`, 29 files)**
- Scattergun (`weapon_scattergun_vm/_world`): heat shield, bead and muzzle mount now live in the
  barrel group so a barrel swap takes them along; fixed lug under the barrel root; tube mag seats
  on the lug; pump pulled back so every barrel length clears it.
- `attach_longbarrel`: host-fitted. Bore-matched per platform (rifle 8.5 mm fluted + gas block,
  scattergun 11.5 mm smoothbore with bead, pistols 11 cm bronze extension). No longer a 42 cm
  rifle tube on a pistol. Flutes were 1 m capsules (a three.js `CapsuleGeometry` argument bug) —
  now correct-length cylinders. This also fixes the stock rifle barrel.
- `attach_drumfeed`: host-fitted tower (rifle mag well 24×52, scattergun loading port 22×50,
  pistol grip base 22×36), 45 mm drum on long guns / 36 mm under pistols, shell hulls in the
  window on the scattergun.
- Sidearm / Ember pistol: magazine and `_mount_magazine` re-centred under the grip (was ~15 mm
  forward), so both the stock mag and the drum hang straight.
- Mount node names are unchanged (`<weapon>_mount_<slot>`, `attach_mount_muzzle`), so the
  viewmodel code path in ART-INTEGRATION.md still applies when it lands.

**Skyboxes (`foundry_skybox`, `switchyard_skybox`)**
- The sky is now a shader (`models/space-sky.js`): black-body starfield, Milky Way, sun disc
  aligned to the key light, a gas giant (Foundry) / ice world (Switchyard) and a moon. glTF
  cannot carry a shader, so the export **bakes it to a 4096×2048 equirect PNG** and ships the
  dome as an unlit (`KHR_materials_unlit`) textured sphere. Godot imports this as an unshaded
  `StandardMaterial3D` — no code change; `MapKit.NoShadow` still applies.
- ⚠ The dome radius is **700 m** (was 400 m — `MapKit.cs` comment). Camera far plane must exceed
  that; Godot's default 4000 m is fine.
- ⚠ Each skybox GLB is ~8–12 MB because of the embedded PNG. If that matters for the release
  size, tell design and we bake at 2048×1024 (~3 MB).

**Design system (`docs/design-system/`)**
- Refreshed `ui-kit.css`, `ui-screens.js`, `ui-branding.js`, tokens.
- New: `ui-weapon-view.js` — the Armory screen renders the real gunsmith build in a WebGL canvas
  (`data-weapon` / `data-build`) instead of a silhouette. It imports `models/weapons.js`,
  `models/studio.js` and three via an import map; the vendored `ui-screens.html` shows the frame
  without it (the slot stays empty). Spec intent only — the game's `UiKit.cs` draws its own.

## Unused — delivered but never requested by code

Restated from `make usage` (184 of 438), by name, so it can be checked against this drop.
Nothing here is a design defect; every group waits on a system.

| Group | Files | Waits on |
|---|---|---|
| Detector / Filament / Overclock towers | `tower_detector_chassis`, `tower_detector_{analysis,field}_s1…10`, `tower_filament_chassis`, `tower_filament_{optics,peak,ramp}_s1…10`, `tower_overclock_chassis`, `tower_overclock_{efficiency,network,potency}_s1…10` (83) | M3–M5 tower defs |
| VFX | `vfx_ability_*` (3), `vfx_status_*` (5), `vfx_reaction_*` (2), `vfx_shield_{pop,regen}`, `vfx_impact_*` (4), `vfx_muzzle_*` (4), `vfx_tracer_*` (5), `vfx_tower_{place,sell,upgrade}`, `vfx_wave_{start,clear}`, `vfx_burrow_spray`, `vfx_cluster_split`, `vfx_core_breach`, `vfx_revive_beam` (34) | VFX system |
| Weapon viewmodels + attachments + ammo + hands | `weapon_*_vm` (5), `weapon_*_world` (4), `attach_*` (10), `ammo_*` (5), `hands_firstperson`, `hands_{ember,forge,tempest}` (28) | First-person weapon rendering |
| `_s1` stage modules | 16 | Intentional — chassis is L1, `_s1` is an empty root |
| Projectile tiers | `proj_{lance_bolt,nova_shell,skywatch_bolt}_t2/_t3` (6) | Tier escalation at L7/L10 |
| Hero revive poses | `hero_{ember,forge,tempest}_revive` (3) | Revive-crouch pose |
| Barricade states | `tower_barricade_{damaged,broken}` | Structure HP (M4) |
| Control point | `shared_controlpoint_{capturing,held}` | Control-point sim hookup |
| Launcher pad | `shared_launcher_{charging,fired}` | Pad charge state |
| Nameplate | `ui_nameplate` | World-space nameplates |
| Icons | forward-manifest-only icons (7) | Content that doesn't exist yet |

Also flagged while comparing against the code:

- `docs/asset-manifest.tsv` marks all `foundry_*`, `switchyard_*`, `shared_*`, `socket_*` and
  trap states `wired=no`, but ASSET-USAGE.md says the map kit is consumed by a played match and
  trap states now follow `ChargesLeft`. The `wired` column is stale for those rows —
  `tools/asset-report.sh --refresh` should be re-run or those rows flipped by hand.
- `shared_core_hit` is consumed (ASSET-USAGE) but `wired=no` in the tsv — same fix.

## Requested by code — not in this drop

Design has no source for these; they will keep drawing placeholders.

- `spire_floor`, `spire_facade`, `spire_roof`, `spire_fireescape`, `spire_path_ground`,
  `spire_skybox` — Sector 3 environment kit. Not modelled yet.
- `icon_cond_fog`, `icon_cond_night` — intermission condition banner icons.
- `hero_glacier`, `hero_specter` (+ `_downed`) — factions offered by the lobby; not modelled.

## Integration

1. Unzip over the repo root (`unzip -o deepfield-3d-assets.zip -d .`).
2. `godot --headless --path game --import` (the working form — see ART-INTEGRATION.md).
3. `./tools/prepare-icons.sh` — icons still use `currentColor`.
4. `make assets && make usage` — expect the same counts as before this drop.

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
