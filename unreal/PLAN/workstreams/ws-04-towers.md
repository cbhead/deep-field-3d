---
ws: 04
slug: towers
title: Towers & build
state: active
owner: session-01HszbJQ-cloud
claimed_at: 2026-09-25T02:29:31Z
lease_expires: 2026-09-26T02:29:31Z
branch: claude/happy-babbage-t6qrhw
last_commit: 
editor_heavy: true
phase: P2-P3
size: L
critical: true
blocked_on: 
---
# WS-04 — Towers & build

## Scope / DoD
**Scope.** ADFStructure/Tower/Trap/Barricade, UDFTowerRigComponent, UDFTargetingComponent with real LOS + registered blockers, kinds Bolt/Mortar(indirect)/Aura/Flak/Tesla/Beam/Barricade/Support, stage attachment, sell/upgrade/breakpoints, UDFBuildSubsystem, tower ownership.

**Definition of done.** Every kind fires at a test enemy; DF.Func.Tower.Aim/AirTracking/Rounds/LosBlockedByTerrain/IndirectOverRidge; stage cumulativeness; refusals as messages.

**Spec.** A1 towers, B§1.4, §3.2 (towers) (in `unreal/PLAN/PROGRAMME.md`). Size L, phase P2-P3.

## Contracts I consume
- C2
- C4
- C5
- C6
- C8

## Contracts / interfaces I provide
- C9
- UDFBuildSubsystem

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-25 · session-01HszbJQ-cloud (a Linux cloud session, no engine; claimed at the owner's request) · **written, NOT BUILT**, on `claude/happy-babbage-t6qrhw`: the tower **logic core**, `Source/DFTowers/Public/Towers/DFTowerMath.h` — pure functions ported from `TowerMath.cs` and the tower half of `Step.cs`, FP-pinned like the wave plan. PathFactor (DetMath.PowInt), MaxLevel, RecipeFor, RangePathIndex / BaseRange / Range (fog factor on top, exempt ids) / RangeAfterUpgrade (the upgrade panel's preview), EffectiveDamage, EffectiveRate (× buff), BeamRamp (ramp/peak paths, dials passed in); BuildCost (Forge discount truncated as the sim casts), CheckPlacement (socket tag / trapSocket / occupied / wouldSeal before money / funds, in the sim's order), QuoteUpgrade (unknownPath / maxLevel / insufficientFunds / insufficientScrap, breakpoint recipes at 4/7/10), SellRefund; PickTarget (Step.cs:1622 exactly: layer, burrowed, stealth needs detection, range and min range, sight, lowest RemainingToCore by strict `<` — no special cases, per INT's 2026-09-21 note), IsSightBlockedByBody (the Monolith geometry, cm), NextChainTarget (Tesla hop), AcquisitionDelaySeconds (night, marked exempt). The refusal FNames spell Step.cs's strings. Tests `DF.Unit.Tower.{PathFactorsMatchSim, RangeMatchesSim, BeamRamp, BuildRulesMatchSim, UpgradeQuoteMatchesSim, PickTargetMatchesSim, SightChainAndNight, ContentTowersMatchSim}` — expected values are the sim's float32 arithmetic reproduced step by step; the last runs on the committed DT_Towers / DT_Conditions. **Next** (needs the Mac/editor for the actor half): `UDFTargetingComponent` (gathers candidates: enemy positions, Detection status, `RemainingToCore`, sight traces against terrain + registered blockers + `IsSightBlockedByBody`, then `PickTarget`), `ADFStructure/ADFTower` with replicated def / socket / path levels / hp / target id / owner, the fire loop per kind on the host, `UDFBuildSubsystem` (place/upgrade/sell RPCs through WS-28's `ADFPlayerController`, refusals via `Client_Refused`, money/scrap through WS-06's economy component), C9 rig. No editor slot taken.

