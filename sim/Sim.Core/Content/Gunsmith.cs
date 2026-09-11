namespace DeepField.Sim.Content;

/// <summary>COD-grade weapon building: every platform exposes attachment slots;
/// attachments are stat-delta rows crafted from scrap recipes; ammo is its own
/// dimension. The wave you just fought funds the counter you build next.</summary>
public enum AttachmentSlot
{
    Barrel,
    Muzzle,
    Optic,
    Magazine,
    Stock,
    Underbarrel,
    Infusion,   // the status-applier slot
}

public sealed record AttachmentDef(
    string Id,
    AttachmentSlot Slot,
    float DamageFactor,
    float RateFactor,
    float RangeFactor,
    string? Applies,              // infusion slot: status added to every hit
    IReadOnlyDictionary<ScrapType, int> Recipe);

public static class Attachments
{
    public static readonly IReadOnlyDictionary<string, AttachmentDef> All =
        new Dictionary<string, AttachmentDef>
        {
            // Barrels: the range/handling trade.
            ["longBarrel"] = new("longBarrel", AttachmentSlot.Barrel, 1.10f, 0.92f, 1.25f, null,
                new Dictionary<ScrapType, int> { [ScrapType.Alloy] = 6 }),
            ["shortBarrel"] = new("shortBarrel", AttachmentSlot.Barrel, 0.95f, 1.18f, 0.8f, null,
                new Dictionary<ScrapType, int> { [ScrapType.Alloy] = 4 }),

            // Muzzle.
            ["compensator"] = new("compensator", AttachmentSlot.Muzzle, 1.12f, 1f, 1f, null,
                new Dictionary<ScrapType, int> { [ScrapType.Alloy] = 4, [ScrapType.Plating] = 2 }),

            // Optic: range discipline.
            ["rangefinder"] = new("rangefinder", AttachmentSlot.Optic, 1f, 1f, 1.2f, null,
                new Dictionary<ScrapType, int> { [ScrapType.Flux] = 3 }),

            // Magazine: sustained output.
            ["drumFeed"] = new("drumFeed", AttachmentSlot.Magazine, 1f, 1.15f, 1f, null,
                new Dictionary<ScrapType, int> { [ScrapType.Alloy] = 5, [ScrapType.Flux] = 2 }),

            // Stock: handling.
            ["braceStock"] = new("braceStock", AttachmentSlot.Stock, 1f, 1.08f, 1.05f, null,
                new Dictionary<ScrapType, int> { [ScrapType.Alloy] = 3 }),

            // Underbarrel: heavy hitter, Gravium-hungry.
            ["stabilizer"] = new("stabilizer", AttachmentSlot.Underbarrel, 1.18f, 0.95f, 1f, null,
                new Dictionary<ScrapType, int> { [ScrapType.Gravium] = 2 }),

            // Infusions: the status slot, priced in the scrap of what they answer.
            ["emberCoil"] = new("emberCoil", AttachmentSlot.Infusion, 1f, 1f, 1f, "burn",
                new Dictionary<ScrapType, int> { [ScrapType.Flux] = 4 }),
            ["cryoCell"] = new("cryoCell", AttachmentSlot.Infusion, 1f, 1f, 1f, "chill",
                new Dictionary<ScrapType, int> { [ScrapType.Flux] = 3, [ScrapType.Alloy] = 2 }),
            ["voltCap"] = new("voltCap", AttachmentSlot.Infusion, 1f, 1f, 1f, "shock",
                new Dictionary<ScrapType, int> { [ScrapType.Flux] = 5 }),
            // M3 — toxin feed. Priced in Plating: you craft the armour answer
            // out of the armour that stopped you, the same rule AP ammo follows.
            ["toxinFeed"] = new("toxinFeed", AttachmentSlot.Infusion, 1f, 1f, 1f, "poison",
                new Dictionary<ScrapType, int> { [ScrapType.Plating] = 3, [ScrapType.Flux] = 2 }),
        };
}

public sealed record AmmoDef(
    string Id,
    float DamageFactor,
    bool IgnoresFlatArmor,        // AP: crafted from the Plating that stopped you
    float UnarmoredBonusFactor,   // hollow-point: shreds soft targets only
    string? Applies,
    IReadOnlyDictionary<ScrapType, int> Recipe);

public static class Ammo
{
    public static readonly AmmoDef Standard = new("standard", 1f, false, 1f, null,
        new Dictionary<ScrapType, int>());

