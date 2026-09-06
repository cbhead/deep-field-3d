namespace DeepField.Sim.Content;

/// <summary>Factions are the co-op identity layer: one per player, no duplicates
/// in a lobby, signature ability + minor passive. Persistent leveling arrives at
/// M2 (lobby faction menu); M1 ships level-1 abilities.</summary>
public sealed record FactionDef(
    string Id,
    string AbilityId,
    float CooldownSeconds,
    float RadiusMeters,
    float DurationSeconds,
    float Magnitude,
    string Passive);

public static class Factions
{
    /// <summary>Overdrive: surge nearby towers' fire rate. The builder's ability —
    /// syncs with tower placement, not aim.</summary>
    public static readonly FactionDef Forge = new(
        Id: "forge", AbilityId: "overdrive",
        CooldownSeconds: 30f, RadiusMeters: 10f, DurationSeconds: 4f,
        Magnitude: 1.5f,   // fire-rate factor while surged
        Passive: "buildDiscount"); // 10% off tower placements (see Balance)

    /// <summary>Ignition Wave: burn everything near the aim point. The shooter's
    /// half of Thermal Shock — walk into the chill field and light it up.</summary>
    public static readonly FactionDef Ember = new(
        Id: "ember", AbilityId: "ignitionWave",
        CooldownSeconds: 22f, RadiusMeters: 6f, DurationSeconds: 0f,
        Magnitude: 1f,     // burn application strength factor (level curve dial)
        Passive: "burnDuration"); // +30% burn duration on everything this player applies

    public static readonly IReadOnlyDictionary<string, FactionDef> All =
        new Dictionary<string, FactionDef>
        {
            [Forge.Id] = Forge,
            [Ember.Id] = Ember,
        };
}
