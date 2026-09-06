# Asset usage

**438 delivered · 254 consumed · 184 unused** — regenerate with `make usage`.

`asset-report.sh` answers *has design shipped it*. This answers the question that
rots silently: an asset can be delivered, imported, and never referenced by a
line of code. That is invisible from the outside, because the game looks
finished right up until you open the folder.

## How it is measured

Two sources, because neither alone is complete:

- `--asset-audit` walks the content tables and builds one view of every enemy,
  tower, upgrade stage, trap, projectile and hero, then requests every icon the
  UI derives from those tables.
- A real solo match on each map with `--dump-assets`, which is the only thing
  that builds a level and therefore the only thing that touches the map kit.

Both print `[asset-audit] requested <name>`, and the script diffs that against
the files on disk. **Requests are reported by the code, not scraped from
source** — icon ids are built by interpolation (`$"enemy_{defId}"`), so grepping
for string literals under-reports by more than half.

## What the unused 184 are

| Group | Count | Why |
|---|---|---|
| M3–M5 tower stage modules | 83 | Detector, Filament and Overclock do not exist as towers yet. Delivered early on purpose — art lead time is the schedule risk. |
| VFX | 34 | No VFX system. Status particles, reaction bursts, ability effects, muzzle flashes and impacts all wait on it. |
| Weapon viewmodels, attachments, ammo models, hands | 28 | No first-person weapon rendering. The armory shows icons; the models mount when viewmodels land. |
| `_s1` stage modules | 16 | **Correct and intentional.** Design's chassis *is* the level-1 state and `_s1` is an empty root, so sim level N asks for stage N+1 and `_s1` is never requested. |
| Projectile tier variants (`_t2`, `_t3`) | 6 | No tier escalation wiring; projectiles use the base model at every level. |
| Hero revive poses | 3 | Downed poses are wired; the revive-crouch pose is not. |
| Barricade damaged/broken | 2 | Structure HP arrives with Ram at M4. |
| Control point capturing/held | 2 | The control point is `sweepInert` — no sim hookup yet. |
| Launcher pad charging/fired | 2 | The pad has no charge state in the sim. |
| Remainder | 8 | Icons for content that exists only in the forward manifest, plus `ui_nameplate` (no world-space nameplates yet). |

Nothing in that list is unused *by mistake*. Everything that was, is now wired.

## What this evaluation found

Three real gaps, all fixed:

**Content ids are camelCase, design's filenames are not.** `emberPistol`,
`longBarrel`, `ignitionWave` and `chainSurge` resolved to
`weapon_emberPistol` / `attach_longBarrel` / `ability_ignitionWave`, none of
which exist — design ships `weapon_emberpistol`, `attach_longbarrel`,
`icon_ability_ignitionwave`. All of them silently fell back to placeholder
chips. This is the exact failure the seam was built to prevent, and it survived
because **the placeholder is designed to look deliberate**. Both loaders now
lower-case the id before lookup.

**Trap states were never asked for.** Design ships armed / triggered / spent for
every trap because charges and rearm are gameplay information, and the client
only ever requested the armed model. A spent plate that still looks armed is a
lie the player pays for. Trap art now follows `ChargesLeft`.

**`shared_core_hit` was never asked for.** A leak costs a life wherever it
happened; the core now flashes to its struck state for a beat on every leak,
on both the host and client paths.

## Keeping it honest

```sh
make usage          # counts
make usage-list     # counts plus every unused name
```

When a system lands that should consume a group above, the count moves and the
row leaves this table. If a number goes *up* without a delivery, something
stopped being referenced.
