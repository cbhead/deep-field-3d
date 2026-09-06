namespace DeepField.Sim.Content;

/// <summary>Content is pure data; no balance literal lives in the sim systems.
/// These records track the plan's schema inventory — M1 carries the vertical
/// slice subset (layers, front armor, statuses, upgrade paths, scrap).</summary>

public enum EnemyLayer
{
    Ground,
    Air,
}

public enum ScrapType
{
    Alloy,      // common — basic walkers
    Flux,       // uncommon — shielded/energy enemies, flyers
    Plating,    // uncommon — armored enemies
}

public sealed record EnemyDef(
    string Id,
    float Hp,
    float SpeedMetersPerSec,
    int Bounty,
    int LeakDamage,
    EnemyLayer Layer,
    float ContactDamage,          // hp/sec dealt to heroes standing in it
    float ScatterWidth,           // lateral spread across the path (Mote swarms)
    bool BlocksSight,             // Monolith: enemies behind it are untargetable by towers
    float FrontArmorArcDegrees,   // Aegis: incoming damage inside this frontal arc is reduced
    float FrontArmorFactor,       // damage multiplier inside the arc (0.25 = 75% reduction)
    float RearWeakFactor,         // damage multiplier from directly behind (Aegis reward)
    float Shield,                 // Warden: regenerating pool that soaks damage first
    float FlatArmor,              // per-hit flat reduction (shred strips it)
    bool Burrower,                // Mole: cycles untargetable underground
    string? SplitInto,            // Cluster: child def id spawned on death
    int SplitCount,
    IReadOnlyDictionary<ScrapType, int> ScrapYield);

public sealed record UpgradePathDef(
    string Id,                    // "damage" | "range" | "rate"
    float PerLevelFactor,         // multiplier applied per level to the governed stat
    IReadOnlyList<int> LevelCosts, // money cost per level (index 0 = level 1)
    IReadOnlyDictionary<ScrapType, int> BreakpointRecipe); // extra cost at L4

public enum TowerKind
{
    Bolt,       // homing single-target (Lance M0 behavior; pierce arrives M2)
    Mortar,     // arcing splash, ground-only, min range (Nova)
    ChillAura,  // no damage; applies chill to everything in range (Singularity)
    Flak,       // fast bolts, air-only (Skywatch)
}

public sealed record TowerDef(
    string Id,
    TowerKind Kind,
    int Cost,
    float RangeMeters,
    float MinRangeMeters,
    float Damage,
    float ShotsPerSecond,
    float ProjectileSpeed,
    float SplashRadius,
    float SplashFalloff,          // damage multiplier at the splash edge
    IReadOnlyList<string> Applies, // status ids attached to this tower's hits/aura
    IReadOnlyList<EnemyLayer> TargetLayers,
    IReadOnlyList<UpgradePathDef> UpgradePaths);

public sealed record WeaponDef(
    string Id,
    int Cost,                     // armory money price (0 = starter, always owned)
    float Damage,
    float ShotsPerSecond,
    float RangeMeters,
    IReadOnlyList<string> Applies);

public enum SocketTag
{
    Ground,
    Wall,
    Trap,       // M2 — present on Foundry, buildable later
}

public sealed record SocketDef(string Id, Vec3 Pos, SocketTag Tag);

public sealed record RouteDef(
    string Id,
    EnemyLayer Layer,
    IReadOnlyList<Vec3> Waypoints);

/// <summary>Auto-hero anchor points with travel-time edges (the traversal graph).</summary>
public sealed record HeroStationDef(string Id, Vec3 Pos);

public sealed record MapDef(
    string Id,
    IReadOnlyList<RouteDef> Routes,
    IReadOnlyList<SocketDef> Sockets,
    Vec3 HeroSpawn,
    Vec3 ArmoryPos,
    IReadOnlyList<HeroStationDef> HeroStations,
    int TotalWaves);
