namespace DeepField.Sim.Content;

public static class Towers
{
    // Towers have health from M4 on. Until the Ram there was nothing that could
    // damage a structure, so every tower carried StructureHp 0 and only the
    // Barricade had a number — which made the Ram a switchyard-only enemy with
    // exactly one target whose loss reshaped the whole match. Spreading health
    // across the roster turns that binary into attrition: losing a Lance costs
    // a lane for as long as it takes to rebuild, and losing the barricade is
    // still the expensive one.

    private static readonly IReadOnlyDictionary<ScrapType, int> NoRecipe =
        new Dictionary<ScrapType, int>();

    private static UpgradePathDef Damage(params int[] costs) => new(
        "damage", PerLevelFactor: 1.10f, LevelCosts: costs,
        BreakpointRecipe: new Dictionary<ScrapType, int> { [ScrapType.Alloy] = 8, [ScrapType.Plating] = 2 });

    private static UpgradePathDef Range(params int[] costs) => new(
        "range", PerLevelFactor: 1.12f, LevelCosts: costs,
        BreakpointRecipe: new Dictionary<ScrapType, int> { [ScrapType.Alloy] = 8, [ScrapType.Flux] = 2 });

    private static UpgradePathDef Rate(params int[] costs) => new(
        "rate", PerLevelFactor: 1.10f, LevelCosts: costs,
        BreakpointRecipe: new Dictionary<ScrapType, int> { [ScrapType.Flux] = 3 });

    /// <summary>Named paths for the M3 towers. The ids match the stage-module
    /// filenames design delivered (tower_detector_field_s1…10 etc.), so wiring
    /// the tower is all it takes for its art to appear.</summary>
    private static UpgradePathDef Path(string id, float perLevel, params int[] costs) => new(
        id, PerLevelFactor: perLevel, LevelCosts: costs,
        BreakpointRecipe: new Dictionary<ScrapType, int> { [ScrapType.Flux] = 4 });

    // M1 ships levels 1–5 (L4 is the breakpoint); costs grow ~×1.35 per level.
    private static readonly int[] StdCosts = { 40, 54, 73, 98, 132 };

    /// <summary>Railgun corridor tower (pierce arrives with the M2 projectile
    /// pipeline; M1 fires single-target bolts). Ground-only by sightline.</summary>
    public static readonly TowerDef Lance = new(
        Id: "lance", Kind: TowerKind.Bolt,
        Cost: 75, RangeMeters: 12f, MinRangeMeters: 0f,
        Damage: 8f, ShotsPerSecond: 1.6f, ProjectileSpeed: 30f,
        SplashRadius: 0f, SplashFalloff: 1f,
        ChainJumps: 0, ChainRange: 0f, ChainFalloff: 1f, StructureHp: 120f,
        Applies: System.Array.Empty<string>(),
        TargetLayers: new[] { EnemyLayer.Ground },
        UpgradePaths: new[] { Damage(StdCosts), Range(StdCosts), Rate(StdCosts) });

    /// <summary>Mortar: arcing splash with a minimum range, cannot hit air.
    /// The clump answer (Mote swarms), weak vs spread lines.</summary>
    public static readonly TowerDef Nova = new(
        Id: "nova", Kind: TowerKind.Mortar,
        Cost: 115, RangeMeters: 16f, MinRangeMeters: 5f,
        Damage: 22f, ShotsPerSecond: 0.5f, ProjectileSpeed: 14f,
        SplashRadius: 3.2f, SplashFalloff: 0.35f,
        ChainJumps: 0, ChainRange: 0f, ChainFalloff: 1f, StructureHp: 140f,
        Applies: System.Array.Empty<string>(),
        TargetLayers: new[] { EnemyLayer.Ground },
        UpgradePaths: new[] { Damage(StdCosts), Range(StdCosts), Rate(StdCosts) });

    /// <summary>Gravity well: no damage, chills everything in its sphere every
    /// tick. Support identity — marginal contribution, not a damage row.</summary>
    public static readonly TowerDef Singularity = new(
        Id: "singularity", Kind: TowerKind.Aura,
        Cost: 110, RangeMeters: 9f, MinRangeMeters: 0f,
        Damage: 0f, ShotsPerSecond: 0f, ProjectileSpeed: 0f,
        SplashRadius: 0f, SplashFalloff: 1f,
        ChainJumps: 0, ChainRange: 0f, ChainFalloff: 1f, StructureHp: 110f,
        Applies: new[] { "chill" },
        TargetLayers: new[] { EnemyLayer.Ground, EnemyLayer.Air },
        UpgradePaths: new[] { Range(StdCosts), Rate(StdCosts) });

