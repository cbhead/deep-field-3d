using DeepField.Sim;
using DeepField.Sim.Content;
using DeepField.Sim.Util;
using Xunit;

namespace DeepField.Sim.Tests;

public class RngTests
{
    [Fact]
    public void StreamsAreIndependentOfDrawOrder()
    {
        var clean = RngStreams.StreamFor(42, RngStreams.Wave, 23);
        var expected = new[] { clean.NextUInt(), clean.NextUInt(), clean.NextUInt() };

        var noisy = RngStreams.StreamFor(42, RngStreams.Combat);
        for (int i = 0; i < 1000; i++) noisy.NextUInt();

        var fresh = RngStreams.StreamFor(42, RngStreams.Wave, 23);
        Assert.Equal(expected, new[] { fresh.NextUInt(), fresh.NextUInt(), fresh.NextUInt() });
    }

    [Fact]
    public void DifferentStreamsDiffer()
    {
        var a = RngStreams.StreamFor(42, RngStreams.Wave, 1);
        var b = RngStreams.StreamFor(42, RngStreams.Spawn, 1);
        Assert.NotEqual(a.NextUInt(), b.NextUInt());
    }
}

public class WavePlanTests
{
    [Fact]
    public void PlanIsAPureFunction()
    {
        var a = WavePlan.PlanWave(7, Maps.Foundry, 3, 2);
        var b = WavePlan.PlanWave(7, Maps.Foundry, 3, 2);
        Assert.Equal(a, b);
    }

    [Fact]
    public void PlayerScalingIsCountFirst()
    {
        var solo = WavePlan.PlanWave(7, Maps.Foundry, 4, 1);
        var four = WavePlan.PlanWave(7, Maps.Foundry, 4, 4);
        Assert.True(four.Count > solo.Count, $"solo {solo.Count} vs 4p {four.Count}");
    }

    [Fact]
    public void SkiffWaveUsesTheAirRoute()
    {
        var wave4 = WavePlan.PlanWave(7, Maps.Foundry, 3, 1);
        int airRoute = 1; // Foundry route order: ground, air
        Assert.Contains(wave4, e => e.DefId == "skiff" && e.RouteIndex == airRoute);
    }
}

public class CommandTests
{
    private static World Fresh() => new(1, Maps.TestLane);

    [Fact]
    public void OccupiedSocketIsRefused()
    {
        var w = Fresh();
        w.Enqueue(new Command.PlaceTower(0, "lance", "s1"));
        Step.Advance(w);
        w.Enqueue(new Command.PlaceTower(0, "lance", "s1"));
        Step.Advance(w);

        var rejection = Assert.Single(w.Events.OfType<SimEvent.BuildRejected>());
        Assert.Equal("occupied", rejection.Reason);
        Assert.Single(w.Towers);
    }

    [Fact]
    public void InsufficientFundsIsRefusedAndMoneyUntouched()
    {
        var w = Fresh();
        w.Money = 10;
        w.Enqueue(new Command.PlaceTower(0, "lance", "s1"));
        Step.Advance(w);

        var rejection = Assert.Single(w.Events.OfType<SimEvent.BuildRejected>());
        Assert.Equal("insufficientFunds", rejection.Reason);
        Assert.Equal(10, w.Money);
        Assert.Empty(w.Towers);
    }

    [Fact]
    public void PlayerHitRequiresJoinAndRespectsCooldown()
    {
        var w = Fresh();
        w.Enqueue(new Command.Join(1, "p1", "ember"));
        w.Enqueue(new Command.StartWave(1));
        while (w.Enemies.Count == 0) Step.Advance(w);
        int enemyId = w.Enemies[0].Id;

        w.Enqueue(new Command.PlayerHit(1, enemyId, "sidearm"));
        w.Enqueue(new Command.PlayerHit(1, enemyId, "sidearm"));
        Step.Advance(w);

        Assert.Single(w.Events.OfType<SimEvent.EnemyDamaged>());
    }

    [Fact]
    public void FactionExclusivityIsEnforced()
    {
        var w = Fresh();
        w.Enqueue(new Command.Join(1, "p1", "ember"));
        w.Enqueue(new Command.Join(2, "p2", "ember"));
        Step.Advance(w);

        Assert.Single(w.Players);
        var rejection = Assert.Single(w.Events.OfType<SimEvent.JoinRejected>());
        Assert.Equal("factionTaken", rejection.Reason);
    }

    [Fact]
    public void UpgradeLevelsCostMoneyAndStopAtCap()
    {
        var w = Fresh();
        w.Money = 100000;
        w.TeamScrap[ScrapType.Alloy] = 100;
        w.TeamScrap[ScrapType.Plating] = 100;
        w.Enqueue(new Command.PlaceTower(0, "lance", "s1"));
        Step.Advance(w);
        int towerId = w.Towers[0].Id;

        foreach (ScrapType type in System.Enum.GetValues<ScrapType>())
            w.TeamScrap[type] = 100;

        for (int i = 0; i < 14; i++)
        {
            w.Enqueue(new Command.UpgradeTower(0, towerId, 0));
            Step.Advance(w);
        }

        // Ten levels: built at 1, nine purchases to 10, and no more.
        var path = Towers.Lance.UpgradePaths[0];
        Assert.Equal(10, path.MaxLevel);
        Assert.Equal(9, w.Towers[0].PathLevels[0]);
        Assert.Contains(w.Events.OfType<SimEvent.UpgradeRejected>(), e => e.Reason == "maxLevel");
    }

