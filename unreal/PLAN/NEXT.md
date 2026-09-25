# Where the rebuild stands, and what to pick up next

**Read this after `STATUS.md` and before claiming anything.** `PROGRAMME.md` is the plan of
record and does not change often; `STATUS.md` is generated from the workstream files and tells you
who owns what; this file is the part neither of them carries — what is actually finished, what is
stranded, and what is worth doing next. It is hand-written and dated. If it disagrees with
`STATUS.md` about ownership, `STATUS.md` wins; if it is more than a week old, distrust it and say so.

_Last written: 2026-09-24 by session-62767025 (WS-05), acting INT because the integration session
that ran the 19th–21st has ended._

## The shape of the thing

P0 and P1 are done: the project exists, it builds on the Mac, the contracts are frozen in code, and
content is text that a commandlet imports. **P2 (the vertical slice, gate G2) is where the work is
now** — and G2 is not close, because the slice needs a player, towers, enemies in a level and a map
to put them on, and only the enemy half has been started.

What that means concretely: every "pure core" in `DFEnemies` — the wave plan, the director, the lane
walker — is logic with no actor attached. **Nothing spawns yet.** The first thing that will make
this project feel real is an `ADFEnemy` that a director spawns and a walker moves, and then a Lance
that shoots it.

## Landed and usable

| Area | What you can build against |
|---|---|
| Foundation (WS-00) | `.uproject`, 14 modules, C1 tags, C15 messages + `UDFMessageBus`, C2 rows, content subsystem, C16 collision/input |
| Content (WS-01) | `unreal/content/json/*` is the source of truth; `-run=DFContentImport` writes the 20 `DT_*` tables; `UDFContentSubsystem` is the only door to them |
| GAS (WS-02) | C4 attribute sets, `UDFStatusComponent` (8 channels, strongest-wins, closed reactions), `UDFDamageExecution` in the sim's order, faction passive GEs, `UDFTintComponent` |
| World (WS-09) | `UDFLaneGraphAsset` (C6), the `level.json`/`terrain.json` importers, `-run=DFMapValidate`, five legacy maps imported |
| Online (WS-11) | `UDFOnlineSubsystem` (C14) over OSSv2, `UDFProfileSave` (C13), join codes, content hash — stops at Null services until EOS credentials exist |
| Art pipeline (WS-30) | `build_heightmap.py`, `terrain.schema.json`, `-run=DFTerrainImport`, the Foundry landform |
| Enemies (WS-05) | `FDFWavePlan` (what spawns), `ADFWaveDirector` (when), `FDFLaneWalker` (where it goes), `KnockBack` (how it gets moved) — all pure, all tested against the frozen sim |
| Automation (WS-15) | `test.sh`, `pr-check.sh`, `int-merge.sh`, `ci-local.sh`, the GitHub workflows, `windows-bringup.md` |

## Stranded — work that exists but is not on `unreal/main`

- **PR #44, `ws/12-ui/tokens-screens`** — WS-12's UI tokens and the CommonUI screen stack. Open and
  mergeable, but **not ready to land: INT reviewed it and confirmed three findings.** The one that
  matters is a design error, not a typo: `DFUIScreenList.inl` puts Hud, Crosshairs, Overheads,
  Prompts, Revive and Endless all on `DF.UI.Layer.Game`, while `UDFUILayout` models a layer as one
  `UCommonActivatableWidgetContainerBase` — and such a container shows exactly one widget at a time,
  deactivating the previous. Six simultaneously-visible pieces of chrome cannot share one container:
  the HUD would be hidden by the crosshair. It does not bite yet only because no push sites exist,
  so it will bite whoever lands `WBP_Layout`. Also: `PushScreen` has no duplicate guard, and
  `FindOpenScreen` returns the oldest (buried) instance. Whoever takes WS-12 should fix these before
  landing rather than inheriting them.

Everything else that was in flight on the 21st has landed.

## The two machines (this changed on 2026-09-24)

**The Windows GPU workstation now exists.** ADR-0016's "Mac-only" constraint is over; ADR-0023
records the split. What each machine is for:

- **The Mac (M1, 8 GB)** — every `DFCore`/gameplay C++ workstream, logic tests under `-nullrhi`, the
  content pipeline, graybox levels, Mac packaging. It is the 8 GB floor: if it runs here it runs
  anywhere. It is *not* a reliable verdict machine under load (see hazards).
- **The GPU box (Windows)** — everything the Mac has never been able to prove: Nanite/Lumen/VSM,
  the Megascans-heavy `L_<Map>_Art` sublevels, Windows packaging and the EOS overlay, perf budgets,
  Gauntlet, the soak, and **the self-hosted CI runner**.

**If you are the session on the GPU box, start with `unreal/Build/windows-bringup.md`.** It is a
checklist from a bare machine to a building, testing, packaging runner, written against this
repository's own config and the engine's own requirement files. Nothing in it has been run yet —
correct it in place as you go, in the same PR as whatever turned out different.

## What to do next, in the order I would do it

1. **Register the self-hosted runner** (`windows-bringup.md` §8, `CONTRACTS/ci.md`). Today
   `unreal-mac` is the only lane that builds or tests the Unreal tree and **it has never run** —
   zero runners are registered, so a green PR check currently means the ledger and schemas are
   consistent and nothing more. Every real verification so far has come from a developer's local
   run. This is the highest-value hour available to anyone.
2. **Take WS-12 and fix PR #44's three findings, then land it** — the layer/container mismatch
   above is the blocker. It is finished work worth rescuing, but it is not a merge-button job.
