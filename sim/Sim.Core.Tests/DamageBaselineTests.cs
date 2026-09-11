using System;
using System.Linq;
using DeepField.Sim;
using DeepField.Sim.Content;
using Xunit;

/// <summary>The relationships a damage change is most likely to break by
/// accident. Raising the guns 40% and Skywatch 45% was asked for and is
/// deliberate; the things below are the ones that were never meant to move and
/// that nothing else was watching.</summary>
public class DamageBaselineTests
{
    private static float Dps(WeaponDef w) => w.Damage * w.ShotsPerSecond;
    private static float Dps(MeleeDef m) => m.Damage * m.SwingsPerSecond;

    [Fact]
    public void MeleeStillOutDamagesEveryGun()
    {
        // Melee's whole bargain is that contact range pays better — more scrap
        // per kill, and the damage to make standing there worth it. Raising
        // gun damage without raising melee nearly inverted that: the best gun
        // came within a whisker of the best blade, and a melee weapon that is
        // outgunned at two metres by a rifle at eighty has no reason to exist.
        float bestGun = Weapons.All.Values.Max(Dps);
        float bestMelee = Melee.All.Values.Max(Dps);
        Assert.True(bestMelee > bestGun,
            $"the best gun does {bestGun:0.0} dps against the best melee's {bestMelee:0.0} — "
            + "melee has stopped being the reason to close the distance");
    }

    [Fact]
    public void SkywatchLeadsEveryOtherTowerThatCanShootAir()
    {
        // It is the only one of the three a player can build from the ground
        // and have it reach the strand, and it can shoot nothing else. If a
        // dual-layer tower matches it, being air-only is pure cost.
        float skywatch = Towers.Skywatch.Damage * Towers.Skywatch.ShotsPerSecond;
        foreach (var def in Towers.All.Values)
        {
            if (def.Id == "skywatch" || def.Damage <= 0f) continue;
            if (!def.TargetLayers.Contains(EnemyLayer.Air)) continue;
            Assert.True(skywatch > def.Damage * def.ShotsPerSecond,
                $"{def.Id} matches or beats Skywatch on paper dps while also shooting ground");
        }
    }

    [Fact]
    public void SkywatchPaysForBeingSingleLayerWithDamage()
    {
        // Against the generalists it competes with for a socket and costs more
        // than: a tower that answers one layer has to be better at that layer.
        float skywatch = Towers.Skywatch.Damage * Towers.Skywatch.ShotsPerSecond;
        float lance = Towers.Lance.Damage * Towers.Lance.ShotsPerSecond;
        Assert.True(skywatch > lance * 1.5f,
            $"Skywatch {skywatch:0.0} dps is not clearly ahead of a Lance's {lance:0.0} despite "
            + $"costing {Towers.Skywatch.Cost} against {Towers.Lance.Cost} and shooting one layer");
    }

    [Fact]
    public void TheStarterGunStaysWorthFiring()
    {
        // The sidearm is free and every player has it. It should stay
        // meaningfully ahead of the cheapest tower a player could have built
        // instead, or shooting is something you do while waiting for money.
        Assert.True(Dps(Weapons.Sidearm) > Towers.Lance.Damage * Towers.Lance.ShotsPerSecond,
            "the free sidearm is outdamaged by the cheapest tower in the game");
    }

    [Fact]
    public void NoGunOutrangesItsDamageIntoADominantStrategy()
    {
        // The long guns trade damage for reach and the short ones the other
        // way. Scattergun at 14 m must out-damage the rifle at 80 m, or reach
        // is free and there is only one gun worth buying.
        Assert.True(Dps(Weapons.Scattergun) > Dps(Weapons.Rifle),
            $"scattergun {Dps(Weapons.Scattergun):0.0} dps at {Weapons.Scattergun.RangeMeters} m "
            + $"no longer beats rifle {Dps(Weapons.Rifle):0.0} at {Weapons.Rifle.RangeMeters} m");
    }
}
