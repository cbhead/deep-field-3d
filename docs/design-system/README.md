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

- **Specter's `weakPoints` passive** is defined on the faction and does nothing.
  It is a client rendering feature — highlight an enemy's weak point, which
  today only the Aegis has — and no code draws it. Recorded here rather than
  quietly shipped, because the lobby offers the faction and states the passive.

- **Magazine / reserve counts** in the loadout cluster. The sim has no ammo
  pool; weapons fire on a cooldown. Waiting on the ammo-quantity system.
- **Per-tower kills / uptime / coverage** in the upgrade panel. The sim tracks
  damage dealt but not the rest.
- **Early-start scrap bonus** on the intermission panel. No such bonus exists.
Closed since: per-player damage / builds / revives now ride `GameView`, the
version-mismatch modal names both builds, and the intermission condition banner
is live — Night and Fog exist, the schedule is authored on the `MapDef`, and the
panel announces next wave's weather one wave ahead.

One asset gap it opened: `icon_cond_fog` / `icon_cond_night` are on
design's forward manifest but not delivered, so the banner draws a placeholder
chip. `make usage` reports them as requested-and-missing rather than the code
pretending they exist.

## Naming

Design's filenames are **all lower case**; sim content ids are camelCase
(`emberPistol`, `longBarrel`, `ignitionWave`). Both loaders lower-case an id
before lookup — without that those resolve to placeholder chips that look
deliberate. Keep filenames lower case and this stays a non-issue.
