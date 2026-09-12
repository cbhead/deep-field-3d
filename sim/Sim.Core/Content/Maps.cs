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
            // The deck's north face is z −12 and a build pad is 1.1 m across,
            // so at z −12.8 all four of these hung 30 cm of pad out over the
            // edge. Pulled back to −13.4, which leaves the pad wholly on the
            // deck and still puts the tower at the rail looking down into the
            // lane. The deck cannot grow north instead: its south ladder
            // stands at z −11.3, just clear of the face, and burying that
            // again is the "ladder to nowhere" this map has already had once.
            // The air strand climbs from the west mouth to 13 m by x −14, and
            // between x −33 and x −21 only two pads on the whole map could
            // reach it — the deck is thirty metres east. This one sits between
            // the two lanes: nine metres north of the ground route, so a Lance
            // still answers that, and inside a Skywatch's reach of the strand
            // overhead. A second pad at x −24 was tried and cut: it landed two
            // metres from g9, and two pads close enough to overlap read as one
            // blurry option.
            new SocketDef("g23", new Vec3(-30f, 0f, -9f), SocketTag.Ground),

            new SocketDef("w4", new Vec3(-10f, 6f, -13.4f), SocketTag.Wall),
            new SocketDef("w5", new Vec3(-2f, 6f, -13.4f), SocketTag.Wall),
            new SocketDef("w6", new Vec3(6f, 6f, -13.4f), SocketTag.Wall),
            new SocketDef("w7", new Vec3(14f, 6f, -13.4f), SocketTag.Wall),
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
        },
        // Names for the junctions the lane graph derives. Foundry has no
        // junction at all — one ground lane, one strand, neither branching —
        // so this is four names for its four ends, and the flatness is the
        // point: it is the map where the graph has nothing to say yet.
        LaneNodeNamesOrNull: new[]
        {
            new LaneNodeNameDef("westGate", new Vec3(-40f, 0f, 0f)),
            new LaneNodeNameDef("core", new Vec3(36f, 0f, 6f)),
            new LaneNodeNameDef("airWest", new Vec3(-40f, 9f, -4f)),
            new LaneNodeNameDef("airCore", new Vec3(36f, 9f, 4f)),
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
            // Under the strand's western climb, where a Skywatch can still
            // reach 13 m of air. Four metres clear of the switchback's leg at
            // x −25 and of the freight cut, which starts at x −18.
            new SocketDef("g22", new Vec3(-21f, 0f, 2f), SocketTag.Ground),

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

            // --- Mid deck (y=5). It sat at z=-18 and this comment claimed it
            // covered the freight cut; it was eighteen metres away and covered
            // nothing the catwalk did not already cover better, which is what
            // made it the deck nobody had a reason to climb to. It is over the
            // long route's southern leg now and within a Nova's reach of the
            // cut, and — deliberately — out of range of the air strand, so the
            // two decks answer different questions instead of the same one.
            new SocketDef("w1", new Vec3(-16f, 5f, -11f), SocketTag.Wall),
            new SocketDef("w2", new Vec3(-7f, 5f, -14.5f), SocketTag.Wall),
            new SocketDef("w5", new Vec3(-19f, 5f, -15f), SocketTag.Wall),
            new SocketDef("w6", new Vec3(-11f, 5f, -11.5f), SocketTag.Wall),
            new SocketDef("w7", new Vec3(-2.5f, 5f, -12.5f), SocketTag.Wall),

            // --- Upper catwalk (y=10) hangs over the air lane — the only
            // sockets that see the whole strand rather than its low dips.
            // The catwalk is thirty metres long and carried its four sockets
            // in the middle twenty of it, so both ends of the strand it exists
            // to cover were out of reach: nothing on the map could reach the
            // strand between x −27 and x −14 except a single pad, and the
            // eastern run had two. These two stand at the catwalk's ends, a
            // clear metre inside the edge, and they are the reason to walk to
            // the end of it.
            new SocketDef("w10", new Vec3(-14.5f, 10f, 6f), SocketTag.Wall),
            new SocketDef("w11", new Vec3(12.5f, 10f, 4.5f), SocketTag.Wall),

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

            // Two doors out of the same gate, which is what makes the map's
            // rule a lived one rather than a proof. `b1` shuts the freight cut,
            // `b2` the switchback — and shutting both would leave the wave
            // nowhere to walk, so the second one is refused. A player finds the
            // edge of the system by walking into it, once, and the refusal says
            // why.
            new SocketDef("b1", new Vec3(-8f, 0f, -1f), SocketTag.Barricade),
            new SocketDef("b2", new Vec3(-30f, 0f, -10f), SocketTag.Barricade),
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
        },
        // The first map whose graph says something. `cutMouth` is the freight
        // cut's midpoint and `switchbackNorth` the long way's — the derivation
        // promotes both, because two routes between the same gate and the same
        // core are only tellable apart by where they go. That pair is this
        // map's whole lesson, and `b1` sits on the edge through `cutMouth`.
        LaneNodeNamesOrNull: new[]
        {
            new LaneNodeNameDef("westGate", new Vec3(-45f, 0f, -10f)),
            new LaneNodeNameDef("cutMouth", new Vec3(-20f, 0f, -2f)),
            new LaneNodeNameDef("switchbackNorth", new Vec3(-5f, 0f, 12f)),
            new LaneNodeNameDef("core", new Vec3(40f, 0f, 8f)),
            new LaneNodeNameDef("airWest", new Vec3(-45f, 9f, 4f)),
            new LaneNodeNameDef("airCore", new Vec3(40f, 9f, 6f)),
        },
        // The barricade, as a door. `b1` shuts the way *into* the cut rather
        // than the stretch it stands on: close the far stretch and anything
        // already past cutMouth has nowhere to go, and an enemy with nowhere to
        // go is the one outcome this system must not produce. Shutting the
        // entrance turns the wave at the fork instead, which is what a player
        // watching it thinks is happening — and, unlike the spawn-time swap it
        // replaces, it turns the ones already walking too.
        //
        // The Ram still comes down the cut: siege enemies route as if every gate
        // were open, so it walks at `b1` and breaks it. Being blocked is what
        // makes it choose the shortcut, not what stops it.
        LaneGatesOrNull: new[]
        {
            new LaneGateDef("westGate-cutMouth", "b1"),
            new LaneGateDef("westGate-switchbackNorth", "b2"),
        });

    /// <summary>Sector 3. A tower block, and the only map where the core is at
    /// the top: enemies come in at street level and climb, so every fight is
    /// uphill and the last stand is on the roof. That inverts the thing both
    /// earlier maps taught — there is no "far end of the lane" to hold, only a
    /// height to give up slowly.
    ///
    /// Two ground routes with opposite characters. The stair winds through the
    /// building's interior, which is tight, sightline-poor and full of corners
    /// worth trapping. The fire escape zigzags up the outside, wide open and
    /// visible from everywhere, which makes it the lane towers want and heroes
    /// find boring. Choosing which one to fortify is the map's question.
    ///
    /// The air route spirals the exterior and arrives at the roof directly,
    /// skipping every floor. On Foundry and Switchyard flyers were a coverage
    /// tax; here they are a shortcut past the entire map, so roof anti-air
    /// stops being optional.</summary>
    public static readonly MapDef Spire = new(
        Id: "spire",
        Routes: new[]
        {
            // Interior stair: lobby, then four flights around the atrium.
            new RouteDef("stair", EnemyLayer.Ground, new[]
            {
                new Vec3(-34f, 0f, 0f),
                new Vec3(-14f, 0f, 0f),
                new Vec3(-14f, 0f, -14f),
                new Vec3(-4f, 10f, -14f),
                new Vec3(14f, 10f, -14f),
                new Vec3(14f, 20f, -4f),
                new Vec3(14f, 20f, 14f),
                new Vec3(-4f, 30f, 14f),
                new Vec3(-14f, 30f, 14f),
                new Vec3(-14f, 40f, 0f),
                new Vec3(0f, 40f, 0f),
            }),
            // Fire escape: the long way up the outside, fully exposed, and it
            // re-enters the building at floor three. Two independent forty-metre
            // climbs was double the defensive burden against one life pool and
            // measured that way; converging them means the lower half is a
            // choice of lanes and the upper half is one shared flight you have
            // to hold. It is also what a fire escape does.
            new RouteDef("escape", EnemyLayer.Ground, new[]
            {
                new Vec3(-34f, 0f, 16f),
                new Vec3(-18f, 0f, 18f),
                new Vec3(18f, 0f, 18f),
                new Vec3(20f, 10f, 10f),
                new Vec3(20f, 20f, 2f),
                new Vec3(14f, 20f, 14f),
                new Vec3(-4f, 30f, 14f),
                new Vec3(-14f, 30f, 14f),
                new Vec3(-14f, 40f, 0f),
                new Vec3(0f, 40f, 0f),
            }),
            // Flyers spiral the outside and land on the roof, skipping every
            // floor between. The whole building is their shortcut.
            new RouteDef("air", EnemyLayer.Air, new[]
            {
                new Vec3(-40f, 8f, 0f),
                new Vec3(-26f, 20f, -26f),
                new Vec3(26f, 30f, -26f),
                new Vec3(26f, 40f, 20f),
                new Vec3(0f, 44f, 0f),
            }),
        },
        // Generated against the routes rather than placed by eye, then filtered
        // on the same rules the placement gate enforces — off the road, not
        // stranded, 4.8 m apart within a tier. The margin lesson from Foundry's
        // deck is baked in: every socket sits about 6 m off its leg, which is
        // reach to spare for every tower and leaves Fog something to take away
        // without switching anything off.
        Sockets: new[]
        {
            new SocketDef("g1", new Vec3(-27.0f, 0.0f, 6.0f), SocketTag.Ground),
            new SocketDef("g2", new Vec3(-27.0f, 0.0f, -6.0f), SocketTag.Ground),
            new SocketDef("g3", new Vec3(-19.0f, 0.0f, 6.0f), SocketTag.Ground),
            new SocketDef("g4", new Vec3(-19.0f, 0.0f, -6.0f), SocketTag.Ground),
            new SocketDef("g5", new Vec3(-8.0f, 0.0f, -4.9f), SocketTag.Ground),
            new SocketDef("g6", new Vec3(-8.0f, 0.0f, -10.5f), SocketTag.Ground),
            new SocketDef("g7", new Vec3(-29.1f, 0.0f, 22.7f), SocketTag.Ground),
            new SocketDef("g8", new Vec3(-22.7f, 0.0f, 23.5f), SocketTag.Ground),
            new SocketDef("g9", new Vec3(-21.3f, 0.0f, 11.5f), SocketTag.Ground),
            new SocketDef("g10", new Vec3(-5.4f, 0.0f, 24.0f), SocketTag.Ground),
            new SocketDef("g11", new Vec3(-5.4f, 0.0f, 12.0f), SocketTag.Ground),
            new SocketDef("g12", new Vec3(9.0f, 0.0f, 24.0f), SocketTag.Ground),
            new SocketDef("g13", new Vec3(9.0f, 0.0f, 12.0f), SocketTag.Ground),
            new SocketDef("w1", new Vec3(-10.5f, 3.5f, -8.0f), SocketTag.Wall),
            new SocketDef("w2", new Vec3(-10.5f, 3.5f, -20.0f), SocketTag.Wall),
            new SocketDef("w3", new Vec3(-6.5f, 7.5f, -8.0f), SocketTag.Wall),
            new SocketDef("w4", new Vec3(-6.5f, 7.5f, -20.0f), SocketTag.Wall),
            new SocketDef("w5", new Vec3(2.3f, 10.0f, -8.0f), SocketTag.Wall),
            new SocketDef("w6", new Vec3(2.3f, 10.0f, -20.0f), SocketTag.Wall),
            new SocketDef("w7", new Vec3(9.5f, 10.0f, -8.0f), SocketTag.Wall),
            new SocketDef("w8", new Vec3(9.5f, 10.0f, -20.0f), SocketTag.Wall),
            new SocketDef("w9", new Vec3(8.0f, 13.5f, -10.5f), SocketTag.Wall),
            new SocketDef("w10", new Vec3(20.0f, 13.5f, -10.5f), SocketTag.Wall),
            new SocketDef("w11", new Vec3(8.0f, 17.5f, -6.5f), SocketTag.Wall),
            new SocketDef("w12", new Vec3(20.0f, 17.5f, -6.5f), SocketTag.Wall),
            new SocketDef("w13", new Vec3(8.0f, 20.0f, 2.3f), SocketTag.Wall),
            new SocketDef("w14", new Vec3(8.0f, 20.0f, 9.5f), SocketTag.Wall),
            new SocketDef("w15", new Vec3(7.7f, 23.5f, 8.0f), SocketTag.Wall),
            new SocketDef("w16", new Vec3(7.7f, 23.5f, 20.0f), SocketTag.Wall),
            new SocketDef("w17", new Vec3(0.5f, 27.5f, 8.0f), SocketTag.Wall),
            new SocketDef("w18", new Vec3(0.5f, 27.5f, 20.0f), SocketTag.Wall),
            new SocketDef("w19", new Vec3(-7.5f, 30.0f, 8.0f), SocketTag.Wall),
            new SocketDef("w20", new Vec3(-7.5f, 30.0f, 20.0f), SocketTag.Wall),
            new SocketDef("w21", new Vec3(-8.0f, 33.5f, 9.1f), SocketTag.Wall),
            new SocketDef("w22", new Vec3(-20.0f, 33.5f, 9.1f), SocketTag.Wall),
            new SocketDef("w23", new Vec3(-8.0f, 37.5f, 3.5f), SocketTag.Wall),
            new SocketDef("w24", new Vec3(-20.0f, 37.5f, 3.5f), SocketTag.Wall),
            new SocketDef("w25", new Vec3(-9.1f, 40.0f, 6.0f), SocketTag.Wall),
            new SocketDef("w26", new Vec3(-9.1f, 40.0f, -6.0f), SocketTag.Wall),
            new SocketDef("w27", new Vec3(-3.5f, 40.0f, 6.0f), SocketTag.Wall),
            new SocketDef("w28", new Vec3(-3.5f, 40.0f, -6.0f), SocketTag.Wall),
            new SocketDef("w29", new Vec3(24.5f, 3.5f, 16.7f), SocketTag.Wall),
            new SocketDef("w30", new Vec3(12.9f, 3.5f, 13.7f), SocketTag.Wall),
            new SocketDef("w31", new Vec3(25.3f, 7.5f, 13.5f), SocketTag.Wall),
            new SocketDef("w32", new Vec3(13.7f, 7.5f, 10.5f), SocketTag.Wall),
            new SocketDef("w33", new Vec3(26.0f, 13.5f, 7.2f), SocketTag.Wall),
            new SocketDef("w34", new Vec3(14.0f, 13.5f, 7.2f), SocketTag.Wall),
            new SocketDef("w35", new Vec3(26.0f, 17.5f, 4.0f), SocketTag.Wall),
            new SocketDef("w36", new Vec3(23.3f, 20.0f, 8.9f), SocketTag.Wall),
            new SocketDef("w37", new Vec3(20.9f, 20.0f, 13.7f), SocketTag.Wall),
            new SocketDef("w38", new Vec3(9.0f, 40.0f, 0.0f), SocketTag.Wall),
            new SocketDef("w39", new Vec3(4.5f, 40.0f, 7.8f), SocketTag.Wall),
            new SocketDef("w40", new Vec3(4.5f, 40.0f, -7.8f), SocketTag.Wall),
            new SocketDef("w41", new Vec3(24.0f, 30.0f, -18.0f), SocketTag.Wall),
            new SocketDef("w43", new Vec3(24.0f, 38.0f, 12.0f), SocketTag.Wall),
            new SocketDef("w44", new Vec3(16.0f, 40.0f, 14.0f), SocketTag.Wall),
            new SocketDef("t1", new Vec3(-24.0f, 0.0f, 0.0f), SocketTag.Trap),
            new SocketDef("t2", new Vec3(-9.0f, 5.0f, -14.0f), SocketTag.Trap),
            new SocketDef("t3", new Vec3(14.0f, 15.0f, -9.0f), SocketTag.Trap),
            new SocketDef("t4", new Vec3(5.0f, 25.0f, 14.0f), SocketTag.Trap),
            new SocketDef("t5", new Vec3(-14.0f, 35.0f, 7.0f), SocketTag.Trap),
            new SocketDef("t6", new Vec3(-26.0f, 0.0f, 17.0f), SocketTag.Trap),
            new SocketDef("t7", new Vec3(19.0f, 5.0f, 14.0f), SocketTag.Trap),
            new SocketDef("t8", new Vec3(17.0f, 20.0f, 8.0f), SocketTag.Trap),
            new SocketDef("t9", new Vec3(-9.0f, 30.0f, 14.0f), SocketTag.Trap),
            new SocketDef("t10", new Vec3(-7.0f, 40.0f, 0.0f), SocketTag.Trap),
        },
        HeroSpawn: new Vec3(-30f, 0f, 8f),
        ArmoryPos: new Vec3(-26f, 0f, 10f),
        HeroStations: new[]
        {
            new HeroStationDef("lobby", new Vec3(-18f, 0f, 6f)),
            new HeroStationDef("mezzanine", new Vec3(4f, 10f, -18f)),
            new HeroStationDef("midFloor", new Vec3(18f, 20f, 6f)),
            new HeroStationDef("upperFloor", new Vec3(-8f, 30f, 18f)),
            new HeroStationDef("roof", new Vec3(0f, 40f, -8f)),
        },
        TotalWaves: 12,
        // Night on the Shade wave, Fog on the roof finale — fog on a map whose
        // fight ends forty metres up is a different threat to fog on a yard.
        ConditionScheduleOrNull: new Dictionary<int, string>
        {
            [7] = Conditions.Night.Id,
            [11] = Conditions.Fog.Id,
        },
        // `floorThree` is where the stair and the fire escape converge — the
        // one junction on the map, and the reason the upper half is a single
        // flight you have to hold rather than two independent forty-metre
        // climbs against one life pool.
        LaneNodeNamesOrNull: new[]
        {
            new LaneNodeNameDef("lobby", new Vec3(-34f, 0f, 0f)),
            new LaneNodeNameDef("escapeFoot", new Vec3(-34f, 0f, 16f)),
            new LaneNodeNameDef("floorThree", new Vec3(14f, 20f, 14f)),
            new LaneNodeNameDef("roofCore", new Vec3(0f, 40f, 0f)),
            new LaneNodeNameDef("airStreet", new Vec3(-40f, 8f, 0f)),
            new LaneNodeNameDef("airCore", new Vec3(0f, 44f, 0f)),
        });

    /// <summary>M4 — "The Toaster": a farm three times the width of any yard
    /// before it, and the first map where the lane is not one line.
    ///
    /// Three ground routes leave the same gate and end on the same last hundred
    /// metres past the house to the core. The difference is what happens in
    /// between: the drive goes straight there in 139 m, and the other two step
    /// through warp gates that throw the wave clean across the property and
    /// back — 313 m through one, 471 m through two. A group that takes the long
    /// way is gone for three minutes and arrives somewhere else, which is the
    /// map's whole question: the ground you can hold is not the ground the wave
    /// is on, and nobody walks 320 m twice, which is what the four vehicles are
    /// parked here for.
    ///
    /// There is no high ground. A farm is flat, so the air strand flies low —
    /// 9 m, and down the line the walkers take — and is answered from the same
    /// lane pads rather than from a deck. That is a decision, not an oversight:
    /// on a map with no tier to put anti-air on, a strand anywhere else is a
    /// lane no tower in the game can reach. The four roofs are the only height
    /// this map has, one per building, and they are there for the player.</summary>
    public static readonly MapDef Toaster = new(
        Id: "toaster",
        Routes: new[]
        {
            // Two warps. Leg 11 is the south pad to the west pad, leg 20 the
            // north pad to the house pad; neither is walked, so the map reads
            // as four separate fights that happen to be the same wave.
            new RouteDef("long", EnemyLayer.Ground, new[]
            {
                new Vec3(72f, 0f, 24f),      //  0  the gate, north-east
                new Vec3(69f, 0f, -4f),
                new Vec3(57f, 0f, -9f),
                new Vec3(17f, 0f, -13f),     //  3  west along the north shore
                new Vec3(5f, 0f, -16f),
                new Vec3(-4f, 0f, -31f),     //  5  down the pond's west side
                new Vec3(-7f, 0f, -50f),
                new Vec3(0f, 0f, -57f),
                new Vec3(17f, 0f, -58f),     //  8  east along the south road
                new Vec3(53f, 0f, -57f),
                new Vec3(74f, 0f, -52f),
                new Vec3(72f, 0f, -40f),     // 11  south pad, by the grnmchn house
                new Vec3(-137f, 0f, 11f),    // 12  west pad, by the buggy house
                new Vec3(-131f, 0f, 13f),
                new Vec3(-110f, 0f, 20f),
                new Vec3(-98f, 0f, 38f),     // 15  up to the barn
                new Vec3(-77f, 0f, 45f),
                new Vec3(-61f, 0f, 49f),
                new Vec3(-54f, 0f, 54f),
                new Vec3(-50f, 0f, 60f),
                new Vec3(-44f, 0f, 60f),     // 20  north pad, on the road
                new Vec3(52f, 0f, 29f),      // 21  house pad, east of the drive
                new Vec3(56f, 0f, 22f),
                new Vec3(56f, 0f, 19f),      // 23  the drive — every route joins here
                new Vec3(53f, 0f, 13f),
                new Vec3(39f, 0f, 12f),
                new Vec3(10f, 0f, 10f),
                new Vec3(-7f, 0f, -3f),
                new Vec3(-25f, 0f, -10f),
                new Vec3(-45f, 0f, -10f),
                new Vec3(-57f, 0f, -10f),    // 30  the core
            }, TeleportLegs: new[] { 11, 20 }),

            // No warps: the gate to the core down the drive, 139 m. The first
            // two waves are this and nothing else, so the core is learned
            // before the pads are.
            new RouteDef("direct", EnemyLayer.Ground, new[]
            {
                new Vec3(72f, 0f, 24f),
                new Vec3(56f, 0f, 19f),
                new Vec3(53f, 0f, 13f),
                new Vec3(39f, 0f, 12f),
                new Vec3(10f, 0f, 10f),
                new Vec3(-7f, 0f, -3f),
                new Vec3(-25f, 0f, -10f),
                new Vec3(-45f, 0f, -10f),
                new Vec3(-57f, 0f, -10f),
            }),

            // One warp: the south loop to the south pad, out at the west pad,
            // then back along the west run at the core's blind side. Everything
            // the drive's kill-box is pointing the wrong way for.
            new RouteDef("west", EnemyLayer.Ground, new[]
            {
                new Vec3(72f, 0f, 24f),
                new Vec3(69f, 0f, -4f),
                new Vec3(57f, 0f, -9f),
                new Vec3(17f, 0f, -13f),
                new Vec3(5f, 0f, -16f),
                new Vec3(-4f, 0f, -31f),
                new Vec3(-7f, 0f, -50f),
                new Vec3(0f, 0f, -57f),
                new Vec3(17f, 0f, -58f),
                new Vec3(53f, 0f, -57f),
                new Vec3(74f, 0f, -52f),
                new Vec3(72f, 0f, -40f),     // 11  the same south pad
                new Vec3(-137f, 0f, 11f),    // 12  out at the west pad
                new Vec3(-124f, 0f, 14f),    //     round the house, not through it
                new Vec3(-108f, 0f, 4f),
                new Vec3(-80f, 0f, -8f),
                new Vec3(-57f, 0f, -10f),
            }, TeleportLegs: new[] { 11 }),

            // The drive again, nine metres up: flyers take the same line the
            // walkers take, and the same pads answer both.
            //
            // On every map before this one the strand was its own route at its
            // own height, and the point of that was to make the deck matter —
            // a lane the ground could not reach. A farm has no deck. Given
            // that, an air lane somewhere else is not a second front, it is a
            // second front nothing in the game can shoot at, which is the
            // failure MAP-AUTHORING §4.5 exists to catch. So the flyers follow
            // the road at a height a Skywatch in the field can hold, and what
            // a mixed wave costs you here is volume rather than position: the
            // pads that cover the drive are already the right pads, and the
            // question is whether you bought enough of the right towers.
            new RouteDef("air", EnemyLayer.Air, new[]
            {
                new Vec3(72f, 9f, 24f),
                new Vec3(56f, 9f, 19f),
                new Vec3(53f, 9f, 13f),
                new Vec3(39f, 9f, 12f),
                new Vec3(10f, 9f, 10f),
                new Vec3(-7f, 9f, -3f),
                new Vec3(-25f, 9f, -10f),
                new Vec3(-45f, 9f, -10f),
                new Vec3(-57f, 9f, -10f),
            }),
        },
        // Generated against the routes and then filtered on the rules the
        // placement gate enforces — off the road, not stranded, 4.8 m apart —
        // and finally repaired until every four-metre sample of every walked
        // leg outside its apron has three pads in reach, and every sample of
        // the air strand has three that can shoot at it. 471 m of lane is
        // roughly four times Foundry's, and the pad count is what that costs:
        // a map this size cannot be padded by eye.
        //
        // The eight metres after a warp pad are apron, exactly as the eight
        // after the spawn gate are. An enemy standing on an arrival pad has its
        // whole walk ahead of it, and demanding three pads around every gate
        // would pay players to camp the four of them instead of the lane.
        Sockets: new[]
        {
            // --- long leg 0
            new SocketDef("g1", new Vec3(61.9f, 0f, 5.3f), SocketTag.Ground),
            new SocketDef("g2", new Vec3(66.1f, 0f, 16.1f), SocketTag.Ground),
            // --- long leg 1
            new SocketDef("g3", new Vec3(56.4f, 0f, 0.2f), SocketTag.Ground),
            // --- long leg 2
            new SocketDef("g4", new Vec3(18.8f, 0f, -1.8f), SocketTag.Ground),
            new SocketDef("g5", new Vec3(24.5f, 0f, -1.2f), SocketTag.Ground),
            new SocketDef("g6", new Vec3(30.2f, 0f, -0.6f), SocketTag.Ground),
            new SocketDef("g7", new Vec3(36.1f, 0f, -1.5f), SocketTag.Ground),
            // --- long leg 3
            new SocketDef("g8", new Vec3(5.8f, 0f, -10.6f), SocketTag.Ground),
            // --- long leg 5
            new SocketDef("g9", new Vec3(-10.8f, 0f, -42.1f), SocketTag.Ground),
            new SocketDef("g10", new Vec3(-9.3f, 0f, -32.6f), SocketTag.Ground),
            // --- long leg 6
            new SocketDef("g11", new Vec3(-0.2f, 0f, -48.6f), SocketTag.Ground),
            // --- long leg 7
            new SocketDef("g12", new Vec3(3.1f, 0f, -52.2f), SocketTag.Ground),
            new SocketDef("g13", new Vec3(13.9f, 0f, -62.8f), SocketTag.Ground),
            // --- long leg 8
            new SocketDef("g14", new Vec3(30.0f, 0f, -62.6f), SocketTag.Ground),
            new SocketDef("g15", new Vec3(35.1f, 0f, -62.5f), SocketTag.Ground),
            new SocketDef("g16", new Vec3(40.0f, 0f, -52.4f), SocketTag.Ground),
            // --- long leg 9
            new SocketDef("g17", new Vec3(59.4f, 0f, -48.8f), SocketTag.Ground),
            new SocketDef("g18", new Vec3(61.5f, 0f, -43.8f), SocketTag.Ground),
            new SocketDef("g19", new Vec3(65.0f, 0f, -49.0f), SocketTag.Ground),
            // --- long leg 12
            new SocketDef("g20", new Vec3(-137.1f, 0f, 16.2f), SocketTag.Ground),
            // --- long leg 13
            new SocketDef("g21", new Vec3(-119.5f, 0f, 22.1f), SocketTag.Ground),
            new SocketDef("g22", new Vec3(-111.0f, 0f, 14.4f), SocketTag.Ground),
            // --- long leg 14
            new SocketDef("g23", new Vec3(-109.7f, 0f, 29.5f), SocketTag.Ground),
            new SocketDef("g24", new Vec3(-99.3f, 0f, 16.1f), SocketTag.Ground),
            // --- long leg 15
            new SocketDef("g25", new Vec3(-93.8f, 0f, 34.1f), SocketTag.Ground),
            new SocketDef("g26", new Vec3(-88.5f, 0f, 35.9f), SocketTag.Ground),
            new SocketDef("g27", new Vec3(-78.0f, 0f, 39.4f), SocketTag.Ground),
            // --- long leg 16
            new SocketDef("g28", new Vec3(-64.9f, 0f, 53.2f), SocketTag.Ground),
            // --- long leg 17
            new SocketDef("g29", new Vec3(-64.8f, 0f, 58.0f), SocketTag.Ground),
            new SocketDef("g30", new Vec3(-58.7f, 0f, 56.8f), SocketTag.Ground),
            new SocketDef("g31", new Vec3(-52.8f, 0f, 48.7f), SocketTag.Ground),
            // --- long leg 18
            new SocketDef("g32", new Vec3(-46.3f, 0f, 51.1f), SocketTag.Ground),
            // --- long leg 21
            new SocketDef("g33", new Vec3(48.7f, 0f, 24.8f), SocketTag.Ground),
            // --- long leg 23
            new SocketDef("g34", new Vec3(57.0f, 0f, 5.8f), SocketTag.Ground),
            new SocketDef("g35", new Vec3(61.1f, 0f, 11.0f), SocketTag.Ground),
            // --- long leg 24
            new SocketDef("g36", new Vec3(47.1f, 0f, 3.1f), SocketTag.Ground),
            new SocketDef("g37", new Vec3(48.1f, 0f, 19.8f), SocketTag.Ground),
            // --- long leg 25
            new SocketDef("g38", new Vec3(12.6f, 0f, 15.2f), SocketTag.Ground),
            new SocketDef("g39", new Vec3(18.4f, 0f, 15.6f), SocketTag.Ground),
            new SocketDef("g40", new Vec3(30.0f, 0f, 16.4f), SocketTag.Ground),
            new SocketDef("g41", new Vec3(35.8f, 0f, 16.8f), SocketTag.Ground),
            // --- long leg 26
            new SocketDef("g42", new Vec3(-3.7f, 0f, 5.8f), SocketTag.Ground),
            new SocketDef("g43", new Vec3(-0.9f, 0f, -6.5f), SocketTag.Ground),
            new SocketDef("g44", new Vec3(4.3f, 0f, -4.8f), SocketTag.Ground),
            new SocketDef("g45", new Vec3(4.8f, 0f, 12.3f), SocketTag.Ground),
            new SocketDef("g46", new Vec3(11.4f, 0f, 1.1f), SocketTag.Ground),
            // --- long leg 27
            new SocketDef("g47", new Vec3(-24.6f, 0f, -4.5f), SocketTag.Ground),
            new SocketDef("g48", new Vec3(-20.1f, 0f, -2.7f), SocketTag.Ground),
            new SocketDef("g49", new Vec3(-15.6f, 0f, -1.0f), SocketTag.Ground),
            new SocketDef("g50", new Vec3(-15.4f, 0f, -20.8f), SocketTag.Ground),
            new SocketDef("g51", new Vec3(-10.6f, 0f, -16.8f), SocketTag.Ground),
            // --- long leg 28
            new SocketDef("g52", new Vec3(-42.5f, 0f, -15.0f), SocketTag.Ground),
            new SocketDef("g53", new Vec3(-37.5f, 0f, -15.0f), SocketTag.Ground),
            new SocketDef("g54", new Vec3(-29.9f, 0f, -14.3f), SocketTag.Ground),
            // --- long leg 29
            new SocketDef("g55", new Vec3(-51.0f, 0f, -5.0f), SocketTag.Ground),
            new SocketDef("g56", new Vec3(-51.0f, 0f, -15.0f), SocketTag.Ground),
            // --- direct leg 0
            new SocketDef("g57", new Vec3(60.6f, 0f, 27.0f), SocketTag.Ground),
            new SocketDef("g58", new Vec3(66.3f, 0f, 30.2f), SocketTag.Ground),
            // --- west leg 13
            new SocketDef("g59", new Vec3(-119.6f, 0f, 5.2f), SocketTag.Ground),
            // --- west leg 14
            new SocketDef("g60", new Vec3(-94.4f, 0f, 3.6f), SocketTag.Ground),
            new SocketDef("g61", new Vec3(-85.0f, 0f, -0.4f), SocketTag.Ground),
            new SocketDef("g62", new Vec3(-80.4f, 0f, -2.4f), SocketTag.Ground),
            // --- west leg 15
            new SocketDef("g63", new Vec3(-70.9f, 0f, -3.8f), SocketTag.Ground),
            new SocketDef("g64", new Vec3(-65.2f, 0f, -4.3f), SocketTag.Ground),

            // The drive's south flank, added when the strand moved onto the
            // road: everything else here covers the lane from one side or the
            // other, and this stretch had both its neighbours on the north.
            // Level with the walkers that is a choice; nine metres up it was a
            // twenty-metre window with two pads in reach instead of three.
            new SocketDef("g65", new Vec3(31.0f, 0f, 6.0f), SocketTag.Ground),

            new SocketDef("w_barn", new Vec3(-95.0f, 6.4f, 45.5f), SocketTag.Wall),
            new SocketDef("w_buggy", new Vec3(-141.5f, 3.8f, 5.0f), SocketTag.Wall),
            new SocketDef("w_vehickle", new Vec3(45.0f, 3.8f, 20.0f), SocketTag.Wall),
            new SocketDef("w_grnmchn", new Vec3(80.0f, 3.8f, -37.0f), SocketTag.Wall),

            new SocketDef("t1", new Vec3(71.1f, 0f, 16.0f), SocketTag.Trap),
            new SocketDef("t2", new Vec3(53.0f, 0f, -9.4f), SocketTag.Trap),
            new SocketDef("t3", new Vec3(21.0f, 0f, -12.6f), SocketTag.Trap),
            new SocketDef("t4", new Vec3(-4.0f, 0f, -31.0f), SocketTag.Trap),
            new SocketDef("t5", new Vec3(12.8f, 0f, -57.8f), SocketTag.Trap),
            new SocketDef("t6", new Vec3(45.0f, 0f, -57.2f), SocketTag.Trap),
            new SocketDef("t7", new Vec3(72.7f, 0f, -44.0f), SocketTag.Trap),
            new SocketDef("t8", new Vec3(-126.8f, 0f, 14.4f), SocketTag.Trap),
            new SocketDef("t9", new Vec3(-100.4f, 0f, 34.4f), SocketTag.Trap),
            new SocketDef("t10", new Vec3(-69.0f, 0f, 47.0f), SocketTag.Trap),
            new SocketDef("t11", new Vec3(-29.0f, 0f, -10.0f), SocketTag.Trap),
            new SocketDef("t12", new Vec3(-100.0f, 0f, 0.6f), SocketTag.Trap),
            new SocketDef("t13", new Vec3(-70.8f, 0f, -8.8f), SocketTag.Trap),
        },
        // South-west of the core, in the lee of the last bend: far enough from
        // the drive that the wave does not walk through the armory, close
        // enough that a rebuild between waves is not a hike.
        HeroSpawn: new Vec3(-60f, 0f, -22f),
        ArmoryPos: new Vec3(-66f, 0f, -22f),
        // One per fightable stretch, and deliberately one at each warp pad: a
        // hero who is not at a pad when a wave comes out of it is three hundred
        // metres from the fight, and the bot's traversal graph is the only
        // thing that models that.
        HeroStations: new[]
        {
            new HeroStationDef("coreYard", new Vec3(-60f, 0f, -18f)),
            new HeroStationDef("coreGate", new Vec3(-45f, 0f, -4f)),
            new HeroStationDef("driveBend", new Vec3(12f, 0f, -4f)),
            new HeroStationDef("gateWatch", new Vec3(66f, 0f, 10f)),
            new HeroStationDef("housePad", new Vec3(56f, 0f, 32f)),
            new HeroStationDef("pondWest", new Vec3(-2f, 0f, -28f)),
            new HeroStationDef("southRoad", new Vec3(30f, 0f, -52f)),
            new HeroStationDef("southPad", new Vec3(78f, 0f, -38f)),
            new HeroStationDef("westPad", new Vec3(-128f, 0f, 4f)),
            new HeroStationDef("westRun", new Vec3(-92f, 0f, -4f)),
            new HeroStationDef("barnDrive", new Vec3(-84f, 0f, 40f)),
            new HeroStationDef("northRoad", new Vec3(-50f, 0f, 56f)),
        },
        TotalWaves: 12,
        // Night on the Shade wave. Fog was left off deliberately: a range tax
        // on a map whose lanes are already answered from single pads takes
        // stretches out of play rather than making them harder, and that is a
        // measurement to make before it ships, not a schedule entry to guess.
        ConditionScheduleOrNull: new Dictionary<int, string>
        {
            [8] = Conditions.Night.Id,
        },
        FieldX: 320f,
        FieldZ: 160f,
        // Parked outside the building each is named for, clear of every lane,
        // and — the part that is not obvious until someone gets in one —
        // pointed at open ground. A vehicle nosed at the wall it is parked
        // against is a vehicle whose first press of W is a crash, and all four
        // of these were until the probe drove them.
        VehiclesOrNull: new[]
        {
            new VehicleSpawnDef("buggy1", "buggy", new Vec3(-120f, 0f, -14f), -90f),
            new VehicleSpawnDef("dagator1", "dagator", new Vec3(-110f, 0f, 40f), 0f),
            new VehicleSpawnDef("grnmchn1", "grnmchn", new Vec3(76f, 0f, -30f), 180f),
            new VehicleSpawnDef("vehickle1", "vehickle", new Vec3(58f, 0f, 38f), -90f),
        },
        // The four warp pads and the drive, named as the route comments already
        // name them. `drive` is waypoint 23 of `long` — "every route joins
        // here" — and it is the node that makes this map's three routes three
        // paths through one graph rather than three polylines: `direct` reaches
        // it straight from the gate, `long` and `west` arrive via the pads.
        LaneNodeNamesOrNull: new[]
        {
            new LaneNodeNameDef("gate", new Vec3(72f, 0f, 24f)),
            new LaneNodeNameDef("southPad", new Vec3(72f, 0f, -40f)),
            new LaneNodeNameDef("westPad", new Vec3(-137f, 0f, 11f)),
            new LaneNodeNameDef("northPad", new Vec3(-44f, 0f, 60f)),
            new LaneNodeNameDef("housePad", new Vec3(52f, 0f, 29f)),
            new LaneNodeNameDef("drive", new Vec3(56f, 0f, 19f)),
            new LaneNodeNameDef("core", new Vec3(-57f, 0f, -10f)),
            new LaneNodeNameDef("airGate", new Vec3(72f, 9f, 24f)),
            new LaneNodeNameDef("airCore", new Vec3(-57f, 9f, -10f)),
        });

    public static readonly IReadOnlyDictionary<string, MapDef> All =
        new Dictionary<string, MapDef>
        {
            [TestLane.Id] = TestLane,
            [Foundry.Id] = Foundry,
            [Switchyard.Id] = Switchyard,
            [Spire.Id] = Spire,
            [Toaster.Id] = Toaster,
        };
}
