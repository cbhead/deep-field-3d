# Art integration

How a model gets from Claude Design into the game. Short version: **copy the
file into `game/assets/<folder>/` and it renders.** There is no code step.

## The contract

Every 3D view in the game is requested by the exact name it carries in
[DESIGN-BRIEF.md](DESIGN-BRIEF.md). `game/scripts/AssetLibrary.cs` resolves that
name to `res://assets/<folder>/<name>.glb`; if the file isn't there, the
graybox in `game/scripts/Placeholders.cs` stands in. So:

- **Partial batches are fine.** A finished Drifter can ship while the Monolith
  is still a capsule.
- **Order doesn't matter.** Nothing has to land before anything else.
- **Wrong name = silent placeholder.** This is the one failure mode, and the
  checks below exist to catch it.

The folder is chosen by name prefix (`enemy_` → `enemies/`, `tower_` →
`structures/`, …); the table lives in `AssetLibrary.Routes` and is repeated in
[game/assets/README.md](../game/assets/README.md).

## What to build next

Always the open **forward manifest**. Each one is a request list for a specific
piece of work, written against something that already runs in the game, so
every dimension in it is measured off the built thing rather than proposed:

- **[FORWARD-MANIFEST-hero.md](FORWARD-MANIFEST-hero.md)** — what the
  2026-09-12 drop taught us: the hero texture set is embedded in every file
  that uses it, the world weapons are the viewmodels under another name, and
  two hand poses are still on the old rig.
- [FORWARD-MANIFEST-toaster.md](FORWARD-MANIFEST-toaster.md) — sector 4's
  whole kit, delivered 2026-09-12 and kept as the record of the ask. It carries the one rule the rest
  of this document does not: anything that repeats on that map is drawn as a
  single instanced mesh, so a file's **part count is its draw-call count** and
  its triangles are multiplied by every placement. That is why those entries
  carry budgets and the older briefs do not.
- [FORWARD-MANIFEST-switchyard.md](FORWARD-MANIFEST-switchyard.md) — the
  tiling-versus-punctuating split, and four pieces that map still grayboxes.
- [FORWARD-MANIFEST-reload.md](FORWARD-MANIFEST-reload.md) — weapons.

`DESIGN-BRIEF.md` §3 remains the standing name list; a forward manifest is what
is being asked for *now*, and anything in one should appear in
`docs/forward-manifest.json` on the next drop. `MAP-AUTHORING.md` is the rules a
map has to satisfy before any of its art is worth drawing.

## Textures the importer writes beside a model

A hero-standard GLB carries its texture set embedded, and Godot's importer
extracts each one as `<model>_<n>.png` next to the file before importing it.
Those PNGs are build output: every `--import` (CI runs one first) regenerates
them, so they are ignored by git (`game/assets/**/*_[0-9]*.png`) and never
committed. The 2026-09-12 drop would otherwise have added 447 of them at
365 MB. The brand icons in `game/assets/brand/` are real files and stay tracked.

## Where the models come from

Design does not hand-model GLBs. The Claude Design project is three.js code —
one module per tower, enemy roster, weapons, map kits, VFX — and an export page
that builds every named file from it. Both are vendored under
[docs/design/](design/) (the `models/` directory is the source of truth for
geometry; the `.html` pages are the viewers design works in), so a drop is
reproducible from the repo:

```sh
make design-export      # rebuild game/assets + docs from docs/design/
```

That opens the export page in your browser — it has to be a browser, because
the skyboxes are shaders baked to a texture on the way out — writes each file
straight into the repo, prepares the icons and re-imports for Godot. The host
is `tools/design-export/` (pinned three.js, a page that uploads instead of
zipping). When design sends a new project export, unpack it over `docs/design/`
and run the same command; `docs/ASSET-DELIVERY.md` is design's note on what
changed.

Because the geometry is code, a tweak is an edit to `docs/design/models/*.js` —
a Drifter's head is `box(.30, .20, .30)` at a named position, not a mesh you
drag. `ONLY=` rebuilds just the part you touched, so checking that edit costs
one file instead of the whole drop:

```sh
make design-export ONLY=lance     # one tower: chassis and its 30 stage modules
make design-export ONLY=drifter   # one model, by item id or by file stem
make design-export ONLY=rifle     # both the viewmodel and the world gun
make design-export ONLY=vfx       # a whole category — everything vfx.js builds
```