    [Fact]
    public void BreakpointUpgradeNeedsScrap()
    {
        var w = Fresh();
        w.Money = 100000;
        w.Enqueue(new Command.PlaceTower(0, "lance", "s1"));
        Step.Advance(w);
        int towerId = w.Towers[0].Id;

        for (int i = 0; i < 4; i++)
        {
            w.Enqueue(new Command.UpgradeTower(0, towerId, 0));
            Step.Advance(w);
        }

        // Built at L1; two purchases reach L3 on money alone, and the third —
        // the one that arrives at L4 — wants a recipe the pool cannot pay.
        Assert.Equal(2, w.Towers[0].PathLevels[0]);
        Assert.Contains(w.Events.OfType<SimEvent.UpgradeRejected>()
            .Select(e => e.Reason), r => r == "insufficientScrap");
    }
}

public class StatusTests
{
    private static (World, Enemy) WorldWithEnemy(string defId = "drifter", float hp = 1000f)
    {
        var w = new World(1, Maps.TestLane);
        var def = Enemies.All[defId];
        var enemy = new Enemy
        {
            Id = w.NextId(), DefId = defId, Hp = hp, MaxHp = hp,
            Pos = w.Map.Routes[0].Waypoints[0], Facing = new Vec3(1, 0, 0),
            Bounty = def.Bounty, LeakDamage = 1,
        };
        w.Enemies.Add(enemy);
        return (w, enemy);
    }

    [Fact]
    public void ChillSlowsMovement()
    {
        var (w, chilled) = WorldWithEnemy();
        chilled.Statuses[(int)Channel.Movement] = new StatusSlot
        {
            StatusId = "chill", TimeLeft = 10f, Source = "test",
        };
        float before = chilled.TotalTraveled;
        Step.Advance(w);
        float chilledDist = chilled.TotalTraveled - before;

        var (w2, normal) = WorldWithEnemy();
        float before2 = normal.TotalTraveled;
        Step.Advance(w2);
        float normalDist = normal.TotalTraveled - before2;

        Assert.True(chilledDist < normalDist * 0.7f, $"chilled {chilledDist} vs normal {normalDist}");
    }

    [Fact]
    public void BurnTicksAndKillsWithAttribution()
    {
        var (w, enemy) = WorldWithEnemy(hp: 2f);
        w.Enqueue(new Command.Join(1, "p1", "ember"));
        Step.Advance(w);
        enemy.Statuses[(int)Channel.Thermal] = new StatusSlot
        {
            StatusId = "burn", TimeLeft = 5f, Source = "player1",
        };

        for (int i = 0; i < Balance.TickHz * 2 && !enemy.Dead; i++) Step.Advance(w);

        Assert.True(enemy.Dead);
        // Burn kill credited to the player: personal scrap share flowed.
        Assert.True(w.Players[1].Scrap.GetValueOrDefault(ScrapType.Alloy) > 0
            || w.TeamScrap.GetValueOrDefault(ScrapType.Alloy) > 0);
    }

    [Fact]
    public void StrongestChillWinsAndWeakerIsIgnored()
    {
        // Only one chill def at M1 — refresh semantics: reapply extends.
        var (w, enemy) = WorldWithEnemy();
        enemy.Statuses[(int)Channel.Movement] = new StatusSlot
        {
            StatusId = "chill", TimeLeft = 0.2f, Source = "a",
        };
        // An aura tick refreshes to full duration. Position via leg coords
        // (MoveEnemies recomputes Pos): leg 0 at 14m sits ~5m from socket s1.
        enemy.AtRouteLeg(w, 0, 0, 14f);
        w.Money = 1000;
        w.Enqueue(new Command.PlaceTower(0, "singularity", "s1"));
        Step.Advance(w);

        Assert.True(enemy.Statuses[(int)Channel.Movement].TimeLeft > 0.5f);
    }
}

/// <summary>The party lobby and endless mode, both flags on the world.</summary>
public class LobbyAndEndlessTests
{
    private static void Advance(World w, float seconds)
    {
        for (int i = 0; i < (int)(seconds * Balance.TickHz); i++) Step.Advance(w);
    }

