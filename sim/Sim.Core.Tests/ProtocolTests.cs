using DeepField.Sim;
using DeepField.Sim.Content;
using Xunit;

namespace DeepField.Sim.Tests;

public class ProtocolTests
{
    [Fact]
    public void EnemySnapshotRoundTrips()
    {
        var w = new World(9, Maps.Foundry);
        w.Enqueue(new Command.StartWave(0));
        for (int i = 0; i < Balance.TickHz * 10; i++) Step.Advance(w);
        Assert.NotEmpty(w.Enemies);

        var packed = Protocol.PackEnemies(w);
        var (tick, snaps) = Protocol.UnpackEnemies(packed);

        Assert.Equal(w.Tick, tick);
        Assert.Equal(w.Enemies.Count(e => !e.Dead), snaps.Count);

        var first = w.Enemies.First(e => !e.Dead);
        var snap = snaps.First(s => s.Id == first.Id);
        Assert.Equal(first.DefId, snap.DefId);
        Assert.True(snap.Pos.DistanceTo(first.Pos) < 0.001f);
        Assert.True(System.Math.Abs(snap.HpFraction - first.Hp / first.MaxHp) < 0.01f);
    }

    [Fact]
    public void EveryCommandRoundTripsTheWire()
    {
        var commands = new Command[]
        {
            new Command.Join(1, "chandler", "ember"),
            new Command.Leave(2),
            new Command.PlayerSync(1, new Vec3(1.5f, -2.25f, 3.75f)),
            new Command.PlaceTower(1, "lance", "g2"),
            new Command.SellTower(1, 42),
            new Command.UpgradeTower(1, 42, 2),
            new Command.StartWave(1),
            new Command.PlayerHit(1, 99, "sidearm"),
            new Command.BuyWeapon(1, "rifle"),
            new Command.SelectWeapon(1, "rifle"),
            new Command.UseAbility(1, new Vec3(-4f, 0f, 12f)),
            new Command.Revive(1, 3),
        };

        foreach (var command in commands)
        {
            var round = Protocol.CommandFromWire(Protocol.CommandToWire(command));
            Assert.Equal(command, round);
        }
    }

    [Fact]
    public void MalformedWireReturnsNullNotThrow()
    {
        Assert.Null(Protocol.CommandFromWire("place|notanum"));
        Assert.Null(Protocol.CommandFromWire("gibberish"));
        Assert.Null(Protocol.CommandFromWire(""));
    }

    [Fact]
    public void SeatSpoofingIsDetectable()
    {
        var spoofed = new Command.PlaceTower(2, "lance", "g1");
        Assert.False(Protocol.CommandClaimsSeat(spoofed, seatPlayerId: 1));
        Assert.True(Protocol.CommandClaimsSeat(spoofed, seatPlayerId: 2));
    }

    [Fact]
    public void PipeInNameIsSanitized()
    {
        var evil = new Command.Join(1, "a|b|c", "ember");
        var round = Protocol.CommandFromWire(Protocol.CommandToWire(evil));
        Assert.Equal("abc", ((Command.Join)round!).Name);
    }
}
