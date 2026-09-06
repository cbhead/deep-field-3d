# Deep Field 3D — Design Brief

For Claude Design, producing 3D models, icons, and visual design for this
codebase. This brief is the contract between design output and the code that
consumes it. The full game plan (milestones M0–M5) lives outside the repo;
everything referenced here is either shipped or specced there.

**Current state**: the game is fully playable graybox — code-built boxes and
capsules stand in for every asset named below. Every placeholder has a named
construction site in `game/scripts/GameRoot.cs`, so integration is mechanical:
a delivered asset replaces one code-built stand-in.

---

## 1. Art direction

- **Stylized flat-shaded kitbash.** Bold readable forms, minimal texture
  detail, strong silhouettes. Think low-poly-plus, not realism.
- **Design owns the palette.** Color, materials, and visual identity are
  Claude Design's decisions — designs already in flight define the language.
  Deliver a short palette spec alongside the first batch (faction accents,
  scrap types, status effects, danger/success semantics) and the code adopts
  it: the runtime tint table in `GameRoot.TintEnemy` and all UI theme colors
  will be updated to match design, not the other way around.
- **One engine constraint on materials** (mechanism, not palette): the engine
  applies status/hp/elite state at runtime by modulating albedo and toggling
  emissive. Deliver albedo that tolerates modulation — no baked lighting, and
  state indication should come from the palette spec rather than being painted
  into base textures.
- **The hard rule: no mechanic ships without a readable silhouette.** If two
  enemies behave differently they must be distinguishable at 40 m in motion.
  Every upgrade breakpoint (L4/L7/L10) must read at 30 m.
- **Procedural animation.** Models arrive static or with simple pivot
  hierarchies (turret yaw pivot, wing flap pivot); motion is added in code.
  No skeletal rigs required in v1.

## 2. Integration contract

- **Format**: glTF binary (`.glb`) into `game/assets/<category>/`
  (`enemies/`, `towers/`, `traps/`, `weapons/`, `heroes/`, `maps/foundry/`,
  `maps/switchyard/`, `shared/`, `vfx/`); icons as SVG or 256 px PNG into
  `game/assets/ui/`.
- **Scale**: 1 unit = 1 meter, matching the sim exactly. Sizes given below
  are gameplay-load-bearing (collision + readability), not suggestions.
- **Orientation**: Y-up, **-Z forward** (Godot convention). Enemies face -Z.
- **Pivots**: ground-center for placeables (towers, traps, props); spine base
  (feet) for enemies and heroes; grip for weapon viewmodels.
- **Staged modules**: tower upgrade paths use ten visual stages per path,
  named `_s1` through `_s10`. Stages 1–3, 5–6, and 8–9 grow the same module
  incrementally; stages 4, 7, and 10 are silhouette jumps (they carry
  mechanics). Code composes chassis + the current stage of each path, so any
  level combination renders without bespoke meshes.
- **Replacement sites** in `game/scripts/GameRoot.cs`: `SpawnEnemyView`,
  `SpawnTowerView`, `SpawnFlatView` (traps), `SpawnBarricadeView`,
  `SpawnProjectileView`, `BuildLevel` / `BuildFoundryStructures` /
  `BuildSwitchyardStructures` (all graybox geometry), `UpdateAvatarView`
  (hero capsules), and the HUD/lobby in `BuildHud` / `BuildLobby`. Sim-side
  dimensions come from `sim/Sim.Core/Content/*.cs` (source of truth for
  ranges, radii, speeds).

## 3. Named asset manifest — shipped content

Every deliverable is called out by its exact title below.

### 3.1 Enemies (`game/assets/enemies/`)
| File | Size / notes |
|---|---|
| `enemy_drifter.glb` | 1.6 m — the baseline grunt, reference silhouette |
| `enemy_mote.glb` | 0.9 m — fast swarm chaff, instantly "the small one" |
| `enemy_monolith.glb` | ~3.8 m — a walking wall; it literally blocks tower sightlines |
| `enemy_skiff.glb` | ~1.5 m wingspan — flyer; must read airborne at distance |
| `enemy_aegis.glb` | 2.2 m — armored 140° front plate, exposed glowing rear weak point |
| `enemy_warden.glb` | 1.8 m — body only |
| `enemy_warden_shield.glb` | separate bubble mesh (pops/regrows independently) |
| `enemy_mole_surfaced.glb` | 1.4 m — surfaced state |
| `enemy_mole_burrowed.glb` | dirt mound + tremor-trail element |
| `enemy_cluster.glb` | 1.8 m — visibly gravid with its five Motes |

### 3.2 Towers (`game/assets/towers/`) — chassis + staged path modules
Chassis (one each):
`tower_lance_chassis.glb` · `tower_nova_chassis.glb` ·
`tower_arc_chassis.glb` · `tower_singularity_chassis.glb` ·
`tower_skywatch_chassis.glb` · `tower_barricade.glb` (4.5 × 2.2 m wall;
plus `tower_barricade_damaged.glb` and `tower_barricade_broken.glb` for M4)

