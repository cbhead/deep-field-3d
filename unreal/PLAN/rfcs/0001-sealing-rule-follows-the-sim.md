# RFC-0001 — Rule 11 (sealing) follows the sim's harness reading; itineraries carry node ids

Status: **Landed** 2026-09-19 (INT ruling; contracts `map-authoring-3d.md` rule 11, `lanegraph.md` C6).

## Motivation
WS-09's port of `LaneGraph.FromRoutes` and its validator surfaced two places where the contract text and the sim disagreed. ADR-0005 makes the sim the spec.

## Changes
1. `map-authoring-3d.md` rule 11 read "closing any closable set never seals". The sim (`Sim.Harness` gate 22c) lets Switchyard's b1+b2 both be closable and has the runtime **refuse the last closure** with `wouldSeal`. The validator therefore *reports* sealing combinations and *fails* only single-edge seals and reachability non-monotonicity. `DF.Unit.LaneGraph.SwitchyardJunctions` asserting that closing both seals is correct.
2. `lanegraph.md` C6 said `Itineraries[].Via` holds edge ids; the sim's `ItineraryDef` holds node ids and the port follows it. Text corrected; `EdgesOf()` resolves node pairs.

## Dependents
WS-09 (validator, tests), WS-04 (barricade gating reads `WouldSeal`), WS-24 (mutables). No code changes required.
