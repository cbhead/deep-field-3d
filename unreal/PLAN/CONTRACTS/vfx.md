# C10 — Niagara parameter contract

**Canonical:** every `NS_*` under `Content/DF/VFX/Niagara/<Category>/`, `DT_VFX` (per-cue rows), and `unreal/DeepField/Source/DFVfx/Public/DFVfxSubsystem.h` (no `U` in the filename; the name WS-14's implementation must use — it does not exist yet). **Owner:** WS-14. **Rule:** R.

**User parameters** (every DF system exposes exactly these, unused ones ignored): `User.Hue` (LinearColor from `DA_Palette`), `User.Intensity` (0–2), `User.Scale` (metres; status/reaction systems receive the target's `BodyHeight` and scale to it), `User.Held` (bool: loop vs burst), `User.Progress` (0–1 gauge), `User.Tier` (1–3), `User.Heat` (0–1), `User.Direction` (vector), `User.Target` (vector; beam end), `User.HopTarget` (vector), `User.Surface` (enum: Metal Concrete Dirt Grass Wood Water Enemy Snow Ice), `User.Wind` (from `MPC_Conditions`).

**Naming:** `NS_<Category>_<Name>[_<Verb>]`, categories `Tower Gun Melee Status Reaction Enemy Elite Boss Ability Player Pickup Wave Core Portal WarpGate Gate Mutable Weather Destruct Vehicle UI`. The full launch list is PROGRAMME.md C§5.

**Spawn points** are sockets (C7), never mesh searches: statuses at `status`, weak-point hits at `weak_n`, muzzle/eject at their sockets, overheads at `overhead`.

**Held effects** are looping systems keyed `(ActorId, CueTag)` in the `UDFVfxSubsystem` registry; the frame that still wants an effect asks for it again and `SweepHeld()` deactivates anything not re-asked (the retained-mode rule from `Vfx.cs`). Bursts are fire-and-forget. `GameplayCue.DF.*` notifies (`UDFGameplayCueNotify_Niagara`) are the only entry point from gameplay; `DT_VFX` maps cue tag → system + default parameters + variant rows (e.g. burn by source hue).

**Lights:** ≤1 Niagara light per burst; none on held statuses except burn/shock. **Reduce flashes:** every additive intensity is multiplied by `MPC_Player.ReduceFlashes ? 0.4 : 1`.

**Decals** spawned by VFX go through `BP_DecalPool` (cap 256/map) with `DM_*` materials (C8).

**Tests:** `DF.Vfx.EveryCueDraws` iterates every cue tag in `DT_VFX`, fires it in `L_Test_Vfx` with `-RenderOffscreen`, and fails on a missing system or a warning; `DF.Func.Vfx.BeamRamp` drives `Heat` 0→1 and checks emissive.
