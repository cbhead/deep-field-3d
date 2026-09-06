namespace DeepField.Sim.Content;

public static class Enemies
{
    private static readonly IReadOnlyDictionary<ScrapType, int> NoScrap =
        new Dictionary<ScrapType, int>();

    /// <summary>Baseline walker — the reference grunt every other stat hangs off.</summary>
    public static readonly EnemyDef Drifter = new(
        Id: "drifter",
        Hp: 20f, SpeedMetersPerSec: 2.5f, Bounty: 6, LeakDamage: 1,
        Layer: EnemyLayer.Ground,
        ContactDamage: 8f, ScatterWidth: 0f, BlocksSight: false,
        FrontArmorArcDegrees: 0f, FrontArmorFactor: 1f, RearWeakFactor: 1f,
        Shield: 0f, FlatArmor: 0f, Mass: 1f, Burrower: false, SplitInto: null, SplitCount: 0,
        ScrapYield: new Dictionary<ScrapType, int> { [ScrapType.Alloy] = 2 });

    /// <summary>Lateral-scatter swarm: spreads across the path width so splash
    /// placement and hero tracking both matter. Cannot split further (M2 rule).</summary>
    public static readonly EnemyDef Mote = new(
        Id: "mote",
        Hp: 7f, SpeedMetersPerSec: 3.4f, Bounty: 2, LeakDamage: 1,
        Layer: EnemyLayer.Ground,
        ContactDamage: 4f, ScatterWidth: 2.8f, BlocksSight: false,
        FrontArmorArcDegrees: 0f, FrontArmorFactor: 1f, RearWeakFactor: 1f,
        Shield: 0f, FlatArmor: 0f, Mass: 1f, Burrower: false, SplitInto: null, SplitCount: 0,
        ScrapYield: new Dictionary<ScrapType, int> { [ScrapType.Alloy] = 1 });

    /// <summary>HP wall, scaled up for 3D: a towering silhouette that blocks tower
    /// sightlines — living cover for whatever walks in its shadow.</summary>
    public static readonly EnemyDef Monolith = new(
        Id: "monolith",
        Hp: 150f, SpeedMetersPerSec: 1.1f, Bounty: 34, LeakDamage: 1,
        Layer: EnemyLayer.Ground,
        ContactDamage: 16f, ScatterWidth: 0f, BlocksSight: true,
        FrontArmorArcDegrees: 0f, FrontArmorFactor: 1f, RearWeakFactor: 1f,
        Shield: 0f, FlatArmor: 0f, Mass: 8f, Burrower: false, SplitInto: null, SplitCount: 0,
        ScrapYield: new Dictionary<ScrapType, int> { [ScrapType.Alloy] = 4, [ScrapType.Plating] = 2 });

    /// <summary>Flyer on the air lane: did you buy vertical coverage? Ground
    /// towers can't see it; Skywatch and hero fire can.</summary>
    public static readonly EnemyDef Skiff = new(
        Id: "skiff",
        Hp: 26f, SpeedMetersPerSec: 3.0f, Bounty: 10, LeakDamage: 1,
        Layer: EnemyLayer.Air,
        ContactDamage: 0f, ScatterWidth: 1.5f, BlocksSight: false,
        FrontArmorArcDegrees: 0f, FrontArmorFactor: 1f, RearWeakFactor: 1f,
        Shield: 0f, FlatArmor: 0f, Mass: 1f, Burrower: false, SplitInto: null, SplitCount: 0,
        ScrapYield: new Dictionary<ScrapType, int> { [ScrapType.Flux] = 2 });

    /// <summary>Directional armor, 140° front arc at 75% reduction; rear hits land
    /// 1.5×. Towers face its front by geometry — the first enemy a hero answers
    /// structurally, by moving.</summary>
    public static readonly EnemyDef Aegis = new(
        Id: "aegis",
        Hp: 55f, SpeedMetersPerSec: 1.6f, Bounty: 18, LeakDamage: 1,
        Layer: EnemyLayer.Ground,
        ContactDamage: 12f, ScatterWidth: 0f, BlocksSight: false,
        FrontArmorArcDegrees: 140f, FrontArmorFactor: 0.25f, RearWeakFactor: 1.5f,
        Shield: 0f, FlatArmor: 0f, Mass: 3f, Burrower: false, SplitInto: null, SplitCount: 0,
        ScrapYield: new Dictionary<ScrapType, int> { [ScrapType.Plating] = 3 });

    /// <summary>M2 — shootable bubble shield that regenerates after a lull;
    /// punishes gaps in coverage, and burn cannot ignite it while shielded.</summary>
    public static readonly EnemyDef Warden = new(
        Id: "warden",
        Hp: 60f, SpeedMetersPerSec: 1.5f, Bounty: 16, LeakDamage: 1,
        Layer: EnemyLayer.Ground,
        ContactDamage: 10f, ScatterWidth: 0f, BlocksSight: false,
        FrontArmorArcDegrees: 0f, FrontArmorFactor: 1f, RearWeakFactor: 1f,
        Shield: 25f, FlatArmor: 0f, Mass: 1f, Burrower: false, SplitInto: null, SplitCount: 0,
        ScrapYield: new Dictionary<ScrapType, int> { [ScrapType.Flux] = 3 });

    /// <summary>M2 — burrower: cycles untargetable underground and surfaces in
    /// windows; the tremor trail telegraphs where. Can you hit windows?</summary>
    public static readonly EnemyDef Mole = new(
        Id: "mole",
        Hp: 34f, SpeedMetersPerSec: 2.2f, Bounty: 12, LeakDamage: 1,
        Layer: EnemyLayer.Ground,
        ContactDamage: 6f, ScatterWidth: 0f, BlocksSight: false,
        FrontArmorArcDegrees: 0f, FrontArmorFactor: 1f, RearWeakFactor: 1f,
        Shield: 0f, FlatArmor: 0f, Mass: 1f, Burrower: true, SplitInto: null, SplitCount: 0,
        ScrapYield: new Dictionary<ScrapType, int> { [ScrapType.Alloy] = 2, [ScrapType.Flux] = 1 });

    /// <summary>M2 — dies into five low-hp Motes scattered radially (up from 3
    /// in 2D): the crowd moment that rewards splash pre-placement and cleanup.</summary>
    public static readonly EnemyDef Cluster = new(
        Id: "cluster",
        Hp: 40f, SpeedMetersPerSec: 1.9f, Bounty: 10, LeakDamage: 1,
        Layer: EnemyLayer.Ground,
        ContactDamage: 8f, ScatterWidth: 0f, BlocksSight: false,
        FrontArmorArcDegrees: 0f, FrontArmorFactor: 1f, RearWeakFactor: 1f,
        Shield: 0f, FlatArmor: 0f, Mass: 1f, Burrower: false, SplitInto: "mote", SplitCount: 5,
        ScrapYield: new Dictionary<ScrapType, int> { [ScrapType.Alloy] = 3 });

    public static readonly IReadOnlyDictionary<string, EnemyDef> All =
        new Dictionary<string, EnemyDef>
        {
            [Drifter.Id] = Drifter,
            [Mote.Id] = Mote,
            [Monolith.Id] = Monolith,
            [Warden.Id] = Warden,
            [Mole.Id] = Mole,
            [Cluster.Id] = Cluster,
            [Skiff.Id] = Skiff,
            [Aegis.Id] = Aegis,
        };
}
