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

    // Filament's ramp: multiplier climbs by RampPerSecond while it holds one
    // target and is capped at RampCap. Both are what the ramp/peak paths scale.
    public const float BeamRampPerSecond = 0.6f;
    public const float BeamRampCap = 3.0f;

    /// <summary>Ember passive: burn duration factor for that player's applications.</summary>
    public const float EmberBurnDurationFactor = 1.3f;

    /// <summary>Tempest passive: weapon fire-rate factor.</summary>
    public const float TempestRateFactor = 1.12f;

    /// <summary>Glacier passive: this player hits chilled targets harder. Reads
    /// the movement channel rather than the chill status by name, so tar and
    /// any future slow count too — the same channel-not-status rule conditions
    /// follow.</summary>
    public const float GlacierChilledDamageFactor = 1.25f;

    public const int SellRefundPercent = 70;

    // ---- M2: shields, cc-resist, burrowing --------------------------------
    /// <summary>Seconds without damage before a Warden shield starts regrowing.</summary>
    public const float ShieldRegenDelaySeconds = 3f;
    public const float ShieldRegenPerSecond = 8f;

    /// <summary>Hard CC fills a per-enemy resistance gauge; at full, immune
    /// until it decays. One dial, swept — perma-stun breaks the genre.</summary>
    public const float CcResistFillPerSecond = 0.45f;   // gauge/sec while controlled
    public const float CcResistDecayPerSecond = 0.125f; // ~8s from full to empty

    /// <summary>Mole cycle, distance-based so it's deterministic and readable:
    /// burrowed for the first stretch of each cycle, surfaced for the rest.</summary>
    /// <summary>Melee's whole economic identity: kills at contact range pay
    /// more scrap. Risk the range, fund your builds. Swept dial — this is the
    /// number that decides whether anyone ever puts the gun away.</summary>
    public const float MeleeScrapBonus = 1.25f;

    /// <summary>Repair per melee swing, as a multiple of the weapon's damage.
    /// Sized so one player with a wrench slows a demolition by about half, two
    /// hold it, and nobody holds it while also shooting — that is the trade the
    /// Ram is for. 1.4 was written to that description and did not match it:
    /// the wrench swings 1.4 times a second for 14, so the factor multiplies
    /// out to 27 hp/s against a Ram's 14, and one player repaired faster than a
    /// Ram could break, forever. A stalemate is the one outcome a siege enemy
    /// must not have — killing it has to be the way out.</summary>
    public const float MeleeRepairFactor = 0.35f;

    public const float BurrowCycleMeters = 14f;
    public const float BurrowedMeters = 8f;

    /// <summary>How far past a weapon's stated range the server still accepts a
    /// hit. The client raycasts and the server sanity-checks, so this absorbs
    /// the gap between an interpolated enemy on screen and its authoritative
    /// position — a dial, not a magic number, because tightening it is a
    /// balance decision and not a networking one.</summary>
    public const float HitRangeSlack = 1.15f;
}