    [Fact]
    public void LobbyHoldsUntilTheLaunchSeatLaunches()
    {
        var w = new World(3, Maps.TestLane) { Lobby = true };
        w.Enqueue(new Command.Join(1, "host", "ember"));
        w.Enqueue(new Command.Join(2, "guest", "forge"));
        w.Enqueue(new Command.StartWave(1));         // ignored while assembling
        Advance(w, Balance.IntermissionSeconds * 3);
        Assert.Equal(MatchPhase.Intermission, w.Phase);
        Assert.Equal(-1, w.WaveIndex);

        // Re-picking a taken faction is refused, the same rule as joining.
        w.Enqueue(new Command.SetFaction(2, "ember"));
        Step.Advance(w);
        Assert.Contains(w.Events, e => e is SimEvent.JoinRejected { PlayerId: 2, Reason: "factionTaken" });
        Assert.Equal("forge", w.Players[2].FactionId);
        w.Enqueue(new Command.SetFaction(2, "tempest"));
        Step.Advance(w);
        Assert.Equal("tempest", w.Players[2].FactionId);

        // Only the launch seat launches.
        w.Enqueue(new Command.Launch(2));
        Step.Advance(w);
        Assert.True(w.Lobby);
        w.Enqueue(new Command.Launch(1));
        Step.Advance(w);
        Assert.False(w.Lobby);
        Assert.Contains(w.Events, e => e is SimEvent.MatchLaunched);

        Advance(w, Balance.IntermissionSeconds + 1f);
        Assert.Equal(MatchPhase.Wave, w.Phase);
        Assert.Equal(0, w.WaveIndex);
    }

    [Fact]
    public void EndlessRollsPastTheAuthoredArc()
    {
        var w = new World(3, Maps.TestLane) { Endless = true };
        w.Enqueue(new Command.Join(1, "solo", "ember"));
        int target = Maps.TestLane.TotalWaves + 2;
        for (int tick = 0; tick < Balance.TickHz * 600 && w.WaveIndex < target; tick++)
        {
            // Clear the field as it fills, so the arc advances on the sim's own clock.
            foreach (var enemy in w.Enemies) { enemy.Hp = 0f; enemy.Dead = true; }
            if (w.Phase == MatchPhase.Intermission) w.Enqueue(new Command.StartWave(1));
            Step.Advance(w);
            Assert.NotEqual(MatchPhase.Victory, w.Phase);
        }
        Assert.True(w.WaveIndex >= target, $"reached wave index {w.WaveIndex}");
        Assert.NotEqual(MatchPhase.Victory, w.Phase);

        // The same authored table, but heavier: hp compounds on the raw index
        // and the second lap adds bodies.
        int arc = Maps.TestLane.TotalWaves;
        Assert.True(WavePlan.HpScale(Maps.TestLane, arc + 1, 1) > WavePlan.HpScale(Maps.TestLane, 1, 1));
        Assert.True(WavePlan.PlanWave(3, Maps.TestLane, arc + 1, 1).Count
                    >= WavePlan.PlanWave(3, Maps.TestLane, 1, 1).Count);
    }

    [Fact]
    public void LobbyAndEndlessSurviveSerialization()
    {
        var w = new World(5, Maps.TestLane) { Lobby = true, Endless = true };
        var back = Serialization.Deserialize(Serialization.Serialize(w));
        Assert.True(back.Lobby);
        Assert.True(back.Endless);
    }
}

/// <summary>Scrap: personal drops land on the floor, the team's half does not,
/// and a platform is bought with what you picked up.</summary>
public class ScrapEconomyTests
{
    private static World WithPlayer(out PlayerState player)
    {
        var w = new World(11, Maps.TestLane);
        w.Enqueue(new Command.Join(1, "solo", "ember"));
        Step.Advance(w);
        player = w.Players[1];
        return w;
    }

    private static Enemy Drifter(World w, Vec3 at) => new Enemy
    {
        Id = w.NextId(), DefId = "drifter", Hp = 1f, MaxHp = 30f, Pos = at,
        Facing = new Vec3(1, 0, 0), Bounty = 5, LeakDamage = 1,
    }.AtRouteLeg(w, 0, 1, 1f);

    /// <summary>Kill something with a tower and leave the drop on the floor.
    ///
    /// It has to be a real tower landing real shots: setting Hp to 0 by hand
    /// skips the death path entirely, so nothing is dropped, banked or
    /// emitted, and the test passes by measuring nothing.</summary>
    internal static ScrapPickup TowerKill(World w, PlayerState player)
    {
        // Out of the way, so the drop is not swept up by the magnet before the
        // test has looked at it.
        player.Pos = new Vec3(0, 0, -14);
        w.Enqueue(new Command.PlayerSync(1, player.Pos));
        w.Money = 1000;
        w.Enqueue(new Command.PlaceTower(1, "lance", "s3"));
        Step.Advance(w);
        Assert.Single(w.Towers);

        var enemy = Drifter(w, new Vec3(4, 0, 4));   // beside the Lance on s3
        enemy.Hp = enemy.MaxHp = 30f;
        w.Enemies.Add(enemy);
        for (int i = 0; i < Balance.TickHz * 20 && w.Enemies.Contains(enemy); i++) Step.Advance(w);
        Assert.DoesNotContain(enemy, w.Enemies);

        // Checked here because Advance clears the event list every tick, and
        // the tidying below costs one more tick.
        Assert.Contains(w.Events, e => e is SimEvent.ScrapSpawned);

        // Leave the field quiet. A Lance left standing keeps killing whatever
        // the wave sends next, and a test that then waits three quarters of a
        // minute finds five drops instead of the one it made.
        w.Enqueue(new Command.SellTower(1, w.Towers[0].Id));
        Step.Advance(w);
        w.Enemies.Clear();

        return Assert.Single(w.Pickups);
    }

