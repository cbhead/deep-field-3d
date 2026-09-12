using System.Linq;
using DeepField.Sim;
using DeepField.Sim.Content;
using Xunit;

/// <summary>Teleport legs: a route may contain a leg that is crossed rather
/// than walked.
///
/// The whole mechanism rests on one invariant — an enemy's Leg never points at
/// a teleport leg between ticks — because a zero-length leg is the one position
/// this movement model cannot represent: the lerp that turns Leg and
/// LegProgress into a position would divide by it. Every test below is either
/// that invariant or something that would quietly break it.</summary>
public class TeleportRouteTests
{
    /// <summary>Two twenty-metre walks with a two-hundred-metre jump between
    /// them. Keeps testlane's id so the wave tables resolve if a wave ever
    /// starts, and its route id so those tables still name a route that
    /// exists.</summary>
    private static MapDef Portal() => Maps.TestLane with
    {
        Routes = new[]
        {
            new RouteDef("ground", EnemyLayer.Ground, new[]
            {
                new Vec3(-30f, 0f, 0f),
                new Vec3(-10f, 0f, 0f),    // leg 0, walked, 20 m
                new Vec3(180f, 0f, 20f),   // leg 1, the jump
                new Vec3(200f, 0f, 20f),   // leg 2, walked, 20 m
            }, TeleportLegs: new[] { 1 }),
        },
        Sockets = new[]
        {
            new SocketDef("s1", new Vec3(-20f, 0f, 6f), SocketTag.Ground),
            new SocketDef("t1", new Vec3(185f, 0f, 20f), SocketTag.Trap),
        },
    };

    private static Enemy Walk(World w, int leg, float progress, string defId = "drifter")
    {
        var def = Enemies.All[defId];
        var enemy = new Enemy
        {
            Id = w.NextId(),
            DefId = defId,
            Hp = def.Hp,
            MaxHp = def.Hp,
            RouteIndex = 0,
            Leg = leg,
            LegProgress = progress,
            TotalTraveled = progress,
            Bounty = def.Bounty,
            LeakDamage = def.LeakDamage,
        };
        w.Enemies.Add(enemy);
        return enemy;
    }

    [Fact]
    public void ATeleportLegIsCrossedInOneTickAndLandsOnTheFarPad()
    {
        var w = new World(7, Portal());
        var enemy = Walk(w, leg: 0, progress: 19.99f);

        Step.Advance(w);

        Assert.Equal(2, enemy.Leg);
        Assert.True(enemy.Pos.DistanceTo(new Vec3(180f, 0f, 20f)) < 0.5f,
            $"landed at {enemy.Pos}, not on the arrival pad");
        var jump = w.Events.OfType<SimEvent.EnemyTeleported>().Single();
        Assert.Equal(1, jump.FromLeg);
        Assert.Equal("ground", jump.RouteId);
        Assert.Equal(180f, jump.X);
    }

    [Fact]
    public void TheJumpCostsNoDistance()
    {
        var w = new World(7, Portal());
        var enemy = Walk(w, leg: 0, progress: 19.99f);
        float before = enemy.TotalTraveled;

        Step.Advance(w);

        // One tick of walking, not two hundred metres of it. TotalTraveled is
        // the "which enemy is in front" metric every tower targets on, so a
        // spike here would make a teleporting enemy permanently the first
        // target on its route.
        float walked = Enemies.All["drifter"].SpeedMetersPerSec * Balance.Dt;
        Assert.InRange(enemy.TotalTraveled - before, walked - 0.01f, walked + 0.01f);
    }

    [Fact]
    public void AFrozenEnemyStillGoesThrough()
    {
        var w = new World(7, Portal());
        // Parked exactly on the departure pad with a hard stop on it: the one
        // state that would leave an enemy sitting on a zero-length leg.
        var enemy = Walk(w, leg: 1, progress: 0f);
        enemy.Statuses[(int)Channel.Control] = new StatusSlot
        {
            StatusId = Statuses.Freeze.Id, TimeLeft = 1f, Source = "test",
        };

        Step.Advance(w);

        Assert.Equal(2, enemy.Leg);
        Assert.False(float.IsNaN(enemy.Pos.X), "position went NaN on a zero-length leg");
        Assert.True(enemy.Pos.DistanceTo(new Vec3(180f, 0f, 20f)) < 0.5f);
    }

