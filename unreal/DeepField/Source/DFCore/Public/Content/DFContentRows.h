#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "DFContentRows.generated.h"

// C2 — DataTable row structs, one per table in unreal/content/json (CONTRACTS/content-rows.md).
// Field names are the C# record fields in PascalCase; the importer maps camelCase JSON keys onto
// them. Vectors arrive in metres Y-up and are converted to cm Z-up +X-forward at import.
// DF.Content.RoundTrip re-exports these tables and diffs against the JSON, so a field here that
// the JSON does not carry (or vice versa) is a failing test, not a silent default.

UENUM(BlueprintType)
enum class EDFScrapType : uint8 { Alloy, Flux, Plating, Gravium, Primecore };

UENUM(BlueprintType)
enum class EDFEnemyLayer : uint8 { Ground, Air };

UENUM(BlueprintType)
enum class EDFTowerKind : uint8 { Bolt, Mortar, Aura, Flak, Tesla, Beam, Barricade, Support };

UENUM(BlueprintType)
enum class EDFStatusChannel : uint8 { Movement, Thermal, Toxin, Defense, Vulnerability, Control, Tether, Detection };

UENUM(BlueprintType)
enum class EDFAttachmentSlot : uint8 { Barrel, Muzzle, Optic, Magazine, Stock, Underbarrel, Infusion };

UENUM(BlueprintType)
enum class EDFMeleeSlot : uint8 { Edge, Grip, CoreInfusion, Counterweight, ChargeCell };

UENUM(BlueprintType)
enum class EDFSocketTag : uint8 { Ground, Wall, Trap, Barricade };

/** A scrap price or yield: type -> count. */
USTRUCT(BlueprintType)
struct DFCORE_API FDFScrapBundle
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<EDFScrapType, int32> Amounts;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFUpgradePathRow
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float PerLevelFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<int32> LevelCosts;                       // nine purchases, L1 -> L10
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<int32, FDFScrapBundle> BreakpointRecipes;    // keyed 4 / 7 / 10
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<int32, FName> Breakpoint;                   // level -> mechanic id (B§1.4), optional
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFTowerRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EDFTowerKind Kind = EDFTowerKind::Bolt;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Cost = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RangeMeters = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MinRangeMeters = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Damage = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ShotsPerSecond = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ProjectileSpeed = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ProjectileGravity = 0.f;                     // >0 = ballistic (Nova, flak)
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SplashRadius = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SplashFalloff = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 ChainJumps = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ChainRange = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ChainFalloff = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StructureHp = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> Applies;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<EDFEnemyLayer> TargetLayers;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FDFUpgradePathRow> UpgradePaths;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float BuffShotsPerSecond = 0.f;                    // Support (Overclock)
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 FeedCapacity = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bIndirect = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor EnergyHue = FLinearColor::White;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFTrapRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Cost = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TriggerRadius = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Charges = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RearmSeconds = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Damage = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Applies;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float KnockbackMeters = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FDFScrapBundle ScrapCost;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFWeakPointRow
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Zone;                                       // physics-asset body / socket
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Factor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Condition;                                  // e.g. shieldDown, surfaced, unphased, plateRemoved
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFEnemyRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Hp = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SpeedMetersPerSec = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Bounty = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 LeakDamage = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EDFEnemyLayer Layer = EDFEnemyLayer::Ground;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ContactDamage = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ScatterWidth = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bBlocksSight = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float FrontArmorArcDegrees = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float FrontArmorFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RearWeakFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Shield = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float FlatArmor = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Mass = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bBurrower = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SplitInto;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 SplitCount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FDFScrapBundle ScrapYield;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bStealth = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StealthSpeedBonus = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HealPerSecond = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HealRadius = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StructureDps = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StructureReach = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float EnrageBelowHpFraction = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float EnrageSpeedFactor = 1.f;
	// Additions for the Unreal build (B§1.5, B§2.3, B§2.5); all default to "no effect".
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FDFWeakPointRow> WeakPoints;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SlopeSpeedUp = 0.85f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SlopeSpeedDown = 1.1f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float LeapRise = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float PhaseSeconds = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TetherRange = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bEliteEligible = true;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFStatusRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EDFStatusChannel Channel = EDFStatusChannel::Movement;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SpeedFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float DamagePerSecond = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float DamageTakenFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ArmorDelta = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bHardControl = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float PullSpeed = 0.f;                              // Tether
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MaxDurationSeconds = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bIgnoresArmor = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bIgnoresShield = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CcResistFill = 1.f;                           // Tether fills at 0.5

	/** Magnitude per channel, exactly as Statuses.cs computes it. */
	float Magnitude() const;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFReactionRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName StatusA;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName StatusB;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float BurstFraction = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName EmitStatus;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float PullMeters = 0.f;                             // implosion
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float PullRadius = 0.f;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFFactionRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName AbilityId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CooldownSeconds = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RadiusMeters = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float DurationSeconds = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Magnitude = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Passive;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Signature;                                  // level-5 mechanic id
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFWeaponRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FDFScrapBundle Recipe;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Damage = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ShotsPerSecond = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RangeMeters = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> Applies;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MagazineSize = 12;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ReloadSeconds = 1.6f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bAutomatic = true;
	// Unreal additions (B§1.2), defaults = the Godot feel (no spread, no recoil).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HipSpreadDeg = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float AdsSpreadDeg = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float BloomPerShot = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RecoverPerSecond = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RecoilKick = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ProjectileSpeed = 0.f;                        // 0 = hitscan
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ProjectileGravity = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ProjectileFuse = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HeadshotFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 PelletCount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 BurstCount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ChargeSeconds = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ChargeDamageMax = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Pierce = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float KnockbackMeters = 0.f;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFMeleeRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FDFScrapBundle Recipe;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Damage = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SwingsPerSecond = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ReachMeters = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ArcDegrees = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float KnockbackMeters = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> Applies;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<int32> MasteryCosts;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SwingKind;                                  // slash / chop / thrust / rev / combo
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RevDps = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 ComboHeavyEvery = 0;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFAttachmentRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EDFAttachmentSlot Slot = EDFAttachmentSlot::Barrel;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float DamageFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RateFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RangeFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SpreadFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RecoilFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float AdsSpeedFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MagazineFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Applies;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FDFScrapBundle Recipe;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFMeleeAttachmentRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EDFMeleeSlot Slot = EDFMeleeSlot::Edge;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float DamageFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SpeedFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ReachFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Applies;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FDFScrapBundle Recipe;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFAmmoRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float DamageFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bIgnoresFlatArmor = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float UnarmoredBonusFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Applies;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FDFScrapBundle Recipe;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFConditionRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Effect;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TowerRangeFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> RangeExemptTowerIds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float AcquisitionDelaySeconds = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bAcquisitionDelayExemptsMarked = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HeroRangeFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StealthWeightFactor = 1.f;                     // spelled as conditions.json / Conditions.cs spell it
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<EDFStatusChannel, float> ChannelDurationFactors;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName HazardKind;                                 // lightning etc. (B§2.10)
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HazardIntervalMin = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HazardIntervalMax = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HazardDamage = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName HazardApplies;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HazardRadius = 0.f;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFVehicleRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Seats = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float BoardRadiusMeters = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MaxSpeedMetersPerSec = 14.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Hp = 400.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MaxGradePercent = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RamDamageBase = 40.f;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFSectorRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Order = -1;                                  // -1 = not in the campaign (test fixtures)
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 TotalWaves = 10;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D FieldMeters = FVector2D(110.f, 80.f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<int32, FName> ConditionSchedule;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> Vehicles;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bBossRated = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Lesson;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> Tiers;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString LegacyLevelFile;
};

