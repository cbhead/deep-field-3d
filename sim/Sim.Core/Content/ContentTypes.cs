namespace DeepField.Sim.Content;

/// <summary>Content is pure data; no balance literal lives in the sim systems.
/// These records grow per the plan's schema inventory (statuses, factions,
/// gunsmith, scrap...) — M0 carries only what the walking skeleton needs.</summary>
public sealed record EnemyDef(
    string Id,
    float Hp,
    float SpeedMetersPerSec,
    int Bounty,
    int LeakDamage);

public sealed record TowerDef(
    string Id,
    int Cost,
    float RangeMeters,
    float Damage,
    float ShotsPerSecond,
    float ProjectileSpeed);

public sealed record WeaponDef(
    string Id,
    float Damage,
    float ShotsPerSecond,
    float RangeMeters);

public sealed record SocketDef(string Id, Vec3 Pos);

public sealed record MapDef(
    string Id,
    IReadOnlyList<Vec3> Route,
    IReadOnlyList<SocketDef> Sockets,
    Vec3 HeroSpawn,
    int TotalWaves);
