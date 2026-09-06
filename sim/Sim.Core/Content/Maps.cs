namespace DeepField.Sim.Content;

public static class Maps
{
    /// <summary>M0 graybox: one S-curve ground lane. Kept as the harness's
    /// minimal fixture and the determinism gate's fixed stage.</summary>
    public static readonly MapDef TestLane = new(
        Id: "testlane",
        Routes: new[]
        {
            new RouteDef("ground", EnemyLayer.Ground, new[]
            {
                new Vec3(-30f, 0f, 0f),
                new Vec3(-12f, 0f, 0f),
                new Vec3(-12f, 0f, 10f),
                new Vec3(8f, 0f, 10f),
                new Vec3(8f, 0f, -6f),
                new Vec3(30f, 0f, -6f),
            }),
        },
        Sockets: new[]
        {
            new SocketDef("s1", new Vec3(-16f, 0f, 5f), SocketTag.Ground),
            new SocketDef("s2", new Vec3(-6f, 0f, 6f), SocketTag.Ground),
            new SocketDef("s3", new Vec3(4f, 0f, 2f), SocketTag.Ground),
            new SocketDef("s4", new Vec3(12f, 0f, -2f), SocketTag.Ground),
        },
        HeroSpawn: new Vec3(0f, 0f, -14f),
        ArmoryPos: new Vec3(-4f, 0f, -14f),
        HeroStations: new[]
        {
            new HeroStationDef("mid", new Vec3(0f, 0f, -2f)),
        },
        TotalWaves: 3);

    /// <summary>The M1 vertical slice: two tiers, ground lane winding under a
    /// catwalk, an air lane overhead, wall sockets on the upper deck, trap
    /// sockets on the path (buildable at M2). Traversal (zipline, launcher,
    /// ladder, vent) is client geometry; the sim sees hero stations.</summary>
    public static readonly MapDef Foundry = new(
        Id: "foundry",
        Routes: new[]
        {
            new RouteDef("ground", EnemyLayer.Ground, new[]
            {
                new Vec3(-40f, 0f, 0f),
                new Vec3(-20f, 0f, 0f),
                new Vec3(-20f, 0f, 14f),
                new Vec3(0f, 0f, 14f),
                new Vec3(0f, 0f, -8f),
                new Vec3(18f, 0f, -8f),
                new Vec3(18f, 0f, 6f),
                new Vec3(36f, 0f, 6f),
            }),
            new RouteDef("air", EnemyLayer.Air, new[]
            {
                new Vec3(-40f, 8f, -6f),
                new Vec3(-10f, 9f, -2f),
                new Vec3(14f, 8f, 4f),
                new Vec3(36f, 8f, 5f),
            }),
        },
        Sockets: new[]
        {
            // Lower yard.
            new SocketDef("g1", new Vec3(-24f, 0f, 6f), SocketTag.Ground),
            new SocketDef("g2", new Vec3(-14f, 0f, 9f), SocketTag.Ground),
            new SocketDef("g3", new Vec3(-4f, 0f, 8f), SocketTag.Ground),
            new SocketDef("g4", new Vec3(4f, 0f, 0f), SocketTag.Ground),
            new SocketDef("g5", new Vec3(12f, 0f, -2f), SocketTag.Ground),
            new SocketDef("g6", new Vec3(24f, 0f, 0f), SocketTag.Ground),
            // Upper deck edge — overlooks the middle of the ground lane and the
            // air lane's midpoint; only reachable by ladder/launcher.
            new SocketDef("w1", new Vec3(-6f, 6f, -16f), SocketTag.Wall),
            new SocketDef("w2", new Vec3(2f, 6f, -16f), SocketTag.Wall),
            new SocketDef("w3", new Vec3(10f, 6f, -16f), SocketTag.Wall),
            // Path floor traps (M2 content; validated but unbuildable at M1).
            new SocketDef("t1", new Vec3(-20f, 0f, 7f), SocketTag.Trap),
            new SocketDef("t2", new Vec3(0f, 0f, 3f), SocketTag.Trap),
            new SocketDef("t3", new Vec3(18f, 0f, -1f), SocketTag.Trap),
        },
        HeroSpawn: new Vec3(0f, 0f, -24f),
        ArmoryPos: new Vec3(-6f, 0f, -24f),
        HeroStations: new[]
        {
            new HeroStationDef("spawnYard", new Vec3(0f, 0f, -20f)),
            new HeroStationDef("upperDeck", new Vec3(2f, 6f, -15f)),
            new HeroStationDef("midLane", new Vec3(-8f, 0f, 5f)),
            new HeroStationDef("coreGate", new Vec3(28f, 0f, 3f)),
        },
        TotalWaves: 10);