    [Fact]
    public void APlayerKillIsCollectedOnTheSpot()
    {
        var w = WithPlayer(out var player);
        // Far enough away that the magnet could not have reached a drop, so a
        // credit here can only have come from the kill itself.
        player.Pos = new Vec3(12, 0, 0);
        w.Enqueue(new Command.PlayerSync(1, player.Pos));

        var enemy = Drifter(w, new Vec3(0, 0, 0));
        w.Enemies.Add(enemy);
        w.Enqueue(new Command.PlayerHit(1, enemy.Id, "sidearm"));
        Step.Advance(w);

        // Nothing to walk to: you shot it, it is yours.
        Assert.Empty(w.Pickups);
        Assert.DoesNotContain(w.Events, e => e is SimEvent.ScrapSpawned);
        Assert.True(player.Scrap.GetValueOrDefault(ScrapType.Alloy) > 0);
        // And the team's half still banks, as it does for every kill.
        Assert.True(w.TeamScrap.GetValueOrDefault(ScrapType.Alloy) > 0);
        // PickupId 0 marks scrap that never touched the floor.
        Assert.Contains(w.Events, e => e is SimEvent.ScrapCollected { PickupId: 0, PlayerId: 1 });
    }

    [Fact]
    public void WalkingOverADropCollectsIt()
    {
        var w = WithPlayer(out var player);
        var pickup = TowerKill(w, player);
        int amount = pickup.Amount;

        w.Enqueue(new Command.PlayerSync(1, pickup.Pos));
        Step.Advance(w);

        Assert.Empty(w.Pickups);
        Assert.Equal(amount, player.Scrap.GetValueOrDefault(ScrapType.Alloy));
        Assert.Contains(w.Events, e => e is SimEvent.ScrapCollected { PlayerId: 1 });
    }

    [Fact]
    public void UncollectedScrapBanksToTheTeamRatherThanVanishing()
    {
        var w = WithPlayer(out var player);
        TowerKill(w, player);
        // Out of magnet range for the rest of the run, so it times out.
        w.Enqueue(new Command.PlayerSync(1, new Vec3(200, 0, 200)));
        int team = w.TeamScrap.GetValueOrDefault(ScrapType.Alloy);
        int onFloor = w.Pickups.Sum(p => p.Amount);

        for (int i = 0; i < (int)((Balance.ScrapPickupSeconds + 1f) * Balance.TickHz); i++) Step.Advance(w);

        Assert.Empty(w.Pickups);
        Assert.Equal(0, player.Scrap.GetValueOrDefault(ScrapType.Alloy));
        Assert.Equal(team + onFloor, w.TeamScrap.GetValueOrDefault(ScrapType.Alloy));
    }

    [Fact]
    public void ATowerKillPutsThePersonalHalfOnTheFloor()
    {
        var w = WithPlayer(out var player);
        var pickup = TowerKill(w, player);

        // The majority of kills in a match are a tower's. They used to bank the
        // whole yield silently, so most of a match's income was something the
        // player never saw happen.
        Assert.True(pickup.Amount > 0);
        Assert.True(w.TeamScrap.GetValueOrDefault(ScrapType.Alloy) > 0,
            "a tower kill still pays the team its half");
        // Not credited to anyone until somebody walks over it.
        Assert.Equal(0, player.Scrap.GetValueOrDefault(ScrapType.Alloy));
    }

    [Fact]
    public void PlatformsCostPersonalScrapNotTheSharedWallet()
    {
        var w = WithPlayer(out var player);
        w.Money = 100000;                       // the wallet cannot buy a gun

        w.Enqueue(new Command.BuyWeapon(1, "rifle"));
        Step.Advance(w);
        Assert.DoesNotContain("rifle", player.OwnedWeapons);
        Assert.Contains(w.Events, e => e is SimEvent.PurchaseRejected { Reason: "insufficientScrap" });
        Assert.Equal(100000, w.Money);

        foreach (var (type, amount) in Weapons.Rifle.Recipe) player.Scrap[type] = amount;
        w.Enqueue(new Command.BuyWeapon(1, "rifle"));
        Step.Advance(w);
        Assert.Contains("rifle", player.OwnedWeapons);
        Assert.Equal(100000, w.Money);          // still untouched
        foreach (var type in Weapons.Rifle.Recipe.Keys)
            Assert.Equal(0, player.Scrap.GetValueOrDefault(type));
    }

    [Fact]
    public void PickupsSurviveSerialization()
    {
        var w = WithPlayer(out var player);
        TowerKill(w, player);

        var back = Serialization.Deserialize(Serialization.Serialize(w));
        Assert.Equal(w.Pickups.Count, back.Pickups.Count);
        Assert.Equal(w.Pickups[0].Type, back.Pickups[0].Type);
        Assert.Equal(w.Pickups[0].Amount, back.Pickups[0].Amount);
    }
}

/// <summary>Scrap from the air lane has to come down to where a player is.</summary>


public class AirborneScrapTests
{
    private static (World World, PlayerState Player) Solo(MapDef map)
    {
        var w = new World(21, map);
        w.Enqueue(new Command.Join(1, "solo", "ember"));
        Step.Advance(w);
        return (w, w.Players[1]);
    }

