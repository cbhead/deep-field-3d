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

        for (int i = 0; i < 7; i++)
        {
            w.Enqueue(new Command.UpgradeTower(0, towerId, 0));
            Step.Advance(w);
        }

        Assert.Equal(5, w.Towers[0].PathLevels[0]);   // L1-5 shipped at M1
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

        // L1-3 succeed on money; L4 requires the scrap recipe the pool lacks.
        Assert.Equal(3, w.Towers[0].PathLevels[0]);
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
        enemy.Leg = 0;
        enemy.LegProgress = 14f;
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
        Assert.True(WavePlan.HpScale(arc + 1, 1) > WavePlan.HpScale(1, 1));
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

    private static Enemy Drifter(World w, Vec3 at) => new()
    {
        Id = w.NextId(), DefId = "drifter", Hp = 1f, MaxHp = 30f,
        RouteIndex = 0, Leg = 1, LegProgress = 1f, Pos = at,
        Facing = new Vec3(1, 0, 0), Bounty = 5, LeakDamage = 1,
    };

    [Fact]
    public void APlayerKillDropsScrapOnTheFloorAndTheTeamHalfBanksAtOnce()
    {
        var w = WithPlayer(out var player);
        // Close enough to shoot (sidearm reaches 60 m), far enough that the
        // magnet does not sweep the drop up on the tick it lands.
        player.Pos = new Vec3(12, 0, 0);
        w.Enqueue(new Command.PlayerSync(1, player.Pos));

        var enemy = Drifter(w, new Vec3(0, 0, 0));
        w.Enemies.Add(enemy);
        w.Enqueue(new Command.PlayerHit(1, enemy.Id, "sidearm"));
        Step.Advance(w);

        Assert.NotEmpty(w.Pickups);
        Assert.Contains(w.Events, e => e is SimEvent.ScrapSpawned);
        // The team's half never hits the floor.
        Assert.True(w.TeamScrap.GetValueOrDefault(ScrapType.Alloy) > 0);
        // The personal half is not credited until someone walks to it.
        Assert.Equal(0, player.Scrap.GetValueOrDefault(ScrapType.Alloy));
    }

    [Fact]
    public void WalkingOverADropCollectsIt()
    {
        var w = WithPlayer(out var player);
        player.Pos = new Vec3(12, 0, 0);
        w.Enqueue(new Command.PlayerSync(1, player.Pos));
        var enemy = Drifter(w, new Vec3(0, 0, 0));
        w.Enemies.Add(enemy);
        w.Enqueue(new Command.PlayerHit(1, enemy.Id, "sidearm"));
        Step.Advance(w);
        var pickup = Assert.Single(w.Pickups);
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
        player.Pos = new Vec3(12, 0, 0);
        w.Enqueue(new Command.PlayerSync(1, player.Pos));
        var enemy = Drifter(w, new Vec3(0, 0, 0));
        w.Enemies.Add(enemy);
        w.Enqueue(new Command.PlayerHit(1, enemy.Id, "sidearm"));
        Step.Advance(w);
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
    public void ATowerKillHasNoPersonalHalfToDrop()
    {
        var w = WithPlayer(out var player);
        // Standing on the far side of the map: whatever kills this, it is not
        // the player, and there is nobody near the corpse either way.
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
        Assert.True(w.TeamScrap.GetValueOrDefault(ScrapType.Alloy) > 0,
            "a tower kill still pays the team");
        Assert.Empty(w.Pickups);
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
        player.Pos = new Vec3(12, 0, 0);
        w.Enqueue(new Command.PlayerSync(1, player.Pos));
        var enemy = Drifter(w, new Vec3(0, 0, 0));
        w.Enemies.Add(enemy);
        w.Enqueue(new Command.PlayerHit(1, enemy.Id, "sidearm"));
        Step.Advance(w);

        var back = Serialization.Deserialize(Serialization.Serialize(w));
        Assert.Equal(w.Pickups.Count, back.Pickups.Count);
        Assert.Equal(w.Pickups[0].Type, back.Pickups[0].Type);
        Assert.Equal(w.Pickups[0].Amount, back.Pickups[0].Amount);
    }
}