A filtered run merges its rows into `game/assets/structures/manifest.json`
rather than replacing it — dropping the rows for models it did not build would
take the rig limits `TowerRig` reads with them — and skips the palette, icon
and design-system bundle, which it never rewrites anyway. A selector that
matches nothing fails the run instead of quietly exporting zero files.

## Checking what's landed

```sh
make assets                 # delivered vs. named, plus filename typos
```

```
wired now : 12/40 delivered  (renders as soon as the file lands)
later     : 0/141 delivered  (waits on M3-M5 code)
```

**"wired now"** is the number that matters: those names are requested by the
running game today. **"later"** are named ahead in the brief for M3–M5 — the
model can be delivered early, but it won't appear until that milestone's code
exists (there is no Shade to render yet).

The report also lists anything on disk the manifest doesn't name, which is
almost always a filename typo — otherwise it looks identical to "not delivered
yet".

## Checking the models themselves

```sh
make model-validate         # every .glb against the rules below
```

The two checks above read file *names*. This one opens the models and checks
what the code consumes: the `_foot → _yaw → _pitch → _muzzle` chain
`TowerRig.Resolve` drives, the shared empties `GameRoot.MergeRig` relies on to
put a stage module's parts in the right place, the `<id>_mount_<slot>` nodes
`WeaponAssembly` hangs attachments from, an aura tower having no yaw to point
with, the manifest agreeing with what is on disk, and — the rule that paid for
the script — a stage whose manifest entry says it *adds* something having more
triangles than the stage below it.

It reads the glTF JSON directly, so it needs no Godot, no browser and no
packages, and the whole drop takes about two seconds. CI runs it in the fast
lane and `make check` runs it before the engine half.

Everything is a **ratchet** against `docs/model-validation-baseline.tsv`: a
violation that is *not* in the baseline fails the build, and one that disappears
is reported so the row can be dropped and the gain locked in. Most rows are the
twenty models that do not sit on their own origin today, several of them
correctly — a Leaper mid-jump is airborne, a broken Barricade sags, a wall
socket hangs off a wall. A row for one of the *contract* checks means a real
defect somebody chose to carry, so give it a reason and a way out; those are
restated on every run rather than passing quietly.

## Checking what the code asks for

```sh
./play --headless -- --asset-audit
```

Builds one view of every enemy, tower, trap, upgrade stage, projectile and hero
in the content tables and prints each name with the path it resolves to. CI
runs this and additionally verifies that every requested name appears in
`docs/asset-manifest.tsv` — so if code and brief ever drift apart, the build
fails instead of a delivered model quietly never loading.

The game also prints `[assets] N/M resolved` on exit.

## Things that are load-bearing

**Scale and pivot.** 1 unit = 1 metre, matching the sim. Placeables pivot at
ground centre; enemies and heroes at the spine base. Collision and overhead
bars are positioned from `Placeholders.EnemyScale`, not from the mesh, so a
model that disagrees with its stated size will look misaligned rather than
resize the hitbox.

**Materials must tolerate modulation.** Status and damage are expressed as a
blend and a darkening factor over whatever albedo the model ships with
(`TintableView.cs`) — a burning Drifter is design's Drifter, lit on fire. Bake
lighting or state into a texture and it will fight this. `ShaderMaterial` is
allowed and means that unit opts out: it owns its look, and status reads
through overhead icons and particles instead.

**Towers are composed, not monolithic.** A placed tower is
`tower_<def>_chassis` plus one `tower_<def>_<path>_s<N>` module per upgraded
path, restacked as levels are bought. Ten stages per path with silhouette jumps
at s4/s7/s10 — that is what makes 10 levels × 3 paths renderable without
30 bespoke meshes. Godot wraps each file's root node in an extra scene root
(`AuxScene → tower_lance_chassis → lance_foot…`), so the merge that moves a
module's parts onto the chassis starts one level down — compare the wrappers
and nothing ever matches.

