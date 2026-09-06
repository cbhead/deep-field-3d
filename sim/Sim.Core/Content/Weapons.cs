namespace DeepField.Sim.Content;

public static class Weapons
{
    /// <summary>The starter sidearm — never taken away, so nobody is ever a spectator.</summary>
    public static readonly WeaponDef Sidearm = new(
        Id: "sidearm",
        Damage: 5f,
        ShotsPerSecond: 3f,
        RangeMeters: 60f);

    public static readonly IReadOnlyDictionary<string, WeaponDef> All =
        new Dictionary<string, WeaponDef>
        {
            [Sidearm.Id] = Sidearm,
        };
}
