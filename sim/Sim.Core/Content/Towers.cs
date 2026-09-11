using System.Linq;
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

    /// <summary>Breakpoint recipes, keyed by the level they are charged at.
    /// Rarer the higher you go, which is the plan's rule and the reason the
    /// tenth level means something: Alloy is dropped by anything that walks,
    /// Plating by armour, Flux by shields and flyers, and Gravium only by the
    /// heavies — so a maxed path is a record of what the team actually fought.</summary>
    private static IReadOnlyDictionary<int, IReadOnlyDictionary<ScrapType, int>> Breakpoints(
        (ScrapType Type, int Amount)[] four,
        (ScrapType Type, int Amount)[] seven,
        (ScrapType Type, int Amount)[] ten)
    {
        static IReadOnlyDictionary<ScrapType, int> Recipe((ScrapType Type, int Amount)[] parts)
            => parts.ToDictionary(p => p.Type, p => p.Amount);
        return new Dictionary<int, IReadOnlyDictionary<ScrapType, int>>
        {
            [4] = Recipe(four),
            [7] = Recipe(seven),
            [10] = Recipe(ten),
        };
    }

    private static UpgradePathDef Damage(params int[] costs) => new(
        "damage", PerLevelFactor: 1.10f, LevelCosts: costs,
        BreakpointRecipes: Breakpoints(
            new[] { (ScrapType.Alloy, 8), (ScrapType.Plating, 2) },
            new[] { (ScrapType.Plating, 6), (ScrapType.Flux, 3) },
            new[] { (ScrapType.Gravium, 4), (ScrapType.Plating, 6) }));

    private static UpgradePathDef Range(params int[] costs) => new(
        "range", PerLevelFactor: 1.12f, LevelCosts: costs,
        BreakpointRecipes: Breakpoints(
            new[] { (ScrapType.Alloy, 8), (ScrapType.Flux, 2) },
            new[] { (ScrapType.Flux, 7), (ScrapType.Alloy, 6) },
            new[] { (ScrapType.Gravium, 4), (ScrapType.Flux, 6) }));

    private static UpgradePathDef Rate(params int[] costs) => new(
        "rate", PerLevelFactor: 1.10f, LevelCosts: costs,
        BreakpointRecipes: Breakpoints(
            new[] { (ScrapType.Flux, 3), (ScrapType.Alloy, 4) },
            new[] { (ScrapType.Flux, 8), (ScrapType.Plating, 3) },
            new[] { (ScrapType.Gravium, 3), (ScrapType.Flux, 8) }));

    /// <summary>Named paths for the M3 towers. The ids match the stage-module
    /// filenames design delivered (tower_detector_field_s1…10 etc.), so wiring
    /// the tower is all it takes for its art to appear.</summary>
    private static UpgradePathDef Path(string id, float perLevel, params int[] costs) => new(
        id, PerLevelFactor: perLevel, LevelCosts: costs,
        BreakpointRecipes: Breakpoints(
            new[] { (ScrapType.Flux, 4), (ScrapType.Alloy, 4) },
            new[] { (ScrapType.Flux, 9), (ScrapType.Plating, 4) },
            new[] { (ScrapType.Gravium, 4), (ScrapType.Flux, 8) }));

    /// <summary>The nine purchases between level 1 and level 10 — design drew
    /// ten stages per path and the sim only ever offered five of them, so a
    /// tower stopped changing shape halfway up its own art.
    ///
    /// Costs grow about ×1.35 a level, which is what makes the top of a path a
    /// real decision: the last four levels cost more than the first six
    /// together, so a campaign run funds roughly one maxed path on one tower
    /// and endless is where the whole grid opens.</summary>
    private static readonly int[] StdCosts = { 40, 54, 73, 98, 132, 178, 240, 324, 438 };

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
        Damage: 8f, ShotsPerSecond: 3.0f, ProjectileSpeed: 45f,
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
