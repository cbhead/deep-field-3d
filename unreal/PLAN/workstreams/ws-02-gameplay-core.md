---
ws: 02
slug: gameplay-core
title: Gameplay core (GAS)
state: active
owner: session-75b58b1b/agent-ws02
claimed_at: 2026-09-19T15:16:34Z
lease_expires: 2026-09-20T15:16:34Z
branch: ws/02-gameplay-core/gas
last_commit: 
editor_heavy: false
phase: P1-P2
size: L
critical: true
blocked_on: 
---
# WS-02 — Gameplay core (GAS)

## Scope / DoD
**Scope.** ASC, attribute sets, damage execution (front arc, flat armor, shield, poison bypass, weak-point factor), 8 status channels, reactions, cc-resist, faction passive GEs, ability bases, cue bases, UDFTintComponent.

**Definition of done.** Unit tests reproduce Statuses.cs semantics and damage formulas; L_Test_Status shows chill+burn → thermalShock 12% via cue.

**Spec.** PROGRAMME.md §3.1 C4–C5, Appendix A1 statuses, B§1.6 (in `unreal/PLAN/PROGRAMME.md`). Size L, phase P1-P2.

## Contracts I consume
- C1
- C2
- C8

## Contracts / interfaces I provide
- C4
- C5

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-21 · INT · **landing round 2 with one open minor, deliberately.** The review confirmed one
  finding and refuted four. `UDFStatusComponent::Now()` falls back to `World->GetTimeSeconds()` when the
  world has no game state, and that fallback is unconditional — so on a **joining client, in the window
  before the `AGameStateBase` channel opens**, a replicated `EndTimeServer` is subtracted from the
  client's own clock and `TimeRemaining` is wrong again, exactly as F2 was. It is narrow (one actor-
  channel window, cosmetic until something gameplay-facing reads it) and the fallback is *required* for
  the dev map and the unit-test world, which have no game state at all. Not landed-with-a-fix because
  the right behaviour on a client that cannot yet know the server clock is a display decision shared
  with WS-12 — "unknown" is not the same as "zero" or "full duration", and inventing that at a landing
  would be worse than leaving it named. **Fix in the next round**: make the fallback conditional on
  authority (or on `GetWorld()->GetNetMode() != NM_Client`), and decide with WS-12 what a client shows
  for a status whose clock it does not have yet. C4/C5 land now because six workstreams are waiting on
  them and this does not touch the rules.

