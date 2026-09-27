---
ws: 06
slug: economy-gunsmith
title: Economy, scrap, gunsmith
state: active
owner: session-gpu-box-2026-09-26
claimed_at: 2026-09-27T07:17:05Z
lease_expires: 2026-09-28T07:17:05Z
branch: ws/04-towers/testlane-lance
last_commit: 
editor_heavy: false
phase: P3
size: M
critical: false
blocked_on: 
---
# WS-06 — Economy, scrap, gunsmith

## Scope / DoD
**Scope.** Money/lives/bounty, team+personal scrap with physics pickups, Primecore, purchases, 7 slots/15 attachments/8 ammo, PaP, melee mastery in scrap, early-call bonus.

**Definition of done.** Balance test reproduces Balance.cs; armory buys/refusals as messages; view-model stats.

**Spec.** A1 weapons/economy, B§1.2, B§1.8, B§2.12 (in `unreal/PLAN/PROGRAMME.md`). Size M, phase P3.

## Contracts I consume
- C2
- C4
- C12

## Contracts / interfaces I provide
- UDFEconomySubsystem
- FDFWeaponBuild

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-25 · INT · ADR-0024 (ruling R1): money, lives, team scrap and bounty replicate from a **`UDFEconomyStateComponent`** (DFGameplay/Economy) that WS-28 attaches to `ADFMatchState`; personal scrap and weapon builds from a **`UDFLoadoutStateComponent`** on `ADFPlayerState`. WS-28 reads lives for Defeat and never writes them; leaks arrive as a message and your component takes the lives off. Scrap can draw from `DFCore/Public/Determinism/DFDetRng.h` (R2).
- 2026-09-25 · INT · The seam for lives now exists: implement `IDFMatchLivesSource` (`Source/DFCore/Public/Match/DFMatchSeams.h`) on `UDFEconomyStateComponent`, and take a leak's lives **before** the leaking body is reported to the director (`NotifyEnemyRemoved`) — that ordering is what makes a last-enemy leak a Defeat (`DF.Unit.Match.StateLastLeakIsDefeat` pins it with a stand-in). The early-call bonus hooks `ADFMatchState::OnEarlyCalled(Seat)`.
- 2026-09-25 · session-01HszbJQ-cloud (WS-04) · A second seam for your component: implement **`IDFTeamWallet`** (`Source/DFCore/Public/Economy/DFEconomySeams.h`) on `UDFEconomyStateComponent` — `GetMoney`, `GetTeamScrap`, all-or-nothing `TrySpend(money, scrap bundle)`, `AddMoney`. WS-04's `UDFBuildSubsystem` finds it on the game state's components and spends through it; until it exists every build is refused as insufficientFunds (one warning in the log).
- 2026-09-27 · session-gpu-box-2026-09-26 · **Claimed for the first playable's money and lives only** (step 3/4, at the owner's request), not the gunsmith: `UDFEconomyStateComponent` (DFGameplay/Economy, per ADR-0024) with replicated money and lives from the balance dials, `IDFTeamWallet` and `IDFMatchLivesSource`, lives off on `DF.Message.EnemyLeaked`, the sim's scaled bounty on `DF.Message.EnemyKilled`. Scrap, personal loadouts, purchases and the early-call bonus stay open.
