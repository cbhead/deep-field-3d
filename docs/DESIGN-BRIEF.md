# Deep Field 3D — Design Brief

For Claude Design, producing 3D models, icons, and visual design for this
codebase. This brief is the contract between design output and the code that
consumes it. The full game plan (milestones M0–M5) lives outside the repo;
everything referenced here is either shipped or specced there.

**Current state**: the game is fully playable graybox — code-built boxes and
capsules stand in for every model listed below. Every placeholder has a named
construction site in `game/scripts/GameRoot.cs`, so integration is mechanical:
a delivered model replaces one code-built stand-in.

---

## 1. Art direction

- **Stylized flat-shaded kitbash.** Bold readable forms, minimal texture
  detail, strong silhouettes. Think low-poly-plus, not realism.
- **Shader tinting carries state.** Status effects (burn/chill/mark/…), hp
  falloff, and elite variants are applied by the engine as albedo modulation
  and emissive toggles (see `GameRoot.TintEnemy`). Deliver albedo that
  *tolerates modulation*: mid-value, low-saturation base colors; no baked
  lighting; state colors must not be painted into textures.
- **The hard rule: no mechanic ships without a readable silhouette.** If two
  enemies behave differently they must be distinguishable at 40 m in motion.
  Every upgrade breakpoint (L4/L7/L10) must read at 30 m.
- **Procedural animation.** Models arrive static or with simple pivot
  hierarchies (turret yaw pivot, wing flap pivot); motion is added in code.
  No skeletal rigs required in v1.

## 2. Integration contract

- **Format**: glTF binary (`.glb`) into `game/assets/<category>/`
  (`enemies/`, `towers/`, `traps/`, `weapons/`, `maps/foundry/`,
  `maps/switchyard/`, `shared/`, `vfx/`, `ui/` for icons as SVG or 256px PNG).
- **Scale**: 1 unit = 1 meter, matching the sim exactly. Sizes below are
  gameplay-load-bearing (collision + readability), not suggestions.
- **Orientation**: Y-up, **-Z forward** (Godot convention). Enemies face -Z.
- **Pivots**: ground-center for placeables (towers, traps, props); spine base
  (feet) for enemies and heroes; grip for weapon viewmodels.
- **Naming**: `enemy_drifter.glb`, `tower_lance_chassis.glb`,
  `tower_lance_damage_s4.glb` (path module, stage 4), `trap_spike_armed.glb`,
  `weapon_rifle_vm.glb`, `attach_longbarrel.glb`, `icon_tower_lance.png`.
- **Replacement sites** in `game/scripts/GameRoot.cs`: `SpawnEnemyView`
  (capsules per enemy scale), `SpawnTowerView` (box+barrel), `SpawnFlatView`
  (trap plates), `SpawnBarricadeView`, `SpawnProjectileView`,
  `BuildLevel`/`BuildFoundryStructures`/`BuildSwitchyardStructures` (all
  graybox geometry), `UpdateAvatarView` (hero capsules), and the HUD/lobby in
  `BuildHud`/`BuildLobby`. Sim-side dimensions come from
  `sim/Sim.Core/Content/*.cs` (the source of truth for ranges, radii, speeds).

## 3. Color system (source of truth)

| Domain | Colors |
|---|---|
| Factions | Forge (industrial orange/steel), Ember (flame red-orange), Tempest (electric violet-blue); reserve identities for Glacier (ice blue), Specter (spectral green), Warden (gold shield) |
| Scrap | Alloy (gunmetal), Flux (cyan energy), Plating (bronze), Gravium (deep purple), Prime Core (white-gold, M4) |
| Statuses | burn = orange-red flame, chill = pale ice blue, mark = yellow glint, shock = violet crackle, freeze = white-blue crystal, shred = spark orange (engine tints per `GameRoot.TintEnemy` — keep these hues) |
| Semantics | danger/breach = red, success/ready = green, unaffordable = desaturate + red edge, air layer = sky blue, ground layer = earth tone |

## 4. Asset manifest — shipped content (replaces live placeholders)