**Turrets are rigged, and the rig is design's.** A chassis carries
`<id>_foot` (static) → `<id>_yaw` (turns about +Y) → `<id>_pitch` (elevates
about X, barrel-up is negative) → `<id>_muzzle` (where the round leaves).
Stage modules mirror the same empties so an upgrade's barrel parts swing with
the barrel. `TowerRig.cs` drives yaw and pitch towards the sim's target with
the limits and slew rates from `game/assets/structures/manifest.json` — the
Nova's 22° pitch floor is its 5 m minimum range in geometry, and a Lance at
90°/s is meant to lose a fast crosser. Aura towers (Singularity, Arc, Detector,
Overclock) have no rig on purpose and never point; their `_spin` group (and
the Skywatch's radar, tagged `extras.role = cosmeticSpin`) turns instead. A
graybox has none of these nodes and still turns as a whole. The muzzle node is
found but not yet used: sim projectiles start 1.5 m above the socket, and the
view follows the sim.

**The first-person view is design's assembly.** `weapon_<id>_vm.glb` has the
grip at the origin and the bore along −Z; `hands_<faction>.glb` is authored in
the same frame, so the two only share a parent under the camera
(`Player.RefreshViewModel`). Fitted modules mount on the platform's
`<id>_mount_<slot>` nodes exactly as on the armory bench (`WeaponAssembly`),
so the gun you built is the gun you hold. Hands ship in the rifle pose; the
export also emits `hands_<faction>_pistol` and `_tool` from the same source,
and the viewmodel picks the pose by platform. Muzzle flash, tracer and impact
come from `Vfx.cs` the instant the trigger is pulled — the sim decides
separately whether the shot hurt.

**Effects are meshes, not particles.** Design's `vfx_*` files are static hero
frames the client scales, turns and fades over a short life, with named
sub-groups (`_spin`, `_pulse`, `_rise`) it can drive without lookups. Towers
flash `vfx_muzzle_<tower>` at their rig's muzzle node and burst
`vfx_impact_<tower>` where a round stops existing; hero weapons use the Lance
flash and impact tinted to the ammo, and `vfx_tracer_<ammo>` stretched from
muzzle to hit. `TowerFired` is not relayed, so only the host sees tower
flashes; a teammate's shot is reconstructed from the damage it does.

**Multi-state units are separate files.** `enemy_warden_shield` is its own node
so it can pop and regrow; `enemy_mole_burrowed` swaps in when the Mole is
under. Both toggle from sim state every frame.

**A `.tscn` beats a `.glb` of the same name.** If a model needs an import tweak
— collision shape, AnimationPlayer, a material override — wrap it in a scene
and drop that in instead. No code change either way.

## Icons

Same contract, different loader: `UiTheme.Icon(id)` resolves
`res://assets/ui/icon_<id>.svg` (or `.png`) and draws a labelled chip until it
exists. Every tower, trap, enemy, status, reaction, scrap type, ammo type,
attachment, weapon, ability and condition needs one — a model without its icon
can't enter the build wheel or the armory.

**Run `./tools/prepare-icons.sh` after any icon delivery.** Design's icons are
stroke line-art using `currentColor`, which is correct — they are meant to take
the colour of whatever draws them. Godot's SVG rasteriser has no CSS context
and resolves `currentColor` to black, so every icon lands as a black silhouette
on a black panel. The script rewrites it to white; the UI then tints at draw
time, which is what `currentColor` was asking for.

## Gotchas found during the first integration

Recorded so the next delivery doesn't rediscover them:

- **`godot --headless --import game` silently does nothing** on 4.7. The
  working form is `godot --headless --path game --import`. Symptom: assets stay
  unimported and every lookup falls back to a placeholder.
- **The skybox is real geometry**, a 700 m dome (400 m in the first drop).
  With a shadow-casting sun inside it, it puts the entire map in shade.
  `MapKit.NoShadow` turns off shadow casting for it. Any camera that wants to
  see the sky needs a far plane past the dome: the player camera uses Godot's
  4000 m default, the review-shot cameras are set explicitly.
- **The skybox is a baked texture.** Design's sky is a shader, which glTF
  cannot carry, so the export bakes it to a 4096×2048 equirect PNG on an unlit
  (`KHR_materials_unlit`) sphere — hence the two ~9 MB files in `maps/`.
  Godot imports that as an unshaded material. One thing to do, and it is done
  in code: three.js drew the dome back-facing, which glTF cannot say, so the
  file is single-sided and Godot culls it from inside — `MapKit.SeenFromInside`
  turns culling off for the dome. Without it the procedural sky shows through
  and looks plausible enough that nothing fails.
- **Map kits are modular tiles at local origin**, authored at true world
  height — a deck segment's mesh already sits at y≈6. They mount at world
  y = 0 wherever the graybox collider's centre happens to be, which is what
  `MapKit.GroundLocal` computes.
