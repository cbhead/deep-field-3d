---
ws: 00
slug: foundation
title: Foundation & contracts
state: review
owner: 
claimed_at: 2026-09-17T07:17:27Z
lease_expires: 
branch: unreal/main
last_commit: ff379ca
editor_heavy: true
phase: P1
size: M
critical: true
blocked_on: git-lfs for committing L_Dev_Empty.umap; WS-15 for CI
---
# WS-00 — Foundation & contracts

## Scope / DoD
**Scope.** uproject, modules, native tags, message structs, content subsystem, message bus, collision/input, material parameter names, ledger scaffold, ADRs.

**Definition of done.** Editor opens; Mac Development Editor build; L_Dev_Empty listen host + PIE client; tags compile; CONTRACTS docs written; CI builds.

**Spec.** PROGRAMME.md §3, §3.1 (in `unreal/PLAN/PROGRAMME.md`). Size M, phase P1.

## Contracts I consume
- (none)

## Contracts / interfaces I provide
- C1
- C7
- C8
- C15
- C16

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-19 · session-75b58b1b (INT) · SSD attached (`/Volumes/Toshiba`, APFS, 3.6 TB); repo cloned to `/Volumes/Toshiba/Deepfield-Unreal/deepfield-3d` (the Unreal working copy; DDC at `../DDC`). Landed the `.uproject` (UE 5.8.2, 26 plugins), Game/Editor targets (BuildSettingsVersion V7 — the installed build refuses V5), 14 module skeletons from `unreal/Build/modules.json` via `new-module.py` (layers 0–6; `layering-check.py` green), DFCore: native tags (C1, X-macro `.inl` — note UE_DEFINE_GAMEPLAY_TAG's .cpp static-assert, we expand it by hand), 110 message tags + 17 payload structs (C15), `UDFMessageBus` (R9 fallback; GameplayMessageRouter is not in the engine), C2 row structs, `UDFContentSubsystem`, `UDFContentDefinition`, `UDFGameInstance`; DFMatch `ADFGameMode`; C16 collision profiles + physmats; DDC/render/nav/net config. **`DeepFieldEditor Mac Development` builds** (5.5 min on the M1). Next: `L_Dev_Empty` via `make_dev_level.py`, listen-host + PIE-client smoke, then commit binaries once git-lfs is installed.
- 2026-09-19 (later) · session-75b58b1b (INT) · `L_Dev_Empty` created headless via `tools/ue-bridge/ue/make_dev_level.py`; **`unreal/Build/smoke-listen.sh` passes**: listen host on port 7788, headless client connects, login accepted, second `player joined` on the host. `CommonGameViewportClient` set (CommonUI input routing). DoD status: editor opens ✓, Mac build ✓, listen host + client ✓, tags compile ✓, CONTRACTS written ✓, CI builds — WS-15 (not claimed). **Blocked:** `L_Dev_Empty.umap` cannot be committed until `git-lfs` is installed (`brew install git-lfs && git lfs install` in both clones); until then any session creating the level re-runs the script.
- 2026-09-25 · INT · `Source/DFMatch/**` passes to the new **WS-28 Match flow** (ADR-0024); WS-00's skeleton `ADFGameMode` is its starting point. DFCore took three contract-appends today on INT's rulings: `Determinism/DFDetRng.h` + `DFDetMath.h` moved in from DFEnemies (R2), `Online/DFJoinSeams.h` (R3), `FDFMsg_Player.OnlineId` (R3), and the `DF.Vehicle.*` leaves in `DFGameplayTagList.inl` (R10). None has been built yet — the first Mac build after landing verifies them.
