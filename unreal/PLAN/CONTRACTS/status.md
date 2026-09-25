# C5 — Status channels and reactions

**Canonical:** `unreal/DeepField/Source/DFGameplay/Public/Status/UDFStatusComponent.h`, `UDFReactionResolver.h`, and `Content/DF/Gameplay/GE/GE_Status_<id>`. **Owner:** WS-02. **Rule:** R. Semantics are those of `sim/Sim.Core/Content/Statuses.cs` and `Step.cs:1070-1140`, reproduced by `DF.Unit.Status.*`.

- 8 channels: Movement, Thermal, Toxin, Defense, Vulnerability, Control, Tether, Detection (`DF.Status.Channel.*`). Each status is one `GE_Status_<id>` granting `DF.Status.<Id>` and its channel tag, duration from the row.
- `UDFStatusComponent::Apply(StatusTag, SourceActor, MagnitudeOverride = -1)`: one active status per channel; **same id refreshes duration; different id → strongest magnitude wins, the weaker is dropped; never stacks.** Magnitude per channel: Movement `1 − SpeedFactor`; Thermal/Toxin dps; Vulnerability `factor − 1`; Defense `−ArmorDelta`; Control duration; Tether pull speed; Detection 1.
- Gates: burn rejected on a shielded target; hard control rejected if `CcResist ≥ 1`; Tether rejected on `MassKg ≥ 6000` (Mass ≥ 6), skater, boss.
- Reactions (`UDFReactionResolver`, table `DT_Reactions`): when a status lands and the partner status is active on the target, fire the reaction: burst `BurstFraction × MaxHealth`, optionally `EmitStatus` written straight to its slot, optionally a physics event. **Reaction outputs are never re-scanned (reactions cannot chain).** Server-only; broadcast as `GameplayCue.DF.Reaction.<id>` and `DF.Message.ReactionTriggered`.
- Heroes may carry only Movement statuses and `Stagger` (Control-like, 0.25 s, no cc-resist), never Thermal/Toxin.
- Conditions multiply channel *durations* (`ConditionRow.ChannelDurationFactors`), never touch status ids.
- Replication: `ADFEnemy::StatusSlots[8] {StatusTag, EndTimeServer}` (push-model); clients derive tint/overhead/VFX from the slots only.
- Time left (INT rulings, 2026-09-21 and R13 2026-09-25): `EndTimeServer` is a **server** timestamp; `UDFStatusComponent::TimeRemaining()` subtracts the shared server clock (`AGameStateBase::GetServerWorldTimeSeconds`). A view reads `TryGetTimeRemaining()`, which returns false on a client whose game state has not replicated yet — the view then shows the status icon **without** its countdown (never a zero, never a full ring).
- Cues: `GameplayCue.DF.Status.<id>.Applied/Removed/Tick` — VFX (C10) and audio (C11) subscribe; the tint component (C8) is driven by the status component directly on both sides.