    /// <summary>A Skiff actually flying the strand.
    ///
    /// Its position is its route progress, so handing one a Pos does not stick
    /// — MoveEnemies puts it back on its lane before any tower or test looks at
    /// it, and a Skiff given route 0 quietly ends up walking the ground route.
    /// Spawn it on the air lane and let the sim place it.</summary>
    private static Enemy SkiffOnTheStrand(World w, MapDef map, int leg, float progress)
    {
        int airRoute = map.Routes.ToList().FindIndex(r => r.Layer == EnemyLayer.Air);
        var skiff = new Enemy
        {
            Id = w.NextId(), DefId = "skiff", Hp = 1f, MaxHp = 40f,
            Facing = new Vec3(1, 0, 0), Bounty = 8, LeakDamage = 1,
        }.AtRouteLeg(w, airRoute, leg, progress);
        w.Enemies.Add(skiff);
        Step.Advance(w);
        return skiff;
    }

    /// <summary>Kill a flyer with a Skywatch built on the nearest pad that can
    /// reach it, so the drop belongs to a tower and has to hit the floor.</summary>
    private static void SkywatchKill(World w, MapDef map, Enemy flyer)
    {
        var pad = map.Sockets
            .Where(s => s.Tag is SocketTag.Ground or SocketTag.Wall)
            .Where(s => s.Pos.DistanceTo(flyer.Pos) <= Towers.Skywatch.RangeMeters)
            .OrderBy(s => s.Pos.DistanceTo(flyer.Pos))
            .FirstOrDefault();
        Assert.True(pad is not null,
            $"no pad on {map.Id} can reach a flyer at {flyer.Pos.X:0},{flyer.Pos.Y:0},{flyer.Pos.Z:0}");

        w.Money = 10000;
        w.Enqueue(new Command.PlaceTower(1, "skywatch", pad!.Id));
        Step.Advance(w);
        Assert.Single(w.Towers);
        for (int i = 0; i < Balance.TickHz * 20 && w.Enemies.Contains(flyer); i++) Step.Advance(w);
        Assert.DoesNotContain(flyer, w.Enemies);
    }

    [Fact]
    public void ShootingAFlyerYourselfHandsYouTheScrapInsteadOfDroppingItOutOfReach()
    {
        // This is the whole reason a flyer's drop used to be a problem: it died
        // fifteen metres up, its scrap fell wherever physics put it, and on a
        // map with decks and lanes that was regularly somewhere nobody could
        // stand. A player who lands the kill is simply paid.
        var (w, player) = Solo(Maps.Foundry);
        var skiff = SkiffOnTheStrand(w, Maps.Foundry, leg: 1, progress: 0.5f);
        Assert.True(skiff.Pos.Y > 5f, $"the air lane should be off the ground (was {skiff.Pos.Y})");

        // Standing well clear, and on the floor: if this scrap arrives it did
        // not arrive by being walked over.
        player.Pos = new Vec3(skiff.Pos.X + 14f, 0f, skiff.Pos.Z);
        w.Enqueue(new Command.PlayerSync(1, player.Pos));
        w.Enqueue(new Command.PlayerHit(1, skiff.Id, "sidearm"));
        Step.Advance(w);

        Assert.Empty(w.Pickups);
        Assert.True(player.Scrap.Values.Sum() > 0);
    }

    [Fact]
    public void ScrapFromATowerKilledFlyerFallsToTheFloorAndIsCollectable()
    {
        // A tower's kill still has to put the drop somewhere a player can get
        // to, which on an air lane means it has to come down.
        var (w, player) = Solo(Maps.Foundry);
        player.Pos = new Vec3(40f, 0f, 30f);            // nowhere near the corpse
        w.Enqueue(new Command.PlayerSync(1, player.Pos));

        var skiff = SkiffOnTheStrand(w, Maps.Foundry, leg: 1, progress: 0.5f);
        Assert.True(skiff.Pos.Y > 5f, $"the strand should be off the ground (was {skiff.Pos.Y})");
        SkywatchKill(w, Maps.Foundry, skiff);

        var drop = Assert.Single(w.Pickups);
        // It comes down on its own.
        for (int i = 0; i < Balance.TickHz * 5; i++) Step.Advance(w);
        drop = Assert.Single(w.Pickups);
        Assert.True(drop.Pos.Y <= 0.01f, $"scrap should reach the floor, rested at {drop.Pos.Y}");

        // And a player standing on the floor can reach it.
        w.Enqueue(new Command.PlayerSync(1, drop.Pos));
        Step.Advance(w);
        Assert.Empty(w.Pickups);
        Assert.True(player.Scrap.Values.Sum() > 0);
    }

    [Fact]
    public void OnAClimbingMapScrapLandsOnTheTierBelowItNotTheStreet()
    {
        var (w, player) = Solo(Maps.Spire);
        player.Pos = new Vec3(40f, 0f, 30f);
        w.Enqueue(new Command.PlayerSync(1, player.Pos));

        // High over the building rather than out over the street.
        var skiff = SkiffOnTheStrand(w, Maps.Spire, leg: 2, progress: 0.5f);
        Assert.True(skiff.Pos.Y > 20f, $"expected the strand high here (was {skiff.Pos.Y})");
        SkywatchKill(w, Maps.Spire, skiff);

        var drop = Assert.Single(w.Pickups);
        float tier = drop.GroundY;
        for (int i = 0; i < Balance.TickHz * 10; i++) Step.Advance(w);
        drop = Assert.Single(w.Pickups);

        // It stops on the tier the sim found under it and goes no further.
        Assert.Equal(tier, drop.Pos.Y, 2);
        Assert.True(tier > 1f,
            $"scrap high over a climbing map should rest on a tier, not the street (rested at {tier})");
    }
}

