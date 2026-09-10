using System;
using System.Collections.Generic;
using System.Linq;
using DeepField.Sim;
using DeepField.Sim.Content;
using Xunit;

/// <summary>Who can actually fight the Skiff.
///
/// The def table says Skywatch, Arc and Filament all list <c>EnemyLayer.Air</c>,
/// and reading it is how you get the answer "three towers answer flyers". The
/// Skiff's own note in Enemies.cs says the opposite — "Ground towers can't see
/// it; Skywatch and hero fire can" — and a player's experience matches the
/// note, not the table.
///
/// Both are half right, and these tests pin down which half. The mechanism
/// works: put an Arc or a Filament in range of a flyer and it kills it. The
/// geometry does not: the strand cruises above what either can reach from the
/// ground, so unless the tower is on a deck it never fires a shot at one.</summary>
public class AirDefenceTests
{
    private static readonly string[] Damaging = { "skywatch", "arc", "filament" };

    private static (World World, Enemy Skiff) FlyerOnTheStrand(MapDef map)
    {
        int airRoute = map.Routes.ToList().FindIndex(r => r.Layer == EnemyLayer.Air);
        var w = new World(1, map);
        var skiff = new Enemy
        {
            Id = w.NextId(), DefId = "skiff", Hp = 1000f, MaxHp = 1000f,
            Facing = new Vec3(1, 0, 0), RouteIndex = airRoute, Leg = 0, LegProgress = 0f,
        };
        w.Enemies.Add(skiff);
        // An enemy's position is its route progress, so setting Pos by hand does
        // not stick — MoveEnemies puts it back on the lane before any tower
        // looks at it. Let it land first, then build beside where it actually is.
        Step.Advance(w);
        return (w, skiff);
    }

    private static float DamageOverThreeSeconds(string towerId, Vec3 offset)
    {
        var def = Towers.All[towerId];
        var (w, skiff) = FlyerOnTheStrand(Maps.All["foundry"]);
        w.Towers.Add(new Tower
        {
            Id = w.NextId(), DefId = towerId, SocketId = "x", Pos = skiff.Pos + offset,
            PathLevels = new int[def.UpgradePaths.Count],
        });
        float before = skiff.Hp;
        for (int i = 0; i < Balance.TickHz * 3; i++) Step.Advance(w);
        return before - skiff.Hp;
    }

    [Fact]
    public void EveryTowerThatListsAirCanActuallyKillAFlyer()
    {
        foreach (string id in Damaging)
            Assert.True(DamageOverThreeSeconds(id, new Vec3(3f, -2f, 0f)) > 20f,
                $"{id} lists Air but did no damage to a Skiff 3.6 m away");
    }

    [Fact]
    public void GroundOnlyAndSupportTowersLeaveFlyersAlone()
    {
        // Lance cannot target air at all; Detector and Singularity reach it and
        // do no damage — reveal and chill. A lane held by those alone is not
        // held, which is why the map validator counts damaging towers only.
        foreach (string id in new[] { "lance", "nova", "detector", "singularity" })
            Assert.Equal(0f, DamageOverThreeSeconds(id, new Vec3(3f, -2f, 0f)), 2);
    }

    [Fact]
    public void ArcAndFilamentCannotReachTheStrandFromTheGroundOnAnyMap()
    {
        // The finding this file exists for. A tower on a ground pad sits at
        // y 0, so its whole range budget is spent climbing: Arc's 11 m and
        // Filament's 12 m are less than the height the strand cruises at on
        // every map in the campaign. They are deck weapons against air, and
        // the def table does not say so anywhere.
        //
        // If a map is ever authored with a strand low enough for them, this
        // test fails and the honest thing is to delete it.
        foreach (var map in Maps.All.Values)
        {
            var air = map.Routes.FirstOrDefault(r => r.Layer == EnemyLayer.Air);
            if (air is null) continue;
            float peak = air.Waypoints.Max(p => p.Y);

            foreach (string id in new[] { "arc", "filament" })
                Assert.True(Towers.All[id].RangeMeters < peak,
                    $"{map.Id}: {id} can now reach the strand's peak at {peak} m from the ground");
        }
    }

