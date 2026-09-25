# RFC-0003 — C6 `ClosableBy` is two fields, and levers close edges

Status: **Landed-as-built** 2026-09-25 (INT ruling R9, `needs-int-rulings-2026-09-25.md`; contract `lanegraph.md` C6).

## Motivation
WS-09's C6 implementation (`DFLaneGraphTypes.h`, landed with `ws/09-world/lanegraph-importer`) differs from
the contract text in one shape and several additions. Nothing outside WS-09 reads these fields yet, so the
contract follows the code rather than the code following the contract — but a change of shape is an RFC,
not an append, so it is recorded here.

## Changes
1. **Shape (the RFC part).** C6 said `Edges[].ClosableBy: SocketId|MutableId|None` — one field holding an id.
   The asset carries `ClosableBy: EDFLaneClosableBy {None, Socket, Mutable, Lever}` **plus** `ClosableById`.
   Two fields because the id alone cannot say what kind of thing it names, and a lever id, a socket id and a
   mutable id live in different tables.
2. **Levers close edges.** The contract never listed operated gates as closers, though `OperatedGates[]` is in
   C6 and the sim's operated gates shut edges (`World.GateCooldowns`, `Step.cs` `UpdateWaves`). `Lever` is the
   closer kind for an `OperatedGates[].Id`. `ClosableEdges()`, `WouldSeal` and the validator's sealing rule
   (RFC-0001) already treat all three kinds alike.
3. **Appends recorded with it** (optional, defaulted — they would have been `contract-append`s on their own):
   `Traversal[].Position` and `.Label`; `Nodes[].Layer` (already in the text); `NearMisses[]` (the builder's
   report, editor-visible only); `ADFLaneGraphInfo`, the one actor in `L_<Map>_Gameplay` that references the
   asset.
4. **The canonical line** named `tools/ue-bridge/ue/build_level.py` as the producer; the producer is the
   `-run=DFLevelImport` commandlet. Corrected.

## Dependents
WS-04 (barricade gating reads `WouldSeal`, unchanged), WS-24 (mutables close edges through `Mutable`),
WS-09's lane state component (ADR-0023) replicates `State` per edge, unchanged. No code change.
