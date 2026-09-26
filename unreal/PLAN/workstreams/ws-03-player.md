---
ws: 03
slug: player
title: Player
state: active
owner: session-01DTQRZ3-cloud
claimed_at: 2026-09-25T04:59:33Z
lease_expires: 2026-09-26T22:01:42Z
branch: ws/03-player/hero-health
last_commit: 25f8061
editor_heavy: true
phase: P2
size: L
critical: true
blocked_on: 
---
# WS-03 — Player

## Scope / DoD
**Scope.** ADFHeroCharacter, CMC modes, input, server-traced weapons with magazines/spread/recoil knobs, reload montage driver scaled to ReloadSeconds, melee arcs, interaction precedence chain, build ghost, downed/revive/drag hooks.

**Definition of done.** 6.5/10/4.8 parity test; DF.Net.WeaponFeel at 150 ms; traversal modes pass DF.Func.Traversal.*.

**Spec.** B§1.1–1.3, B§1.13–1.14, §3.2 (player), §3.3 (in `unreal/PLAN/PROGRAMME.md`). Size L, phase P2.

## Contracts I consume
- C4
- C5
- C6
- C9
- C16

## Contracts / interfaces I provide
- ADFHeroCharacter
- UDFWeaponInstance

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->
- 2026-09-25 · **`ADFHeroCharacter` first implementation** (PR 1, no RFC: the interface had no code yet). `UDFHeroMovementComponent` (sprint and ADS as predicted wants in saved-move custom flags 0 and 1; `IsSprinting`, `GetHeroSpeeds`), `DFHeroMove::*` rules, `DFHeroCollision::Profile()`. Dependents: WS-28 (the pawn class, see Needs INT), WS-12 (a hero view model binds here later).
- 2026-09-25 · **`UDFHeroStateComponent` first implementation** (PR 2, no RFC). Readers: `GetLife IsUp IsDowned IsRespawning IsBleeding GetReviveProgress GetReviveFraction TryGetBleedoutSecondsLeft TryGetRespawnSecondsLeft GetSeatIndex GetSeatVehicle GetReviver GetStateTags GetRules`; host: `HostDeplete HostBeginRevive HostEndRevive HostAdvance ShouldRespawnAtWaveBoundary HostRespawn HostTakeSeat HostLeaveSeat`; `OnHeroLifeEvent` (host), `OnHeroStateChanged` (all). Rules in `DFHeroLife::*`. Dependents: WS-28 (attaches and relays, see Needs INT), WS-12 (`UDFPlayerViewModel` `bDowned ReviveProgress BleedoutSecondsLeft Seat` read here), WS-08 (seats).
- 2026-09-25 · **PR 3:** `ADFHeroCharacter` implements `IAbilitySystemInterface` (`GetAbilitySystemComponent`, `GetHeroAbilitySystem`, `GetHealthSet`, `GetHeroSet`); `UDFHeroStateComponent::HostDeplete(ConnectedPlayers, BleedoutSeconds = 0)` (the override is appended, existing calls unchanged); `UDFHeroMovementComponent::SetHeroLife`/`GetHeroLife`, `MaxDownedSpeed`; `DFHeroMove::MaxSpeedForLife`, `DownedCrawlCmPerSec`; `DFHeroLife::NoteDamaged`/`RegenStep`, `FDFHeroRegen`.

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->
- **C16 input assets.** `IA_Move IA_Look IA_Jump IA_Sprint IA_Crouch IA_Aim` and `IMC_DF_Default` do not exist yet (`Content/DF/Core/Input`, INT-owned). `ADFHeroCharacter` exposes them as Blueprint slots and binds nothing until they are assigned. For Godot parity, IA_Look's mouse scale is 0.0022 rad per pixel (0.126° per pixel, `Player.cs` BaseMouseSensitivity) with Y negated; Sprint, Crouch and Aim are held.
- **The pawn class.** `ADFGameMode` (WS-28, unclaimed) sets no `DefaultPawnClass`, so nothing spawns a hero yet. Setting it to `ADFHeroCharacter` (or its Blueprint) needs `DFPlayer` in `DFMatch.Build.cs` (layer 4 → 3, legal; Build.cs lists are INT's).
- **Hero collision profile name.** `DFHeroCollision::Profile()` belongs beside `DFCollision::GrayboxProfile()` once DFCollision moves to DFCore by contract-append.
- **WS-28's attach PR for `UDFHeroStateComponent`** (ADR-0024; WS-28 is unclaimed, so INT per the ADR). Create it on `ADFPlayerState` (DFMatch.Build.cs needs DFPlayer); at the intermission's end call `HostRespawn()` on each component whose `ShouldRespawnAtWaveBoundary()` and move that pawn to the hero spawn; subscribe `OnHeroLifeEvent` and publish through `ADFEventRelay`: `Downed`/`SoloDowned` → `PlayerDowned{PlayerId = seat}`, `Revived` → `PlayerRevived{PlayerId = reviver's seat, TargetPlayerId = revived seat}` plus `AddRevive()` and `AddMatchXp(5)` on the reviver (Step.cs:931), `Respawned` → `PlayerRespawned{PlayerId = seat}`. Health: set to `RevivedHpFraction` × max on `Revived` and to full on `Respawned` (the hero's health set, WS-02 attributes, once the hero has its ASC).

## Open questions
- **Acceleration (for the user).** Godot writes velocity directly: instant start, stop and turn, full air control. PR 1 approximates that (20000 cm/s² acceleration and braking, AirControl 1). If the Unreal hero should have weight instead, that is a design call, and it is one constant.
- **Sprint direction.** Godot sprints in any direction while Shift is held; PR 1 keeps that. Many shooters sprint forward only.
- **Crouch.** Held, as coded; B§1.1 does not say hold or toggle, and does not give a crouched height (the CMC default half-height of 40 cm is in use, about 0.8 m).
- **Crouched ADS.** B§1.1 gives crouch 3.0 and ADS 3.5 but not both at once; PR 1 takes the slower.
- **Revive is server time, not per command (decided in PR 2, flagging it).** The sim adds a tick per Revive command and decays half a tick every tick, so its hold time depends on the client's command rate; B§1.14 says server-timed and one reviver, so PR 2 accrues 1 s/s while one standing reviver in range holds and decays only while nobody does. A held revive takes exactly 4 s.
- **A solo hero is `Respawning`, not `Downed`.** Step.cs emits PlayerDowned for both, and PR 2's `SoloDowned` event does too, but only a teammate-present down sets `bDowned` and the Downed tag; the view model shows a respawn countdown for the solo case. Say if the UI wants one state for both.
- **Where the hero's health lives — decided in PR 3, flagging it.** On the pawn: PROGRAMME.md §3.3 lists `ADFHeroCharacter (CMC + attributes)` among the replicated state. The hero's ASC is therefore not on the player state (Lyra's choice); a respawn in place keeps the pawn and its attributes. If INT prefers the player state, the move is contained in `ADFHeroCharacter`.

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-25 · INT · ADR-0024 (ruling R1): downed, revive progress and vehicle seat replicate from a **`UDFHeroStateComponent`** you write in DFPlayer and WS-28 attaches to `ADFPlayerState`; WS-28 respawns bleedout-expired players at the wave boundary through your hook.
- 2026-09-25 · session-01DTQRZ3-cloud · **claim** — cloud session, on the user's ask (NEXT.md item 4). Read STATUS.md, NEXT.md, this file and INT's ADR-0024 note above. Plan for PR 1 on `ws/03-player/hero-movement`: `ADFHeroCharacter` + the hero movement component carrying B§1.1's parity numbers (walk 6.5, sprint 10, jump 4.8, crouch 3.0, ADS 3.5) through DFBalance dials, with DF.Unit tests for the 6.5/10/4.8 parity. Code-only, no editor slot. This session has no engine, so PR 1 will say it is unbuilt and can land only through `int-merge.sh` (CONTRACTS/ci.md).
- 2026-09-25 · session-01DTQRZ3-cloud · **session end** — PR 1 on `ws/03-player/hero-movement` (`f1f2147`): `ADFHeroCharacter` (Godot's capsule, eye height, FOV and pitch clamp; DF_Hero; Enhanced Input slots) on `UDFHeroMovementComponent` (walk 6.5 / sprint 10 / jump 4.8 parity, crouch 3.0, ADS 3.5, sprint and ADS predicted), rules in `DFHeroMove`, tests `DF.Unit.Player.{MoveParity,SpeedRules,ComponentSpeed,PredictedWants,HeroDefaults}`. **Not built and not run**: this cloud session has no engine. What ran: `layering-check` OK, `ownership-check --ws 03` 10 files 0 violations, `validate-content-json` OK, `check-test-coverage` 6/6 gated. Before it lands it needs, on the GPU box (unreal/README.md §5.4): `unreal\deepfield check`, `unreal\deepfield test DF.Unit.Player`, then the full gate. `int-merge.sh` has no machine until its Windows port (ADR-0028), which also corrects this session's claim line: there is no int-merge route today. No editor slot, no LFS locks. **Next:** PR 2 = `UDFHeroStateComponent` (ADR-0024: downed, bleedout 30 s, revive progress 4 s at 2.5 m by one reviver, vehicle seat) as a pure core plus the component, since WS-28 attaches it; then weapons (`UDFWeaponInstance`, B§1.2 server-traced shots).
- 2026-09-25 · session-01DTQRZ3-cloud · **session start (2)** — lease renewed. PR 1 landed as #56 (merge button, 2026-09-25 21:45 UTC; whether it was built first is not recorded here). Taking PR 2 on `ws/03-player/hero-state`: `UDFHeroStateComponent` (ADR-0024: downed, bleedout, revive, seat) as a pure core plus the component WS-28 hosts on `ADFPlayerState`. Code-only.
- 2026-09-25 · session-01DTQRZ3-cloud · **session end (2)** — PR 2 on `ws/03-player/hero-state` (`4d02abe`): `DFHeroLife` (Deplete, Tick, InReviveRange, ShouldRespawnAtWaveBoundary, Respawn) and `UDFHeroStateComponent` (host stepping, transition-only replication with server-clock deadlines and client-side revive extrapolation, events, state tags, seat). Tests `DF.Unit.Player.{LifeRulesMatchSim,LifeDeplete,LifeReviveServerTimed,LifeReviveDecay,LifeBleedoutAndWaveBoundary,LifeSoloRespawn,LifeReviveRange,HeroStateHostFlow}`. **Not built and not run** (no engine here). Ran: `layering-check` OK, `ownership-check --ws 03` 0 violations, `validate-content-json` OK, `check-test-coverage` 6/6. Before landing: `unreal\deepfield pr-check` on the GPU box. **Next:** PR 3 = downed movement (crawl 1 m/s, sidearm only, no sprint/jump) and drag (hold E, 2 m/s), reading this component from the hero; or the hero's ASC and health wiring, once INT says where the hero ASC lives.
- 2026-09-25 · session-01DTQRZ3-cloud · **session end (3)** — PR 3 on `ws/03-player/hero-health` (`25f8061`): the hero's ASC with UDFHealthSet/UDFHeroSet from DT_Balance; 0 hp → `HostDeplete` (with the hero's BleedoutSeconds); revive → half health, respawn → full; Step.cs regen; the hero follows its state component and crawls at 1 m/s when down, still while waiting for the solo respawn. Tests `DF.Unit.Player.{RegenMatchesSim,DownedMovement}`, plus HeroDefaults and HeroStateHostFlow extended. **Not built and not run.** Ran: `layering-check` OK, `ownership-check --ws 03` 0 violations, `validate-content-json` OK, `check-test-coverage` 6/6. **Next:** weapons (`UDFWeaponInstance`, B§1.2: server-traced shots with ≤200 ms rewind, magazines, spread and recoil knobs, reload scaled to ReloadSeconds) toward DoD `DF.Net.WeaponFeel`; drag (B§1.14) after interaction exists.