### Enemies (8) — at collision scale, with status-tint-safe albedo
| Enemy | Size | Must read as |
|---|---|---|
| Drifter | 1.6 m | the baseline grunt (reference silhouette) |
| Mote | 0.9 m | fast swarm chaff; instantly the "small one" |
| Monolith | ~3.8 m | a walking wall — it literally blocks tower sightlines |
| Skiff | ~1.5 m wingspan | flyer on the air lane; reads airborne at distance |
| Aegis | 2.2 m | armored 140° front plate + exposed glowing rear weak point — the flank-me enemy |
| Warden | 1.8 m | separate shield-bubble mesh (pops/regrows independently) |
| Mole | 1.4 m | two states: burrowed (dirt mound + tremor trail) and surfaced |
| Cluster | 1.8 m | visibly gravid with its five Motes |

### Towers (6) — chassis + path-module stacks
Each tower = one **base chassis** + one **module set per upgrade path**, 10
visual stages per path. Stages 1–3, 5–6, 8–9 grow the same module; stages
4, 7, 10 are silhouette jumps (they carry mechanics). Code composes chassis +
each path's current stage, so every level combination renders without bespoke
meshes. M2 uses stages 1–5; model the full 10 (endless mode opens 6–10).

| Tower | Chassis motif | damage path | range path | rate path |
|---|---|---|---|---|
| Lance | railgun pillar | barrel mass/rails | optics/sensor mast | feed + heat sinks |
| Nova | squat mortar | bore caliber | blast amplifier ring | autoloader drums |
| Arc | tesla column | coil stack | antenna array (chain) | capacitor bank |
| Singularity | containment orb | — | field ring diameter | stabilizer fins (persistence) |
| Skywatch | AA turret | flak caliber | mast height | radar dishes (tracking) |
| Barricade | plated wall (4.5×2.2 m) | bulk plating layers | — | — (+ damaged states for M4 Ram) |

**Projectiles per tower** (+ L4/L7/L10 escalation variants): Lance rail-bolt +
tracer; Nova shell with arc trail + ground burst; Arc beam/arc segments
(source→target→hop); Skywatch flak bolt + proximity puff. Muzzle + impact
effects per tower.

### Traps (3) — on the 1.5 m trap-socket plate, states are information
Spike (armed blades / triggered / spent-recharging), Tar (full pool /
depleting film), Launcher (charged piston / fired / rearming).

### Weapons & gunsmith — every item is a model AND an icon
- **Platforms (4 shipped)**: Sidearm, Rifle, Scattergun, Ember Pistol —
  first-person viewmodel + hero hands, world model, third-person avatar prop.
- **Attachments (10)** as visible viewmodel modules: long/short barrel,
  compensator, rangefinder optic, drum feed, brace stock, stabilizer
  underbarrel, ember coil / cryo cell / volt cap (emissive matches status
  color). The gun you built must be the gun you see.
- **Ammo (5)**: magazine/round visuals + distinct tracer/impact per type
  (standard, AP, hollow-point, incendiary, cryo) + selector icons.
- **Melee**: starter wrench viewmodel (doubles as repair tool); slots
  reserved for Blade/Maul/Spear/Gauntlets/Chainblade (see §5).

### Heroes
Per-faction avatar models (Forge/Ember/Tempest) replacing capsules: distinct
at 40 m, faction accent colors, downed + revive-crouch poses, first-person
arms per faction, name-tag plate design.

