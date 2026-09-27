---
ws: 28
slug: match-flow
title: Match flow (DFMatch)
state: active
owner: session-gpu-box-2026-09-26
claimed_at: 2026-09-27T06:30:33Z
lease_expires: 2026-09-28T06:30:33Z
branch: ws/05-enemies-ai/walking-enemies
last_commit: 0228420
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
- 2026-09-25 · **`ADFPlayerState` hosts `UDFHeroStateComponent`** (ADR-0024, WS-03's component): `GetHeroState()`, `ReviveMatchXp` (5, Step.cs:931); its host events become `PlayerDowned` / `PlayerRevived{PlayerId = reviver, TargetPlayerId = revived}` / `PlayerRespawned`. `ADFMatchState::HostRespawnBledOutHeroes()`, called at the wave boundary before `OnWaveBoundary`. Dependents: WS-12 (the player view model's `bDowned ReviveProgress BleedoutSecondsLeft Seat` can read `GetHeroState()` now), WS-03.

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->
- **`DefaultPawnClass`.** Setting it to `ADFHeroCharacter` is WS-28's to do, but not yet: the hero binds C16's input actions, which do not exist (WS-03's Needs INT), so a match would give every player a pawn that cannot move. Switch it in the same PR that the input assets land in, or right after.
- **The shells are still unbuilt.** INT's note asks the first claimant to build `DFMatch`, run `DF.Unit.Match` and the smoke, and fix what the compiler finds. This session has no engine, so that is `unreal\deepfield pr-check -Ws 28` plus the smoke on the GPU box; send the failures here.

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-25 · session-01DTQRZ3-cloud · **claim** — cloud session, on the user's ask; the same session holds WS-03. INT's note asks whoever claims first to build the shells, run `DF.Unit.Match` and the smoke, and fix what the compiler finds: this session has no engine, so that first build has to happen on the GPU box (`unreal\deepfield pr-check -Ws 28`), and whatever it finds comes back here. First PR on `ws/28-match-flow/hero-state`: host WS-03's `UDFHeroStateComponent` on `ADFPlayerState`, relay its events as `PlayerDowned`/`PlayerRevived`/`PlayerRespawned`, credit the reviver, respawn bled-out heroes at the wave boundary and move respawned pawns to a player start (ADR-0024; ws-03-player.md Needs INT). Code-only.
- 2026-09-25 · session-01DTQRZ3-cloud · **session end** — PR on `ws/28-match-flow/hero-state` (`ea6a1d4`): `ADFPlayerState` hosts WS-03's hero state and relays its events (reviver credited); `ADFMatchState::HostRespawnBledOutHeroes` at the boundary; respawned pawns moved to a player start. Tests `DF.Unit.Match.{PlayerStateHostsHeroState,HeroEventsBecomeMessages,BledOutHeroesRespawnAtBoundary}`. **Not built and not run.** Ran: `layering-check` OK, `ownership-check --ws 28` 0 violations, `validate-content-json` OK, `check-test-coverage` 6/6. **Next:** `DefaultPawnClass` once the input assets exist; WS-12's real feed (their file); the Commands.cs RPCs as their domains arrive.

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
- 2026-09-27 · session-gpu-box-2026-09-26 · `ADFGameMode::DefaultPawnClass = ADFHeroCharacter` (was the engine's `ADefaultPawn`), for the first playable on Testlane (WS-03 log). Next: a solo match that starts its waves by itself, and a spawner for `ADFWaveDirector::OnSpawnRequested` (step 2, with WS-05).
- 2026-09-27 · session-gpu-box-2026-09-26 · The match spawns enemies: `ADFMatchState::BindEnemySpawner` answers its own director's `OnSpawnRequested` with `ADFEnemy` (WS-05 log), and `HandleEnemyLeaked` logs each leak; lives stay WS-06's. Solo Testlane runs its three waves unattended.
- 2026-09-25 · session-01HszbJQ-cloud (WS-04) · `ADFEventRelay` now registers itself as the bus's team relay (`UDFMessageBus::SetTeamRelay`) on a networked host and `Publish` is `BroadcastTeam` — so modules below DFMatch (towers first) reach clients without naming DFMatch. messages.md updated. Not built.
- 2026-09-25 · session-01HszbJQ-cloud (WS-04) · `ADFPlayerController` gains the build commands, as its header invites: `Server_PlaceTower(TowerId, SocketId)`, `Server_UpgradeTower(StructureId, PathIndex)`, `Server_SellTower(StructureId)`, forwarding to `UDFBuildSubsystem` (DFTowers) with the seat as PlayerId; refusals go back through `Client_Refused` as `BuildRejected` (Subject socket, Detail tower id) / `UpgradeRejected` (Subject def id, Detail structure id).
- 2026-09-27 · session-gpu-box-2026-09-26 · ADFMatchState carries WS-06's economy as the "Economy" default subobject (`GetEconomy()`); the leak handler now only logs (the lives went on the message). ADFPlayerController binds IA_Build / IA_Sell (hold E / X) for quick build and sell on the aimed pad. **`-DFDemo`** (dev builds): the host spawns ADFDemoDirector (`Public/Dev/`), which builds towers pad by pad from the economy (Lance, a Nova every third) and orbits a camera over the map for recordings; `deepfield play -Demo` starts it, and `deepfield play` / `host` now default to L_Testlane.
- 2026-09-27 · session-gpu-box-2026-09-26 · **Placeholders were grey**: UE 5.8's /Engine/BasicShapes meshes carry DefaultMaterial (no parameters), so every "Color" tint on an instance of the mesh's own material did nothing (seen in the first -DFDemo recording). `DFShapeLook::Tint` (DFCore, contract-append) puts BasicShapeMaterial on the slot first; ADFEnemy, the tower placeholder and the rounds use it, and DFWorld's portal/core look is switched to it after WS-09's branch lands. `/Engine/BasicShapes` is added to DirectoriesToAlwaysCook. The demo camera orbits closer (0.9 x the map's half-extent, 18-36 m) and leans its focus toward the live enemies.
- 2026-09-27 · session-gpu-box-2026-09-26 · Demo polish: ADFDemoDirector builds from the core end (a first tower by the portal killed every body as it appeared) and, with `-DFDemoCapture`, requests a screenshot with the HUD every frame (`-dumpmovie` leaves the UI out). `record-demo.ps1` uses it and plays `?endless` by default so the waves keep coming (`-NoEndless` for the arc).
- 2026-09-27 · session-gpu-box-2026-09-26 · Hold U on a built pad upgrades its tower (C16 IA_Upgrade, 0.3 s; gamepad D-pad up): ADFPlayerController picks the least-bought path (first on a tie) and sends Server_UpgradeTower; a maxed or unaffordable path comes back as UpgradeRejected, which the HUD toasts.
- 2026-09-27 · session-gpu-box-2026-09-26 · **Pick the tower** until the radial build wheel: C16's IA_Wheel (Axis1D; mouse wheel, D-pad right/left) steps ADFPlayerController::QuickBuildChoices (lance, nova, arc, filament: the ground fighters) and hold E builds the choice. The choice reaches the HUD through the view model (UDFPlayerViewModel.BuildChoice, local player only), and the prompt reads "Wheel: Nova (115)   Hold E on a pad: build   Hold U on a tower: upgrade   Hold X: sell".
- 2026-09-27 · session-gpu-box-2026-09-26 (agent) · **Play-again and hero kill credit** (`ws/28-match-flow/play-again`). A finished match leads to a new one: `FDFMatchPhaseMachine` starts a restart clock at Victory or Defeat and returns a new `Restart` step once when it runs out (never in the lobby; an endless run ends only by Defeat, so it never restarts while it lasts). `ADFMatchState` broadcasts `OnRestartRequested` (host, once); `ADFGameMode` binds it and server-travels to `?Restart`, which the engine resolves to the last URL (same map, same options, as `AGameMode::RestartGame` does). It is a hard travel: a listen host's clients follow and reconnect, `?lobby` returns to its lobby, and `-DFDemo` spawns its director again. The delay is `ADFMatchState::RestartDelaySeconds` (EditDefaultsOnly, 15) or `?restart=<seconds>` (0 = never). The deadline replicates in `PhaseEndsAtServerTime`, so the HUD shows "New match in 12s" under the banner (WS-12 log). Everything comes back fresh with the map. The bus is the game instance's, so the match state now unsubscribes at EndPlay, as the economy does. The process-wide id counters (`ADFEnemy` target ids, `ADFTower::AllocateStructureId`) keep counting across matches on purpose: ids are only matched for equality, so an id that is never reused cannot alias a body or tower from the previous match (comment in DFEnemy.cpp). WS-06's open item "a restart resetting the purse" is met by the reload. **Hero kills are credited:** `IDFSeatHolder` (DFCore contract-append, `Match/DFSeatHolder.h`, CONTRACTS/README row) is implemented by `ADFPlayerState`. `ADFEnemy` fills `EnemyKilled.KillerPlayerId` and a new per-hit `EnemyDamaged.SourcePlayerId` (WS-05 log). On the host, `ADFMatchState` credits that seat's kills, +1 match XP per kill (Step.cs:2090) and damage dealt (Step.cs:2071). Tests: `DF.Unit.Match.{RestartAfterVictoryOrDefeat, NoRestartInLobbyOrEndlessRun, StateRestartDeadline, StateNoRestartInLobbyOrEndless, SeatOfFindsThePlayer, HeroKillCredited, NothingListensAfterTheMatch}`. **Not yet:** no automated test exercises the travel itself (unit tests use the hook, and a PIE travel in the gate would tear down the test world); the end screen is still the banner, with no scoreboard and no "play again now" button; after a restart, seats are re-assigned in reconnect order.
