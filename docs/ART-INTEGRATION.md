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
30 bespoke meshes.

**Multi-state units are separate files.** `enemy_warden_shield` is its own node
so it can pop and regrow; `enemy_mole_burrowed` swaps in when the Mole is
under. Both toggle from sim state every frame.

**A `.tscn` beats a `.glb` of the same name.** If a model needs an import tweak
— collision shape, AnimationPlayer, a material override — wrap it in a scene
and drop that in instead. No code change either way.

## Icons

Same contract, different loader: `UiTheme.Icon(id)` resolves
`res://assets/ui/icon_<id>.png` and draws a labelled chip until it exists. Every
tower, trap, enemy, status, reaction, scrap type, ammo type, attachment,
weapon, ability and condition needs one — a model without its icon can't enter
the build wheel or the armory.
