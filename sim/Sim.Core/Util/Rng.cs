namespace DeepField.Sim.Util;

/// <summary>Deterministic seeded RNG (mulberry32 port from the 2D game).
/// Never use System.Random in the sim — all randomness flows through named
/// streams so results are a pure function of (seed, stream, index).</summary>
public sealed class Rng
{
    private uint _state;

    public Rng(uint seed) => _state = seed;

    public uint NextUInt()
    {
        _state = unchecked(_state + 0x6D2B79F5u);
        uint t = _state;
        t = unchecked((t ^ (t >> 15)) * (t | 1u));
        t ^= unchecked(t + unchecked((t ^ (t >> 7)) * (t | 61u)));
        return t ^ (t >> 14);
    }

    /// <summary>Uniform in [0, 1).</summary>
    public double NextDouble() => NextUInt() / 4294967296.0;

    public float NextFloat() => (float)NextDouble();

    /// <summary>Uniform integer in [minInclusive, maxExclusive).</summary>
    public int NextInt(int minInclusive, int maxExclusive) =>
        minInclusive + (int)(NextUInt() % (uint)(maxExclusive - minInclusive));
}

/// <summary>Streams are isolated by purpose (FNV-1a mixing of seed + name + index),
/// so wave N's content is byte-identical regardless of what earlier systems drew.
/// This is the invariant replays, drop-in join, and balance sweeps rely on.</summary>
public static class RngStreams
{
    public const string Wave = "wave";
    public const string Spawn = "spawn";
    public const string Combat = "combat";
    public const string Bot = "bot";

    public static Rng StreamFor(uint seed, string stream, uint index = 0)
    {
        uint h = 2166136261u;

        void MixUInt(uint v)
        {
            for (int i = 0; i < 4; i++)
            {
                h ^= (v >> (i * 8)) & 0xFFu;
                h = unchecked(h * 16777619u);
            }
        }

        MixUInt(seed);
        foreach (char c in stream)
        {
            h ^= c;
            h = unchecked(h * 16777619u);
        }
        MixUInt(index);

        return new Rng(h);
    }
}