    /// <summary>M2 — "Switchyard": three tiers and the barricade lesson. The
    /// ground shortcut through the freight cut is fast and badly covered; a
    /// barricade on b1 closes it and forces the long switchback climb past the
    /// kill-boxes. Wall sockets sit on two separate deck heights.</summary>
    public static readonly MapDef Switchyard = new(
        Id: "switchyard",
        Routes: new[]
        {
            // The long way: switchbacks under both decks.
            new RouteDef("ground", EnemyLayer.Ground, new[]
            {
                new Vec3(-45f, 0f, -10f),
                new Vec3(-25f, 0f, -10f),
                new Vec3(-25f, 0f, 12f),
                new Vec3(-5f, 0f, 12f),
                new Vec3(-5f, 0f, -12f),
                new Vec3(15f, 0f, -12f),
                new Vec3(15f, 0f, 10f),
                new Vec3(40f, 0f, 8f),
            }),
            // The freight cut: straight through the middle, gated by b1.
            new RouteDef("groundShort", EnemyLayer.Ground, new[]
            {
                new Vec3(-45f, 0f, -10f),
                new Vec3(-20f, 0f, -2f),
                new Vec3(5f, 0f, 0f),
                new Vec3(40f, 0f, 8f),
            }, BarricadeGate: "b1", FallbackRouteId: "ground"),
            new RouteDef("air", EnemyLayer.Air, new[]
            {
                new Vec3(-45f, 9f, 4f),
                new Vec3(-12f, 10f, 0f),
                new Vec3(16f, 9f, -4f),
                new Vec3(40f, 9f, 6f),
            }),
        },
        Sockets: new[]
        {
            new SocketDef("g1", new Vec3(-30f, 0f, 2f), SocketTag.Ground),
            new SocketDef("g2", new Vec3(-18f, 0f, 8f), SocketTag.Ground),
            new SocketDef("g3", new Vec3(-10f, 0f, 4f), SocketTag.Ground),
            new SocketDef("g4", new Vec3(0f, 0f, -4f), SocketTag.Ground),
            new SocketDef("g5", new Vec3(10f, 0f, 2f), SocketTag.Ground),
            new SocketDef("g6", new Vec3(20f, 0f, -4f), SocketTag.Ground),
            new SocketDef("g7", new Vec3(30f, 0f, 2f), SocketTag.Ground),
            // Mid deck (y=5) over the freight cut; upper catwalk (y=10) sees both.
            new SocketDef("w1", new Vec3(-12f, 5f, -18f), SocketTag.Wall),
            new SocketDef("w2", new Vec3(0f, 5f, -18f), SocketTag.Wall),
            // Upper catwalk hangs over the air lane — the only sockets that can
            // see the whole strand (ground Skywatches only reach its low dips).
            new SocketDef("w3", new Vec3(-4f, 10f, 6f), SocketTag.Wall),
            new SocketDef("w4", new Vec3(10f, 10f, 0f), SocketTag.Wall),
            new SocketDef("t1", new Vec3(-25f, 0f, 0f), SocketTag.Trap),
            new SocketDef("t2", new Vec3(-5f, 0f, 0f), SocketTag.Trap),
            new SocketDef("t3", new Vec3(15f, 0f, -2f), SocketTag.Trap),
            new SocketDef("t4", new Vec3(5f, 0f, 0f), SocketTag.Trap),
            new SocketDef("b1", new Vec3(-8f, 0f, -1f), SocketTag.Barricade),
        },
        HeroSpawn: new Vec3(0f, 0f, -26f),
        ArmoryPos: new Vec3(6f, 0f, -26f),
        HeroStations: new[]
        {
            new HeroStationDef("yard", new Vec3(0f, 0f, -22f)),
            new HeroStationDef("midDeck", new Vec3(-6f, 5f, -17f)),
            new HeroStationDef("catwalk", new Vec3(2f, 10f, 3f)),
            new HeroStationDef("cutMouth", new Vec3(-16f, 0f, -2f)),
            new HeroStationDef("coreGate", new Vec3(32f, 0f, 5f)),
        },
        TotalWaves: 12);

    public static readonly IReadOnlyDictionary<string, MapDef> All =
        new Dictionary<string, MapDef>
        {
            [TestLane.Id] = TestLane,
            [Foundry.Id] = Foundry,
            [Switchyard.Id] = Switchyard,
        };
}
