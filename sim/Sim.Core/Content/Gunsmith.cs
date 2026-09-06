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
            ["cryoRounds"] = new("cryoRounds", 0.9f, false, 1f, "chill",
                new Dictionary<ScrapType, int> { [ScrapType.Flux] = 4 }),
        };
}

/// <summary>A weapon build: chosen attachment per slot + ammo. Effective stats
/// are pure functions so the harness sweeps builds, not just platforms.</summary>
public sealed class WeaponBuild
{
    public Dictionary<AttachmentSlot, string> Attachments = new();
    public string AmmoId = Ammo.Standard.Id;

    public float DamageFactor(bool targetArmored)
    {
        float f = 1f;
        foreach (var id in Attachments.Values) f *= Content.Attachments.All[id].DamageFactor;
        var ammo = Content.Ammo.All[AmmoId];
        f *= ammo.DamageFactor;
        if (!targetArmored) f *= ammo.UnarmoredBonusFactor;
        return f;
    }

    public float RateFactor()
    {
        float f = 1f;
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
