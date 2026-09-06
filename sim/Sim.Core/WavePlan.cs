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
        var groups = Waves.ByMap[map.Id][waveIndex];

        float countScale = 1f + Balance.CountScalePerExtraPlayer * (playerCount - 1);
        float hpScale = (1f + Balance.HpScalePerExtraPlayer * (playerCount - 1))
                        * MathF.Pow(Balance.HpGrowth, waveIndex);

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

        entries.Sort((a, b) => a.TickOffset != b.TickOffset
            ? a.TickOffset.CompareTo(b.TickOffset)
            : string.CompareOrdinal(a.DefId, b.DefId));
        return entries;
    }

    private static int RouteIndexOf(MapDef map, string routeId)
    {
        for (int i = 0; i < map.Routes.Count; i++)
            if (map.Routes[i].Id == routeId) return i;
        throw new InvalidOperationException($"map {map.Id} has no route {routeId}");
    }
}