3. **`ADFEnemy` + `UDFEnemyMovement`** (WS-05, mine) — the actor that turns three tested cores into
   something that walks down a lane and can be shot.
4. **WS-03 Player** and **WS-04 Towers** — both unclaimed, both critical path, and G2 needs both.
   WS-04 has the most leverage: a Lance that fires at a walking Drifter is the first time this is a
   game rather than a library.
5. **WS-10a Foundry** — the map the slice happens on. Text-authored terrain plus a redesigned
   `level.json`; the validator is already there to iterate against.
6. **The Windows lanes once the box is up** — perf baselines on `L_Dev_Empty` so the harness exists
   before a map does, then hardware Lumen/Nanite look-dev when WS-10a lands a landform.

## Hazards a new session should know

- **Every lease in `STATUS.md` was stale on 2026-09-24** and has been swept (see the digest). A
  workstream marked `paused` has real work in its session log — read it before starting over.
- **The Mac produces incorrect test verdicts under memory pressure.** A run with ~0.1 GB free and
  swap near full has failed with a thread-lock assertion and a segfault twelve minutes after the
  same code passed. `CONTRACTS/ci.md` has the rule: **one red is unproven — re-run once, and say
  that you re-ran.** Two reds are a defect. Check `vm_stat` before trusting a verdict.
- **`/Volumes/Toshiba` unmounts spontaneously.** A commit made seconds before can vanish. After any
  odd failure, compare `git log` against `origin` before assuming you imagined it.
- **`test.sh` now refuses to run against a binary older than the source** (`1f0f18f`). If you see
  that refusal, do not pass `DF_TEST_ALLOW_STALE=1` to get past it — build.

## Addendum, 2026-09-25 (INT rulings branch `claude/happy-babbage-t6qrhw`)
Written after the above and merged on top of it. If the branch has landed:
- **DFMatch has an owner:** WS-28 *Match flow* (ADR-0024, registered, unclaimed). The first PR's shells —
  phase machine, `ADFMatchState`, `ADFPlayerState`, `ADFPlayerController`, `ADFEventRelay`, 13
  `DF.Unit.Match` tests — are written but were **never built**. They sit between item 3 above (`ADFEnemy`)
  and a match that runs to victory: the director's `OnSpawnRequested` needs a spawner, and
  `ADFMatchState` is what begins the waves.
- **The deterministic headers moved** to `Source/DFCore/Public/Determinism/` (R2). Include
  `Determinism/DFDetMath.h`, not `Waves/DFDetMath.h`. The merge fixed `DFLaneWalker.cpp`.
- **Every open Needs-INT item has been ruled** (`rfcs/needs-int-rulings-2026-09-25.md`, ADRs 0024–0027,
  RFC-0003). None of that branch's C++ has been compiled: build, then `test.sh DF.Unit+DF.Content`,
  `DF.Online`, and the smoke, before trusting any of it.

## Addendum, 2026-09-25 late: `unreal/main` broke, and what is still unknown

**What happened.** PR #48 was merged through the GitHub UI while INT was verifying it on the Mac. Its
own description said none of its C++ had been compiled. The build then failed:
`DFMatch/Private/DFGameMode.cpp:98` called `UGameInstance::GetSubsystemArray<T>()`, which **does not
exist in UE 5.8** — the engine has `GetSubsystemArrayCopy<T>()` (`GameInstance.h:463`;
`SubsystemCollection.h:79` deprecates the old internal helper in favour of it). Fixed in `fcc1e1d`,
verified against the installed engine's headers. It is the ADR-0020 class of defect exactly: code
written against an API the installed build does not have, which no Python check can see and only a
compiler on the right engine version catches.

**What is still unknown, and is the first thing to establish on either machine.** PR #51
(`[WS-04] tower logic core`) merged onto the broken trunk an hour later, **also labelled unbuilt**, and
has never been compiled by anything. `fcc1e1d` fixes the one error that was found; whether
`unreal/main` compiles *now* has not been demonstrated. The verifying build was interrupted. So:

```
unreal/Build/int-merge.sh <any branch> --ws INT --dry-run     # or, for the trunk itself:
"$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" DeepFieldEditor Mac Development \
  -Project=<worktree>/unreal/DeepField/DeepField.uproject -WaitMutex -NoHotReload
```
Then `editor-lock.sh test.sh` with **no filter** (that runs the whole gate) and `smoke-listen.sh`.
Do not assume green. Two unbuilt PRs landed; one error is fixed; the rest is unmeasured.

**One thing that now proves less than it did.** #48 makes `PreLogin` run `ValidateJoinOptions` and
`GlobalDefaultGameMode` spawn `ADFMatchState`/`ADFPlayerState`/`ADFPlayerController` on **every** map,
including `L_Dev_Empty`, which has no lane graph. `smoke-listen.sh` decides success by counting
`player joined` to ≥2 (it was ≥1 until the host was found counting itself). That count now sits behind
a validator that did not exist when the threshold was chosen, so **re-derive the smoke's success
condition against the new join path before trusting a green smoke.** WS-05's spawn work sits on the far
side of that seam.

**And the rule that came out of it** is in `CONTRACTS/ci.md`: every change to `unreal/main` lands
through `int-merge`, never the merge button, because the merge button skips the only step that
compiles. Branch protection would enforce it — recommended to the repository owner, sequenced after the
runner exists, since requiring a check with no runner blocks everything.