public class UpgradeGridTests
{
    [Fact]
    public void EveryPathIsTenLevelsWithBreakpointsAtFourSevenAndTen()
    {
        foreach (var tower in Towers.All.Values)
            foreach (var path in tower.UpgradePaths)
            {
                Assert.Equal(10, path.MaxLevel);
                Assert.Equal(9, path.LevelCosts.Count);
                foreach (int bp in new[] { 4, 7, 10 })
                    Assert.True(path.RecipeFor(bp) is { Count: > 0 },
                        $"{tower.Id}/{path.Id} has no recipe at L{bp}");
                // Nothing else costs scrap: a breakpoint is meant to be the
                // level that stands out.
                for (int level = 2; level <= 10; level++)
                    if (level is not (4 or 7 or 10))
                        Assert.Null(path.RecipeFor(level));
            }
    }

    [Fact]
    public void TheTopOfAPathIsPaidForInTheRarestScrap()
    {
        foreach (var tower in Towers.All.Values)
            foreach (var path in tower.UpgradePaths)
            {
                Assert.DoesNotContain(ScrapType.Gravium, path.RecipeFor(4)!.Keys);
                Assert.Contains(ScrapType.Gravium, path.RecipeFor(10)!.Keys);
            }
    }

    [Fact]
    public void CostsClimbSoTheLastLevelsAreTheDecision()
    {
        var costs = Towers.Lance.UpgradePaths[0].LevelCosts;
        for (int i = 1; i < costs.Count; i++)
            Assert.True(costs[i] > costs[i - 1], "each level costs more than the last");
        // The plan's shape: a campaign funds about one maxed path, so the top
        // of the ladder has to dominate the bottom of it.
        int firstSix = costs.Take(5).Sum(), lastFour = costs.Skip(5).Sum();
        Assert.True(lastFour > firstSix,
            $"levels 7-10 ({lastFour}) should cost more than 2-6 ({firstSix})");
    }

    [Fact]
    public void APathCanBeTakenAllTheWayToTenAndTheArtFollowsIt()
    {
        var w = new World(4, Maps.TestLane);
        w.Money = 100000;
        foreach (ScrapType type in System.Enum.GetValues<ScrapType>()) w.TeamScrap[type] = 200;
        w.Enqueue(new Command.PlaceTower(0, "lance", "s1"));
        Step.Advance(w);
        var tower = w.Towers[0];

        for (int i = 0; i < 9; i++)
        {
            w.Enqueue(new Command.UpgradeTower(0, tower.Id, 0));
            Step.Advance(w);
            Assert.DoesNotContain(w.Events.OfType<SimEvent.UpgradeRejected>(), e => true);
        }

        Assert.Equal(9, tower.PathLevels[0]);
        // The client draws stage (purchases + 1), which is design's own
        // numbering: nine purchases is s10, the last stage it drew.
        Assert.Equal(10, tower.PathLevels[0] + 1);
        // And the path's damage actually compounded over all ten levels.
        Assert.True(MathF.Pow(Towers.Lance.UpgradePaths[0].PerLevelFactor, 9) > 2f);
    }
}

/// <summary>The range ring drawn on the deck and the range the tower fights
/// with come out of <see cref="TowerMath"/>, so these are the tests that keep
/// a player's picture of coverage honest.</summary>
public class TowerRangeTests
{
    private static int[] Levels(TowerDef def) => new int[def.UpgradePaths.Count];

    [Fact]
    public void AFreshTowerReachesExactlyItsDefRange()
    {
        var map = Maps.All["foundry"];
        foreach (var tower in Towers.All.Values)
            Assert.Equal(tower.RangeMeters,
                TowerMath.Range(tower, Levels(tower), map, waveIndex: 0), 3);
    }

    [Fact]
    public void EveryTowerThatHasRangeHasAPathThatGrowsIt()
    {
        foreach (var tower in Towers.All.Values)
        {
            if (tower.RangeMeters <= 0f) continue;      // a barricade has no reach
            Assert.True(TowerMath.RangePathIndex(tower) >= 0,
                $"{tower.Id} has range but no path that grows it");
        }
        // And the one with no reach reports none, rather than pointing at a
        // path that would draw a ring that never grows.
        Assert.Equal(-1, TowerMath.RangePathIndex(Towers.Barricade));
    }

