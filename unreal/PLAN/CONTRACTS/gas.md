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

Damage execution (`UDFDamageExecution`) order: source factors (`DamageFactor`, ammo factor, weak-point factor, Pack-a-Punch) → target front-arc/rear factor (from `EnemyRow.FrontArmorArcDegrees`, hit direction) → flat armor (unless `IgnoresFlatArmor`) → vulnerability (`DamageTakenFactor`, mark) → shield then health (unless `IgnoresShield`). Every number comes from the content rows; the execution has no literals.

Ability base classes: `UDFGameplayAbility` (net execution policy default `LocalPredicted`; `ActivationOwnedTags`; cost/cooldown from rows via `DF.SetByCaller.*`), `UDFAbilitySet` (grants abilities/effects/attribute sets to an ASC; copied from Lyra's pattern).