    [Fact]
    public void KnockbackStopsAtTheArrivalPad()
    {
        var w = new World(7, Portal());
        w.Money = 1000;
        foreach (ScrapType type in System.Enum.GetValues<ScrapType>())
            w.TeamScrap[type] = 50;
        w.Enqueue(new Command.PlaceTower(1, "launcher", "t1"));
        Step.Advance(w);
        Assert.Single(w.Traps);

        // Five metres past the pad, hit by a launcher that throws eight.
        var enemy = Walk(w, leg: 2, progress: 5f);
        enemy.TotalTraveled = 25f;
        Step.Advance(w);

        Assert.Equal(2, enemy.Leg);
        Assert.Equal(0f, enemy.LegProgress);
        // Knockback is a route displacement; the drawn position catches up on
        // the next move, which is also where a bad clamp would show as an
        // enemy standing two hundred metres away or dividing by a zero leg.
        Step.Advance(w);
        Assert.Equal(2, enemy.Leg);
        Assert.True(enemy.Pos.DistanceTo(new Vec3(180f, 0f, 20f)) < 1.5f,
            $"knocked back to {enemy.Pos}, not held at the pad");
    }

    [Fact]
    public void ATeleportLegHasNoLength()
    {
        var w = new World(7, Portal());
        Assert.Equal(20f, w.RouteLegLengths[0][0], 3);
        Assert.Equal(0f, w.RouteLegLengths[0][1]);
        Assert.Equal(20f, w.RouteLegLengths[0][2], 3);
        Assert.Equal(40f, w.RouteWalkLengths[0], 3);
    }

    [Fact]
    public void SamplesSkipTheJumpAndResetTheApronAfterIt()
    {
        var route = Portal().Routes[0];
        var samples = route.Samples(4f).ToList();

        Assert.All(samples, s => Assert.NotEqual(1, s.Leg));
        // Nothing is sampled out in the middle of the chord.
        Assert.DoesNotContain(samples, s => s.At.X > -10f && s.At.X < 180f);
        // The arrival pad is an entrance, so the walk from it starts at zero —
        // which is what makes the first metres after a pad spawn apron.
        var arrival = samples.First(s => s.Leg == 2);
        Assert.Equal(0f, arrival.MetersSinceEntry, 3);
        Assert.Equal(20f, samples.Last().MetersSinceEntry, 3);
    }

    [Fact]
    public void TheShippedMapsHaveNoneOfThisAndAreUnchanged()
    {
        foreach (var map in Maps.All.Values)
        {
            if (map.Id == "toaster") continue;
            var w = new World(1, map);
            for (int r = 0; r < map.Routes.Count; r++)
            {
                var route = map.Routes[r];
                Assert.False(route.HasTeleportLegs, $"{map.Id}/{route.Id} grew a teleport leg");
                for (int i = 0; i < route.Waypoints.Count - 1; i++)
                {
                    Assert.Equal(
                        route.Waypoints[i].DistanceTo(route.Waypoints[i + 1]),
                        w.RouteLegLengths[r][i], 4);
                }
            }
        }
    }

    [Fact]
    public void EveryTeleportLegIsInteriorAndAlone()
    {
        foreach (var map in Maps.All.Values)
            foreach (var route in map.Routes)
            {
                if (!route.HasTeleportLegs) continue;
                Assert.Equal(EnemyLayer.Ground, route.Layer);
                foreach (int leg in route.TeleportLegs!)
                {
                    // Never first: an enemy spawns on waypoint 0 and its facing
                    // comes from leg 0. Never last: the leak check fires on
                    // running out of legs, and a jump is not an arrival.
                    Assert.InRange(leg, 1, route.LegCount - 2);
                    // Never adjacent: two in a row is a pad an enemy passes
                    // through in the same tick it arrives, which no gate, VFX
                    // or event ordering has any reason to expect.
                    Assert.False(route.IsTeleportLeg(leg + 1),
                        $"{map.Id}/{route.Id}: legs {leg} and {leg + 1} both teleport");
                }
            }
    }
}
