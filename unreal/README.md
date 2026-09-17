# unreal/ — Deep Field 3D on Unreal Engine 5.8

The Unreal rebuild lives here beside the frozen Godot client (`../game/`) and the C# sim (`../sim/`, now the written spec).

- **Start here:** [PLAN/README.md](PLAN/README.md) — how sessions claim work and stay aware of each other — and [PLAN/PROGRAMME.md](PLAN/PROGRAMME.md), the plan of record.
- `PLAN/` — ledger: status, ownership, decisions, contracts, workstreams, RFCs, digests.
- `content/` — text source of truth for content numbers (`json/`), level files (`levels/`), terrain specs (`terrain/`) and their schemas.
- `Build/` — ledger scripts (`plan-status.py`, `plan-scaffold.py`, `plan-claim.py`), test wrapper, CI checks.
- `DeepField/` — the `.uproject` (created by WS-00 once the external SSD is attached; see PROGRAMME.md §4.1 P0).
- `.gitattributes` (here) and `../.lfsconfig` (repo root) — Git LFS rules for binaries (ADR-0010); run `git lfs install` once per clone.

Tooling for the art and terrain lanes is under `../tools/ue-bridge/`; the one-time content bootstrap under `../tools/content-export/`.
