# C2 — Content row schema

**Canonical:** `unreal/DeepField/Source/DFCore/Public/Content/DFContentRows.h` (every row is a `USTRUCT(BlueprintType)` deriving `FTableRowBase`) and the JSON schemas in `unreal/content/schema/*.schema.json` (one per table). Two gates, and neither is the one the other looks like: `DF.Content.RoundTrip` pins the **DataTable** against the JSON, and `validate-content-json.py` pins the **JSON** against the schema. Nothing proves the header against the schema, so a field is authorable only once it is in that table's schema — ten tables currently carry header fields their schema still forbids (the vehicles row below is the worked example). **Owner:** WS-01. **Rule:** R; adding an *optional* field with a default is A.

Field names are the C# record field names from `sim/Sim.Core/Content/ContentTypes.cs` in PascalCase. Ids are the DataTable row name (`FName`) and are also resolved to a `FGameplayTag` at import (C1). Enums are strings in JSON and `UENUM(BlueprintType)` in C++. Scrap bundles are `TMap<EDFScrapType,int32>`. Vectors are `[x,y,z]` metres, Y-up in JSON; the importer converts to cm, Z-up, +X forward.

Tables and their rows (all in one header; one `DT_<Table>` each):

| Table | Row struct | Notes |
|---|---|---|
| towers | `FDFTowerRow` | `Kind, Cost, RangeMeters, MinRangeMeters, Damage, ShotsPerSecond, ProjectileSpeed, ProjectileGravity, SplashRadius, SplashFalloff, ChainJumps, ChainRange, ChainFalloff, StructureHp, Applies[], TargetLayers[], UpgradePaths[] {Id, PerLevelFactor, LevelCosts[], BreakpointRecipes{level→bundle}, Breakpoint{L4,L7,L10 mechanic ids}}, Support{BuffShotsPerSecond, FeedCapacity}, Indirect(bool), EnergyHue` |
| traps | `FDFTrapRow` | `Cost, TriggerRadius, Charges, RearmSeconds, Damage, Applies[], KnockbackMeters, Scrap` |
| enemies | `FDFEnemyRow` | every `EnemyDef` field + `WeakPoints[] {Zone, Factor, Condition}`, `Mass` (the sim's abstract mass, not kg — every `÷ Mass` rule reads this), `SlopeSpeedUp/Down`, `LeapRise`, `PhaseSeconds`, `TetherRange` |
| elites | `FDFEliteRow` | `RimColor, HpFactor, SpeedFactor, BountyFactor, ScrapFactor, ExtraScrap{}, MassFactor, FlatArmorDelta, Immunities[], Mechanic (enum), DebutSector, DebutWave, NotEligible[]` |
| boss | `FDFBossPhaseRow` | one row per phase: `HpFrom, HpTo, FrontArcDeg, FrontArcFactor, FlatArmor, SpeedFactor, Attack{Id, IntervalSeconds, Radius, Damage, KnockbackMeters, StaggerSeconds, TelegraphSeconds}, Spawns{EnemyId, Count, MaxAlive, IntervalSeconds}, Siege{Dps, Reach}, WeakPointFactor` |
| statuses | `FDFStatusRow` | `Channel, SpeedFactor, DamagePerSecond, DamageTakenFactor, ArmorDelta, HardControl, PullSpeed, MaxDurationSeconds, IgnoresArmor, IgnoresShield, CcResistFill` |
| reactions | `FDFReactionRow` | `StatusA, StatusB, BurstFraction, EmitStatus, PhysicsEvent{PullMeters, Radius}` |
| factions | `FDFFactionRow` | `AbilityId, CooldownSeconds, RadiusMeters, DurationSeconds, Magnitude, Passive, Signature (L5 mechanic id)` |
| combos | `FDFComboRow` | `FactionA, FactionB (or Any), Effect (enum), Magnitude, XP` |
| weapons | `FDFWeaponRow` | `Recipe{}, Damage, ShotsPerSecond, RangeMeters, Applies, Magazine, ReloadSeconds, Automatic, HipSpreadDeg, AdsSpreadDeg, BloomPerShot, RecoverPerSecond, RecoilKick, Projectile{Speed,Gravity,Fuse}, HeadshotFactor, PelletCount, BurstCount, ChargeSeconds, ChargeDamageMax, Pierce, KnockbackMeters` |
| melee | `FDFMeleeRow` | `Recipe{}, Damage, SwingsPerSecond, ReachMeters, ArcDegrees` (a half-angle, per `Melee.cs:36`)`, KnockbackMeters, Applies[], MasteryCosts[], SwingKind, RevDps, ComboHeavyEvery` |
| attachments | `FDFAttachmentRow` | `Slot, DamageFactor, RateFactor, RangeFactor, SpreadFactor, RecoilFactor, AdsSpeedFactor, MagazineFactor, Applies, Recipe{}` |
| meleeAttachments | `FDFMeleeAttachmentRow` | `Slot (EDFMeleeSlot), DamageFactor, SpeedFactor, ReachFactor, Applies, Recipe{}` — its own struct and its own table, **not** `FDFAttachmentRow` with a melee slot |
| ammo | `FDFAmmoRow` | `DamageFactor, IgnoresFlatArmor, UnarmoredBonus, Applies, Recipe{}` |
| chargecells | `FDFChargeCellRow` | `HeavyEffect (enum), Magnitude, Radius, Recipe{}` |
| balance | `FDFBalanceRow` (single row `default`) | every `Balance.cs` dial + new knobs (knockdown thresholds, ram formula, primecore drops, early-call fraction, elite chance/cap, spending cap, XP cap, slope speed factors, vehicle grades) |
| waves_<map> | `FDFWaveGroupRow` | `WaveIndex, EnemyId, Count, SpacingTicks, StartDelayTicks, RouteId, Elite (optional id), Boss (bool)` |
| conditions | `FDFConditionRow` | `TowerRangeFactor, RangeExempt[], AcquisitionDelay, ExemptsMarked, HeroRangeFactor, StealthWeightFactor, ChannelDurationFactors{}, Hazard{Kind, IntervalMin, IntervalMax, Damage, Applies, Radius}, World{} (map-specific toggles)` |
| maps | `FDFSectorRow` | `Order, TotalWaves, FieldMeters, ConditionSchedule{wave→condition}, Vehicles[], BossRated, Lesson, Tiers{}` |
| vehicles | `FDFVehicleRow` | `Seats, BoardRadiusMeters, MaxSpeedMetersPerSec`, and header-only until `vehicles.schema.json` is extended: `Hp, MaxGradePercent, RamDamageBase`. There is no `SurfaceFactors` field — the per-physmat grip/accel/top table is still unbuilt and needs an RFC (WS-08) |
| mutables | `FDFMutableRow` | per kind: cooldowns, damages, hp, yields (B§2.11) |

Runtime access is only through `UDFContentSubsystem` (`Tower(FName)`, `Enemy(FName)`, …) which joins the row with its Primary Data Asset binding (`DA_<Domain>_<id>`); `DF.Content.Bindings` fails if a row lacks a DA or a DA lacks a row.
