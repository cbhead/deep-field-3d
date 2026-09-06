namespace DeepField.Sim.Content;

public static class Enemies
{
    /// <summary>Baseline walker — the reference grunt every other stat hangs off.</summary>
    public static readonly EnemyDef Drifter = new(
        Id: "drifter",
        Hp: 20f,
        SpeedMetersPerSec: 2.5f,
        Bounty: 6,
        LeakDamage: 1);

    public static readonly IReadOnlyDictionary<string, EnemyDef> All =
        new Dictionary<string, EnemyDef>
        {
            [Drifter.Id] = Drifter,
        };
}
