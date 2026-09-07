namespace DeepField.Sim.Content;

public static class Weapons
{
    /// <summary>The starter sidearm — never taken away, so nobody is ever a spectator.</summary>
    public static readonly WeaponDef Sidearm = new(
        Id: "sidearm", Cost: 0,
        Damage: 5f, ShotsPerSecond: 3f, RangeMeters: 60f,
        Applies: System.Array.Empty<string>(),
        MagazineSize: 12, ReloadSeconds: 1.3f, Automatic: false);

    /// <summary>Hitscan rifle: single-target priority tool. Alt-fire applies mark
    /// (modeled as its Applies — every hit marks at M1; alt-fire split arrives
    /// with gunsmith v2).</summary>
    public static readonly WeaponDef Rifle = new(
        Id: "rifle", Cost: 120,
        Damage: 11f, ShotsPerSecond: 2.2f, RangeMeters: 80f,
        Applies: new[] { "mark" },
        MagazineSize: 24, ReloadSeconds: 1.9f, Automatic: true);

    /// <summary>Close-range burst: the panic weapon for swarms at the perch.</summary>
    public static readonly WeaponDef Scattergun = new(
        Id: "scattergun", Cost: 100,
        Damage: 24f, ShotsPerSecond: 1.1f, RangeMeters: 14f,
        Applies: System.Array.Empty<string>(),
        MagazineSize: 6, ReloadSeconds: 2.4f, Automatic: false);

    /// <summary>Ember pistol: low dps, applies burn — the shooter's half of the
    /// Thermal Shock combo when no Ember player is in the lobby.</summary>
    public static readonly WeaponDef EmberPistol = new(
        Id: "emberPistol", Cost: 90,
        Damage: 4f, ShotsPerSecond: 2.0f, RangeMeters: 40f,
        Applies: new[] { "burn" },
        MagazineSize: 10, ReloadSeconds: 1.5f, Automatic: false);

    /// <summary>M3 status-applier: low direct damage, high rate, applies poison.
    /// The hero half of the toxin answer — it rots a Warden that eats fire whole
    /// and an Aegis that shrugs off chip, because poison ignores both.</summary>
    public static readonly WeaponDef PoisonStream = new(
        Id: "poisonStream", Cost: 110,
        Damage: 2f, ShotsPerSecond: 6f, RangeMeters: 18f,
        Applies: new[] { "poison" },
        MagazineSize: 40, ReloadSeconds: 2.2f, Automatic: true);

    /// <summary>M3 — the chill applier. Pairs with a Singularity for Flash
    /// Freeze, or with any burn source for Thermal Shock: its job is to be the
    /// hero half of a reaction the builder sets up.</summary>
    public static readonly WeaponDef CryoSprayer = new(
        Id: "cryoSprayer", Cost: 105,
        Damage: 1.6f, ShotsPerSecond: 7f, RangeMeters: 14f,
        Applies: new[] { "chill" },
        MagazineSize: 45, ReloadSeconds: 2.1f, Automatic: true);

    public static readonly IReadOnlyDictionary<string, WeaponDef> All =
        new Dictionary<string, WeaponDef>
        {
            [CryoSprayer.Id] = CryoSprayer,
            [Sidearm.Id] = Sidearm,
            [Rifle.Id] = Rifle,
            [Scattergun.Id] = Scattergun,
            [EmberPistol.Id] = EmberPistol,
            [PoisonStream.Id] = PoisonStream,
        };
}
