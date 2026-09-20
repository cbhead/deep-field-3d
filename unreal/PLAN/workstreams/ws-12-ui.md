---
ws: 12
slug: ui
title: UI (Common UI + MVVM)
state: active
owner: session-fae2d0c5
claimed_at: 2026-09-19T20:58:00Z
lease_expires: 2026-09-20T23:19:18Z
branch: ws/12-ui/tokens-screens
last_commit: 5fee3aa
editor_heavy: true
phase: P2-P5
size: XL
critical: true
blocked_on: 
---
# WS-12 — UI (Common UI + MVVM)

## Scope / DoD
**Scope.** 17 screens + teleport picker + world markers + coverage decal driver + boss bar + elite strip + ping UI + early-call vote + tiers; M_UI_Chamfer, styles from DA_UITokens, BP_ModelWell studio, icons registry, magenta-chip fallback.

**Definition of done.** Every screen reachable in L_Test_UI with a fake view model; no net branching (CI grep); settings persisted.

**Spec.** C12, A3 UI, C§6 (in `unreal/PLAN/PROGRAMME.md`). Size XL, phase P2-P5.

## Contracts I consume
- C1
- C8
- C15

## Contracts / interfaces I provide
- C12

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->
- 2026-09-19 · **C12 first implementation** (no RFC: the contract had no code yet). Decisions and the appended fields are written into `CONTRACTS/viewmodel.md` § "As implemented". Dependents: none yet (WS-13/WS-14 read messages, not view models).
- 2026-09-19 · **`contract-append` to DFCore**: new header `Source/DFCore/Public/Match/DFMatchTypes.h` — `EDFMatchPhase` (= `World.cs MatchPhase`) and `FDFWeaponBuild` (= `Gunsmith.cs WeaponBuild`). New file, nothing existing changed. Future users: whoever writes `ADFMatchState`/`ADFPlayerState` (WS-00/INT skeleton, WS-05 wave director) and WS-06 gunsmith — please use these rather than redefining them (UHT rejects a second `FDFWeaponBuild` anyway).

- 2026-09-19 · **New, WS-12-owned (no contract number):** `UDFUITokens` (the class of `DA_UITokens`) with `FillFromDesignSystem()` as the importer's single entry point, `DFTokens::*` names (`DFUITokenList.inl`), the screen/layer vocabulary (`Config/Tags/DF_UI.ini`, `DFUIScreenList.inl`), `UDFActivatableScreen`, `UDFUILayout`. Described in `Source/DFUI/README.md`. Dependents to notify when they are claimed: **WS-31** (`import_tokens.py` should call `FillFromDesignSystem` rather than re-parse the CSS), **WS-45** (styles read tokens by these names).

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->
- Label the first PR `contract-append` for `Source/DFCore/Public/Match/DFMatchTypes.h` (new file; ownership-check warns, as it should).
- ~~For WS-15: `test.sh` matched `Result={Passed|Failed}` but 5.8.2 logs `Result={Success|Fail}`.~~ Fixed by WS-15 in `0735b89` (verdict now comes from the JSON report); nothing owed.

## Open questions
- **C1 has no `DF.Vehicle.*` root** although C12 gives the vehicle view model a `DefTag`. Carried `DefId` (FName) beside it; WS-08 (or INT) to add the root — native (RFC) or `Config/Tags/DF_Vehicles.ini`.
- **Connection-state tags.** `DF.Message.ConnectionState` carries `FDFMsg_Tagged`; WS-11's in-flight `EDFConnectionState` (Offline/LoggingIn/LoggedIn/Hosting/Joining/Connected/Failed) has no tag twins in C1 yet. The connection screen needs one tag per state (e.g. `DF.Connection.State.*`) — WS-11 to say where they live; the view model just stores whatever tag arrives.
- **Who writes `ADFMatchState` / `ADFPlayerState`?** `Source/DFMatch/**` is WS-00's and only `ADFGameMode` exists. The replicated-state → view-model feed (the real one) is blocked on those classes; everything else in WS-12 (kit widgets, screens against the fake feed) is not.

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-19 · session-fae2d0c5 · **session start** — claimed WS-12 (the user redirected this session here after it withdrew from WS-01; see the correction in the WS-01 log). Worktree `/Volumes/Toshiba/Deepfield-Unreal/wt-ws12`, branch `ws/12-ui/viewmodels`. Read PROGRAMME §3/§5/§6, A3, C§6, C12/C1/C15/C14, digest 2026-09-19. No editor slot taken: this PR is code-only (`-nullrhi` tests); a slot will be needed from the first `WBP_`/`L_Test_UI` PR on.
- 2026-09-19 · session-fae2d0c5 · **session end** — PR 1 on `ws/12-ui/viewmodels` (`278e262`): the five C12 view models, `UDFViewModelSubsystem` ("DFMatch" in the MVVM global collection, ConnectionState from the bus), `FDFFakeMatchFeed`, DFCore append `Match/DFMatchTypes.h`. Verified on the Mac: `DeepFieldEditor Mac Development` builds; `DF.UI` 4/4 green with `-nullrhi` (`NoNetBranching` caught a comment of mine on its first run, which is the check working); `layering-check` OK; `ownership-check --ws 12` 0 violations (3 expected text warnings); `smoke-listen.sh` OK. No LFS locks taken, no editor slot taken. **Next:** PR 2 = `DA_UITokens` C++ type + `import_tokens.py` input contract with WS-45/WS-30, `UDFActivatableScreen`/layer stack (`UCommonActivatableWidget` bases, no assets); PR 3 (needs an editor slot) = `L_Test_UI` + `WBP_Kit_*` against the fake feed. Blocked piece: the replicated-state feed, until `ADFMatchState`/`ADFPlayerState` exist (see Open questions).
- 2026-09-19 · session-fae2d0c5 · **session start (2)** — lease renewed; rebased `ws/12-ui/viewmodels` onto `ca75e70` (PR #40 still open, no review yet); no contract hits since the last session. PR 2 on `ws/12-ui/tokens-screens`, stacked on PR #40: UI tokens + the CommonUI screen/layer stack, code-only (no editor slot).
- 2026-09-20 · session-fae2d0c5 · **session end (2)** — PR 2 on `ws/12-ui/tokens-screens`, stacked on PR #40 (both rebased onto `7f7b9b8`, after WS-15's harness and WS-01's importer landed): `UDFUITokens` + CSS parser + `DFTokens::*` names, the layer/screen tags, `UDFActivatableScreen`, `UDFUILayout`, `Source/DFUI/README.md`. Verified on the rebased stack: editor build OK; `test.sh DF.UI+DF.Content+DF.Unit` 16/16 (8 of them DF.UI); `pr-check.sh --ws 12 --smoke` OK (layering, ownership 0 violations, schemas, listen smoke). No editor slot, no LFS locks. Small thing seen in passing for WS-15: `smoke-listen.sh:34` prints a zsh "no matches found" for `smoke-client-*.log` on a clean `Saved/Logs` (harmless; the smoke still reports correctly). **Next:** PR 3 needs an editor slot — `WBP_Layout` (four stacks registered), `L_Test_UI`, `WBP_Kit_Panel/Button/Bar/Slot/Tag` reading `DA_UITokens`, a `UDFScreenSet` (screen tag -> class) and `DF.UI.EveryScreenReachable` against the fake feed. `DA_UITokens` itself needs an importer run: WS-31 is unclaimed, so PR 3 will create the asset through `FillFromDesignSystem` from an editor utility in `Content/DF/UI` unless WS-31 is claimed first.