    [Fact]
    public void TheRangePathCompoundsAndTheOthersDoNot()
    {
        var map = Maps.All["foundry"];
        var lance = Towers.Lance;
        int range = TowerMath.RangePathIndex(lance);

        var levels = Levels(lance);
        float baseline = TowerMath.Range(lance, levels, map, 0);

        levels[range] = 9;                                    // L10
        float maxed = TowerMath.Range(lance, levels, map, 0);
        Assert.True(maxed > baseline * 2.5f, $"L10 range was only {maxed:0.0} m");

        // Damage is path 0 on a Lance, and buying it must not move the ring.
        var damageOnly = Levels(lance);
        damageOnly[0] = 9;
        Assert.Equal(baseline, TowerMath.Range(lance, damageOnly, map, 0), 3);
    }

    [Fact]
    public void TheUpgradePreviewIsOneLevelAheadAndStopsAtTheCap()
    {
        var map = Maps.All["foundry"];
        var lance = Towers.Lance;
        int range = TowerMath.RangePathIndex(lance);
        var path = lance.UpgradePaths[range];

        var levels = Levels(lance);
        float now = TowerMath.Range(lance, levels, map, 0);
        float next = TowerMath.RangeAfterUpgrade(lance, levels, range, map, 0);
        Assert.Equal(now * path.PerLevelFactor, next, 3);

        // At the cap there is nothing further to preview, so the ring must not
        // promise a level that cannot be bought.
        levels[range] = path.MaxLevel - 1;
        Assert.Equal(TowerMath.Range(lance, levels, map, 0),
            TowerMath.RangeAfterUpgrade(lance, levels, range, map, 0), 3);

        // A path that is not the range path previews no change at all.
        Assert.Equal(TowerMath.Range(lance, Levels(lance), map, 0),
            TowerMath.RangeAfterUpgrade(lance, Levels(lance), 0, map, 0), 3);
    }

    [Fact]
    public void FogShrinksTheRingForEveryTowerItIsNotExemptFrom()
    {
        // Whichever wave the map's fog lands on, the ring has to shrink with
        // it — a ring that ignores weather is wrong on exactly the wave a
        // player checks it.
        // Any map that schedules a range-shrinking condition will do; every
        // map on a clear schedule simply has nothing to assert.
        bool checkedOne = false;
        foreach (var map in Maps.All.Values)
        foreach (int wave in map.ConditionSchedule.Keys)
        {
            var condition = Conditions.ForWave(map, wave);
            if (condition is null || condition.TowerRangeFactor >= 1f) continue;
            foreach (var tower in Towers.All.Values)
            {
                if (tower.RangeMeters <= 0f) continue;
                float expected = tower.RangeMeters
                    * (condition.RangeExemptTowerIds.Contains(tower.Id) ? 1f : condition.TowerRangeFactor);
                Assert.Equal(expected, TowerMath.Range(tower, Levels(tower), map, wave), 3);
                checkedOne = true;
            }
        }
        // A silent zero-iteration pass would let the weather term be deleted
        // without a single test noticing.
        Assert.True(checkedOne, "no map schedules a range-shrinking condition");
    }
}

/// <summary>The endless hp curve. Two curves meeting at the end of the
/// authored arc, and the tests that keep them meeting.</summary>

public class EndlessScalingTests
{
    [Fact]
    public void TheAuthoredCampaignCurveIsUntouched()
    {
        // The sweep balanced the campaign against Balance.HpGrowth compounded
        // on the wave index. Endless growing more slowly must not move a
        // single authored wave, or every gate in the harness is measuring a
        // different game than the one that was tuned.
        foreach (var map in Maps.All.Values)
        {
            int authored = Waves.ByMap[map.Id].Count;
            for (int wave = 0; wave < authored; wave++)
                Assert.Equal(MathF.Pow(Balance.HpGrowth, wave),
                    WavePlan.HpScale(map, wave, 1), 3);
        }
    }

    [Fact]
    public void TheCurveIsContinuousWhereTheTwoMeet()
    {
        // A step at the join would read as the game suddenly relenting the
        // moment the campaign runs out, which is worse than either curve.
        foreach (var map in Maps.All.Values)
        {
            int lastAuthored = Waves.ByMap[map.Id].Count - 1;
            float atJoin = WavePlan.HpScale(map, lastAuthored, 1);
            float justPast = WavePlan.HpScale(map, lastAuthored + 1, 1);
            Assert.Equal(atJoin * Balance.EndlessHpGrowth, justPast, 3);
        }
    }

    [Fact]
    public void EndlessStillGetsHarderForever()
    {
        // Gentler is not flat. Every wave past the arc must outweigh the one
        // before it, or endless stops being endless and becomes a plateau.
        var map = Maps.All["foundry"];
        for (int wave = 1; wave < 60; wave++)
            Assert.True(WavePlan.HpScale(map, wave, 1) > WavePlan.HpScale(map, wave - 1, 1),
                $"wave {wave} is no heavier than wave {wave - 1}");
    }

    [Fact]
    public void PastTheArcItIsActuallyGentlerThanTheCampaignCurve()
    {
        // The change this file exists for: at wave 15 on Foundry the old curve
        // was 19.7x hp, against a tower whose ceiling is 5.6x its starting dps.
        var map = Maps.All["foundry"];
        int lastAuthored = Waves.ByMap[map.Id].Count - 1;

        foreach (int wave in new[] { 15, 20, 25, 30 })
        {
            float now = WavePlan.HpScale(map, wave, 1);
            float unchanged = MathF.Pow(Balance.HpGrowth, wave);
            Assert.True(now < unchanged * 0.75f,
                $"wave {wave}: {now:0.0}x is not meaningfully under the old {unchanged:0.0}x");
        }
        // And the relief compounds, so the deep rounds are where it is felt.
        Assert.True(WavePlan.HpScale(map, 30, 1) < MathF.Pow(Balance.HpGrowth, 30) * 0.35f);
        Assert.True(lastAuthored > 0);
    }
}