- 2026-09-25 · session-01HszbJQ-cloud · **The actor half, written, NOT BUILT** (Windows is now the build machine; the owner's call). `IDFTargetable` + `UDFTargetRegistry` in DFCore (how a tower reads an enemy across the sibling-module line; WS-05 implements); `UDFMessageBus::BroadcastTeam` + `SetTeamRelay` (C15 append: a lower module reaches clients through DFMatch's relay without naming it); `DFTowerMath` gains shot flight (`AdvanceShot`, Step.cs StepTowerProjectiles), `SplashFactor`, and a lazy-sight `PickTarget` that traces only candidates that could win (same answer as the sim's order, proved in a test). `UDFTargetingComponent`: gathers candidates from the registry, detection/mark from the body's status component, sight = DF_Sight trace from the 1.6 m muzzle + the Monolith body rule, night acquisition bookkeeping. `ADFTower`: replicated def/socket/structure id/owner seat/path levels/hp/active condition/current target; host `StepTower` = FireTowers + StepTowerProjectiles per kind (Bolt/Mortar/Flak rounds with cooldown and acquisition delay, homing, splash with falloff, vanishing when the target dies; Tesla strike + chain; Beam ramp; Aura statuses; Barricade/Support none); damage through `UDFGE_Damage` with a tower `UDFDamageContext`, then Applies via `UDFStatusComponent`; `TowerFired` / `ProjectileLanded` / `BeamHeld` via `BroadcastTeam`. Tests `DF.Unit.Tower.{ShotFlightAndSplash, SightOnlyTracedForContenders, TargetingOverRegistry, LanceFiresAndLands}` (a test dummy stands in for the enemy; the fire-loop timeline was checked with a numeric replica). **Next:** `UDFBuildSubsystem` (place/upgrade/sell via WS-28's controller RPCs, `CheckPlacement`/`QuoteUpgrade`, money and scrap through WS-06's economy component, barricade `WouldSeal` from WS-09's lane graph), structure damage + destruction from sieging enemies, the C9 rig (yaw/pitch from `CurrentTarget`), `DF.Func.Tower.*` in a real level on Windows.

## From INT (2026-09-21) — two things targeting must know before you write `UDFTargetingComponent`
1. **`RemainingToCore` is an opaque sort key, not a distance.** A walker with nowhere to go returns
   `TNumericLimits<float>::Max()` so it sorts last (sim parity, `World.cs:116`). Never do arithmetic on
   it — float max plus anything is float max, and the difference of two of them is zero. Compare only.
2. **A sieging enemy is NOT stranded and must be targeted first.** It reports a genuinely small distance
   because siege routing sees through the shut edge it is chewing (`Step.cs:1215-1224`). The sim gets
   "the Ram breaking your barricade is the priority target" for free from one comparison, with no special
   case — do not add one. See `CONTRACTS/lanegraph.md`, "A stranded walker sorts last".
Targeting otherwise follows `Step.cs:1620-1653` exactly: layer filter, not burrowed, stealth needs the
Detection channel, range and min-range, sight, then lowest `RemainingToCore` wins.

- 2026-09-25 · session-01HszbJQ-cloud · **Build service, written, NOT BUILT** (while the owner's Windows engine installed). Audit of the unbuilt WS-04 C++ against the project's own APIs and the patterns already compiled: every call resolves (tags, rows, dials, damage context, status, message structs, test utils); one change, `extern DFTOWERS_API` (the conventional order) for the reason names. New: **`UDFBuildSubsystem`** (`Towers/DFBuildSubsystem.h`, world subsystem): `PlaceTower` / `UpgradeTower` / `SellTower` port Step.cs `ApplyPlaceTower` / `ApplyUpgradeTower` / `ApplySellTower` through `DFTowerMath` (sim order: unknownSocket, unknownTower, tag/trapSocket, occupied, wouldSeal, funds; unknownPath, maxLevel, funds, scrap), spawns `ADFTower` on the pad top, announces `TowerPlaced` / `TowerUpgraded` (NewLevel = the level the player sees; bBreakpoint at 4/7/10) / `TowerSold` with `BroadcastTeam`, and returns refusals for the issuer. Money and scrap move through the new **`IDFTeamWallet`** seam (`DFCore/Public/Economy/DFEconomySeams.h`, contract-append; WS-06 implements; found on the game state or its components, test override). **`ADFPlayerController`** gains `Server_PlaceTower` / `Server_UpgradeTower` / `Server_SellTower` (the header already says domains add their RPCs there), refusals via `Client_Refused` as `BuildRejected` / `UpgradeRejected`. Tests `DF.Unit.Tower.BuildPlaceInSimOrder`, `BuildUpgradeAndBreakpointScrap`, `BuildSellRefundsSeventyPercent`, `BuildWithoutWalletRefuses` (sockets spawned in a test world, a test wallet). **Gaps, each owned elsewhere:** traps (ADFTrap) are refused as unknownTower with a warning; the Forge discount waits on WS-07's faction (the RPC passes false); wouldSeal counts standing barricades only until WS-09's lane state reports operated gates and mutables; edge refresh after a barricade is WS-09's, from the messages. **Next:** build on Windows and run `DF.Unit.Tower`; structure damage/destruction; the C9 rig; `DF.Func.Tower.*`.
- 2026-09-25 · session-01HszbJQ-cloud · **Structure damage, destruction and repair, written, NOT BUILT.** `IDFStructure` + `UDFStructureRegistry` in DFCore (contract-append; enemies and heroes are sibling modules): the sim's siege pick (`Siege` / `FindSiegeTarget`, Step.cs SiegeStructures) and repair pick (`RepairNearby` / `FindRepairTarget`, ApplyPlayerMelee) ported once, for WS-05 and WS-03 to call (notes left in their files). `ADFTower` implements it: `ApplySiegeDamage` (hp may go below 0 when several rams hit in a frame; StructureDamaged is a host-local broadcast and clients re-broadcast from `OnRep_Hp`, not a reliable RPC per tick), `ApplyRepair` (capped; `StructureRepaired` to the team, a new C15 append), barricade `BarricadeState` intact/damaged/broken on change, `IsBroken` silences the weapon at once. Towers now tick in `TG_PostPhysics` (after the enemies: Step.cs moves, sieges, then fires). `UDFBuildSubsystem` is a `UTickableWorldSubsystem` whose end-of-frame `RemoveBroken` removes what siege broke (`TowerDestroyed`, State `breached` for a barricade) — so every hit in the frame lands first, as the sim removes after every enemy has swung. Tests: `DF.Unit.Tower.SiegeBreaksThenRemovesAtFrameEnd`, `BrokenTowerDoesNotFire`, `RepairFirstHurtInReach`. **Next:** the C9 rig; `DF.Func.Tower.*`.
- 2026-09-25 · session-01HszbJQ-cloud · **C9 rig, written, NOT BUILT.** `Rig/DFTowerRig.h`: `FDFTowerRigLimits` (the manifest's rig block; pitchSign re-read for Unreal's + up, see rig.md) and pure math — `DesiredAngles`, `ClampToLimits`, `Slew` (traverse/elevate rates, short way across the ±180 seam when unlimited, never across it when limited, settled within 0.5°), `ManifestLimitsFor` (lance/nova/skywatch/filament until DA_Tower_<id> exists). `Rig/DFTowerDefinition.h`: `UDFTowerDefinition` (DA_Tower_<id>: Foot/Yaw/Pitch/Spin meshes, Rig, stage sets per path). `Rig/DFTowerRigComponent.h`: builds Foot→Yaw→Pitch→Muzzle or Foot→Spin from the definition (no meshes without one), `AimAt` / `Idle` / `StartSearchWobble` (night), spin 25/110 °/s, cumulative stage modules (`SetPathLevels`: level N shows S02..SN, fewer purchases take modules off). `ADFTower` owns a `Rig`: configured on init (host) and `OnRep_DefId` (clients), stages on upgrade / `OnRep_PathLevels`, driven every frame on every machine that draws from the replicated `CurrentTarget` (search wobble when the target changes under an acquisition-delay condition). rig.md's canonical path now names the real files. Tests: `DF.Unit.Tower.RigAngles`, `RigSlewRatesSeamAndLimits`, `RigTurretSpinAndSearch`, `RigStagesAreCumulative`. **Next:** build on Windows; `DF.Func.Tower.*` (needs a map and enemies: WS-05's `ADFEnemy`); visual rounds from the Muzzle (0.2 s lead, rig.md "Firing") with WS-19's VFX.
