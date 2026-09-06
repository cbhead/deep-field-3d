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
    float ArmorDelta,         // defense channel (negative = shredded)
    bool HardControl,         // control channel: full stop, gated by cc-resist
    float MaxDurationSeconds,
    bool IgnoresArmor = false,    // toxin: flat armor does not reduce the tick
    bool IgnoresShield = false)   // toxin: the tick lands on hp through a shield
{
    /// <summary>Magnitude decides strongest-wins within a channel.</summary>
    public float Magnitude => Channel switch
    {
        Channel.Movement => 1f - SpeedFactor,
        Channel.Thermal or Channel.Toxin => DamagePerSecond,
        Channel.Vulnerability => DamageTakenFactor - 1f,
        Channel.Defense => -ArmorDelta,
        Channel.Control => MaxDurationSeconds,
        _ => 0f,
    };
}

public static class Statuses
{
    /// <summary>Singularity's aura and cryo infusions. Strongest chill wins.</summary>
    public static readonly StatusDef Chill = new(
        Id: "chill", Channel: Channel.Movement,
        SpeedFactor: 0.65f, DamagePerSecond: 0f, DamageTakenFactor: 1f,
        ArmorDelta: 0f, HardControl: false, MaxDurationSeconds: 1.5f);

    /// <summary>Ember faction ability and ember-coil infusion. High dps, short —
    /// and shields eat it entirely (the burn-vs-poison identity).</summary>
    public static readonly StatusDef Burn = new(
        Id: "burn", Channel: Channel.Thermal,
        SpeedFactor: 1f, DamagePerSecond: 6f, DamageTakenFactor: 1f,
        ArmorDelta: 0f, HardControl: false, MaxDurationSeconds: 3f);

    /// <summary>Rifle alt-fire. Short window of amplified damage — the
    /// prioritization tool.</summary>
    public static readonly StatusDef Mark = new(
        Id: "mark", Channel: Channel.Vulnerability,
        SpeedFactor: 1f, DamagePerSecond: 0f, DamageTakenFactor: 1.25f,
        ArmorDelta: 0f, HardControl: false, MaxDurationSeconds: 4f);

    /// <summary>Arc tower and Tempest's Chain Surge. A micro-stagger in the
    /// control channel (cc-resist gates it) — mostly reaction fuel.</summary>
    public static readonly StatusDef Shock = new(
        Id: "shock", Channel: Channel.Control,
        SpeedFactor: 0f, DamagePerSecond: 0f, DamageTakenFactor: 1f,
        ArmorDelta: 0f, HardControl: true, MaxDurationSeconds: 0.25f);

    /// <summary>Flash Freeze output — the real hard stop. Never applied
    /// directly at M2; only the chill+shock reaction produces it.</summary>
    public static readonly StatusDef Freeze = new(
        Id: "freeze", Channel: Channel.Control,
        SpeedFactor: 0f, DamagePerSecond: 0f, DamageTakenFactor: 1f,
        ArmorDelta: 0f, HardControl: true, MaxDurationSeconds: 1.2f);

    /// <summary>Armor shred: strips flat armor and softens Aegis's front arc —
    /// the answer to armor the team hasn't out-leveled.</summary>
    public static readonly StatusDef Shred = new(
        Id: "shred", Channel: Channel.Defense,
        SpeedFactor: 1f, DamagePerSecond: 0f, DamageTakenFactor: 1f,
        ArmorDelta: -2f, HardControl: false, MaxDurationSeconds: 4f);

    /// <summary>Poison is the chip answer burn is not: it ignores flat armor
    /// AND passes through shields, so a Warden that eats fire whole still rots.
    /// Low dps, long duration — it beats sustain, not health bars.</summary>
    public static readonly StatusDef Poison = new(
        Id: "poison", Channel: Channel.Toxin,
        SpeedFactor: 1f, DamagePerSecond: 3f, DamageTakenFactor: 1f,
        ArmorDelta: 0f, HardControl: false, MaxDurationSeconds: 6f,
        IgnoresArmor: true, IgnoresShield: true);

    /// <summary>Detector output. Carries no combat effect of its own — being
    /// seen is the effect, because a revealed Shade is targetable.</summary>
    public static readonly StatusDef Reveal = new(
        Id: "reveal", Channel: Channel.Detection,
        SpeedFactor: 1f, DamagePerSecond: 0f, DamageTakenFactor: 1f,
        ArmorDelta: 0f, HardControl: false, MaxDurationSeconds: 3f);

    public static readonly IReadOnlyDictionary<string, StatusDef> All =
        new Dictionary<string, StatusDef>
        {
            [Chill.Id] = Chill,
            [Burn.Id] = Burn,
            [Mark.Id] = Mark,
            [Shock.Id] = Shock,
            [Freeze.Id] = Freeze,
            [Shred.Id] = Shred,
            [Poison.Id] = Poison,
            [Reveal.Id] = Reveal,
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
    float BurstFraction,     // burst damage = fraction of victim MaxHp
    string? EmitStatus);     // status applied by the reaction's output (bypasses further reactions)

public static class Reactions
{
    /// <summary>The M1 showpiece: builder's Singularity chills, shooter's Ember
    /// burn detonates it. Both statuses consumed.</summary>
    public static readonly ReactionDef ThermalShock = new(
        Id: "thermalShock",
        StatusA: Statuses.Chill.Id,
        StatusB: Statuses.Burn.Id,
        BurstFraction: 0.12f,
        EmitStatus: null);

    /// <summary>M2: chill + shock locks the target solid. Emitted freeze goes
    /// straight into the control slot — reaction outputs never chain (closure).</summary>
    public static readonly ReactionDef FlashFreeze = new(
        Id: "flashFreeze",
        StatusA: Statuses.Chill.Id,
        StatusB: Statuses.Shock.Id,
        BurstFraction: 0f,
        EmitStatus: Statuses.Freeze.Id);

    /// <summary>M3: poison + shred corrodes. Shred is the armor answer and
    /// poison the sustain answer, so the pair is what a team brings to something
    /// that is both — and the burst is the reward for bringing both halves.</summary>
    public static readonly ReactionDef Corrode = new(
        Id: "corrode",
        StatusA: Statuses.Poison.Id,
        StatusB: Statuses.Shred.Id,
        BurstFraction: 0.10f,
        EmitStatus: null);

    public static readonly IReadOnlyList<ReactionDef> All =
        new[] { ThermalShock, FlashFreeze, Corrode };

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
