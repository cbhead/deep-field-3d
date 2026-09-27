---
ws: 14
slug: vfx
title: VFX (Niagara)
state: active
owner: session-gpu-box-2026-09-26
claimed_at: 2026-09-27T18:35:07Z
lease_expires: 2026-09-28T18:35:07Z
branch: ws/14-vfx/placeholder-combat-cues
last_commit: 
editor_heavy: true
phase: P2-P5
size: XL
critical: false
blocked_on: 
---
# WS-14 — VFX (Niagara)

## Scope / DoD
**Scope.** ~110 systems on ~12 emitter templates, DT_VFX variants, held-effect registry, decal pool (256), PPM_RevealPulse/HeatShimmer/Downed, reduce-flash.

**Definition of done.** DF.Vfx.EveryCueDraws; corrode/cryofield/revealpulse/teleport-burst/implosion exist.

**Spec.** C10, C§5 Niagara list (in `unreal/PLAN/PROGRAMME.md`). Size XL, phase P2-P5.

## Contracts I consume
- C1
- C5
- C8

## Contracts / interfaces I provide
- C10

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-27 · session-gpu-box-2026-09-26 · **Claimed for placeholder combat cues** (next step after the first playable, at the owner's request): a DFVfx subsystem that draws every tower kind's shots on every machine from the relayed DF.Message.TowerFired / ProjectileLanded / BeamHeld (rounds, mortar arcs, tesla arcs, beams, impacts) with engine basic shapes (DFShapeLook) until the Niagara systems, replacing the host-only rounds view in ADFTower; branch `ws/14-vfx/placeholder-combat-cues`.
- 2026-09-27 · session-gpu-box-2026-09-26 (agent) · **Placeholder combat cues, on `ws/14-vfx/placeholder-combat-cues`; built with no warnings, gate 209 passed.** `UDFCombatCueSubsystem` (`DFVfx/Public/Cues/DFCombatCueSubsystem.h`), a tickable world subsystem in Game/PIE worlds only: it starts itself at world BeginPlay unless on a dedicated server, under -nullrhi or in an automation test's world (a test calls `StartDrawing`). It hears TowerFired / ProjectileLanded / BeamHeld on the local bus (ADFEventRelay re-broadcasts them on clients) and draws pooled engine basic shapes on `DFShapeLook::Glow`, in the firing tower's placeholder barrel colour (`DFTowerRig::PlaceholderEnergy`, new in DFTowers). Bolt/Flak: a ball from Origin to the aim point (Impact + 0.8 m, where the sim aims) at the row's ProjectileSpeed, then a flash; Mortar (by kind, or bIndirect): the same on a lob (apex 0.3 x distance, 1.5-9 m) and a disc of the row's SplashRadius on the ground; a ProjectileLanded for a round still in flight (same StructureId/TargetId) lands it there. Tesla: a 4-cube jittered bolt for 0.12 s (reshaped once) and a flash, per strike and, with ADFTower now announcing them, per chain hop. Beam: every frame a cylinder from each Beam tower's rig muzzle to its replicated CurrentTarget's aim point, 5-14 cm and 2.5x-10x glow with Heat; towers are tracked from UWorld's actor-spawned hook plus one scan at start, so nothing iterates actors per frame; BeamHeld sparks on lock-on. Parts are hidden and reused, and the pool grows only past the most ever shown at once. ADFTower's host-only RoundsView/DrawRounds is gone. DFVfx now depends on DFTowers (`modules.json` + `DFVfx.Build.cs`). Tests: `DF.Unit.Vfx.RoundFliesAtRowSpeed`, `.MortarLobsAndSplashes`, `.TeslaArcIsBrief`, `.BeamHoldsItsTarget`, `.PartsArePooled`, `.OnlyInGameWorldsThatDraw`, `.IgnoresShotsWithoutARow`. **Left:** nobody has looked at it yet; tune sizes, glow gains and the lob by eye on L_Testlane / L_Foundry, with a client, then C10's Niagara systems (`UDFVfxSubsystem`, DT_VFX) replace it. ADFEventRelay's multicast is Reliable and each shot (and each tesla hop) is one multicast: fine at today's fire rates (about one per tower per 0.6 s), a candidate for an unreliable cosmetic channel later.
