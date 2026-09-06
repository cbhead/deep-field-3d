namespace DeepField.Sim.Content;

public static class Towers
{
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

    // M1 ships levels 1–5 (L4 is the breakpoint); costs grow ~×1.35 per level.
    private static readonly int[] StdCosts = { 40, 54, 73, 98, 132 };

    /// <summary>Railgun corridor tower (pierce arrives with the M2 projectile
    /// pipeline; M1 fires single-target bolts). Ground-only by sightline.</summary>
    public static readonly TowerDef Lance = new(
        Id: "lance", Kind: TowerKind.Bolt,
        Cost: 75, RangeMeters: 12f, MinRangeMeters: 0f,
        Damage: 8f, ShotsPerSecond: 1.6f, ProjectileSpeed: 30f,
        SplashRadius: 0f, SplashFalloff: 1f,
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
        Applies: System.Array.Empty<string>(),
        TargetLayers: new[] { EnemyLayer.Ground },
        UpgradePaths: new[] { Damage(StdCosts), Range(StdCosts), Rate(StdCosts) });

    /// <summary>Gravity well: no damage, chills everything in its sphere every
    /// tick. Support identity — marginal contribution, not a damage row.</summary>
    public static readonly TowerDef Singularity = new(
        Id: "singularity", Kind: TowerKind.ChillAura,
        Cost: 110, RangeMeters: 9f, MinRangeMeters: 0f,
        Damage: 0f, ShotsPerSecond: 0f, ProjectileSpeed: 0f,
        SplashRadius: 0f, SplashFalloff: 1f,
        Applies: new[] { "chill" },
        TargetLayers: new[] { EnemyLayer.Ground, EnemyLayer.Air },
        UpgradePaths: new[] { Range(StdCosts), Rate(StdCosts) });

    /// <summary>Anti-air: exists so flyers are a coverage question, not a hero tax.
    /// Fast low-damage bolts, air layer only.</summary>
    public static readonly TowerDef Skywatch = new(
        Id: "skywatch", Kind: TowerKind.Flak,
        Cost: 90, RangeMeters: 15f, MinRangeMeters: 0f,
        Damage: 4f, ShotsPerSecond: 3.0f, ProjectileSpeed: 45f,
        SplashRadius: 0f, SplashFalloff: 1f,
        Applies: System.Array.Empty<string>(),
        TargetLayers: new[] { EnemyLayer.Air },
        UpgradePaths: new[] { Damage(StdCosts), Range(StdCosts), Rate(StdCosts) });

    public static readonly IReadOnlyDictionary<string, TowerDef> All =
        new Dictionary<string, TowerDef>
        {
            [Lance.Id] = Lance,
            [Nova.Id] = Nova,
            [Singularity.Id] = Singularity,
            [Skywatch.Id] = Skywatch,
        };
}
