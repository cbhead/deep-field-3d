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
            // The strand enters and leaves low — ground Skywatches answer the
            // dips — but climbs to 13 m across the middle, where the only
            // sockets that reach it are on the upper deck. Before this it flew
            // at a flat 8 m straight over the yard, which put 100% of it inside
            // ground range and left the deck with nothing of its own to do.
            new RouteDef("air", EnemyLayer.Air, new[]
            {
                new Vec3(-40f, 9f, -4f),
                new Vec3(-14f, 13f, -14f),
                new Vec3(8f, 13f, -16f),
                new Vec3(36f, 9f, 4f),
            }),
        },
        // The deck is deliberately three rows deep — a front lip (w4-w7) that
        // overlooks the lane with reach to spare, a middle row (w1-w3), and a
        // back row (w8/w9) that trades the lane for the air strand. That spread
        // is the placement decision the deck exists to offer, so it survived
        // the margin pass untouched: pulling every socket forward would have
        // collapsed three choices into one.
        //
        // Twelve sockets left exactly one defence to build, so every match
        // looked the same. The graph is now dense enough that placement is a
        // decision: both flanks of most legs are buildable, corners are
        // contested, and the deck can cover the yard or the air lane but not
        // comfortably both. Every original id keeps its original position, so
        // the harness's scripted build orders — and therefore the balance
        // gates — measure exactly what they measured before.
        Sockets: new[]
        {
            // --- Ground, west approach (legs A and B).
            new SocketDef("g7", new Vec3(-34f, 0f, -6f), SocketTag.Ground),
            new SocketDef("g8", new Vec3(-33f, 0f, 7f), SocketTag.Ground),
            new SocketDef("g9", new Vec3(-26f, 0f, -6f), SocketTag.Ground),
            new SocketDef("g1", new Vec3(-24f, 0f, 6f), SocketTag.Ground),
            new SocketDef("g11", new Vec3(-26f, 0f, 14f), SocketTag.Ground),
            new SocketDef("g10", new Vec3(-14f, 0f, -4f), SocketTag.Ground),

            // --- Ground, north sweep (leg C).
            new SocketDef("g2", new Vec3(-14f, 0f, 9f), SocketTag.Ground),
            new SocketDef("g12", new Vec3(-16f, 0f, 20f), SocketTag.Ground),
            new SocketDef("g13", new Vec3(-6f, 0f, 20f), SocketTag.Ground),
            new SocketDef("g3", new Vec3(-4f, 0f, 8f), SocketTag.Ground),

            // --- Ground, the long spine (leg D) — both flanks.
            new SocketDef("g14", new Vec3(6f, 0f, 12f), SocketTag.Ground),
            new SocketDef("g4", new Vec3(4f, 0f, 0f), SocketTag.Ground),
            new SocketDef("g15", new Vec3(9f, 0f, 4f), SocketTag.Ground),
            new SocketDef("g16", new Vec3(-5f, 0f, -5f), SocketTag.Ground),

            // --- Ground, the elbow and the run to the core (legs E, F, G).
            new SocketDef("g5", new Vec3(12f, 0f, -2f), SocketTag.Ground),
            new SocketDef("g17", new Vec3(0f, 0f, -13f), SocketTag.Ground),
            new SocketDef("g18", new Vec3(22f, 0f, -13f), SocketTag.Ground),
            new SocketDef("g6", new Vec3(24f, 0f, 0f), SocketTag.Ground),
            new SocketDef("g19", new Vec3(30f, 0f, 0f), SocketTag.Ground),
            new SocketDef("g20", new Vec3(30f, 0f, 13f), SocketTag.Ground),
            new SocketDef("g21", new Vec3(14f, 0f, 10f), SocketTag.Ground),
            new SocketDef("g22", new Vec3(36f, 0f, -2f), SocketTag.Ground),

            // --- Upper deck. The north row overlooks the ground lane; the deep
            // row trades that for a longer sightline down the air strand.
            new SocketDef("w1", new Vec3(-6f, 6f, -16f), SocketTag.Wall),
            new SocketDef("w2", new Vec3(2f, 6f, -16f), SocketTag.Wall),
            new SocketDef("w3", new Vec3(10f, 6f, -16f), SocketTag.Wall),
            new SocketDef("w4", new Vec3(-10f, 6f, -12.8f), SocketTag.Wall),
            new SocketDef("w5", new Vec3(-2f, 6f, -12.8f), SocketTag.Wall),
            new SocketDef("w6", new Vec3(6f, 6f, -12.8f), SocketTag.Wall),
            new SocketDef("w7", new Vec3(14f, 6f, -12.8f), SocketTag.Wall),
            new SocketDef("w8", new Vec3(-10f, 6f, -19.5f), SocketTag.Wall),
            new SocketDef("w9", new Vec3(6f, 6f, -19.5f), SocketTag.Wall),
            // Gantry bridge — it reaches out over the lane's elbow, so these
            // two look straight down at the corner enemies have to turn.
            new SocketDef("w10", new Vec3(9f, 6f, -7f), SocketTag.Wall),
            new SocketDef("w11", new Vec3(9f, 6f, -1f), SocketTag.Wall),

            // --- Path plates. Traps are consumable, so density here is about
            // choosing where to spend them, not about holding every metre.
            new SocketDef("t4", new Vec3(-30f, 0f, 0f), SocketTag.Trap),
            new SocketDef("t1", new Vec3(-20f, 0f, 7f), SocketTag.Trap),
            new SocketDef("t5", new Vec3(-20f, 0f, 13f), SocketTag.Trap),
            new SocketDef("t6", new Vec3(-10f, 0f, 14f), SocketTag.Trap),
            new SocketDef("t7", new Vec3(0f, 0f, 10f), SocketTag.Trap),
            new SocketDef("t2", new Vec3(0f, 0f, 3f), SocketTag.Trap),
            new SocketDef("t8", new Vec3(0f, 0f, -4f), SocketTag.Trap),
            new SocketDef("t9", new Vec3(9f, 0f, -8f), SocketTag.Trap),
            new SocketDef("t3", new Vec3(18f, 0f, -1f), SocketTag.Trap),
            new SocketDef("t10", new Vec3(18f, 0f, 5f), SocketTag.Trap),
            new SocketDef("t11", new Vec3(28f, 0f, 6f), SocketTag.Trap),
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
        TotalWaves: 10,
        // Fog on W9, late enough that a team has towers worth blinding.
        ConditionScheduleOrNull: new Dictionary<int, string>
        {
            [8] = Conditions.Fog.Id,
        });

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
            // Same shape as Foundry: low at the mouths, 15 m across the middle
            // where only the upper catwalk reaches. The comment on w3/w4 had
            // always claimed this; the geometry never backed it up.
            new RouteDef("air", EnemyLayer.Air, new[]
            {
                new Vec3(-45f, 9f, 4f),
                new Vec3(-14f, 15f, 1f),
                new Vec3(14f, 15f, -2f),
                new Vec3(40f, 9f, 6f),
            }),
        },
        // Same density pass as Foundry, with the extra job of covering two
        // ground routes: sockets that watch the switchback are mostly blind to
        // the freight cut and vice versa, so the barricade decision now comes
        // with a real question about which of them you already paid to cover.
        Sockets: new[]
        {
            // --- Ground, the west yard where both routes still share a mouth.
            new SocketDef("g8", new Vec3(-38f, 0f, -1f), SocketTag.Ground),
            new SocketDef("g20", new Vec3(-28f, 0f, -18f), SocketTag.Ground),
            new SocketDef("g1", new Vec3(-30f, 0f, 2f), SocketTag.Ground),
            new SocketDef("g9", new Vec3(-32f, 0f, 8f), SocketTag.Ground),

            // --- Ground, the long switchback.
            new SocketDef("g2", new Vec3(-18f, 0f, 8f), SocketTag.Ground),
            new SocketDef("g10", new Vec3(-20f, 0f, 16f), SocketTag.Ground),
            new SocketDef("g11", new Vec3(-10f, 0f, 16f), SocketTag.Ground),
            new SocketDef("g3", new Vec3(-10f, 0f, 4f), SocketTag.Ground),
            new SocketDef("g13", new Vec3(2f, 0f, 8f), SocketTag.Ground),
            new SocketDef("g21", new Vec3(8f, 0f, 16f), SocketTag.Ground),

            // --- Ground, over the freight cut — these are the sockets that go
            // quiet the moment b1 closes it.
            new SocketDef("g12", new Vec3(-14f, 0f, -6f), SocketTag.Ground),
            new SocketDef("g4", new Vec3(0f, 0f, -4f), SocketTag.Ground),
            new SocketDef("g14", new Vec3(4f, 0f, -8f), SocketTag.Ground),

            // --- Ground, the east half and the core approach.
            new SocketDef("g5", new Vec3(10f, 0f, 5f), SocketTag.Ground),
            new SocketDef("g15", new Vec3(10f, 0f, -6f), SocketTag.Ground),
            new SocketDef("g6", new Vec3(20f, 0f, -4f), SocketTag.Ground),
            new SocketDef("g16", new Vec3(24f, 0f, 0f), SocketTag.Ground),
            new SocketDef("g17", new Vec3(26f, 0f, -6f), SocketTag.Ground),
            new SocketDef("g7", new Vec3(30f, 0f, 2f), SocketTag.Ground),
            new SocketDef("g18", new Vec3(34f, 0f, -2f), SocketTag.Ground),
            new SocketDef("g19", new Vec3(36f, 0f, 14f), SocketTag.Ground),

            // --- Mid deck (y=5) over the freight cut.
            new SocketDef("w1", new Vec3(-12f, 5f, -18f), SocketTag.Wall),
            new SocketDef("w2", new Vec3(0f, 5f, -18f), SocketTag.Wall),
            new SocketDef("w5", new Vec3(-16.5f, 5f, -15f), SocketTag.Wall),
            new SocketDef("w6", new Vec3(-6f, 5f, -16f), SocketTag.Wall),
            new SocketDef("w7", new Vec3(5f, 5f, -21f), SocketTag.Wall),

            // --- Upper catwalk (y=10) hangs over the air lane — the only
            // sockets that see the whole strand rather than its low dips.
            new SocketDef("w3", new Vec3(-4f, 10f, 6f), SocketTag.Wall),
            new SocketDef("w4", new Vec3(10f, 10f, 0f), SocketTag.Wall),
            new SocketDef("w8", new Vec3(-12f, 10f, 2f), SocketTag.Wall),
            new SocketDef("w9", new Vec3(2f, 10f, 4f), SocketTag.Wall),

            // --- Path plates on both routes.
            new SocketDef("t5", new Vec3(-35f, 0f, -10f), SocketTag.Trap),
            new SocketDef("t13", new Vec3(-32f, 0f, -5.84f), SocketTag.Trap),
            new SocketDef("t1", new Vec3(-25f, 0f, 0f), SocketTag.Trap),
            new SocketDef("t6", new Vec3(-25f, 0f, 8f), SocketTag.Trap),
            new SocketDef("t7", new Vec3(-15f, 0f, 12f), SocketTag.Trap),
            new SocketDef("t8", new Vec3(-5f, 0f, 8f), SocketTag.Trap),
            new SocketDef("t2", new Vec3(-5f, 0f, 0f), SocketTag.Trap),
            new SocketDef("t9", new Vec3(-5f, 0f, -8f), SocketTag.Trap),
            new SocketDef("t4", new Vec3(5f, 0f, 0f), SocketTag.Trap),
            new SocketDef("t10", new Vec3(5f, 0f, -12f), SocketTag.Trap),
            new SocketDef("t3", new Vec3(15f, 0f, -2f), SocketTag.Trap),
            new SocketDef("t11", new Vec3(15f, 0f, 6f), SocketTag.Trap),
            new SocketDef("t12", new Vec3(25f, 0f, 9.2f), SocketTag.Trap),

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
        TotalWaves: 12,
        // One condition per sector, the "one to learn on" convention applied to
        // weather. Night lands on the Shade wave (W9), where more stealth and
        // slower acquisition sharpen exactly the lesson the wave already
        // teaches; Fog is sector 1's, so a player meets them one at a time.
        // Both on one map was measured and cut: each costs the campaign about
        // two lives and the arc only carries three.
        // A third condition wave on the finale was tried and cut: at a quarter
        // of the campaign, weather stopped being an event and became the
        // baseline, which is the one thing a factor-over-baseline system must
        // not do.
        ConditionScheduleOrNull: new Dictionary<int, string>
        {
            [8] = Conditions.Night.Id,
        });

    public static readonly IReadOnlyDictionary<string, MapDef> All =
        new Dictionary<string, MapDef>
        {
            [TestLane.Id] = TestLane,
            [Foundry.Id] = Foundry,
            [Switchyard.Id] = Switchyard,
        };
}
