# DFUI — how the UI is put together (WS-12)

Three pieces, each testable without an asset. Contracts: C12 (`unreal/PLAN/CONTRACTS/viewmodel.md`), Appendix C§6.

## 1. View models — what a widget may read (`Public/ViewModels/`)
`UDFMatchViewModel` and its children are the only thing a widget reads; see C12 "As implemented".
Resolve it in a `WBP_` from the Global Viewmodel Collection by the name **`DFMatch`**, or in C++ with
`UDFActivatableScreen::GetMatch()`. Nothing in this module may ask for authority or net mode
(`DF.UI.NoNetBranching`).

## 2. Tokens — where every colour, size and duration comes from (`Public/Tokens/`)
`docs/design-system/{colors,spacing,typography,effects}.css` is the source. `UDFUITokens` (the class of
`DA_UITokens`) holds every `--name` with `var()` resolved, sorted by what the value is:

| CSS value | Table | Read with |
|---|---|---|
| `#RGB` `#RRGGBB` `#RRGGBBAA` `rgb()` `rgba()` | `Colors` (sRGB in, linear out) | `Color(DFTokens::SurfacePanel)` |
| `12px`, bare `0` | `Lengths` (px at 1080p) | `Length(DFTokens::Space6)` |
| `140ms` `1.6s` | `Durations` (seconds) | `Duration(DFTokens::DurFast)` |
| `cubic-bezier(a,b,c,d)` | `Easings` | `Ease(DFTokens::EaseOut, Alpha)` |
| `700` `1.25` `.14em` `50%` | `Numbers` | `Number(DFTokens::WeightBold)` |
| shadows, gradients, clip polygons, font stacks, composite type roles | `Raw` (verbatim) | built as materials / styles (C§6) |

- **Names in code** come from `DFUITokenList.inl` (`DFTokens::<PascalName>`), append-only.
  `DF.UI.Tokens.NamesMatchDesignSystem` parses the real CSS and fails if a listed name has no token of
  that kind **or** a token in the CSS has no line — so a design drop that renames or removes a token
  breaks a test, not a panel.
- **A missing token** is one logged error naming it, and magenta / 0 / linear — never a quiet default.
- **Importer (WS-31, `import_tokens.py`).** The whole job is: create or load `DA_UITokens`
  (`UDFUITokens`), call `FillFromDesignSystem(<repo>/docs/design-system, Errors)`, refuse to save if
  it returns false, save. Parsing rules live here so the editor import and the tests cannot disagree.
  A token redefined with a different value, an unknown or circular `var()`, or a malformed
  colour / easing is an error naming the token.

## 3. Screens — where a widget lives (`Public/Screens/`, `Config/Tags/DF_UI.ini`)
Four layers, bottom to top, one `UCommonActivatableWidgetStack` each in `WBP_Layout` (a `UDFUILayout`):

| Layer | Holds | Input |
|---|---|---|
| `DF.UI.Layer.Game` | HUD, crosshairs, overheads, prompts, revive, endless | none (`Default`) |
| `DF.UI.Layer.GameMenu` | wheel, upgrade, armory, blueprints, teleport picker | `Game` / `GameAndMenu` — play continues |
| `DF.UI.Layer.Menu` | lobby, sector, intermission, end of match, pause, how-to | `Menu` |
| `DF.UI.Layer.Modal` | connection | `Menu` |

- A screen is a `WBP_<Screen>` deriving **`UDFActivatableScreen`**; set its `ScreenTag`
  (`DF.UI.Screen.*`) and `InputMode` in class defaults. The tag fixes the layer
  (`DFUIScreenList.inl`), so callers say `Layout->PushScreen(Class)` and never pick one.
- Adding a screen: one line in `DFUIScreenList.inl` **and** one in `Config/Tags/DF_UI.ini`
  (`DF.UI.Screens.TagsResolve` compares the two).
- `WBP_Layout` registers its stacks with `RegisterLayer(Tag, Stack)` on construct; `HasAllLayers()`
  is what `L_Test_UI` asserts first.

## Tests
`unreal/Build/test.sh DF.UI` — `ViewModel.*`, `NoNetBranching`, `Tokens.NamesMatchDesignSystem`,
`Tokens.ParserAndFallbacks`, `Screens.TagsResolve`, `Screens.LayersAndInput`. All run with `-nullrhi`
and need no asset. Pushing real widgets through the stacks is `L_Test_UI`'s job.
