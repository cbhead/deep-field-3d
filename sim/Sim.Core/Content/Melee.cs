namespace DeepField.Sim.Content;

/// <summary>Melee's identity is economic, not tactical. Every platform trades
/// the same thing — you must be inside contact range of something that deals
/// contact damage — and what it pays is scrap: melee kills yield more of it
/// (see <see cref="Balance.MeleeScrapBonus"/>). That is the whole reason a
/// shooter ever puts the gun away, and it is why the bonus is a swept dial
/// rather than flavour.
///
/// Like the gunsmith, platforms are stat rows and customization is deltas, so
/// the harness can price any build without special-casing melee.</summary>
public enum MeleeSlot
{
    Edge,           // damage profile
    Grip,           // swing speed / handling
    CoreInfusion,   // status application — the reaction table at point blank
    Counterweight,  // reach and knockback
    ChargeCell,     // defines the charged heavy (M4+; the slot exists now)
}

public sealed record MeleeAttachmentDef(
    string Id,
    MeleeSlot Slot,
    float DamageFactor,
    float SpeedFactor,
    float ReachFactor,
    string? Applies,
    IReadOnlyDictionary<ScrapType, int> Recipe);

public sealed record MeleeDef(
    string Id,
    /// <summary>Personal scrap, like a ranged platform: see WeaponDef.Recipe.</summary>
    IReadOnlyDictionary<ScrapType, int> Recipe,
    float Damage,
    float SwingsPerSecond,
    float ReachMeters,
    float ArcDegrees,          // half-angle either side of the swing direction
    float KnockbackMeters,
    IReadOnlyList<string> Applies,
    IReadOnlyList<int> MasteryCosts)
{
    /// <summary>The arc as the swing test actually wants it. A cone check is a
    /// dot product against a cosine, and the tick was computing that cosine from
    /// degrees on every swing — a transcendental in the hot path, for a value
    /// that is a constant of the weapon. Computed once, here, so the tick does a
    /// comparison and nothing else. See <see cref="DetMath"/>.</summary>
    public float CosArc { get; } = DetMath.CosDegrees(ArcDegrees);
}

public static class Melee
{
    // Mastery mirrors the tower grid's shape: ten levels, breakpoints at 4/7/10,
    // costs growing about x1.35. Levels 6-10 are authored and gated with the
    // rest of the endless-mode grid at M4.
    private static readonly int[] MasteryCosts = { 30, 41, 55, 74, 100, 135, 182, 246, 332, 448 };

    /// <summary>The wrench. Never taken away, never bought, and the reason
    /// nobody is ever a spectator — a player with no money and no scrap can
    /// still walk up to something and get paid for it.</summary>
    public static readonly MeleeDef Wrench = new(
        Id: "wrench", Recipe: new Dictionary<ScrapType, int>(),
        Damage: 14f, SwingsPerSecond: 1.4f, ReachMeters: 2.6f, ArcDegrees: 45f,
        KnockbackMeters: 0f,
        Applies: System.Array.Empty<string>(),
        MasteryCosts: MasteryCosts);

    /// <summary>Fast and narrow. The single-target answer, and the platform an
    /// infusion pays off on most because it applies its status the most often.</summary>
    public static readonly MeleeDef Blade = new(
        Id: "blade", Recipe: new Dictionary<ScrapType, int> { [ScrapType.Alloy] = 7 },
        Damage: 19f, SwingsPerSecond: 2.0f, ReachMeters: 2.4f, ArcDegrees: 35f,
        KnockbackMeters: 0f,
        Applies: System.Array.Empty<string>(),
        MasteryCosts: MasteryCosts);

    /// <summary>Slow, wide, and it moves things. The crowd answer: a Maul swing
    /// into a Mote scatter is worth more than any number of Blade pokes, and
    /// the knockback buys the space a melee player otherwise does not have.</summary>
    public static readonly MeleeDef Maul = new(
        Id: "maul", Recipe: new Dictionary<ScrapType, int>
            { [ScrapType.Alloy] = 6, [ScrapType.Plating] = 4 },
        Damage: 34f, SwingsPerSecond: 0.8f, ReachMeters: 3.0f, ArcDegrees: 80f,
        KnockbackMeters: 3.5f,
        Applies: System.Array.Empty<string>(),
        MasteryCosts: MasteryCosts);