    [Fact]
    public void SkywatchIsTheOnlyAnswerAPlayerCanBuildWithoutClimbing()
    {
        // For every map, count the ground pads from which each tower can reach
        // any part of the strand. Skywatch's 15 m buys it a real presence from
        // the yard; the other two are near enough absent.
        foreach (var map in Maps.All.Values)
        {
            var air = map.Routes.FirstOrDefault(r => r.Layer == EnemyLayer.Air);
            if (air is null) continue;

            var samples = AirSamples(air);
            var groundPads = map.Sockets.Where(s => s.Tag == SocketTag.Ground).ToList();

            int skywatch = groundPads.Count(p => samples.Any(s => p.Pos.DistanceTo(s) <= 15f));
            int arc = groundPads.Count(p => samples.Any(s => p.Pos.DistanceTo(s) <= 11f));
            int filament = groundPads.Count(p => samples.Any(s => p.Pos.DistanceTo(s) <= 12f));

            Assert.True(skywatch >= arc && skywatch >= filament,
                $"{map.Id}: skywatch {skywatch}, arc {arc}, filament {filament} ground pads");
        }
    }

    /// <summary>One Skiff, one tower on one real socket, the whole flight —
    /// how much damage does it actually take?</summary>
    private static float DamageOverAWholeFlight(MapDef map, SocketDef socket, string towerId)
    {
        var def = Towers.All[towerId];
        int airRoute = map.Routes.ToList().FindIndex(r => r.Layer == EnemyLayer.Air);
        var w = new World(1, map);
        var skiff = new Enemy
        {
            Id = w.NextId(), DefId = "skiff", Hp = 100000f, MaxHp = 100000f,
            Facing = new Vec3(1, 0, 0), RouteIndex = airRoute, Leg = 0, LegProgress = 0f,
        };
        w.Enemies.Add(skiff);
        w.Towers.Add(new Tower
        {
            Id = w.NextId(), DefId = towerId, SocketId = socket.Id, Pos = socket.Pos,
            PathLevels = new int[def.UpgradePaths.Count],
        });

        float before = skiff.Hp;
        // Given enough health not to die, the flight ends when it leaks.
        for (int i = 0; i < Balance.TickHz * 120 && w.Enemies.Contains(skiff); i++) Step.Advance(w);
        return before - skiff.Hp;
    }

    [Fact]
    public void OnADeckSocketFilamentIsAsGoodAnAirAnswerAsSkywatch()
    {
        // The other half of the finding, and the one worth protecting: from a
        // deck, Filament is not a token air option — it is Skywatch's equal.
        // A Skiff has 26 hp, so these are whole flights' worth of kills.
        foreach (var map in Maps.All.Values)
        {
            if (!map.Routes.Any(r => r.Layer == EnemyLayer.Air)) continue;
            var decks = map.Sockets.Where(s => s.Tag == SocketTag.Wall).ToList();
            if (decks.Count == 0) continue;

            float filament = decks.Max(d => DamageOverAWholeFlight(map, d, "filament"));
            float skywatch = decks.Max(d => DamageOverAWholeFlight(map, d, "skywatch"));
            float skiffHp = Enemies.All["skiff"].Hp;

            Assert.True(filament > skiffHp * 3f,
                $"{map.Id}: the best deck socket only got {filament:0} damage out of a Filament, "
                + $"under three Skiffs' worth");
            Assert.True(filament > skywatch * 0.7f,
                $"{map.Id}: Filament {filament:0} is far behind Skywatch {skywatch:0} from a deck");
        }
    }

    [Fact]
    public void FromTheGroundArcIsNoAnswerAtAll()
    {
        // Switchyard is the map where this bites hardest: not one ground pad
        // lands a single hit on a flyer with an Arc on it. If that ever stops
        // being true the map has changed and this should be re-measured.
        var map = Maps.All["switchyard"];
        float best = map.Sockets
            .Where(s => s.Tag == SocketTag.Ground)
            .Max(s => DamageOverAWholeFlight(map, s, "arc"));
        Assert.Equal(0f, best, 2);
    }

    private static List<Vec3> AirSamples(RouteDef air)
    {
        var samples = new List<Vec3>();
        for (int i = 0; i < air.Waypoints.Count - 1; i++)
        {
            var a = air.Waypoints[i];
            var b = air.Waypoints[i + 1];
            int steps = Math.Max(1, (int)((b - a).Length() / 4f));
            for (int k = i == 0 ? 0 : 1; k <= steps; k++)
            {
                float t = (float)k / steps;
                samples.Add(new Vec3(a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t, a.Z + (b.Z - a.Z) * t));
            }
        }
        return samples;
    }
}
