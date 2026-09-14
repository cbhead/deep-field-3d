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

    /// <summary>M2 — Chain Surge: an AoE shock burst at the aim point. Walking
    /// reaction fuel: pair with any chill source for Flash Freeze columns.</summary>
    public static readonly FactionDef Tempest = new(
        Id: "tempest", AbilityId: "chainSurge",
        CooldownSeconds: 26f, RadiusMeters: 7f, DurationSeconds: 0f,
        Magnitude: 6f,     // burst damage per enemy struck
        Passive: "reloadSpeed"); // +12% weapon fire rate (see Balance)

    /// <summary>M3 — Cryo Field: a chill dome over the aim point. The builder's
    /// Singularity does this passively in one small sphere; Glacier does it on
    /// demand, anywhere, which makes it the half of Thermal Shock and Flash
    /// Freeze that can be aimed at the problem rather than built next to it.</summary>
    public static readonly FactionDef Glacier = new(
        Id: "glacier", AbilityId: "cryoField",
        CooldownSeconds: 24f, RadiusMeters: 8f, DurationSeconds: 0f,
        Magnitude: 1f,     // chill application strength (level curve dial)
        Passive: "chilledBonus"); // +25% of this player's damage to chilled targets

    /// <summary>M3 — Reveal Pulse: map-wide detection for a moment. Deliberately
    /// not a Detector substitute — it is instantaneous where the tower is
    /// permanent, so it answers "where is it right now" and never "cover this
    /// approach". A Specter still wants Detectors built.</summary>
    public static readonly FactionDef Specter = new(
        Id: "specter", AbilityId: "revealPulse",
        CooldownSeconds: 34f, RadiusMeters: 0f, DurationSeconds: 0f,
        Magnitude: 1f,
        Passive: "weakPoints"); // sees enemy weak points highlighted (client-side)

    public static readonly IReadOnlyDictionary<string, FactionDef> All =
        new Dictionary<string, FactionDef>
        {
            [Forge.Id] = Forge,
            [Ember.Id] = Ember,
            [Tempest.Id] = Tempest,
            [Glacier.Id] = Glacier,
            [Specter.Id] = Specter,
        };

    // ---- Persistent leveling (profile XP → level, applied at Join) ---------

    public const int MaxLevel = 5;
    public const int XpPerLevel = 100;

    public static int LevelForXp(int xp) =>
        System.Math.Clamp(1 + xp / XpPerLevel, 1, MaxLevel);

    /// <summary>Per-level improvements, uniform across factions for M2:
    /// shorter cooldown, wider radius, stronger magnitude. Swept dials.</summary>
    public static float CooldownFactor(int level) => DetMath.PowInt(0.94f, level - 1);
    public static float RadiusFactor(int level) => 1f + 0.06f * (level - 1);
    public static float MagnitudeFactor(int level) => 1f + 0.05f * (level - 1);
}
