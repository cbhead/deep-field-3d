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

    /// <summary>What a kill pays, and how that keeps up with what it takes to
    /// make the kill.
    ///
    /// A bounty used to be a flat number off the enemy's def — a Drifter paid
    /// six credits on wave 1 and six on wave 20, while its health compounded
    /// every wave in between. Measured on Foundry, a wave paid 0.300 credits
    /// per point of enemy health at the start and 0.011 by wave 20: the fight
    /// got twenty-seven times more expensive per credit earned, which is a
    /// defence that cannot be funded rather than one that is hard to fund.
    ///
    /// <see cref="BountyGrowth"/> compounds on the wave index the way hp does,
    /// so a harder enemy is worth more: +15% a wave, which is +252% by wave 9
    /// and more than fifteenfold by wave 20. It sits under the campaign's
    /// <see cref="HpGrowth"/> on purpose — income is meant to fall behind the
    /// threat a little, or difficulty stops meaning anything — and level with
    /// <see cref="EndlessHpGrowth"/>, so endless settles instead of drifting
    /// further out of reach every lap.
    ///
    /// <see cref="BountyScale"/> is a flat lift on top, and it is 1.0 because
    /// the measurement said so rather than because nobody tried. At 1.15 the
    /// weather gate stops passing: the mid-band bot can afford enough tower
    /// that a night-and-fog campaign costs it no lives at all against a clear
    /// one, and a condition that changes nothing is worse than a thin wave 1.
    /// The dial is here, documented, for when wave 1 genuinely needs it — but
    /// re-run the harness, because that gate is what it costs.</summary>
    public const float BountyScale = 1.0f;
    public const float BountyGrowth = 1.15f;

    /// <summary>What a kill yields in scrap, on the same curve as the credits
    /// it yields.
    ///
    /// Scrap had the identical hole bounty did: a flat number off the enemy's
    /// def, so a Drifter dropped the same alloy on wave 20 as on wave 1 while
    /// everything it buys — attachments, ammo, breakpoint recipes and now Pack
    /// a Punch — stayed priced for a whole campaign. Flat income against
    /// compounding costs is a bench you stop being able to afford, and it is
    /// what would have made an unlimited upgrade track unlimited only on
    /// paper.
    ///
    /// Held level with <see cref="BountyGrowth"/> on purpose: the two
    /// currencies come off the same corpse and there is no reason for one to
    /// outrun the other.</summary>
    public const float ScrapGrowth = 1.15f;

    // ---- Pack a Punch: the upgrade track with no ceiling -------------------

    /// <summary>What the first Pack a Punch costs, in Alloy, and what each one
    /// after it multiplies that by.
    ///
    /// The track has no cap, so the cost curve is the only thing holding it:
    /// power compounds at <see cref="PackDamagePerLevel"/> x
    /// <see cref="PackRatePerLevel"/> = 1.40 a level, and cost compounds at
    /// 1.50, so every level buys a little less than the one before and the
    /// track asymptotes instead of running away. That is the whole design —
    /// unlimited has to mean "you may always buy another", not "you may
    /// eventually buy everything".
    ///
    /// 50, 75, 113, 169, 253, 380, 570... against a player who has roughly 150
    /// Alloy banked by the end of Foundry's campaign and 270 by Switchyard's,
    /// so the first two or three are a campaign-length goal and the rest are
    /// what endless is for.</summary>
    public const int PackFirstCost = 50;
    public const float PackCostGrowth = 1.5f;

    /// <summary>Per level. Deliberately lopsided toward damage: rate alone
    /// makes a weapon louder, damage is what a player feels against an enemy
    /// whose health compounds every wave.</summary>
    public const float PackDamagePerLevel = 1.25f;
    public const float PackRatePerLevel = 1.12f;

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

    /// <summary>Endless: extra bodies per lap of the authored wave tables, on
    /// top of the hp curve that never stops. Count-first, like player scaling,
    /// so late waves are crowds rather than sponges.</summary>
    public const float EndlessCountGrowthPerLap = 1.15f;

    /// <summary>Endless: the per-wave hp multiplier *past the authored arc*.
    ///
    /// The campaign curve is <see cref="HpGrowth"/> at 1.22, swept and gated
    /// over ten to twelve waves, and it stays that. Endless used to keep
    /// compounding the same 1.22 forever against a player whose ceiling is
    /// fixed: a tower taken to level 10 on both its damage and rate paths is
    /// 1.10^9 × 1.10^9, which is 5.6× the dps it started with, and there are
    /// only so many sockets. At wave 15 the old curve was 19.7× hp with 1.15×
    /// the bodies — 23× the threat against 5.6× the answer — and by wave 20 it
    /// was 71×. Nobody was reaching the high rounds because the curve had
    /// already decided they would not.
    ///
    /// 1.15 past the last authored wave, matching the count knob above, so the
    /// two endless dials read the same. That is still exponential and endless
    /// still ends; it ends later and for a reason the player can see coming.
    /// Wave 15 goes 19.7× → 13.8×, wave 20 53.4× → 27.9×.</summary>
    public const float EndlessHpGrowth = 1.15f;

    /// <summary>Scrap on the floor: how long the personal half of a drop waits
    /// to be collected before it banks to the team pool, how close you have to
    /// be for it to come to you, how fast it comes, and how close it has to get
    /// before you have it. Generous on purpose — the decision worth pricing is
    /// "do I leave the perch", not "can I stand on a pixel".</summary>
    public const float ScrapPickupSeconds = 45f;
    public const float ScrapMagnetMeters = 4.5f;
    public const float ScrapMagnetSpeed = 9f;
    public const float ScrapCollectMeters = 1.6f;

    /// <summary>How fast a drop from the air lane falls to the floor. Fast
    /// enough not to be a wait, slow enough to read as a thing falling.</summary>
    public const float ScrapFallSpeed = 11f;
}
