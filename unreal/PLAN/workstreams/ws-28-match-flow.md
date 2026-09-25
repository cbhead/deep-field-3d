---
ws: 28
slug: match-flow
title: Match flow (DFMatch)
state: active
owner: session-01DTQRZ3-cloud
claimed_at: 2026-09-25T22:58:32Z
lease_expires: 2026-09-26T22:58:32Z
branch: ws/28-match-flow/hero-state
last_commit: 
editor_heavy: false
phase: P2-P3
size: L
critical: true
blocked_on: 
---
# WS-28 — Match flow (DFMatch)

## Scope / DoD
**Scope.** Owns Source/DFMatch: ADFGameMode (from WS-00's skeleton), ADFMatchState + ADFPlayerState as hosts of domain-owned state components (ADR-0024), ADFPlayerController Server RPC surface (one RPC per Commands.cs command; every refusal a DF.Message.*Rejected to the issuer only), ADFEventRelay, the phase machine (lobby Launch gate, intermission timer + early call, BeginWave on the director, WaveStarted/WaveCleared messages, lives-first Victory/Defeat), endless toggle, campaign/sector chain, the host match record WS-11 persists.

**Definition of done.** DF.Unit.Match.* reproduce Step.cs phase order incl. LastLeakIsDefeat, lobby gate, early call and endless rollover; WS-12's view models read the real replicated state (no fake feed) on a listen host + 1 client; DF.Match.Solo.Testlane runs to victory in automation.

**Spec.** §3 (DFMatch), §3.3, ADR-0024, A1 economy/waves (Step.cs UpdateWaves + CheckEndState), B§1.11 (in `unreal/PLAN/PROGRAMME.md`). Size L, phase P2-P3.

## Contracts I consume
- C4
- C6
- C12
- C14
- C15

## Contracts / interfaces I provide
- ADFGameMode
- ADFMatchState
- ADFPlayerState
- ADFPlayerController
- ADFEventRelay
- match phase machine

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->
- 2026-09-25 · contract-append to DFCore: `Source/DFCore/Public/Match/DFMatchSeams.h` — `IDFMatchLivesSource::GetLives()` (the economy component answers; the match flow reads lives and never writes them) and the ordering it asks of a leak: lives off BEFORE `NotifyEnemyRemoved`. Dependents: WS-06 (implements it), WS-05 (leak ordering).
- 2026-09-25 · C15 transport made concrete: `ADFEventRelay::Publish(WorldContext, Tag, Payload)` is the one team-wide send; `messages.md` corrected (no separate envelope type). Dependents: every system that produces a discrete fact hands DFMatch a payload (messages.md "who broadcasts").

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-25 · session-01DTQRZ3-cloud · **claim** — cloud session, on the user's ask; the same session holds WS-03. INT's note asks whoever claims first to build the shells, run `DF.Unit.Match` and the smoke, and fix what the compiler finds: this session has no engine, so that first build has to happen on the GPU box (`unreal\deepfield pr-check -Ws 28`), and whatever it finds comes back here. First PR on `ws/28-match-flow/hero-state`: host WS-03's `UDFHeroStateComponent` on `ADFPlayerState`, relay its events as `PlayerDowned`/`PlayerRevived`/`PlayerRespawned`, credit the reviver, respawn bled-out heroes at the wave boundary and move respawned pawns to a player start (ADR-0024; ws-03-player.md Needs INT). Code-only.

## From INT (2026-09-25) — why this workstream exists, and its first PR
Registered by ruling R1 (`rfcs/needs-int-rulings-2026-09-25.md`, ADR-0024). Until now no workstream
owned DFMatch beyond WS-00's skeleton (`ADFGameMode`, 44 lines), so three workstreams were waiting on it:
WS-12's real view-model feed (`ADFMatchState`/`ADFPlayerState`), WS-05's `DF.Message.Wave*` (the
director hands the payload out through `DescribeWave` and waits for the phase owner to send it), and
WS-11's PreLogin handshake (R3).

**The state actors host components; they do not own every field.** A domain's replicated fields live
in a ModularGameplay `UGameStateComponent` / `UPlayerStateComponent` that the domain's workstream writes
in its own module; WS-28 attaches it (a one-line PR from the domain) and never edits it:

| field (PROGRAMME §3.3) | lives on | owner, module |
|---|---|---|
| phase, wave index, phase timer, threat multiplier, endless flag, seat roster | `ADFMatchState` | WS-28, DFMatch |
| money, lives, team scrap, bounty | `UDFEconomyStateComponent` on the match state | WS-06, `DFGameplay/Economy` |
| lane + mutable edge states (FastArray) | `UDFLaneStateComponent` on the match state | WS-09, DFWorld |
| faction, level | `UDFFactionStateComponent` on the player state | WS-07, `DFGameplay/Factions` |
| personal scrap, weapon builds | `UDFLoadoutStateComponent` on the player state | WS-06 |
| downed, revive progress, seat (vehicle) | `UDFHeroStateComponent` on the player state | WS-03, DFPlayer |
| stats (kills, damage, xp events) | `ADFPlayerState` | WS-28 (the host match record WS-11 reads) |

Until a domain's component exists, WS-28 does not stand in a field for it: WS-12's view models keep the
fake feed for that field and switch when the component lands.

**The phase machine is a port of `Step.cs` `UpdateWaves` (Step.cs:940) + `CheckEndState` (Step.cs:1977),
with WS-05's proposed hand-off accepted as is:**
- Intermission runs `Balance.IntermissionSeconds` (8 s, a balance dial). The clock does not run while
  `Lobby` (until the lowest-seated connected player sends Launch) nor while `WaitForPlayers` with no one
  connected. `StartWave` (early call) zeroes the timer in intermission and is ignored in the lobby.
- At the boundary: players whose bleedout expired respawn (WS-03 hook), `WaveIndex++`, then
  `ADFWaveDirector::BeginWave(WaveIndex, max(1, ConnectedPlayerCount))`; WS-28 broadcasts
  `DF.Message.WaveStarted` with `DescribeWave`'s payload (the director never broadcasts it).
- On `OnWaveCleared`: broadcast `WaveCleared`; Victory if `!Endless && WaveIndex + 1 >= TotalWaves`,
  else Intermission with the timer reset.
- **Lives first, every tick.** `CheckEndState` tests `Lives <= 0` before the cleared test, so a last enemy
  that leaks the core to zero is a **Defeat**. Pin it: `DF.Unit.Match.LastLeakIsDefeat`.
- Leaks arrive as a message (WS-05); the economy component takes the lives; WS-28 reads lives and never
  writes them.

**First PR = the shells**, and it unblocks WS-12 and WS-05 on its own: `ADFMatchState` / `ADFPlayerState` /
`ADFPlayerController` with WS-28's own fields replicated and the component slots empty; the phase machine
driving the director; `DF.Unit.Match.*` for the phase order in a test world; R3's PreLogin loop (INT may
land that line first — then it is already in the file). **If no session claims WS-28 within one INT cycle,
INT lands the shells under this row and hands them over at claim.**
- 2026-09-25 · INT (cloud session, the R1 fallback started early at the user's request) · **written, NOT BUILT**, on `claude/happy-babbage-t6qrhw`: the first PR's shells. `FDFMatchPhaseMachine` (pure port of Step.cs UpdateWaves + CheckEndState + ApplyLaunch + StartWave; lives optional, a map with no arc idles); `ADFMatchState` (replicates phase, wave index, total waves, lobby, endless, threat, lap, enemies remaining and the intermission clock as a server-time deadline; spawns the event relay and — from the lane graph's MapId, or `?wavesmap=` — the wave director on the host; sends WaveStarted / EndlessLap / WaveCleared / Intermission (next wave's payload) / Victory / Defeat through the relay; reads lives through `IDFMatchLivesSource`; `OnWaveBoundary` for WS-03, `OnEarlyCalled` for WS-06, `OnMatchStateChanged` for WS-12); `ADFPlayerState` (seat 1–4 + the match record: kills, damage, towers built, revives, match XP); `ADFPlayerController` (`Server_Launch`, `Server_CallEarly`, `Client_Refused`); `ADFEventRelay`; `ADFGameMode` picks the three classes, parses `?seed ?lobby ?endless ?waitforplayers ?wavesmap ?intermission`, seats players and sends PlayerJoined/Left through the relay (the smoke's "player joined" line kept). Tests: `DF.Unit.Match.{PhaseOrderMatchesSim, LastLeakIsDefeat, LivesCheckedEveryTick, LobbyGate, EarlyCall, WaitForPlayers, EndlessRollsOver, NoWavesMapIdles, StaleClearIgnored}` (pure) and `{StateDrivesDirector, StateLastLeakIsDefeat, DefeatStopsTheWave, StateEndlessPastTheArc}` (world, actors driven by hand). **Whoever claims WS-28 first:** build, run `test.sh DF.Unit.Match` and `smoke-listen.sh`, fix what the compiler finds, then own it. **Known gaps, deliberately left:** held seats on disconnect/rejoin (AGameModeBase keeps no inactive states — with WS-11's rejoin flow); the Commands.cs RPCs beyond Launch/StartWave (each arrives with its domain); WS-12's view models are not wired to the real state yet (WS-12's file; bind `OnMatchStateChanged` / `OnPlayerStateChanged`); on a map with waves but no enemy spawner yet (WS-05's `ADFEnemy`), a released wave never clears — expected until WS-05 lands.
