---
ws: 12
slug: ui
title: UI (Common UI + MVVM)
state: active
owner: session-fae2d0c5
claimed_at: 2026-09-25T02:11:40Z
lease_expires: 2026-09-26T02:11:40Z
branch: ws/12-ui/tokens-screens
last_commit: 042f7d7
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
- **`unreal/main` has one red test, and it is not WS-12's — it blocks every `int-merge` landing, including PR #44.** `DF.Func.Status.ThermalShockInLevel` fails on the trunk: the test's own logic passes (the log shows `chill + burn -> thermalShock ... 12.0 of 100 hp, health 88.0, ReactionTriggered seen`), but it runs its PIE session in `L_Dev_Empty`, and `DFWorldSubsystem.cpp:121` logs `Error: <level> has no lane graph` for any level without one. The automation framework turns an unexpected Error log into a failure, and `DFStatusLevelTests.cpp` declares no expected error for it. Two workstreams that each landed unbuilt (#48, #51) met here. Not caused by this branch: it differs from `origin/unreal/main` only in `Source/DFUI/**`, `Config/Tags/DF_UI.ini` and this file, and the failure involves only DFGameplay and DFWorld. **The one-line fix is WS-02's to make** (the test knowingly runs in a level with no lane graph, so it should declare that expected error) **or WS-09's** (a dev level without a lane graph is not an error). I have not touched either path.
- Label the first PR `contract-append` for `Source/DFCore/Public/Match/DFMatchTypes.h` (new file; ownership-check warns, as it should).
- ~~For WS-15: `test.sh` matched `Result={Passed|Failed}` but 5.8.2 logs `Result={Success|Fail}`.~~ Fixed by WS-15 in `0735b89` (verdict now comes from the JSON report); nothing owed.

## Open questions
- ~~C1 has no `DF.Vehicle.*` root.~~ Answered by **R10**: the four leaves are native C1 tags now. Follow-up (not in PR #44, which is code already reviewed): fill `UDFVehicleViewModel::DefTag` and retire the `DefId` workaround.
- ~~Connection-state tags.~~ Answered by **R10**: `DF.Online.Connection.*` exist in WS-11's `DF_Online.ini`. The connection modal maps one state to one body of copy; `UDFMatchViewModel::ConnectionState` already stores whatever tag arrives.
- ~~Who writes `ADFMatchState` / `ADFPlayerState`?~~ Answered by **R1 / ADR-0024**: **WS-28 Match flow**, whose shells have landed. Per-domain fields arrive as components, so the fake feed stays per field until each one lands.
- **Whose editor utility creates `DA_UITokens`?** `UDFUITokens::FillFromDesignSystem()` is the whole import, but the asset has to be created and saved by something. WS-31 owns `import_tokens.py` and is unclaimed. If it is still unclaimed when the first `WBP_` PR is written, WS-12 will create the asset from an editor utility under `Content/DF/UI` and hand the call site to WS-31 later.

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-19 · session-fae2d0c5 · **session start** — claimed WS-12 (the user redirected this session here after it withdrew from WS-01; see the correction in the WS-01 log). Worktree `/Volumes/Toshiba/Deepfield-Unreal/wt-ws12`, branch `ws/12-ui/viewmodels`. Read PROGRAMME §3/§5/§6, A3, C§6, C12/C1/C15/C14, digest 2026-09-19. No editor slot taken: this PR is code-only (`-nullrhi` tests); a slot will be needed from the first `WBP_`/`L_Test_UI` PR on.
- 2026-09-19 · session-fae2d0c5 · **session end** — PR 1 on `ws/12-ui/viewmodels` (`278e262`): the five C12 view models, `UDFViewModelSubsystem` ("DFMatch" in the MVVM global collection, ConnectionState from the bus), `FDFFakeMatchFeed`, DFCore append `Match/DFMatchTypes.h`. Verified on the Mac: `DeepFieldEditor Mac Development` builds; `DF.UI` 4/4 green with `-nullrhi` (`NoNetBranching` caught a comment of mine on its first run, which is the check working); `layering-check` OK; `ownership-check --ws 12` 0 violations (3 expected text warnings); `smoke-listen.sh` OK. No LFS locks taken, no editor slot taken. **Next:** PR 2 = `DA_UITokens` C++ type + `import_tokens.py` input contract with WS-45/WS-30, `UDFActivatableScreen`/layer stack (`UCommonActivatableWidget` bases, no assets); PR 3 (needs an editor slot) = `L_Test_UI` + `WBP_Kit_*` against the fake feed. Blocked piece: the replicated-state feed, until `ADFMatchState`/`ADFPlayerState` exist (see Open questions).
- 2026-09-19 · session-fae2d0c5 · **session start (2)** — lease renewed; rebased `ws/12-ui/viewmodels` onto `ca75e70` (PR #40 still open, no review yet); no contract hits since the last session. PR 2 on `ws/12-ui/tokens-screens`, stacked on PR #40: UI tokens + the CommonUI screen/layer stack, code-only (no editor slot).
- 2026-09-20 · session-fae2d0c5 · **session end (2)** — PR 2 on `ws/12-ui/tokens-screens`, stacked on PR #40 (both rebased onto `7f7b9b8`, after WS-15's harness and WS-01's importer landed): `UDFUITokens` + CSS parser + `DFTokens::*` names, the layer/screen tags, `UDFActivatableScreen`, `UDFUILayout`, `Source/DFUI/README.md`. Verified on the rebased stack: editor build OK; `test.sh DF.UI+DF.Content+DF.Unit` 16/16 (8 of them DF.UI); `pr-check.sh --ws 12 --smoke` OK (layering, ownership 0 violations, schemas, listen smoke). No editor slot, no LFS locks. Small thing seen in passing for WS-15: `smoke-listen.sh:34` prints a zsh "no matches found" for `smoke-client-*.log` on a clean `Saved/Logs` (harmless; the smoke still reports correctly). **Next:** PR 3 needs an editor slot — `WBP_Layout` (four stacks registered), `L_Test_UI`, `WBP_Kit_Panel/Button/Bar/Slot/Tag` reading `DA_UITokens`, a `UDFScreenSet` (screen tag -> class) and `DF.UI.EveryScreenReachable` against the fake feed. `DA_UITokens` itself needs an importer run: WS-31 is unclaimed, so PR 3 will create the asset through `FillFromDesignSystem` from an editor utility in `Content/DF/UI` unless WS-31 is claimed first.
- 2026-09-25 · INT · Rulings for WS-12: **R1 / ADR-0024** — `ADFMatchState` / `ADFPlayerState` belong to the new **WS-28 Match flow**; its first PR is the shells with WS-28's own fields (phase, wave, timer, threat, endless, seats, stats), and each domain's fields arrive as components (economy WS-06, lane states WS-09, faction/level WS-07, loadout WS-06, hero state WS-03) — keep the fake feed per field until its component lands. **R10** — the connection-state tags already exist (`DF.Online.Connection.*`, WS-11's `DF_Online.ini`); `DF.Vehicle.{Buggy,Dagator,Grnmchn,Vehickle}` are now native C1 tags, so `UDFVehicleViewModel::DefTag` can be filled and the `VehicleId` workaround retired. **R13** — any status countdown reads `TryGetTimeRemaining`; on false show the icon without the ring. **R4 / ADR-0026** — two new UI pieces: a "Share code" panel (lobby and pause menu, copy button, shows the rotated code after each admission) and a **non-modal** admit toast (joiner's name, hold a key to admit, never pauses play or takes input).
- 2026-09-25 · INT · The real feed's first half exists (not yet built): `ADFMatchState` replicates phase, wave index, total waves, lobby, endless, threat, lap, enemies remaining and `GetPhaseSecondsLeft()` (a server-time deadline, so no per-frame replication) and fires `OnMatchStateChanged` on host and clients; `ADFPlayerState` carries seat and the match stats and fires `OnPlayerStateChanged`. Bind `UDFMatchViewModel` / `UDFPlayerViewModel` to those for WS-28's fields; money, lives, scrap, faction etc. stay on the fake feed until their components land (ADR-0024). Lobby Launch and the early call are `ADFPlayerController::Server_Launch` / `Server_CallEarly`.
- 2026-09-25 · session-fae2d0c5 · **re-claim** — the same session that opened PR #44; the lease expired on the 20th and INT's sweep paused the workstream, so this re-claims it rather than continuing a dead lease. Read the cycle-3 digest, NEXT.md and INT's rulings above. Taking NEXT.md item 2: fix PR #44's three confirmed findings and land it. Still code-only, no editor slot.
- 2026-09-25 · session-fae2d0c5 · **PR #44 rebased and its three findings fixed.** The conflict was `ws-12-ui.md` only (INT's rulings and the stale-lease sweep against this branch's session-end entry); resolved as a dated union, and no source file changed in the rebase. The findings: (1) **the design error** — `Layer.Game` held six screens, but a layer is one activatable container and shows one widget at a time, so the HUD would have been hidden by the crosshair. Crosshairs, overheads, prompts, revive and endless are now **parts** (`DF.UI.Part.*`, `DFUIPartList.inl`, new `UDFUIPart` base) that live inside the HUD layout; `Layer.Game` holds the HUD alone and a test asserts it. (2) `PushScreen` now refuses a duplicate and returns the open instance. (3) `FindOpenScreen` searches from the top down. **Next:** the follow-up above (`DefTag` from R10), then PR 3 — `WBP_Layout`, `WBP_HudLayout` + its parts, `L_Test_UI`, the kit widgets — which needs an editor slot and `DA_UITokens`.

