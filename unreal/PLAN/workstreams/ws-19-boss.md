---
ws: 19
slug: boss
title: Boss Frame01
state: active
owner: session-01EeMqPt-cloud
claimed_at: 2026-09-25T14:34:44Z
lease_expires: 2026-09-26T14:34:44Z
branch: ws/19-boss/core
last_commit: 
editor_heavy: true
phase: P4
size: XL
critical: false
blocked_on: 
---
# WS-19 — Boss Frame01

## Scope / DoD
**Scope.** 3 phases, sweep/slam/stomp, plates, brood vents, boss routes, boss HUD, horn; Source/DFEnemies/Boss/.

**Definition of done.** DF.Func.Boss.PhaseTransitions; kill-window gate 55–85% of walk on Crown/Foundry/Toaster.

**Spec.** B§2.4 (in `unreal/PLAN/PROGRAMME.md`). Size XL, phase P4.

## Contracts I consume
- C2
- C5
- C6
- C12

## Contracts / interfaces I provide
- FDFBossPhaseRow
- `Source/DFEnemies/Public/Boss/DFBoss.h` — `FDFBoss` (Begin / CurrentPhase / PhaseTag / ResolveHit / ProfileHit / ArmorProfile / FlatArmor / SpeedFactor / ApplyDamage / Heal / Advance), `FDFBossState` (what replicates to the boss bar and what a save writes), `FDFBossTables` (+ `Validate`), `FDFBossBody`, `FDFBossHitTarget`, `FDFBossEvent` / `EDFBossEventKind`, `EDFBossZone`, `FDFBossKillWindow`. Pure: no actor, no world, no GAS.

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->
- 2026-09-25 · **C2 contract-append** (no RFC: a new row struct the contract already names, plus one optional field with a no-effect default): `FDFBossPhaseRow` and its parts `FDFBossAttackRow` / `FDFBossSpawnRow` / `FDFBossSiegeRow` in `DFContentRows.h`, with exactly the fields `CONTRACTS/content-rows.md` lists, plus `bPlatesHeld` (default false), which the contract does not list and B§2.4 needs ("P2: plates drop"). **Not registered as a table yet.** `boss.json`, its schema, `DFContentTables.cpp`, `GTableNames` and `DT_Boss.uasset` go in one PR from a machine that can run the importer, because a JSON table with no DataTable turns `DF.Content.RoundTrip` red. Dependents: none today. WS-01 owns the file, and WS-12's boss bar will read `FDFBossState` through the view model.

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->
- `OWNERSHIP.md`: add `unreal/DeepField/Source/DFEnemies/{Public,Private}/Boss/**` and `Source/DFEnemies/Private/Tests/DFBoss*` → WS-19 (shared with WS-05, whose module it is; PROGRAMME §5.3 puts WS-19 in "WS-05 (new `Boss/` subfolder)"). Text only, so today it is four ownership warnings and no violation.
- `CONTRACTS/content-rows.md`, the `boss` line: add `bPlatesHeld` (see Interfaces I changed).

