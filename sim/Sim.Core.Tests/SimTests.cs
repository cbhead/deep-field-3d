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
        // Drawing from one stream must not perturb another — the wave-23 invariant.
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
        var a = WavePlan.PlanWave(7, 2);
        var b = WavePlan.PlanWave(7, 2);
        Assert.Equal(a, b);
    }

    [Fact]
    public void WavesGrow()
    {
        Assert.True(WavePlan.PlanWave(7, 2).Count > WavePlan.PlanWave(7, 0).Count);
    }
}

public class CommandTests
{
    private static World Fresh() => new(1, Maps.TestLane);

    [Fact]
    public void OccupiedSocketIsRefused()
    {
        var w = Fresh();
        w.Enqueue(new Command.PlaceTower(0, Towers.Lance.Id, "s1"));
        Step.Advance(w);
        w.Enqueue(new Command.PlaceTower(0, Towers.Lance.Id, "s1"));
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
        w.Enqueue(new Command.PlaceTower(0, Towers.Lance.Id, "s1"));
        Step.Advance(w);

        var rejection = Assert.Single(w.Events.OfType<SimEvent.BuildRejected>());
        Assert.Equal("insufficientFunds", rejection.Reason);
        Assert.Equal(10, w.Money);
        Assert.Empty(w.Towers);
    }

    [Fact]
    public void PlayerHitRespectsWeaponCooldown()
    {
        var w = Fresh();
        // Skip intermission and let one enemy spawn.
        w.Enqueue(new Command.StartWave(0));
        while (w.Enemies.Count == 0) Step.Advance(w);
        int enemyId = w.Enemies[0].Id;

        w.Enqueue(new Command.PlayerHit(1, enemyId, Weapons.Sidearm.Id));
        w.Enqueue(new Command.PlayerHit(1, enemyId, Weapons.Sidearm.Id));
        Step.Advance(w);

        // Two hits enqueued the same tick: only the first lands (cooldown eats the second).
        Assert.Single(w.Events.OfType<SimEvent.EnemyDamaged>());
    }
}

public class LeakTests
{
    [Fact]
    public void LeakedEnemyCostsALife()
    {
        var w = new World(1, Maps.TestLane);
        int before = w.Lives;
        w.Enqueue(new Command.StartWave(0));

        // No towers, no player: everything leaks eventually.
        long safety = Balance.TickHz * 120;
        while (w.Lives == before && w.Tick < safety) Step.Advance(w);

        Assert.True(w.Lives < before);
    }
}
