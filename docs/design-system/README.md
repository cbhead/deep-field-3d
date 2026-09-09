# Design system — vendored source

Claude Design's export, checked in so the implementation has a source of truth
that doesn't live in someone's Downloads folder.

| File | What it is |
|---|---|
| `colors.css` `typography.css` `spacing.css` `effects.css` `fonts.css` `base.css` | The tokens. Transcribed one-to-one into `game/scripts/ui/DesignTokens.cs`. |
| `ui-kit.css` | Component styles — panel, button, bar, slot, tag, pips, toast, table, spark. Mirrored in `game/scripts/ui/UiKit.cs`. |
| `ui-screens.js` | **All sixteen screens.** Each entry has an `id`, a `note` explaining the intent, and a `render()` producing a 1920×1080 frame. This is the spec to build against. |
| `ui-branding.js` | Logo, app icon and title-treatment usage. |
| `ui-screens.html` | The viewer. Open it from this directory to see the frames — it loads the files above by relative path. |

## Reading a screen spec

```sh
node -e "import('./ui-screens.js').then(m => console.log(m.SCREENS.map(s => s.id).join('\n')))"
```

…or just grep for `id: '<name>'` — the `note` field beside it states the design
intent in a sentence, which is usually the part worth implementing carefully.

## Direction of travel

**Design decides, code adopts.** If a colour, size or layout needs to change it
changes in the source here first, then gets carried into `DesignTokens.cs` /
`UiKit.cs`. The one thing code owns is what data actually exists — where a frame
shows a number the sim doesn't model, the implementation shows what is real and
the gap gets written down rather than faked.

Known gaps of that kind:

- **The Spire wears graybox.** Sector 3 is built and balanced, but its
  environment kit (`spire_floor`, `spire_facade`, `spire_roof`,
  `spire_fireescape`, plus the shared traversal set its lift, pads and nests
  need) is specced in the brief and not delivered. `make usage` reports them as
  requested-and-missing rather than the code pretending otherwise.

- **Specter's `weakPoints` passive** is defined on the faction and does nothing.
  It is a client rendering feature — highlight an enemy's weak point, which
  today only the Aegis has — and no code draws it. Recorded here rather than
  quietly shipped, because the lobby offers the faction and states the passive.

- **Magazine / reserve counts** in the loadout cluster. The sim has no ammo
  pool; weapons fire on a cooldown. Waiting on the ammo-quantity system.
- **Per-tower kills / uptime** in the upgrade panel. The sim tracks damage
  dealt but not the rest. Coverage is no longer on this list: the panel now
  prints reach in metres and the world draws the ring, which is the part of
  "coverage" a player can act on.

> The Switchyard asks below are written up in full, with dimensions, the map's
> geometry and the reasoning, in **[docs/FORWARD-MANIFEST-switchyard.md](../FORWARD-MANIFEST-switchyard.md)**.
> That document is the one to hand to design; these entries are the summary.

- **Switchyard's terrain tile ships one variant of four.** Design's
  `buildVariant` draws four: standing water and weed tufts (v0), a **switch
  stand** (v1), a cable trough (v2), dropped sleepers (v3). The exporter calls
  `build()`, which returns v0, so only v0 exists as a GLB. Two things follow.
  The ground repeats — one tile laid twenty-four times puts the same puddle in
  the same place on a lattice, mitigated for now by turning alternate tiles a
  half-turn. And the switch stand, the lever and target that work a turnout,
  has never been delivered at all: the code that hid it from the tiles was
  hiding a node that was never in the file. **Ask: export all four variants**
  (`switchyard_terrain_v1…v3`) so the yard's ground stops repeating and the
  turnouts can be marked with the prop design already drew. Until then a
  signal stands at the throat and at one turnout, which is delivered art doing
  the job approximately.

- **Switchyard's terrain tile bakes track into the ground.** The 20 m tile
  carries two "disused sidings" at its local z ±6, drawn as scenery. Tiled six
  by four that is eight full-width tracks across the map on a perfect grid,
  running through the lanes, the tower pads and the spawn, with no throat, no
  turnout and no buffer at either end — out-numbering and contradicting every
  metre of track laid deliberately. The tile's ballast bed is what it is for,
  so the sidings are hidden and the railway is laid by
  `BuildSwitchyardRailway`. If the sidings are wanted back, they need to be a
  separate optional piece rather than part of the ground, because ground
  repeats and track cannot. Alternate tiles are also turned a half-turn: one
  tile repeated twenty-four times puts its puddle and its weed tufts on a
  lattice.

- **The coverage ring is code's, not design's.** Design's Structure frame gives
  the upgrade paths pips, a price and a breakpoint recipe, and nothing on the
  deck. Holding U now also draws the tower's reach as a ground ring, a dome
  when it can shoot air, and a second brighter ring at where reach *would* be
  after the next Range level. A multiplier never answers the only question
  being asked at that moment — does this buy the corner I keep leaking from —
  and the same rings were already the build wheel's answer to it at placement
  time. If design wants different colours or a different read for the preview,
  the source here is the place to change it. It is drawn by
  `CoverageRings`, shared with the build ghost so preview and owned coverage
  cannot drift apart, and the numbers come from `TowerMath`, shared with the
  sim so the ring cannot disagree with what the tower shoots.