Path module sets (each is ten files, `_s1` … `_s10`):
- `tower_lance_damage_s1…10.glb` (barrel mass/rails) · `tower_lance_range_s1…10.glb` (optics/sensor mast) · `tower_lance_rate_s1…10.glb` (feed + heat sinks)
- `tower_nova_damage_s1…10.glb` (bore caliber) · `tower_nova_range_s1…10.glb` (blast amplifier ring) · `tower_nova_rate_s1…10.glb` (autoloader drums)
- `tower_arc_damage_s1…10.glb` (coil stack) · `tower_arc_range_s1…10.glb` (antenna array) · `tower_arc_rate_s1…10.glb` (capacitor bank)
- `tower_singularity_range_s1…10.glb` (field ring diameter) · `tower_singularity_rate_s1…10.glb` (stabilizer fins)
- `tower_skywatch_damage_s1…10.glb` (flak caliber) · `tower_skywatch_range_s1…10.glb` (mast height) · `tower_skywatch_rate_s1…10.glb` (radar dishes)

Projectiles & fire effects:
`proj_lance_bolt.glb` · `proj_nova_shell.glb` · `proj_arc_beam.glb`
(source→target→hop segments) · `proj_skywatch_bolt.glb`, with tier variants
`proj_<tower>_bolt_t2.glb` / `_t3.glb` (L7/L10 escalation) and
`vfx_muzzle_<tower>.glb` + `vfx_impact_<tower>.glb` per firing tower.

### 3.3 Traps (`game/assets/traps/`) — states are gameplay information
`trap_spike_armed.glb` · `trap_spike_triggered.glb` · `trap_spike_spent.glb`
`trap_tar_full.glb` · `trap_tar_depleted.glb`
`trap_launcher_charged.glb` · `trap_launcher_fired.glb` · `trap_launcher_rearming.glb`
(all sized to the 1.5 m trap-socket plate)

### 3.4 Weapons & gunsmith (`game/assets/weapons/`)
Platforms — each needs a first-person viewmodel and a world model:
`weapon_sidearm_vm.glb` / `weapon_sidearm_world.glb`
`weapon_rifle_vm.glb` / `weapon_rifle_world.glb`
`weapon_scattergun_vm.glb` / `weapon_scattergun_world.glb`
`weapon_emberpistol_vm.glb` / `weapon_emberpistol_world.glb`
`weapon_wrench_vm.glb` (starter melee / repair tool)
`hands_firstperson.glb` (base arms; faction variants in §3.6)

Attachments — visible modules mounted on viewmodels (the gun you built must
be the gun you see):
`attach_longbarrel.glb` · `attach_shortbarrel.glb` · `attach_compensator.glb`
`attach_rangefinder.glb` · `attach_drumfeed.glb` · `attach_bracestock.glb`
`attach_stabilizer.glb` · `attach_embercoil.glb` · `attach_cryocell.glb`
`attach_voltcap.glb` (infusions carry emissive elements per the palette spec)

Ammo — magazine/round visual + distinct tracer/impact per type:
`ammo_standard.glb` · `ammo_ap.glb` · `ammo_hollowpoint.glb`
`ammo_incendiary.glb` · `ammo_cryo.glb`
`vfx_tracer_standard.glb` · `vfx_tracer_ap.glb` · `vfx_tracer_hollowpoint.glb`
`vfx_tracer_incendiary.glb` · `vfx_tracer_cryo.glb`

### 3.5 Heroes (`game/assets/heroes/`)
`hero_forge.glb` · `hero_ember.glb` · `hero_tempest.glb`
(distinct at 40 m; include downed pose `hero_<faction>_downed.glb` and
revive-crouch `hero_<faction>_revive.glb`)
`hands_forge.glb` · `hands_ember.glb` · `hands_tempest.glb` (first-person arms)
`ui_nameplate.glb` (world-space name-tag plate)

