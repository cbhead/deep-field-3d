namespace DeepField.Sim.Content;

/// <summary>Channels are the exclusive slots; statuses are data rows inside them.
/// One active status per channel: strongest wins, re-hit refreshes, never stacks —
/// the 2D game's slow triple generalized. M1 ships three channels; the enum is the
/// full plan-of-record so content can land without sim changes.</summary>
public enum Channel
{
    Movement,       // speed factor (chill, tar)
    Thermal,        // hp DoT, blocked by shield (burn)
    Toxin,          // hp DoT, ignores armor and shield (poison) — M3
    Defense,        // flat armor delta (shred) — M2
    Vulnerability,  // damage-taken factor (mark)
    Control,        // hard stop (stun/freeze) — M4
    Tether,         // pull (magnetize) — M5+
    Detection,      // visible-to-towers flag (reveal) — M3
}

public sealed record StatusDef(
    string Id,
    Channel Channel,
    float SpeedFactor,        // movement channel (1 = no effect)
    float DamagePerSecond,    // thermal/toxin channels
    float DamageTakenFactor,  // vulnerability channel (1 = no effect)
    float MaxDurationSeconds)
{
    /// <summary>Magnitude decides strongest-wins within a channel.</summary>
    public float Magnitude => Channel switch
    {
        Channel.Movement => 1f - SpeedFactor,
        Channel.Thermal or Channel.Toxin => DamagePerSecond,
        Channel.Vulnerability => DamageTakenFactor - 1f,
        _ => 0f,
    };
}

public static class Statuses
{
    /// <summary>Singularity's aura and cryo infusions. Strongest chill wins.</summary>
    public static readonly StatusDef Chill = new(
        Id: "chill", Channel: Channel.Movement,
        SpeedFactor: 0.65f, DamagePerSecond: 0f, DamageTakenFactor: 1f,
        MaxDurationSeconds: 1.5f);

    /// <summary>Ember faction ability and ember-coil infusion. High dps, short —
    /// and at M2+ shields eat it (the burn-vs-poison identity).</summary>
    public static readonly StatusDef Burn = new(
        Id: "burn", Channel: Channel.Thermal,
        SpeedFactor: 1f, DamagePerSecond: 6f, DamageTakenFactor: 1f,
        MaxDurationSeconds: 3f);

    /// <summary>Rifle alt-fire. Short window of amplified damage — the
    /// prioritization tool.</summary>
    public static readonly StatusDef Mark = new(
        Id: "mark", Channel: Channel.Vulnerability,
        SpeedFactor: 1f, DamagePerSecond: 0f, DamageTakenFactor: 1.25f,
        MaxDurationSeconds: 4f);

    public static readonly IReadOnlyDictionary<string, StatusDef> All =
        new Dictionary<string, StatusDef>
        {
            [Chill.Id] = Chill,
            [Burn.Id] = Burn,
            [Mark.Id] = Mark,
        };
}

/// <summary>The closed reaction table: (active status, incoming status) consume
/// both and emit a discrete effect. Gate-enforced closure: no reaction output may
/// itself trigger a reaction, and both inputs must be applicable by at least one
/// tower AND one hero source (the co-op combo requirement).</summary>
public sealed record ReactionDef(
    string Id,
    string StatusA,
    string StatusB,
    float BurstFraction);   // burst damage = fraction of victim MaxHp

public static class Reactions
{
    /// <summary>The M1 showpiece: builder's Singularity chills, shooter's Ember
    /// burn detonates it. Both statuses consumed.</summary>
    public static readonly ReactionDef ThermalShock = new(
        Id: "thermalShock",
        StatusA: Statuses.Chill.Id,
        StatusB: Statuses.Burn.Id,
        BurstFraction: 0.12f);

    public static readonly IReadOnlyList<ReactionDef> All = new[] { ThermalShock };

    public static ReactionDef? Match(string active, string incoming)
    {
        foreach (var r in All)
        {
            if ((r.StatusA == active && r.StatusB == incoming) ||
                (r.StatusB == active && r.StatusA == incoming))
                return r;
        }
        return null;
    }
}