## Open questions
B§2.4 is the whole spec (the sim has no boss). Everywhere it is silent `DFBoss.h` makes one reading and states it at the declaration. Each is a number in a row or a one-line change, and each is a design call, so I am flagging them rather than treating them as settled:
- **Phases count body hp only; plates are their own pool.** Read from "1200 raw hp + 6 plates × 40": the plates are extra, not part of the 1200. The other reading, phases over 1440 with plate damage moving the bar, makes stripping plates in P1 advance the fight by itself.
- **A phase is entered at its HpFrom exactly (≤), and never left.** A Mender healing the boss does not re-arm it, because plates that have dropped cannot come back. `Validate` enforces the second half: no phase may hold plates after one that shed them.
- **The hit that breaks a plate is spent on the plate.** No overflow into the body. Overflow is the obvious alternative and is one branch.
- **A plate is armour: no arc, no flat armour, no weak point, ×2 under shred, and hollow point gets no unarmoured bonus against it** (`RowFlatArmor` positive, `FlatArmor` 0). A body hit takes the phase's arc and flat armour through `FDFDamageMath` unchanged, including the ×1.35 shred leak while an arc is up. A P1 vent hit from the front is ×2.0 × 0.2, because both the vent and the arc are true of the hit.
- **A hit that crosses two thresholds enters both, in order, even the killing blow**, so B§1.7's +10 XP per phase is paid for a phase the boss was beaten through. The events arrive `PhaseEntered… Died` in order, so audio/UI can skip a stinger for a phase entered by the killing hit.
- **Attacks keep their rhythm whether or not anyone is in reach.** Each telegraphs `Interval − Telegraph` after the phase starts or its last landing, and lands at `Interval`. A phase change or death cancels a telegraph in flight (`AttackCancelled`). **A vent at the cap still resets its clock** and emits nothing, so a `BroodVented` always means "spawn exactly Count".
- **Numbers B§2.4 does not give** (the test fixture picks them, and no test depends on the pick): P1's flat armour (P2 says "FlatArmor 0", which implies P1 has some); Slam's interval; the telegraphs of Slam and Stomp; Sweep's damage.
- **Siege per phase.** The sim's `SiegeStructures` swings at the nearest structure within `StructureReach`, and that is exactly P2's "sieges first structure within 8 m", so P2 is `Siege{30, 8}`. P1 and P3 ("walks to core") must still break a wall across the boss route, yet not stop for a tower beside it. The sim cannot tell those two cases apart, so **WS-05's siege loop needs a "blocking structures only" mode** for the boss. `FDFBossSiegeRow` carries the numbers; the rule is not written anywhere yet.
- **"Movement ×0.5"** (halved Movement-status magnitudes: chill 0.35 → 0.175) has no hook. `UDFStatusResolver` compares magnitudes but never scales them. It belongs to WS-02 and is shared with WS-17's swift elite ("Movement magnitudes ×0.5"). The boss core deliberately leaves it out, so there are not two formulas.
- **Attack fields C2 lacks**: Stomp's crater (`rubble` patch r5 for 6 s), Slam's "ragdolls Mass ≤ 2", Stomp's 0.6 s camera shake honouring reduce-flash. Nothing in the pure core reads them. They join `FDFBossAttackRow` as optional fields in the PR that writes the attack resolver.
- **Where the plate numbers live in content.** `FDFBossBody` is plain data today. I recommend a C2 append of `PlateCount`, `PlateHp` and `PlateShredFactor` on `FDFEnemyRow` (defaults: no plates), because WS-20's carapace needs the same three numbers ("6 plates 25 hp… shred ×2 on plates").
- **"Breaks operated gates (4 s)"** is routing (WS-09/WS-05): a shut operated gate priced as 4 s × speed × bias for the boss. It is not modelled here.

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-25 · session-01EeMqPt-cloud (a Linux cloud session with no engine, claimed at the owner's request) · **written, NOT BUILT**, on `ws/19-boss/core`: the boss **logic core** (`Source/DFEnemies/{Public,Private}/Boss/DFBoss.{h,cpp}`) and the C2 append `FDFBossPhaseRow` (see Interfaces I changed). What the core does: phases by body-hp fraction, monotonic, several per hit in order; plates as their own pool on the wave's hp curve, shred ×2, broken at 0, shed on entering a plateless phase; vents covered by their plate and then open at the phase's weak-point factor; `ProfileHit` fills the target half of an `FDFDamageInput`, so every boss hit is still priced by WS-02's `FDFDamageMath` in the sim's order; the attack telegraphs and lands every interval, and a phase change or death cancels it; brood vents hold `MaxAlive` across one long frame; one step and many small ones give the same events at the same times; `FDFBossKillWindow` holds the DoD gate (55–85 %, inclusive; a leak or a broken measurement fails). **Tests (11):** `DF.Unit.Boss.{ValidateRejectsBadTables, BeginScalesHpAndPlates, PhasesFollowBodyHpOnly, OneHitCrossesPhasesInOrder, PlatesCoverVentsUntilTheyBreak, HitsPricedThroughDamageMath, AttackTelegraphsThenLands, SameEventsAtAnyFrameRate, BroodVentHoldsItsCap, HealNeverRearms, KillWindow}`. **How far "not built" goes:** the new files and the real `DFDamageMath.cpp` were compiled with clang 18 against hand-written stand-ins for the UE containers, macros and automation base, and all 11 tests *ran* and passed there. Then 28 single-rule mutations of `DFBoss.cpp` each turned at least one test red, in the test named for that rule, with no survivors. That proves the logic and the tests' power to fail. It proves **nothing about the UE API or UHT**: the stand-ins are mine. Two defects the run caught before the push: a test lambda that priced a variant table's hits against the default fixture, which the Python mirror written first had not reproduced; and my stand-in `FName` comparing case-sensitively, so the duplicate-id case (`Armoured` vs `armoured`) failed for the wrong reason. Repo checks: layering OK, check-test-coverage 6/6 gated, ownership 0 violations (4 text warnings, see Needs INT), schemas OK. No editor slot taken. **On the box before landing:** build, then `DF.Unit.Boss` and the full gate. **Next:** `boss.json` + schema + table registration + `DT_Boss` import (a box session, one PR with the Needs-INT contract line); a `bossFrame01` row in `enemies.json` plus its binding; then the body (`ADFBoss` or an `ADFEnemy` subclass once WS-05's actor lands) and `DF.Func.Boss.PhaseTransitions`.

## Invariant from WS-05 (wave director) — read before you write a spawner
**Anything that puts an enemy into the world outside the wave plan MUST call
`ADFWaveDirector::NotifyEnemyAdded` for it**, or the wave clears while your bodies are still alive
(the director owns "what spawns when" and "when the wave is over", and counts only what it knows about).
This reaches the boss brood vents (B§2.4 P2: 4 motes every 15 s, max 12). INT recorded it on 2026-09-21 from PR #45; it is a one-line call, and finding it after
the fact costs a wave-clear bug that only shows up with that content in play.
