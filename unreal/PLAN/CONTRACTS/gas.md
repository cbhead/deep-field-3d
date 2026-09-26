# C4 — GAS attribute sets

**Canonical:** `unreal/DeepField/Source/DFGameplay/Public/Attributes/*.h`. **Owner:** WS-02. **Rule:** R; adding a new attribute that no existing execution reads is A.

| Set | Attributes | Notes |
|---|---|---|
| `UDFHealthSet` | `Health, MaxHealth, Shield, MaxShield, ShieldRegen, FlatArmor, DamageTakenFactor, IncomingDamage (meta), IncomingHeal (meta)` | damage execution writes `IncomingDamage`; shield absorbs before health; burn blocked while `Shield > 0`; poison bypasses armor and shield |
| `UDFMovementSet` | `BaseSpeed, SpeedFactor` | statuses on the Movement channel write `SpeedFactor`; slope factors are applied by the movement component, not here |
| `UDFCombatSet` | `DamageFactor, RateFactor, RangeFactor, ChilledBonus, WeakPointBonus` | faction passives and Overclock feed write here |
| `UDFAmmoSet` | `Magazine, MagazineSize, ReloadSeconds` | predicted on the client for fire/reload |
| `UDFControlSet` | `CcResist, CcResistFill, CcResistDecay` | fill 0.45/s while controlled, decay 0.125/s; Tether fills at ×0.5 |
| `UDFStructureSet` | `StructureHp, StructureMaxHp` | towers, traps, barricade, walls, containers, vehicles |
| `UDFHeroSet` | `RegenPerSecond, RegenDelay, BleedoutSeconds` | |

Damage execution (`UDFDamageExecution`) order, **as `sim/Sim.Core/Step.cs:2020-2060` orders it** — the sim is the spec and this text has been corrected to match it (RFC-0002): source factors (`DamageFactor`, ammo factor, weak-point factor, Pack-a-Punch) → target front-arc/rear factor (`EnemyRow.FrontArmorArcDegrees`, hit direction) → **vulnerability (`DamageTakenFactor`, mark)** → **shred front-arc leak** (an arc-armored target under a Defense-channel status leaks ×1.35 regardless of aspect) → **flat armor** (unless `IgnoresFlatArmor`; the attribute already carries the shred delta), floored so a hit never drops below 0.5 → shield then health (unless `IgnoresShield`).

The order is load-bearing: mark **before** armor means the mark amplifies the hit and armor takes its flat bite out of the amplified number. 10 damage on a marked (×1.25) Ram with FlatArmor 2 is `max(0.5, 10 × 1.25 − 2)` = **10.5**, not `(10 − 2) × 1.25` = 10.0. Every armored hit differs by `DamageTakenFactor × FlatArmor`.

Numbers come from the content rows, with three exceptions that are `constexpr` defaults in `Source/DFGameplay/Public/Damage/DFDamageMath.h` until WS-27 adds their `balance.json` dials: `RearThresholdDegrees` 150 (`ContentTypes.cs:64`), `ShredFrontArcLeakFactor` 1.35 and `PostArmorDamageFloor` 0.5 (both `Step.cs` `Damage()`). Each carries its source line in a comment; do not add a fourth without a dial.

Ability base classes: `UDFGameplayAbility` (net execution policy default `LocalPredicted`; `ActivationOwnedTags`; cost/cooldown from rows via `DF.SetByCaller.*`), `UDFAbilitySet` (grants abilities/effects/attribute sets to an ASC; copied from Lyra's pattern).
