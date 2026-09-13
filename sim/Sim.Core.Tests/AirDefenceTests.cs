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
            Facing = new Vec3(1, 0, 0),
        }.AtRouteLeg(w, airRoute, 0, 0f);
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
        // the three maps built around a deck. They are deck weapons against
        // air there, and the def table does not say so anywhere.
        //
        // The Toaster is the exception and it is deliberate: a farm is flat,
        // there is no deck to put anything on, and a strand at 15 m would have
        // been answerable from nowhere. It flies at 9 m instead, which is
        // under every air tower's reach from the ground. So the claim is not
        // "Arc and Filament never answer air" — it is that a map with a deck
        // puts the strand above them, and a map without one has to put it
        // below them. A map that does neither is the bug.
        var deckless = new[] { "toaster" };
        foreach (var map in Maps.All.Values)
        {
            var air = map.Routes.FirstOrDefault(r => r.Layer == EnemyLayer.Air);
            if (air is null) continue;
            float peak = air.Waypoints.Max(p => p.Y);

            foreach (string id in new[] { "arc", "filament" })
            {
                bool reaches = Towers.All[id].RangeMeters >= peak;
                Assert.True(reaches == deckless.Contains(map.Id),
                    reaches
                        ? $"{map.Id}: {id} can now reach the strand's peak at {peak} m from the "
                          + "ground. If that is the design, say so in the deckless list above."
                        : $"{map.Id}: has no deck, and its strand at {peak} m is over {id}'s head "
                          + "from the only tier it has.");
            }
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
            Facing = new Vec3(1, 0, 0),
        }.AtRouteLeg(w, airRoute, 0, 0f);
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
    public void FromADeckFilamentIsRealAntiAirAndSkywatchIsTheSpecialist()
    {
        // This test used to claim Filament was Skywatch's equal from a deck,
        // and it was — 127 against 149 over a flight. Skywatch's base damage
        // went from 5.5 to 8 deliberately, to make the one tower a player can
        // build anywhere against flyers actually finish the job, and that
        // relationship is now 216 against 127 by design.
        //
        // Both halves still matter. Filament must stay a real answer for
        // anyone who has committed to the catwalk, and Skywatch must be the
        // one that specialises. A Skiff has 26 hp, so these are flights' worth
        // of kills.
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
                + "under three Skiffs' worth — it has stopped being an air option at all");
            Assert.True(skywatch > filament,
                $"{map.Id}: Skywatch {skywatch:0} no longer leads Filament {filament:0} from a deck, "
                + "which is the whole reason it is air-only");
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
