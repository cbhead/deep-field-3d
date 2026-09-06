using DeepField.Sim.Content;
using DeepField.Sim.Util;

namespace DeepField.Sim;

/// <summary>Wave content is a pure function of (seed, waveIndex) — no sequential
/// draws, so wave N is byte-identical regardless of what happened in waves 1..N-1.
/// This is what makes replays, drop-in join, and sweeps trivially consistent.</summary>
public static class WavePlan
{
    public static List<SpawnEntry> PlanWave(uint seed, int waveIndex)
    {
        var rng = RngStreams.StreamFor(seed, RngStreams.Wave, (uint)waveIndex);

        // M0: growing packs of Drifters with mild jitter in spacing.
        int count = 5 + waveIndex * 3;
        float hpFactor = MathF.Pow(Balance.HpGrowth, waveIndex);

        var entries = new List<SpawnEntry>(count);
        int tickOffset = 0;
        for (int i = 0; i < count; i++)
        {
            entries.Add(new SpawnEntry(Enemies.Drifter.Id, tickOffset, hpFactor));
            // 0.6–1.0s spacing at 30 Hz.
            tickOffset += 18 + rng.NextInt(0, 13);
        }

        return entries;
    }
}
