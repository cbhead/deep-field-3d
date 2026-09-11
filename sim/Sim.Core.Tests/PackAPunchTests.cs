using System;
using System.Linq;
using DeepField.Sim;
using DeepField.Sim.Content;
using Xunit;

/// <summary>Pack a Punch: an upgrade track on a weapon with no ceiling.
///
/// The thing that makes "unlimited" a design rather than a hole is that cost
/// compounds faster than power — 1.50 a level against 1.40 — so every level
/// buys a little less than the one before. Unlimited has to mean "you may
/// always buy another", not "you may eventually buy everything".</summary>
public class PackAPunchTests
{
    private static (World World, PlayerState Player) Solo()
    {
        var w = new World(9, Maps.TestLane);
        w.Enqueue(new Command.Join(1, "solo", "ember"));
        Step.Advance(w);
        return (w, w.Players[1]);
    }

    [Fact]
    public void TheFirstOneCostsFiftyAlloy()
    {
        var (w, player) = Solo();
        Assert.Equal(Balance.PackFirstCost, player.BuildFor("sidearm").NextPackCost);
        Assert.Equal(50, Balance.PackFirstCost);
    }

    [Fact]
    public void BuyingOneSpendsTheAlloyAndRaisesTheLevel()
    {
        var (w, player) = Solo();
        player.Scrap[ScrapType.Alloy] = 500;
        w.Enqueue(new Command.PackAPunch(1, "sidearm"));
        Step.Advance(w);

        var build = player.BuildFor("sidearm");
        Assert.Equal(1, build.PackLevel);
        Assert.Equal(450, player.Scrap[ScrapType.Alloy]);
        Assert.Contains(w.Events, e => e is SimEvent.PackedAPunch { Level: 1, Cost: 50 });
    }

    [Fact]
    public void ItRaisesBothDamageAndFireRate()
    {
        var (w, player) = Solo();
        var build = player.BuildFor("rifle");
        float damage0 = build.DamageFactor(false), rate0 = build.RateFactor();

        build.PackLevel = 1;
        Assert.True(build.DamageFactor(false) > damage0 * 1.2f, "damage barely moved");
        Assert.True(build.RateFactor() > rate0 * 1.1f, "fire rate barely moved");

        // And they compound, which is the point of a track rather than a perk.
        build.PackLevel = 5;
        Assert.True(build.DamageFactor(false) > damage0 * 3f);
        Assert.True(build.RateFactor() > rate0 * 1.7f);
    }

    [Fact]
    public void ThereIsNoCeiling()
    {
        var (w, player) = Solo();
        player.Scrap[ScrapType.Alloy] = 1_000_000_000;
        for (int i = 0; i < 12; i++)
        {
            w.Enqueue(new Command.PackAPunch(1, "sidearm"));
            Step.Advance(w);
        }
        Assert.Equal(12, player.BuildFor("sidearm").PackLevel);
        Assert.DoesNotContain(w.Events, e => e is SimEvent.CraftRejected);
    }

    [Fact]
    public void CostOutrunsPowerSoEveryLevelBuysLessThanTheLast()
    {
        // The whole safety argument for an uncapped track, as a number.
        float power = Balance.PackDamagePerLevel * Balance.PackRatePerLevel;
        Assert.True(Balance.PackCostGrowth > power,
            $"cost grows {Balance.PackCostGrowth:0.00} a level against power's {power:0.00} — "
            + "an uncapped track that gets cheaper per point of dps is a broken one");

        // Concretely: the tenth level costs far more than the first and adds a
        // smaller share of the total.
        Assert.True(WeaponBuild.PackCostAt(10) > WeaponBuild.PackCostAt(1) * 30);
    }

    [Fact]
    public void ItIsRefusedWithoutTheAlloyAndWithoutTheWeapon()
    {
        var (w, player) = Solo();
        player.Scrap[ScrapType.Alloy] = 49;
        w.Enqueue(new Command.PackAPunch(1, "sidearm"));
        Step.Advance(w);
        Assert.Equal(0, player.BuildFor("sidearm").PackLevel);
        Assert.Equal(49, player.Scrap[ScrapType.Alloy]);
        Assert.Contains(w.Events, e => e is SimEvent.CraftRejected { Reason: "insufficientScrap" });

        player.Scrap[ScrapType.Alloy] = 10000;
        w.Enqueue(new Command.PackAPunch(1, "rifle"));      // not owned
        Step.Advance(w);
        Assert.Equal(0, player.BuildFor("rifle").PackLevel);
        Assert.Contains(w.Events, e => e is SimEvent.CraftRejected { Reason: "weaponNotOwned" });
    }

    [Fact]
    public void ItIsPerWeaponNotPerPlayer()
    {
        var (w, player) = Solo();
        player.Scrap[ScrapType.Alloy] = 500;
        w.Enqueue(new Command.PackAPunch(1, "sidearm"));
        Step.Advance(w);
        Assert.Equal(1, player.BuildFor("sidearm").PackLevel);
        Assert.Equal(0, player.BuildFor("rifle").PackLevel);
    }

    [Fact]
    public void ItSurvivesSerialization()
    {
        var (w, player) = Solo();
        player.Scrap[ScrapType.Alloy] = 500;
        w.Enqueue(new Command.PackAPunch(1, "sidearm"));
        Step.Advance(w);
        w.Enqueue(new Command.PackAPunch(1, "sidearm"));
        Step.Advance(w);

        var back = Serialization.Deserialize(Serialization.Serialize(w));
        Assert.Equal(2, back.Players[1].BuildFor("sidearm").PackLevel);
    }

    [Fact]
    public void ItRoundTripsOnTheWire()
    {
        var command = new Command.PackAPunch(3, "scattergun");
        var back = Protocol.CommandFromWire(Protocol.CommandToWire(command));
        Assert.Equal(command.ToString(), back?.ToString());
        Assert.True(Protocol.CommandClaimsSeat(command, 3));
        Assert.False(Protocol.CommandClaimsSeat(command, 4));
    }
}
