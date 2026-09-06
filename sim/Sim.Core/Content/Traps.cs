namespace DeepField.Sim.Content;

public static class Traps
{
    /// <summary>Burst damage on the pack that steps on it; recharges. Applies
    /// shred, which until M3 nothing in the game did — Corrode was a defined,
    /// closure-gated reaction with no way to ever occur in a match. Spikes are
    /// the right owner: they tear armour off, and putting it on the builder's
    /// side makes Corrode a genuine co-op combo (trap shreds, hero poisons).</summary>
    public static readonly TrapDef Spike = new(
        Id: "spike", Cost: 45,
        TriggerRadius: 1.8f, Charges: 3, RearmSeconds: 6f,
        Damage: 18f, Applies: "shred", KnockbackMeters: 0f,
        ScrapCost: new Dictionary<ScrapType, int> { [ScrapType.Alloy] = 3 });

    /// <summary>The cheap movement-channel source: chills whoever wades through.</summary>
    public static readonly TrapDef Tar = new(
        Id: "tar", Cost: 35,
        TriggerRadius: 2.2f, Charges: 6, RearmSeconds: 4f,
        Damage: 0f, Applies: "chill", KnockbackMeters: 0f,
        ScrapCost: new Dictionary<ScrapType, int> { [ScrapType.Flux] = 2 });

    /// <summary>Throws enemies back down the route; mass divides the distance
    /// (a Monolith barely shudders).</summary>
    public static readonly TrapDef Launcher = new(
        Id: "launcher", Cost: 55,
        TriggerRadius: 1.8f, Charges: 2, RearmSeconds: 8f,
        Damage: 4f, Applies: null, KnockbackMeters: 8f,
        ScrapCost: new Dictionary<ScrapType, int> { [ScrapType.Alloy] = 2, [ScrapType.Plating] = 1 });

    public static readonly IReadOnlyDictionary<string, TrapDef> All =
        new Dictionary<string, TrapDef>
        {
            [Spike.Id] = Spike,
            [Tar.Id] = Tar,
            [Launcher.Id] = Launcher,
        };
}
