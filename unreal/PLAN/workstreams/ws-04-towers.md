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