    /// <summary>M3 — reach. The Spear is what makes melee survivable against
    /// things that hurt to stand next to: it out-ranges contact damage, so an
    /// Aegis can be hit in the back without being hit back.</summary>
    public static readonly MeleeDef Spear = new(
        Id: "spear", Recipe: new Dictionary<ScrapType, int>
            { [ScrapType.Alloy] = 5, [ScrapType.Flux] = 3 },
        Damage: 22f, SwingsPerSecond: 1.1f, ReachMeters: 4.2f, ArcDegrees: 25f,
        KnockbackMeters: 0f,
        Applies: System.Array.Empty<string>(),
        MasteryCosts: MasteryCosts);

    public static readonly IReadOnlyDictionary<string, MeleeDef> All =
        new Dictionary<string, MeleeDef>
        {
            [Wrench.Id] = Wrench,
            [Blade.Id] = Blade,
            [Maul.Id] = Maul,
            [Spear.Id] = Spear,
        };

    public static readonly IReadOnlyDictionary<string, MeleeAttachmentDef> Attachments =
        new Dictionary<string, MeleeAttachmentDef>
        {
            // Edge: the damage/speed trade, same shape as weapon barrels.
            ["honedEdge"] = new("honedEdge", MeleeSlot.Edge, 1.18f, 0.92f, 1f, null,
                new Dictionary<ScrapType, int> { [ScrapType.Alloy] = 5 }),
            ["serratedEdge"] = new("serratedEdge", MeleeSlot.Edge, 1.05f, 1.10f, 1f, null,
                new Dictionary<ScrapType, int> { [ScrapType.Alloy] = 4, [ScrapType.Plating] = 1 }),

            // Grip: handling.
            ["balancedGrip"] = new("balancedGrip", MeleeSlot.Grip, 1f, 1.15f, 1f, null,
                new Dictionary<ScrapType, int> { [ScrapType.Alloy] = 3 }),

            // Counterweight: reach and impulse.
            ["heavyWeight"] = new("heavyWeight", MeleeSlot.Counterweight, 1.10f, 0.90f, 1.15f, null,
                new Dictionary<ScrapType, int> { [ScrapType.Plating] = 3 }),

            // Core infusions — the point of the slot. Point-blank reaction fuel:
            // chill a target with the tower and shatter it with the wrench.
            // Priced the same as their gunsmith counterparts so neither route to
            // a status is cheaper than the other.
            ["emberCore"] = new("emberCore", MeleeSlot.CoreInfusion, 1f, 1f, 1f, "burn",
                new Dictionary<ScrapType, int> { [ScrapType.Flux] = 4 }),
            ["cryoCore"] = new("cryoCore", MeleeSlot.CoreInfusion, 1f, 1f, 1f, "chill",
                new Dictionary<ScrapType, int> { [ScrapType.Flux] = 3, [ScrapType.Alloy] = 2 }),
            ["voltCore"] = new("voltCore", MeleeSlot.CoreInfusion, 1f, 1f, 1f, "shock",
                new Dictionary<ScrapType, int> { [ScrapType.Flux] = 5 }),
            ["toxinCore"] = new("toxinCore", MeleeSlot.CoreInfusion, 1f, 1f, 1f, "poison",
                new Dictionary<ScrapType, int> { [ScrapType.Plating] = 3, [ScrapType.Flux] = 2 }),
        };

    /// <summary>Mastery is flat damage growth; the breakpoints at 4/7/10 are
    /// where the mechanics live (heavy attack, lunge, execute) and land with the
    /// endless grid at M4. Same per-level factor as a tower damage path, so one
    /// dial moves both.</summary>
    public const float MasteryPerLevelFactor = 1.10f;
    public const int MaxMasteryLevel = 10;
    public const int CampaignMasteryCap = 5;   // levels 6-10 unlock with endless
}

/// <summary>A melee build: one attachment per slot plus mastery. Pure factor
/// functions so a sweep can price a build without running a match.</summary>
public sealed class MeleeBuild
{
    public Dictionary<MeleeSlot, string> Attachments = new();
    public int MasteryLevel;

    private float Product(System.Func<MeleeAttachmentDef, float> pick)
    {
        float f = 1f;
        foreach (var id in Attachments.Values) f *= pick(Melee.Attachments[id]);
        return f;
    }

    public float DamageFactor() =>
        Product(a => a.DamageFactor) * DetMath.PowInt(Melee.MasteryPerLevelFactor, MasteryLevel);

    public float SpeedFactor() => Product(a => a.SpeedFactor);
    public float ReachFactor() => Product(a => a.ReachFactor);

    public IEnumerable<string> ExtraApplies()
    {
        foreach (var id in Attachments.Values)
            if (Melee.Attachments[id].Applies is { } status) yield return status;
    }
}
