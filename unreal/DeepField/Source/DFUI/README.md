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

## 3. Screens and parts — where a widget lives (`Public/Screens/`, `Config/Tags/DF_UI.ini`)

A **layer** is one `UCommonActivatableWidgetContainerBase`, and such a container displays exactly one
of its widgets at a time — *"Only the widget at the top of the stack is displayed and activated. All
others are deactivated."* So **two screens on the same layer are a claim that they are mutually
exclusive.** Four layers, bottom to top, one stack each in `WBP_Layout` (a `UDFUILayout`):

| Layer | Holds | Input |
|---|---|---|
| `DF.UI.Layer.Game` | the HUD layout, alone | none (`Default`) |
| `DF.UI.Layer.GameMenu` | wheel, upgrade, armory, blueprints, teleport picker | `Game` / `GameAndMenu` — play continues |
| `DF.UI.Layer.Menu` | lobby, sector, intermission, end of match, pause, how-to | `Menu` |
| `DF.UI.Layer.Modal` | connection | `Menu` |

The always-on play chrome — crosshairs, overheads, prompts, the revive column, the endless strip —
is drawn **at the same time** as the HUD and as each other, so it cannot be screens sharing
`Layer.Game`: the first push would hide the HUD behind the crosshair. It is **parts**
(`DFUIPartList.inl`, `DF.UI.Part.*`): plain `UDFUIPart` widgets that are children of `WBP_HudLayout`
and are shown and hidden by it, never pushed to a layer. `DF.UI.Screens.LayersAndInput` asserts
`Layer.Game` holds exactly one screen, so putting the chrome back on the layer fails a test.

- A screen is a `WBP_<Screen>` deriving **`UDFActivatableScreen`**; set its `ScreenTag`
  (`DF.UI.Screen.*`) and `InputMode` in class defaults. The tag fixes the layer
  (`DFUIScreenList.inl`), so callers say `Layout->PushScreen(Class)` and never pick one.
  `PushScreen` refuses a screen that is already open and returns the live instance rather than
  burying it under an invisible copy; `FindOpenScreen` returns the displayed instance, never a
  buried one.
- A part is a `WBP_Part_<Name>` deriving **`UDFUIPart`** with its `PartTag` set. It has no
  activation, no input config and no focus — it reads the view model and draws.
- Adding either: one line in `DFUIScreenList.inl` / `DFUIPartList.inl` **and** one in
  `Config/Tags/DF_UI.ini` (`DF.UI.Screens.TagsResolve` compares the two, both directions).
- `WBP_Layout` registers its stacks with `RegisterLayer(Tag, Stack)` on construct; `HasAllLayers()`
  is what `L_Test_UI` asserts first.

## 4. The real feed and the first HUD (code only, no asset)
- **Feed.** `FDFMatchStateFeed` (`Public/ViewModels/`) copies `ADFMatchState`, its economy component, the
  seated `ADFPlayerState`s and their hero pawns into the view models; `UDFMatchFeedSubsystem` runs it every
  frame in Game and PIE worlds whose game state is an `ADFMatchState`, and resets the model when the world
  goes. Same code on host and client. C12 "As implemented" lists what it writes and what is still default.
- **On screen.** `UDFUIRootSubsystem` (a local-player subsystem, so never on a dedicated server or in a
  unit-test world) puts a `UDFUILayout` on the player's screen whenever the player gets a controller in a
  game world, and pushes `UDFHudScreen` onto `DF.UI.Layer.Game`. Not under `-nullrhi` and not during an
  automation run (`ShouldShowUI`), so the gate's PIE tests never grow a HUD. One layout per controller;
  map travel replaces it. `UDFUILayout` created from its own class (no `WBP_Layout` yet) builds the four
  stacks in code.
- **The HUD.** `UDFHudScreen` builds its widget tree in C++: wave / phase / countdown / enemies (top left),
  money and lives (top right), the local hero's hp (bottom left), the crosshair and prompt **parts**
  (`UDFCrosshairPart`, `UDFPromptPart`), a toast for `DF.Message.{Build,Upgrade,Sell}Rejected`, and a
  victory / defeat banner. Wording is `DFHudText` (pure, tested); the look is `FDFHudStyle`, which reads
  `DA_UITokens` when it exists, else parses `docs/design-system` in place, else uses its built-in copy of
  the same values (`DF.UI.Hud.StyleFromTokens` keeps that copy equal to the CSS). It never logs a missing
  token. `WBP_HudLayout` replaces it when WS-12's asset PR lands; the parts become `WBP_Part_*`.

## Tests
`unreal\deepfield test DF.UI` (`unreal/README.md` §5.1; `Build/test.sh` was the Mac wrapper and has no machine — ADR-0028) — `ViewModel.*`, `NoNetBranching`, `Tokens.NamesMatchDesignSystem`,
`Tokens.ParserAndFallbacks`, `Screens.TagsResolve`, `Screens.LayersAndInput`, `Feed.*` (a real
`ADFMatchState` in a test world), `Hud.Text`, `Hud.StyleFromTokens`, `Hud.NoHudInTestWorlds`. All run with
`-nullrhi` and need no asset. Pushing real widgets through the stacks is `L_Test_UI`'s job; the code HUD
is checked in the running game (`deepfield play`, or `-game` on L_Testlane).
