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

- **Magazine / reserve counts** in the loadout cluster. The sim has no ammo
  pool; weapons fire on a cooldown. Waiting on the ammo-quantity system.
- **Per-tower kills / uptime / coverage** in the upgrade panel. The sim tracks
  damage dealt but not the rest.
- **Per-player damage, builds, revives** on the end-of-match screen. Tracked in
  the sim, not yet carried on `GameView`.
