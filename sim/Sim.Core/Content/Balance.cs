namespace DeepField.Sim.Content;

/// <summary>Every global dial in one file, per the 2D game's doctrine.
/// Numbers here are placeholders until the M1 pre-ship sweep — never nudged by feel.</summary>
public static class Balance
{
    public const int TickHz = 30;
    public const float Dt = 1f / TickHz;

    public const int StartingLives = 20;
    public const int StartingMoney = 250;

    /// <summary>Per-wave enemy hp multiplier. Re-sweep after any roster change.</summary>
    public const float HpGrowth = 1.22f;

    public const float IntermissionSeconds = 8f;
    public const float ProjectileHitRadius = 0.4f;

    // ---- Co-op scaling: count-first, hp-second (standard @ 1p is the identity) --
    public const float CountScalePerExtraPlayer = 0.6f;
    public const float HpScalePerExtraPlayer = 0.15f;

    // ---- Heroes ---------------------------------------------------------------
    public const float PlayerMaxHp = 100f;
    public const float PlayerRegenDelaySeconds = 6f;
    public const float PlayerRegenPerSecond = 10f;
    public const float ReviveSeconds = 4f;
    public const float BleedoutSeconds = 30f;
    public const float SoloRespawnSeconds = 10f;
    public const float ReviveRangeMeters = 2.5f;
    public const float ContactRadiusMeters = 1.2f;   // enemy body vs hero overlap

    // ---- Economy --------------------------------------------------------------
    /// <summary>Fraction of every scrap drop that goes to the team pool (tower
    /// upgrades); the rest goes to damage participants (gunsmith).</summary>
    public const float ScrapTeamShare = 0.5f;

    /// <summary>Forge passive: tower placement cost factor.</summary>
    public const float ForgeBuildDiscount = 0.9f;

    /// <summary>Ember passive: burn duration factor for that player's applications.</summary>
    public const float EmberBurnDurationFactor = 1.3f;

    public const int SellRefundPercent = 70;
}
