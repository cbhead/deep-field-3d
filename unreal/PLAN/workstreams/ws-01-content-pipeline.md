---
ws: 01
slug: content-pipeline
title: Content pipeline
state: unclaimed
owner: 
claimed_at: 
lease_expires: 
branch: 
last_commit: 
editor_heavy: false
phase: P1
size: M
critical: true
blocked_on: 
---
# WS-01 — Content pipeline

## Scope / DoD
**Scope.** tools/content-export (.NET), JSON schemas, DFContentPipeline commandlet (JSON→DT), DT_ContentRegistry, DF.Content.RoundTrip/Bindings/TagCoverage, palette + UI tokens import.

**Definition of done.** Every Sim.Core content table exported, imported, round-trips; registry audit fails on any id without a row/binding or any Placeholder=true asset in a cook.

**Spec.** PROGRAMME.md §3.1 C2–C3, ADR-0005 (in `unreal/PLAN/PROGRAMME.md`). Size M, phase P1.

## Contracts I consume
- C1
- C2
- C3

## Contracts / interfaces I provide
- C2
- C3

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
