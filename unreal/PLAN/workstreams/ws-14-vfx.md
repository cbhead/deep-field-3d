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