### 3.6 Maps — full environment kits
Coordinates in `sim/Sim.Core/Content/Maps.cs`; the graybox builders in
`GameRoot.cs` give exact platform/ladder/zipline positions. Dressing must
never block socket→route sightlines (the harness's coverage math is truth).

**Foundry** (`game/assets/maps/foundry/`, industrial works theme):
`foundry_terrain.glb` · `foundry_path_ground.glb` (roadway surface kit) ·
`foundry_deck.glb` · `foundry_deck_rail.glb` · `foundry_pillar.glb` ·
`foundry_vent_tunnel.glb` · `foundry_wall_boundary.glb` ·
`foundry_skybox.(hdr|glb)` · dressing set: `foundry_dress_crucible.glb`,
`foundry_dress_pipes.glb`, `foundry_dress_gantry.glb`,
`foundry_dress_lightrig.glb`, `foundry_dress_steamvent.glb`

**Switchyard** (`game/assets/maps/switchyard/`, rail-yard theme):
`switchyard_terrain.glb` (three tiers) · `switchyard_cut_channel.glb`
(the freight cut) · `switchyard_middeck.glb` · `switchyard_catwalk.glb` ·
`switchyard_retainingwall.glb` · `switchyard_skybox.(hdr|glb)` · dressing
set: `switchyard_dress_railcar.glb`, `switchyard_dress_container.glb`,
`switchyard_dress_signaltower.glb`, `switchyard_dress_buffer.glb`

**Shared traversal & interactive kit** (`game/assets/shared/`):
`shared_ladder.glb` · `shared_zipline_anchor.glb` · `shared_zipline_cable.glb`
· `shared_zipline_trolley.glb` · `shared_launcher_idle.glb` ·
`shared_launcher_charging.glb` · `shared_launcher_fired.glb` ·
`shared_vent_grate.glb` · `shared_controlpoint_neutral.glb` ·
`shared_controlpoint_capturing.glb` · `shared_controlpoint_held.glb` ·
`shared_armory_kiosk.glb` · `shared_core.glb` · `shared_core_hit.glb` ·
`shared_spawn_portal.glb` · `shared_airlane_pylon.glb` (the visible air-lane
strand) · socket markers: `socket_ground_empty.glb`, `socket_ground_base.glb`,
`socket_wall_empty.glb`, `socket_wall_base.glb`, `socket_trap_empty.glb`,
`socket_barricade_empty.glb`

### 3.7 VFX (`game/assets/vfx/`)
`vfx_reaction_thermalshock.glb` · `vfx_reaction_flashfreeze.glb` ·
`vfx_status_burn.glb` · `vfx_status_chill.glb` · `vfx_status_shock.glb` ·
`vfx_status_shred.glb` · `vfx_status_mark.glb` · `vfx_shield_pop.glb` ·
`vfx_shield_regen.glb` · `vfx_burrow_spray.glb` · `vfx_cluster_split.glb` ·
`vfx_core_breach.glb` · `vfx_revive_beam.glb` ·
`vfx_ability_overdrive.glb` · `vfx_ability_ignitionwave.glb` ·
`vfx_ability_chainsurge.glb` · `vfx_tower_place.glb` · `vfx_tower_sell.glb` ·
`vfx_tower_upgrade.glb` · `vfx_wave_start.glb` · `vfx_wave_clear.glb`

### 3.8 Icons (`game/assets/ui/`) — blocking for the UI milestone
Towers/traps: `icon_tower_lance` · `icon_tower_nova` · `icon_tower_arc` ·
`icon_tower_singularity` · `icon_tower_skywatch` · `icon_tower_barricade` ·
`icon_trap_spike` · `icon_trap_tar` · `icon_trap_launcher`
Enemies: `icon_enemy_drifter` · `icon_enemy_mote` · `icon_enemy_monolith` ·
`icon_enemy_skiff` · `icon_enemy_aegis` · `icon_enemy_warden` ·
`icon_enemy_mole` · `icon_enemy_cluster`
Statuses/reactions: `icon_status_burn` · `icon_status_chill` ·
`icon_status_mark` · `icon_status_shock` · `icon_status_freeze` ·
`icon_status_shred` · `icon_reaction_thermalshock` · `icon_reaction_flashfreeze`
Scrap: `icon_scrap_alloy` · `icon_scrap_flux` · `icon_scrap_plating` ·
`icon_scrap_gravium`
Weapons/ammo/attachments: `icon_weapon_sidearm` · `icon_weapon_rifle` ·
`icon_weapon_scattergun` · `icon_weapon_emberpistol` · `icon_weapon_wrench` ·
`icon_ammo_standard` · `icon_ammo_ap` · `icon_ammo_hollowpoint` ·
`icon_ammo_incendiary` · `icon_ammo_cryo` · one `icon_attach_<name>` per
attachment in §3.4
Factions/abilities: `icon_faction_forge` · `icon_faction_ember` ·
`icon_faction_tempest` · `icon_ability_overdrive` ·
`icon_ability_ignitionwave` · `icon_ability_chainsurge`
Upgrade paths: `icon_path_damage` · `icon_path_range` · `icon_path_rate`

## 4. Forward manifest — design ahead of M3–M5

Same conventions; titles fixed now so nothing renames later.

- **Enemies**: `enemy_shade.glb` (+ `enemy_shade_shimmer.glb` hero-visible
  rendering) · `enemy_mender.glb` · `enemy_broodmother.glb` ·
  `enemy_leaper.glb` (+ `_windup` / `_airborne` poses) · `enemy_ram.glb`
  (+ `_enraged`) · `enemy_carapace.glb` + `enemy_carapace_plate.glb`
  (detachable armor plates over glowing weak points — plates get shot OFF) ·
  `enemy_skater.glb` (+ `_phased`) · `enemy_nullifier.glb` (+ tether VFX
  `vfx_nullifier_tether.glb`) · `boss_frame01.glb` with per-phase variants
  `_p1/_p2/_p3` (arena scale)
- **Elite treatments** (composable overlays + tags): `elite_gilded` ·
  `elite_juggernaut` · `elite_voltaic` · `elite_swift` · `elite_umbral`,
  each as a material/attachment treatment plus `icon_elite_<name>`
- **Towers**: `tower_detector_chassis.glb` (+ path modules + reveal-pulse
  `vfx_detector_pulse.glb`) · `tower_filament_chassis.glb` (+ modules +
  `proj_filament_beam.glb` with visible heat build-up — the beam must read
  "ramping") · `tower_overclock_chassis.glb` (+ modules +
  `vfx_overclock_link.glb` lines to fed towers)
- **Weapons 5–15**: `weapon_poisonstream_*` · `weapon_cryosprayer_*` ·
  `weapon_flakcannon_*` · `weapon_lmg_*` · `weapon_chargesniper_*` ·
  `weapon_burstdmr_*` · `weapon_autoscattergun_*` ·
  `weapon_concussionmortar_*` · `weapon_acidstream_*` (each `_vm` +
  `_world` + icon); **melee**: `melee_blade_*` · `melee_maul_*` ·
  `melee_spear_*` · `melee_gauntlets_*` · `melee_chainblade_*` with visible
  module sets `meleemod_edge_*` / `_grip_*` / `_infusion_*` /
  `_counterweight_*` / `_chargecell_*`
- **Conditions** (environment treatment sets + banner icons):
  `cond_fog` · `cond_night` (lighting rig + `vfx_flashlight_cone.glb` +
  enemy eye-glow treatment) · `cond_storm` · `cond_heatwave` ·
  `cond_coldsnap` · `cond_tremor`, each with `icon_cond_<name>`
- **Map elements**: `shared_teleporter_pad.glb` (idle/charged/cooldown) ·
  `shared_elevator.glb` + `shared_elevator_shaft.glb` ·
  `shared_snipernest.glb` · `shared_floodgate.glb` + lane-wash VFX ·
  `shared_crusher.glb` · `shared_gate_operated.glb` ·
  `shared_wall_destructible.glb` (+ `_broken` + debris) ·
  `prop_barrel_explosive.glb` · `prop_container_droppable.glb` ·
  `shared_cache_hidden.glb` / `_opened.glb` · map-morph states (e.g.
  `switchyard_bridge_intact.glb` / `_collapsed.glb`)
- **Map 3 kit**: theme TBD with the user — budget a third full environment
- **Scrap pickups**: `pickup_alloy.glb` · `pickup_flux.glb` ·
  `pickup_plating.glb` · `pickup_gravium.glb` · `pickup_primecore.glb` (M4)
  — visually distinct per type so "what did that wave pay" reads at a glance

## 5. UI surfaces needing visual design

Layouts specced in the build plan; each needs visual treatment: build wheel
(radial, socket-tag filtered, cost + ghost preview + range sphere), upgrade
panel (3 paths × level pips × breakpoint recipes), in-match HUD (vitals /
economy / wave / loadout clusters; crosshair state set incl. shielded,
armored, and burrowed feedback; teammate strip; toast feed), world-space
overheads (hp + shield + status icons), interaction prompts, revive beacon,
armory/gunsmith screen (7-slot grid, stat-delta bars, blueprint recraft),
lobby + faction menu (level curves, variant slot) + sector select,
intermission panel (next-wave preview + condition banner), end-of-match
stats, pause/settings, how-to-play cards, connection/version/drop-in states,
downed/spectate treatment, endless HUD (wave counter + elite tags).

## 6. Branding & identity

Game logo/title treatment (working name "Deep Field 3D" — final name is the
user's call), app icon (mac `.icns`, windows `.ico`), main-menu background
scene, loading screen, release-page banner, typography system (HUD numerals
legible in motion at small sizes).

## 7. Priorities

1. Build-wheel + HUD icon set (§3.8 — unblocks the UI milestone)
2. Enemy models with status states (§3.1)
3. Tower chassis + path-module stacks + projectiles (§3.2)
4. Weapon viewmodels + attachment modules + ammo visuals (§3.4)
5. Foundry environment kit, then Switchyard (§3.6)
6. Traversal/interactive kit + heroes (§3.6, §3.5)
7. VFX + screens/branding (§3.7, §5, §6)
8. Forward manifest in milestone order (§4)

Ship icons alongside each batch, and the palette spec with the first batch —
code adopts design's colors from that point on.