- **Early-start scrap bonus** on the intermission panel. No such bonus exists.
- **Platform prices are scrap, not credits.** The brief and design's armory
  frame price weapon platforms in money; they now cost personal scrap, because
  money is the shared team wallet and a gun is not a shared thing. Attachments
  and ammo already worked this way, so the whole personal ladder is one
  currency and the shared wallet is the towers'. Design's own card anticipated
  it — the buy state reads `Scrap N` — and the rail draws the have/need chips
  the recipe rows use. Melee platforms moved with them.
- **Switchyard wants three things from design that do not exist.** The yard is
  laid out as a railway now — a main line, a ladder off it, sidings closed with
  buffer stops — and the gaps show:
  - a **switch stand** as its own prop, so a turnout can be marked where it
    actually is. One is baked into a terrain-tile variant, and because the
    tiles are laid on a grid it put stands in open ballast with no points near
    them. They are hidden for now (`MapKit.HideNamed`), which loses the detail
    rather than misplacing it.
  - `switchyard_bridge_intact` and a **tunnel portal**. The brief names the
    bridge and design has not modelled it; both are kitbashed from deck bays,
    columns and retaining wall, which reads but is not the real thing.
  - the **lane module carries a rail down its centre**, and the routes it is
    laid along turn square corners. The rail is hidden on Switchyard's routes,
    so the lane is ballast, kerbs and markers — a haul road, which is what a
    walking route through a yard should be. A haul-road module without the
    rail would say that properly.
- **The Spire's four west ladders go nowhere.** They are embedded in the west
  facade: the wall occupies x −20.5…−19.5 and the west wing's plate starts at
  x −19, so the half-metre between them is narrower than the player, and every
  one of the four climbs stops against the underside of the floor it serves.
  Foundry and Switchyard had the same defect and are fixed; the Spire's needs a
  route that exists rather than a nudge, since the east side is already the
  fire escape's. Reproduce with
  `./play --headless --quit-after 6000 -- --shot spire /tmp/t.txt traversal`.
  The lift, the teleport pads and the fire escape still work, so no tier is cut
  off — the ladders are redundant as well as broken.
- **Melee mastery is still bought with money.** Every other personal upgrade is
  scrap; this one was not converted with the platforms and is the last thing in
  the gunsmith drawing on the shared wallet.
- **Elite tags on the endless HUD.** Endless exists (waves cycle, threat
  climbs, best wave is kept), but elite modifiers (`EliteModDef`: gilded,
  juggernaut, voltaic, swift, umbral) are not in the sim yet, so the HUD shows
  wave, threat and best without the elite strip.
Closed since: per-player damage / builds / revives now ride `GameView`, the
version-mismatch modal names both builds, and the intermission condition banner
is live — Night and Fog exist, the schedule is authored on the `MapDef`, and the
panel announces next wave's weather one wave ahead.

One asset gap it opened: `icon_cond_fog` / `icon_cond_night` are on
design's forward manifest but not delivered, so the banner draws a placeholder
chip. `make usage` reports them as requested-and-missing rather than the code
pretending they exist.

### Icons still missing after the 2026-09-07 drop

The drop closed seven of the M3 gaps — `icon_tower_detector`,
`icon_tower_filament`, `icon_tower_overclock`, `icon_faction_glacier`,
`icon_faction_specter`, `icon_ability_cryofield`, `icon_ability_revealpulse` —
so the build wheel and the lobby are whole. **Fourteen icons are still requested
and missing**, all M3 content that is not on design's forward manifest yet:

- enemies: `icon_enemy_shade`, `icon_enemy_mender`, `icon_enemy_ram`
  (intermission preview, overheads)
- upgrade paths: `icon_path_field`, `icon_path_analysis`, `icon_path_ramp`,
  `icon_path_peak`, `icon_path_optics` (the Detector's and Filament's panels)
- status: `icon_status_reveal`
- gunsmith: `icon_ammo_shock`, `icon_ammo_toxin`, `icon_attach_toxinfeed`,
  `icon_weapon_cryosprayer`, `icon_weapon_poisonstream`

List them with `./play --headless --quit-after 600 -- --asset-audit | grep MISSING`.

Five *models* are in the same position — requested by the armory and the
first-person view, not yet on design's forward manifest: `ammo_shock`,
`ammo_toxin`, `attach_toxinfeed`, `vfx_tracer_shock`, `vfx_tracer_toxin`. The
ammo rail and the slot card draw the icon until they land.

This class of gap was invisible until the build wheel became reviewable. The
audit printed what the game *asked for* and said nothing about what arrived,
and the fallback is a labelled chip that reads as a deliberate control rather
than as a hole — so the Detector and Filament sat on the wheel as blank squares
for a whole milestone with nobody in a position to notice.
`UiTheme.MissingIcons` records the fallbacks now and the audit prints them.

The same audit caught a name that had drifted the other way: the brief and the
drop name the cryo ammo icon `icon_ammo_cryo`, and the sim's M3 ammo rows were
`cryoRounds` / `shockRounds` / `toxinRounds` — so the armory asked for
`icon_ammo_cryorounds` and drew a chip beside a delivered icon. The rows are
now `cryo` / `shock` / `toxin`, matching `standard` / `ap` / `hollowpoint` /
`incendiary`; the brief is the contract, code adopts.

## Naming

Design's filenames are **all lower case**; sim content ids are camelCase
(`emberPistol`, `longBarrel`, `ignitionWave`). Both loaders lower-case an id
before lookup — without that those resolve to placeholder chips that look
deliberate. Keep filenames lower case and this stays a non-issue.
