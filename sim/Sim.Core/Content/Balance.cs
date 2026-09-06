namespace DeepField.Sim.Content;

/// <summary>Every global dial in one file, per the 2D game's doctrine.
/// Numbers here are M0 placeholders — swept before M1 ships, never nudged by feel.</summary>
public static class Balance
{
    public const int TickHz = 30;
    public const float Dt = 1f / TickHz;

    public const int StartingLives = 20;
    public const int StartingMoney = 250;

    /// <summary>Per-wave enemy hp multiplier. Re-sweep after any roster change.</summary>
    public const float HpGrowth = 1.22f;

    /// <summary>Seconds between wave clear and next wave auto-start
    /// (a StartWave command starts it early).</summary>
    public const float IntermissionSeconds = 5f;

    /// <summary>Homing bolt is considered a hit within this distance.</summary>
    public const float ProjectileHitRadius = 0.4f;
}
