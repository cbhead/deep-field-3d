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
