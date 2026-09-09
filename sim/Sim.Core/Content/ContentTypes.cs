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
    Gravium,    // rare — heavies and elites (M2+)
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
    float Mass,                   // knockback divisor (heavy = barely moves)
    bool Burrower,                // Mole: cycles untargetable underground
    string? SplitInto,            // Cluster: child def id spawned on death
    int SplitCount,
    IReadOnlyDictionary<ScrapType, int> ScrapYield,
    bool Stealth = false,          // Shade: towers cannot target it unless revealed
    float StealthSpeedBonus = 0f,  // Shade: faster while unseen — being ignored pays
    float HealPerSecond = 0f,      // Mender: hp/sec restored to nearby allies
    float HealRadius = 0f,
    float StructureDps = 0f,       // Ram: hp/sec dealt to a structure in reach
    float StructureReach = 0f,     // how close it must be to start swinging
    float EnrageBelowHpFraction = 0f, // Ram: speeds up when hurt
    float EnrageSpeedFactor = 1f);

/// <summary>One of a tower's upgrade paths, ten levels deep.
///
/// A tower is built at level 1 and bought up to 10, so LevelCosts holds the
/// nine purchases between them, indexed by the level you are leaving. That is
/// also exactly design's stage numbering — `tower_&lt;id&gt;_&lt;path&gt;_s1…s10`,
/// where s1 is empty because the chassis *is* level 1 — so the level a player
/// sees, the stage file that draws it, and the breakpoint keys below are all
/// the same number.
///
/// BreakpointRecipes is keyed by that level: 4, 7 and 10 cost scrap from the
/// team pool on top of the money, in rarer types the higher you go.</summary>
public sealed record UpgradePathDef(
    string Id,
    float PerLevelFactor,         // multiplier applied per level to the governed stat
    IReadOnlyList<int> LevelCosts,
    IReadOnlyDictionary<int, IReadOnlyDictionary<ScrapType, int>> BreakpointRecipes)
{
    /// <summary>The highest level this path can reach: level 1 plus a purchase
    /// for each cost. Ten, for every path design drew stages for.</summary>
    public int MaxLevel => LevelCosts.Count + 1;

    /// <summary>What arriving at a level costs in scrap, if anything.</summary>
    public IReadOnlyDictionary<ScrapType, int>? RecipeFor(int level) =>
        BreakpointRecipes.TryGetValue(level, out var recipe) ? recipe : null;
}

public enum TowerKind
{
    Bolt,       // homing single-target (Lance M0 behavior; pierce arrives M2)
    Mortar,     // arcing splash, ground-only, min range (Nova)
    Aura,       // no damage; applies its Applies[] to everything in range
                // (Singularity chills, Detector reveals — same mechanism)
    Flak,       // fast bolts, air-only (Skywatch)
    Tesla,      // instant chain arc, jumps between nearby targets (Arc)
    Beam,       // continuous single-target damage that ramps while held and
                // resets on target switch (Filament)
    Barricade,  // no weapon: closes its route gate while alive
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
    int ChainJumps,               // Tesla: extra targets after the first
    float ChainRange,             // Tesla: max hop distance between targets
    float ChainFalloff,           // Tesla: damage multiplier per hop
    float StructureHp,            // Barricade (and towers, once Ram lands at M4)
    IReadOnlyList<string> Applies, // status ids attached to this tower's hits/aura
    IReadOnlyList<EnemyLayer> TargetLayers,
    IReadOnlyList<UpgradePathDef> UpgradePaths);

/// <summary>Path-floor traps: charge-based, rearming, placed on trap sockets.</summary>
public sealed record TrapDef(
    string Id,
    int Cost,
    float TriggerRadius,
    int Charges,
    float RearmSeconds,
    float Damage,                 // Spike
    string? Applies,              // Tar: chill
    float KnockbackMeters,        // Launcher: route displacement, divided by mass
    IReadOnlyDictionary<ScrapType, int> ScrapCost);

public sealed record WeaponDef(
    string Id,
    // A weapon is yours, so you buy it with what is yours. Money is the shared
    // team wallet and belongs to the towers everyone benefits from; a platform
    // comes out of the personal scrap you picked up off the floor, in the same
    // currency its attachments and ammo already cost. Empty = the starter,
    // always owned. (The design brief priced platforms in money; this is a
    // deliberate departure, recorded in docs/design-system/README.md.)
    IReadOnlyDictionary<ScrapType, int> Recipe,
    float Damage,
    float ShotsPerSecond,
    float RangeMeters,
    IReadOnlyList<string> Applies,
    // Ammo is a magazine, not a pool: the reserve is unlimited so nobody is
    // ever disarmed, but a magazine runs dry and refilling it costs seconds you
    // do not have. That is what stops holding the trigger in front of the lane
    // from being a strategy.
    int MagazineSize = 12,
    float ReloadSeconds = 1.6f,
    // Semi-automatic weapons fire once per click. A pistol you can hold down is
    // a worse pistol than one you cannot.
    bool Automatic = true);

public enum SocketTag
{
    Ground,
    Wall,
    Trap,
    Barricade,  // barricade slots gate shortcut routes
}

public sealed record SocketDef(string Id, Vec3 Pos, SocketTag Tag);

/// <summary>A route with a BarricadeGate is a shortcut: usable only while no
/// living barricade occupies that slot. Enemies pick their route at spawn
/// (never mid-walk), so pathing stays deterministic and sweep-enumerable.
/// FallbackRouteId names the long way around.</summary>
public sealed record RouteDef(
    string Id,
    EnemyLayer Layer,
    IReadOnlyList<Vec3> Waypoints,
    string? BarricadeGate = null,
    string? FallbackRouteId = null);

/// <summary>Auto-hero anchor points with travel-time edges (the traversal graph).</summary>
public sealed record HeroStationDef(string Id, Vec3 Pos);

public sealed record MapDef(
    string Id,
    IReadOnlyList<RouteDef> Routes,
    IReadOnlyList<SocketDef> Sockets,
    Vec3 HeroSpawn,
    Vec3 ArmoryPos,
    IReadOnlyList<HeroStationDef> HeroStations,
    int TotalWaves,
    // Wave index -> condition id. Authored rather than rolled so the sweep can
    // run a column per condition, and so the intermission panel can announce
    // next wave's weather one wave ahead.
    IReadOnlyDictionary<int, string>? ConditionScheduleOrNull = null)
{
    public IReadOnlyDictionary<int, string> ConditionSchedule =>
        ConditionScheduleOrNull ?? EmptySchedule;

    private static readonly IReadOnlyDictionary<int, string> EmptySchedule =
        new Dictionary<int, string>();
}
