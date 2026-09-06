namespace DeepField.Sim.Content;

public static class Weapons
{
    /// <summary>The starter sidearm — never taken away, so nobody is ever a spectator.</summary>
    public static readonly WeaponDef Sidearm = new(
        Id: "sidearm", Cost: 0,
        Damage: 5f, ShotsPerSecond: 3f, RangeMeters: 60f,
        Applies: System.Array.Empty<string>());

    /// <summary>Hitscan rifle: single-target priority tool. Alt-fire applies mark
    /// (modeled as its Applies — every hit marks at M1; alt-fire split arrives
    /// with gunsmith v2).</summary>
    public static readonly WeaponDef Rifle = new(
        Id: "rifle", Cost: 120,
        Damage: 11f, ShotsPerSecond: 2.2f, RangeMeters: 80f,
        Applies: new[] { "mark" });

    /// <summary>Close-range burst: the panic weapon for swarms at the perch.</summary>
    public static readonly WeaponDef Scattergun = new(
        Id: "scattergun", Cost: 100,
        Damage: 24f, ShotsPerSecond: 1.1f, RangeMeters: 14f,
        Applies: System.Array.Empty<string>());

    /// <summary>Ember pistol: low dps, applies burn — the shooter's half of the
    /// Thermal Shock combo when no Ember player is in the lobby.</summary>
    public static readonly WeaponDef EmberPistol = new(
        Id: "emberPistol", Cost: 90,
        Damage: 4f, ShotsPerSecond: 2.0f, RangeMeters: 40f,
        Applies: new[] { "burn" });

    public static readonly IReadOnlyDictionary<string, WeaponDef> All =
        new Dictionary<string, WeaponDef>
        {
            [Sidearm.Id] = Sidearm,
            [Rifle.Id] = Rifle,
            [Scattergun.Id] = Scattergun,
            [EmberPistol.Id] = EmberPistol,
        };
}