    public static readonly IReadOnlyDictionary<string, AmmoDef> All =
        new Dictionary<string, AmmoDef>
        {
            [Standard.Id] = Standard,
            ["ap"] = new("ap", 0.85f, true, 1f, null,
                new Dictionary<ScrapType, int> { [ScrapType.Plating] = 4 }),
            ["hollowPoint"] = new("hollowPoint", 1f, false, 1.3f, null,
                new Dictionary<ScrapType, int> { [ScrapType.Alloy] = 5 }),
            ["incendiary"] = new("incendiary", 0.9f, false, 1f, "burn",
                new Dictionary<ScrapType, int> { [ScrapType.Flux] = 3, [ScrapType.Alloy] = 2 }),
            ["cryo"] = new("cryo", 0.9f, false, 1f, "chill",
                new Dictionary<ScrapType, int> { [ScrapType.Flux] = 4 }),
            // Ids match the icon names in DESIGN-BRIEF.md (icon_ammo_<id>): the
            // first M3 rows were "cryoRounds" / "toxinRounds" / "shockRounds"
            // and asked the armory for icons design never named.
            // M3 — the two status rounds. Toxin trades the most raw damage
            // because what it applies bypasses armour and shields entirely;
            // shock trades least because its value is a reaction it can't
            // trigger alone.
            ["toxin"] = new("toxin", 0.8f, false, 1f, "poison",
                new Dictionary<ScrapType, int> { [ScrapType.Plating] = 3, [ScrapType.Gravium] = 1 }),
            ["shock"] = new("shock", 0.92f, false, 1f, "shock",
                new Dictionary<ScrapType, int> { [ScrapType.Flux] = 5 }),
        };
}

/// <summary>A weapon build: chosen attachment per slot + ammo. Effective stats
/// are pure functions so the harness sweeps builds, not just platforms.</summary>
public sealed class WeaponBuild
{
    public Dictionary<AttachmentSlot, string> Attachments = new();
    public string AmmoId = Ammo.Standard.Id;

    /// <summary>How many times this weapon has been through the Pack a Punch.
    /// Uncapped by design — attachments are a build you finish, this is a sink
    /// that never closes.</summary>
    public int PackLevel;

    /// <summary>What the next level costs, in Alloy. Level 1 is
    /// <see cref="Balance.PackFirstCost"/> and each one after multiplies.</summary>
    public int NextPackCost => PackCostAt(PackLevel + 1);

    public static int PackCostAt(int level) => level < 1
        ? Balance.PackFirstCost
        : (int)MathF.Round(Balance.PackFirstCost
            * MathF.Pow(Balance.PackCostGrowth, level - 1), MidpointRounding.AwayFromZero);

    /// <summary>Gravium the next level wants on top of its Alloy, or zero.</summary>
    public int NextPackGravium => PackGraviumAt(PackLevel + 1);

    /// <summary>Every <see cref="Balance.PackGraviumEvery"/>th level, growing
    /// by a step each milestone: 4 at level 5, 8 at 10, 12 at 15.</summary>
    public static int PackGraviumAt(int level) =>
        level >= 1 && level % Balance.PackGraviumEvery == 0
            ? Balance.PackGraviumPerStep * (level / Balance.PackGraviumEvery)
            : 0;

    public float PackDamageFactor => MathF.Pow(Balance.PackDamagePerLevel, PackLevel);
    public float PackRateFactor => MathF.Pow(Balance.PackRatePerLevel, PackLevel);

    public float DamageFactor(bool targetArmored)
    {
        float f = PackDamageFactor;
        foreach (var id in Attachments.Values) f *= Content.Attachments.All[id].DamageFactor;
        var ammo = Content.Ammo.All[AmmoId];
        f *= ammo.DamageFactor;
        if (!targetArmored) f *= ammo.UnarmoredBonusFactor;
        return f;
    }

    public float RateFactor()
    {
        float f = PackRateFactor;
        foreach (var id in Attachments.Values) f *= Content.Attachments.All[id].RateFactor;
        return f;
    }

    public float RangeFactor()
    {
        float f = 1f;
        foreach (var id in Attachments.Values) f *= Content.Attachments.All[id].RangeFactor;
        return f;
    }

    public bool IgnoresFlatArmor => Content.Ammo.All[AmmoId].IgnoresFlatArmor;

    public IEnumerable<string> ExtraApplies()
    {
        foreach (var id in Attachments.Values)
            if (Content.Attachments.All[id].Applies is { } status) yield return status;
        if (Content.Ammo.All[AmmoId].Applies is { } ammoStatus) yield return ammoStatus;
    }
}
