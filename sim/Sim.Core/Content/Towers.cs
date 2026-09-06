namespace DeepField.Sim.Content;

public static class Towers
{
    /// <summary>Railgun corridor tower. Pierce arrives with the full projectile
    /// pipeline; M0 fires single-target homing bolts.</summary>
    public static readonly TowerDef Lance = new(
        Id: "lance",
        Cost: 75,
        RangeMeters: 12f,
        Damage: 8f,
        ShotsPerSecond: 1.6f,
        ProjectileSpeed: 30f);

    public static readonly IReadOnlyDictionary<string, TowerDef> All =
        new Dictionary<string, TowerDef>
        {
            [Lance.Id] = Lance,
        };
}