USTRUCT(BlueprintType)
struct DFCORE_API FDFWaveGroupRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 WaveIndex = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName EnemyId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Count = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 SpacingTicks = 30;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 StartDelayTicks = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName RouteId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Elite;                                      // optional modifier id (campaign-authored)
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bBoss = false;
};

/** A boss phase's one attack (B§2.4: Sweep, Slam, Stomp). IntervalSeconds 0 = the phase has none. */
USTRUCT(BlueprintType)
struct DFCORE_API FDFBossAttackRow
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float IntervalSeconds = 0.f;                       // landing to landing
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Radius = 0.f;                                // metres
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Damage = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float KnockbackMeters = 0.f;                       // before ÷ Mass
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StaggerSeconds = 0.f;                        // heroes (B§1.6)
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TelegraphSeconds = 0.f;                      // warning before it lands
};

/** A boss phase's brood vent (B§2.4 P2: 4 motes every 15 s, at most 12 alive). Count 0 = none. */
USTRUCT(BlueprintType)
struct DFCORE_API FDFBossSpawnRow
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName EnemyId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Count = 0;                                   // per vent
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MaxAlive = 0;                                // this boss's brood, not the wave
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float IntervalSeconds = 0.f;
};

/** What the boss does to structures this phase: FDFEnemyRow's StructureDps / StructureReach, per phase. */
USTRUCT(BlueprintType)
struct DFCORE_API FDFBossSiegeRow
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Dps = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Reach = 0.f;                                 // metres
};

/** One row per boss phase (table `boss`, WS-19), in order from full hp down. HpFrom/HpTo are fractions
 *  of the body's max hp; phases are contiguous, the first starts at 1 and the last ends at 0. */
USTRUCT(BlueprintType)
struct DFCORE_API FDFBossPhaseRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HpFrom = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HpTo = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float FrontArcDeg = 0.f;                           // full arc, as FrontArmorArcDegrees
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float FrontArcFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float FlatArmor = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SpeedFactor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FDFBossAttackRow Attack;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FDFBossSpawnRow Spawns;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FDFBossSiegeRow Siege;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float WeakPointFactor = 1.f;                       // an exposed vent
	/** The armour plates are on in this phase. Entering a phase without them sheds every plate still on (B§2.4 P2,
	 *  "plates drop"); once off they stay off. Not in the contract's field list — a C2 append with a no-plates default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bPlatesHeld = false;
};

/** Every dial in Balance.cs (52 today) plus the Unreal-era knobs, one row named "default".
 *  Kept as a name->float map so a new dial in the JSON is not a schema change; typed accessors
 *  live on UDFContentSubsystem (Balance(FName) with a checked default). */
USTRUCT(BlueprintType)
struct DFCORE_API FDFBalanceRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, float> Dials;
};