### Maps — full environment kits (both maps replaced wholesale)
Coordinates in `sim/Sim.Core/Content/Maps.cs`; graybox builders in
`GameRoot.cs` give exact platform/ladder/zipline positions.
- **Foundry** (industrial works): terrain, path roadway surfaces (routes must
  read at a glance), upper deck + rails + pillars, vent tunnel, boundary
  walls, skybox, dressing (crucibles, pipes, gantries, steam) — dressing must
  never block socket→route sightlines (the harness's coverage math is truth).
- **Switchyard** (rail yard): three-tier terrain, freight-cut channel, mid
  deck + upper catwalk, switchback retaining walls, skybox, dressing (rail
  cars, containers, signal towers).
- **Shared traversal/interactive kit**: ladder, zipline (anchor + cable +
  trolley), launcher pad (idle/charging/fired), vent grates, control point
  pad (neutral/capturing/held), armory kiosk, core structure (healthy/hit),
  spawn portal, socket markers per tag (ground/wall/trap/barricade; empty vs
  occupied), **air-lane strand** (nav-light pylons/cable so the Skiff route
  reads without debug rendering).

### VFX
Reaction bursts (Thermal Shock shatter, Flash Freeze crystal), status
particles (burn/chill/shock/shred/mark), shield pop + regen shimmer, burrow
dirt spray, Cluster split burst, leak/core-hit flash, revive beam, faction
abilities (Overdrive tower surge, Ignition Wave cone, Chain Surge arc web),
tower place/sell/upgrade poofs, wave start/clear flourishes.

## 5. Forward manifest — design ahead of M3–M5

- **Enemies 9–16 + boss**: Shade (stealth shimmer: hero-visible vs
  tower-invisible renderings), Mender (heal-beam emitter, retreat posture),
  Broodmother (emit ports, gravid death-burst), Leaper (wind-up/airborne/
  landing — it gets shot mid-jump), Ram (armored head, exposed rear engine,
  enrage, structure-attack pose), Carapace (detachable armor plates over
  glowing weak points — plates are shot OFF), Skater (phase-blink states),
  Nullifier (suppression aura + visible tethers to affected towers), first
  boss frame (per-phase silhouette changes, arena scale).
- **Elite modifiers** (composable onto any enemy): Gilded, Juggernaut,
  Voltaic, Swift, Umbral — shader/attachment treatments + overhead tag icons.
- **Towers 7–10**: Detector (radar/lens, reveal pulse), Filament (ramp beam
  with heat-glow — the beam must read "ramping"), Overclock (buff pylon +
  link lines to fed towers) — same chassis + module-stack treatment.
- **Weapon platforms 5–15**: poison stream, cryo sprayer, flak cannon,
  LMG/minigun, charge sniper, burst DMR, auto-scattergun, concussion mortar,
  acid stream. **Melee set** with visible edge/grip/infusion/counterweight/
  charge-cell modules + charged-heavy VFX.
- **Conditions** (environment treatments + HUD banner icons): Fog, Night
  (lighting rig + flashlight cone + enemy eye-glow), Storm, Heatwave,
  Coldsnap, Tremor.
- **Map elements**: teleporter pad pair, elevator + shaft, sniper nest,
  floodgate lever + lane wash, crusher piston, operated gate, destructible
  wall (intact/broken + debris), explosive barrel, droppable container,
  secret cache, map-morph states (collapsing bridge).
- **Map 3 kit** (theme TBD with the user) — budget a third full environment.
- **Scrap pickups**: per-type drop visuals color-coded (the "what did that
  wave pay" readability), team vs personal share feedback; Prime Core at M4.

## 6. UI surfaces needing visual design

Layouts specced in the build plan; each needs visual treatment:
build wheel (radial, socket-tag filtered, cost + ghost preview + range
sphere), upgrade panel (3 paths × level pips × breakpoint recipes), in-match
HUD (vitals / economy / wave / loadout clusters, crosshair state set incl.
shielded/armored/burrowed feedback, teammate strip, toast feed), world-space
overheads (hp + shield + status icons), interaction prompts, revive beacon,
armory/gunsmith screen (7-slot grid, stat-delta bars, blueprint recraft),
lobby + faction menu (level curves, variant slot) + sector select, intermission
panel (next-wave preview + condition banner), end-of-match stats, pause/
settings, how-to-play cards, connection/version/drop-in states, downed/
spectate treatment, endless HUD (wave counter + elite tags).

**Iconography set (blocking for UI work)**: every tower, trap, enemy, status,
reaction, scrap type, ammo type, attachment, weapon, ability, condition, and
elite modifier. An asset without its icon can't enter the build wheel or
armory.

## 7. Branding & identity

Game logo/title treatment (working name "Deep Field 3D" — final name is the
user's call), app icon (mac `.icns`, windows `.ico`), main-menu background
scene, loading screen, release-page banner, typography system (HUD numerals
legible in motion at small sizes).

## 8. Priorities

1. Build-wheel + HUD icon set (unblocks the UI milestone)
2. Enemy models with status states
3. Tower chassis + path-module stacks + projectiles
4. Weapon viewmodels + attachment modules + ammo visuals
5. Foundry environment kit, then Switchyard
6. Traversal/interactive kit + heroes
7. VFX + screens/branding
8. Forward manifest in milestone order (M3 roster → conditions → M4 elements → M5)

Ship icons alongside each batch.