    /// <summary>Anti-air: exists so flyers are a coverage question, not a hero tax.
    /// Fast low-damage bolts, air layer only.</summary>
    public static readonly TowerDef Skywatch = new(
        Id: "skywatch", Kind: TowerKind.Flak,
        Cost: 90, RangeMeters: 15f, MinRangeMeters: 0f,
        Damage: 5.5f, ShotsPerSecond: 3.0f, ProjectileSpeed: 45f,
        SplashRadius: 0f, SplashFalloff: 1f,
        ChainJumps: 0, ChainRange: 0f, ChainFalloff: 1f, StructureHp: 110f,
        Applies: System.Array.Empty<string>(),
        TargetLayers: new[] { EnemyLayer.Air },
        UpgradePaths: new[] { Damage(StdCosts), Range(StdCosts), Rate(StdCosts) });

    /// <summary>M2 — tesla: instant chain arc that jumps to a nearby second
    /// target and applies shock (Flash Freeze fuel next to a Singularity).</summary>
    public static readonly TowerDef Arc = new(
        Id: "arc", Kind: TowerKind.Tesla,
        Cost: 90, RangeMeters: 11f, MinRangeMeters: 0f,
        Damage: 9f, ShotsPerSecond: 1.2f, ProjectileSpeed: 0f,
        SplashRadius: 0f, SplashFalloff: 1f,
        ChainJumps: 1, ChainRange: 6f, ChainFalloff: 0.6f, StructureHp: 120f,
        Applies: new[] { "shock" },
        TargetLayers: new[] { EnemyLayer.Ground, EnemyLayer.Air },
        UpgradePaths: new[] { Damage(StdCosts), Range(StdCosts), Rate(StdCosts) });

    /// <summary>M2 — closes its barricade slot's shortcut route while alive.
    /// StructureHp matters when Ram arrives (M4); until then it's a toggle.</summary>
    public static readonly TowerDef Barricade = new(
        Id: "barricade", Kind: TowerKind.Barricade,
        Cost: 60, RangeMeters: 0f, MinRangeMeters: 0f,
        Damage: 0f, ShotsPerSecond: 0f, ProjectileSpeed: 0f,
        SplashRadius: 0f, SplashFalloff: 1f,
        ChainJumps: 0, ChainRange: 0f, ChainFalloff: 1f, StructureHp: 300f,
        Applies: System.Array.Empty<string>(),
        TargetLayers: System.Array.Empty<EnemyLayer>(),
        UpgradePaths: System.Array.Empty<UpgradePathDef>());

    /// <summary>M3 — sees what towers cannot. No damage: it applies reveal to
    /// everything in range, which is the whole answer to a Shade, and reveal is
    /// worthless against anything else. A pure information purchase.</summary>
    public static readonly TowerDef Detector = new(
        Id: "detector", Kind: TowerKind.Aura,
        Cost: 70, RangeMeters: 13f, MinRangeMeters: 0f,
        Damage: 0f, ShotsPerSecond: 0f, ProjectileSpeed: 0f,
        SplashRadius: 0f, SplashFalloff: 1f,
        ChainJumps: 0, ChainRange: 0f, ChainFalloff: 1f, StructureHp: 100f,
        Applies: new[] { "reveal" },
        TargetLayers: new[] { EnemyLayer.Ground, EnemyLayer.Air },
        UpgradePaths: new[] { Path("field", 1.14f, StdCosts), Path("analysis", 1.10f, StdCosts) });

    /// <summary>M3 — ramp beam. Damage climbs the longer it holds one target and
    /// resets the moment it switches, so it is the answer to one big thing and
    /// actively bad against a swarm. The opposite question to Nova.</summary>
    public static readonly TowerDef Filament = new(
        Id: "filament", Kind: TowerKind.Beam,
        Cost: 125, RangeMeters: 12f, MinRangeMeters: 0f,
        Damage: 7f, ShotsPerSecond: 0f, ProjectileSpeed: 0f,
        SplashRadius: 0f, SplashFalloff: 1f,
        ChainJumps: 0, ChainRange: 0f, ChainFalloff: 1f, StructureHp: 130f,
        Applies: System.Array.Empty<string>(),
        TargetLayers: new[] { EnemyLayer.Ground, EnemyLayer.Air },
        UpgradePaths: new[]
        {
            Path("ramp", 1.12f, StdCosts),      // how fast it climbs
            Path("peak", 1.15f, StdCosts),      // how high it climbs
            Path("optics", 1.10f, StdCosts),    // range
        });

    public static readonly IReadOnlyDictionary<string, TowerDef> All =
        new Dictionary<string, TowerDef>
        {
            [Arc.Id] = Arc,
            [Barricade.Id] = Barricade,
            [Lance.Id] = Lance,
            [Nova.Id] = Nova,
            [Detector.Id] = Detector,
            [Filament.Id] = Filament,
            [Singularity.Id] = Singularity,
            [Skywatch.Id] = Skywatch,
        };
}
