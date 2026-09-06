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

    public static readonly IReadOnlyDictionary<string, MapDef> All =
        new Dictionary<string, MapDef>
        {
            [TestLane.Id] = TestLane,
            [Foundry.Id] = Foundry,
        };
}
