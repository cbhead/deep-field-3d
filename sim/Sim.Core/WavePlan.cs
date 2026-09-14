using DeepField.Sim.Content;
using DeepField.Sim.Util;

namespace DeepField.Sim;

/// <summary>Wave content is a pure function of (seed, map, waveIndex, playerCount)
/// — no sequential draws, so wave N is byte-identical regardless of what happened
/// in waves 1..N-1. Authored composition tables give the shape; the seeded stream
/// jitters spacing and scatter; player scaling multiplies counts (count-first,
/// hp-second — more targets, not spongier ones).</summary>
public static class WavePlan
{
    public static List<SpawnEntry> PlanWave(uint seed, MapDef map, int waveIndex, int playerCount)
    {
        var rng = RngStreams.StreamFor(seed, RngStreams.Wave, (uint)waveIndex);
        // Past the authored arc (endless) the tables cycle. The hp curve keeps
        // compounding on the raw wave index, and each lap adds bodies on top,
        // so a wave 27 is an authored wave 7 with the numbers of a 27.
        var tables = Waves.ByMap[map.Id];
        int lap = waveIndex / tables.Count;
        var groups = tables[waveIndex % tables.Count];

        float countScale = (1f + Balance.CountScalePerExtraPlayer * (playerCount - 1))
                           * DetMath.PowInt(Balance.EndlessCountGrowthPerLap, lap);
        float hpScale = HpScale(map, waveIndex, playerCount);

        var entries = new List<SpawnEntry>();
        foreach (var group in groups)
        {
            var def = Enemies.All[group.EnemyId];
            int routeIndex = RouteIndexOf(map, group.RouteId);
            int count = System.Math.Max(1, (int)MathF.Round(group.Count * countScale));

            int tick = group.StartDelayTicks;
            for (int i = 0; i < count; i++)
            {
                float lateral = def.ScatterWidth > 0f
                    ? (rng.NextFloat() - 0.5f) * def.ScatterWidth
                    : 0f;
                entries.Add(new SpawnEntry(def.Id, tick, hpScale, routeIndex, lateral));
                // Jitter keeps packs from metronoming; ±20% of base spacing.
                int jitter = group.SpacingTicks > 0 ? rng.NextInt(0, group.SpacingTicks / 5 + 1) : 0;
                tick += group.SpacingTicks + jitter;
            }
        }

        AppendConditionSpawns(seed, map, waveIndex, hpScale, entries);

        entries.Sort((a, b) => a.TickOffset != b.TickOffset
            ? a.TickOffset.CompareTo(b.TickOffset)
            : string.CompareOrdinal(a.DefId, b.DefId));
        return entries;
    }

    /// <summary>What a kill on this wave pays, as a multiple of the enemy's
    /// def bounty. Compounded on the wave index for the same reason hp is: an
    /// enemy that takes four times as long to kill should not pay the same as
    /// the one on wave one.</summary>
    public static float BountyScale(int waveIndex) =>
        Balance.BountyScale * DetMath.PowInt(Balance.BountyGrowth, waveIndex);

    /// <summary>What a kill on this wave yields in scrap, as a multiple of the
    /// enemy's def yield. Same shape as <see cref="BountyScale"/>, and for the
    /// same reason.</summary>
    public static float ScrapScale(int waveIndex) =>
        DetMath.PowInt(Balance.ScrapGrowth, waveIndex);

    /// <summary>The hp multiplier a wave spawns with, times the player-count
    /// factor. Exposed so the HUD's endless threat readout is the sim's
    /// number, not a copy.
    ///
    /// Two curves meeting at the end of the authored arc. Through the campaign
    /// it is <see cref="Balance.HpGrowth"/> compounded on the wave index,
    /// which is what the sweep balanced. Past the last authored wave it
    /// carries on from that value at the gentler
    /// <see cref="Balance.EndlessHpGrowth"/>, because the campaign curve was
    /// tuned against ten waves of it and endless asks for forty.</summary>
    public static float HpScale(MapDef map, int waveIndex, int playerCount)
    {
        float player = 1f + Balance.HpScalePerExtraPlayer * (playerCount - 1);
        int lastAuthored = Waves.ByMap[map.Id].Count - 1;
        if (waveIndex <= lastAuthored)
            return player * DetMath.PowInt(Balance.HpGrowth, waveIndex);

        // Continuous at the join: the last authored wave keeps its campaign
        // value and endless grows from there.
        return player * DetMath.PowInt(Balance.HpGrowth, lastAuthored)
                      * DetMath.PowInt(Balance.EndlessHpGrowth, waveIndex - lastAuthored);
    }

    /// <summary>Weather's contribution to a wave, appended after the authored
    /// groups are fully planned and drawn from the condition stream. Order
    /// matters: the authored loop above must have finished with the wave stream
    /// before anything here runs, or a condition would reroll scatter and
    /// spacing for enemies it never touched.</summary>
    private static void AppendConditionSpawns(
        uint seed, MapDef map, int waveIndex, float hpScale, List<SpawnEntry> entries)
    {
        var condition = Conditions.ForWave(map, waveIndex);
        if (condition is null || condition.StealthWeightFactor <= 1f) return;

        // Only the stealth already authored into this wave is amplified. Night
        // makes a stealth wave darker; it does not conjure Shades into a wave
        // that never asked for them.
        var stealthGroups = Waves.ByMap[map.Id][waveIndex]
            .Where(g => Enemies.All[g.EnemyId].Stealth)
            .ToList();
        if (stealthGroups.Count == 0) return;

        var rng = RngStreams.StreamFor(seed, RngStreams.Condition, (uint)waveIndex);
        foreach (var group in stealthGroups)
        {
            var def = Enemies.All[group.EnemyId];
            int extra = (int)MathF.Round(group.Count * (condition.StealthWeightFactor - 1f));
            int routeIndex = RouteIndexOf(map, group.RouteId);

            // Slotted between the authored ones rather than trailing behind, so
            // the wave reads as denser and not as a second wave stapled on.
            int tick = group.StartDelayTicks + group.SpacingTicks / 2;
            for (int i = 0; i < extra; i++)
            {
                float lateral = def.ScatterWidth > 0f
                    ? (rng.NextFloat() - 0.5f) * def.ScatterWidth
                    : 0f;
                entries.Add(new SpawnEntry(def.Id, tick, hpScale, routeIndex, lateral));
                tick += group.SpacingTicks + rng.NextInt(0, group.SpacingTicks / 5 + 1);
            }
        }
    }

    private static int RouteIndexOf(MapDef map, string routeId)
    {
        for (int i = 0; i < map.Routes.Count; i++)
            if (map.Routes[i].Id == routeId) return i;
        throw new InvalidOperationException($"map {map.Id} has no route {routeId}");
    }
}