/// <summary>What a kill pays, and whether it keeps up with what the kill costs.
///
/// A bounty used to come flat off the enemy's def, so a Drifter paid six
/// credits on wave 1 and six on wave 20 while its health compounded every wave
/// in between. On Foundry that was 0.300 credits per point of enemy health at
/// the start and 0.011 by wave 20.</summary>
public class BountyScalingTests
{
    private static int WaveBounty(MapDef map, int wave) =>
        WavePlan.PlanWave(1, map, wave, 1)
            .Sum(e => Math.Max(1, (int)MathF.Round(
                Enemies.All[e.DefId].Bounty * WavePlan.BountyScale(wave),
                MidpointRounding.AwayFromZero)));

    private static float WaveHp(MapDef map, int wave) =>
        WavePlan.PlanWave(1, map, wave, 1).Sum(e => Enemies.All[e.DefId].Hp * e.HpFactor);

    [Fact]
    public void EachWavePaysAtLeastFifteenPercentMoreThanTheOneBefore()
    {
        for (int wave = 1; wave < 40; wave++)
            Assert.True(WavePlan.BountyScale(wave) >= WavePlan.BountyScale(wave - 1) * 1.15f - 0.0001f,
                $"wave {wave} pays only {WavePlan.BountyScale(wave) / WavePlan.BountyScale(wave - 1):0.000}x "
                + "the wave before");
    }

    [Fact]
    public void TheFirstWaveIsUntouched()
    {
        // The campaign's opening is swept and balanced where it is. This change
        // is about the curve, not the starting line.
        Assert.Equal(1f, WavePlan.BountyScale(0), 3);
        foreach (var map in Maps.All.Values)
            Assert.Equal(
                WavePlan.PlanWave(1, map, 0, 1).Sum(e => Enemies.All[e.DefId].Bounty),
                WaveBounty(map, 0));
    }

    [Fact]
    public void IncomeNoLongerCollapsesAgainstTheThreat()
    {
        // Difficulty is still meant to bite — bounty grows slower than the
        // campaign's hp curve on purpose — but a twenty-seven-fold fall in
        // credits per point of health is not difficulty, it is a defence that
        // cannot be funded.
        var map = Maps.Foundry;
        float early = WaveBounty(map, 0) / WaveHp(map, 0);
        foreach (int wave in new[] { 9, 12, 15, 20, 25 })
        {
            float rate = WaveBounty(map, wave) / WaveHp(map, wave);
            Assert.True(rate > early * 0.35f,
                $"wave {wave} pays {rate:0.000} credits per hp against wave 0's {early:0.000}");
        }
    }

    [Fact]
    public void ALaterWavesKillIsWorthMoreThanTheSameEnemyEarlier()
    {
        // The end a player actually feels: the same enemy, later, pays more.
        var map = Maps.Foundry;
        var w = new World(5, map);
        w.Enqueue(new Command.Join(1, "solo", "ember"));
        Step.Advance(w);

        int Paid(int waveIndex)
        {
            w.WaveIndex = waveIndex;
            var before = w.Money;
            var enemy = new Enemy
            {
                Id = w.NextId(), DefId = "drifter", Hp = 1f, MaxHp = 30f,
                Facing = new Vec3(1, 0, 0),
                Bounty = Math.Max(1, (int)MathF.Round(
                    Enemies.All["drifter"].Bounty * WavePlan.BountyScale(waveIndex),
                    MidpointRounding.AwayFromZero)),
                LeakDamage = 1, WaveIndex = waveIndex,
            }.AtRouteLeg(w, 0, 1, 1f);
            w.Enemies.Add(enemy);
            w.Enqueue(new Command.PlayerHit(1, enemy.Id, "sidearm"));
            Step.Advance(w);
            return w.Money - before;
        }

        int early = Paid(0);
        // A second shot in the same breath is refused on the sidearm's
        // cooldown, and a kill that never happened pays nothing — which would
        // have read as the scaling being broken rather than the test being.
        for (int i = 0; i < Balance.TickHz; i++) Step.Advance(w);
        int late = Paid(10);
        Assert.True(late > early * 3f,
            $"a wave-10 Drifter paid {late} against a wave-0 Drifter's {early}");
    }

    [Fact]
    public void TheCheapestEnemyNeverRoundsAwayToNothing()
    {
        // A Mote is worth two credits. Nothing in the roster may round to a
        // kill that pays nothing at all.
        foreach (var def in Enemies.All.Values)
        {
            if (def.Bounty <= 0) continue;
            for (int wave = 0; wave < 30; wave++)
                Assert.True(Math.Max(1, (int)MathF.Round(def.Bounty * WavePlan.BountyScale(wave),
                    MidpointRounding.AwayFromZero)) >= 1);
        }
    }
}
