namespace DeepField.Sim.Content;

/// <summary>Map conditions are factors over the swept baseline, never absolute
/// overrides — so a condition can never make a wave unwinnable on its own, and
/// rebalancing the baseline carries into every condition for free.
///
/// Two rules the framework enforces rather than trusts:
///
/// Conditions touch <em>channels</em>, not individual statuses. A condition
/// that named "chill" would silently ignore every movement status added after
/// it; naming the channel means new statuses inherit weather automatically.
///
/// Conditions never perturb the RNG streams. Anything a condition adds to a
/// wave is drawn from its own stream and appended after the authored groups
/// are planned, so a wave's authored content is byte-identical whether or not
/// weather is active. Without that rule, turning on Fog would silently reroll
/// every enemy's lateral scatter and the sweep would be comparing two
/// different games.</summary>
public sealed record ConditionDef(
    string Id,
    string Name,
    string Effect,                        // the one-line summary the HUD announces

    // Targeting.
    float TowerRangeFactor,               // fog: towers see less far
    IReadOnlyCollection<string> RangeExemptTowerIds,
    float AcquisitionDelaySeconds,        // night: a beat before a tower locks on
    bool AcquisitionDelayExemptsMarked,   // ...unless someone marked the target

    // Hero.
    float HeroRangeFactor,                // fog: falloff starts closer

    // Composition. Applied as appended spawns, never as a rescale of authored
    // counts — see the RNG rule above.
    float StealthWeightFactor,

    // Channel-level duration factors, keyed by channel. Empty for Night and
    // Fog; Heatwave and Coldsnap are the reason the hook exists now rather
    // than being retrofitted around them later.
    IReadOnlyDictionary<Channel, float> ChannelDurationFactors);

public static class Conditions
{
    private static readonly IReadOnlyDictionary<Channel, float> NoChannelFactors =
        new Dictionary<Channel, float>();

    /// <summary>Fog sells the Detector twice: it is the only tower that keeps
    /// its full range, and it is also the answer to the Shades that Night
    /// brings.
    ///
    /// The range factor is 0.9, not the 0.7 the design called for, and the
    /// reason is measured rather than felt. On switchyard, 28 of 30 sockets
    /// reach the ground route at full range; at x0.7 only 17 do. Fog at the
    /// specced number does not reduce eleven towers, it switches them off —
    /// which is an absolute override wearing a factor's clothing, and the one
    /// thing this framework is supposed to make impossible. The cliff showed up
    /// in the sweep as a cliff: 0.85 loses the campaign, 0.9 clears it, with
    /// nothing in between.
    ///
    /// 0.9 is therefore a symptom fix. The real one is map geometry — sockets
    /// placed with enough margin that losing a third of a radius costs coverage
    /// instead of erasing it. Until that lands, the gate below holds the line.
    ///
    /// Skywatch is exempt for a reason that is design rather than balance: fog
    /// lies low. Flyers are above it and so is the anti-air looking up at them.
    /// This also removes the sharpest edge the gate found — the air lane is
    /// covered by only seven sockets on switchyard, because it was deliberately
    /// raised so the elevated tiers would own a stretch of it, and a lane with
    /// no redundancy cannot survive any range factor at all.</summary>
    public static readonly ConditionDef Fog = new(
        Id: "fog",
        Name: "FOG",
        Effect: "Tower range reduced. Detectors see through it.",
        TowerRangeFactor: 0.9f,
        RangeExemptTowerIds: new[] { "detector", "skywatch" },
        AcquisitionDelaySeconds: 0f,
        AcquisitionDelayExemptsMarked: false,
        HeroRangeFactor: 0.7f,
        StealthWeightFactor: 1f,
        ChannelDurationFactors: NoChannelFactors);

    /// <summary>Night asks for information twice over: more Shades, and a beat
    /// of hesitation before any tower locks on to something nobody has marked.
    /// Mark is the cheap answer, reveal is the thorough one.</summary>
    public static readonly ConditionDef Night = new(
        Id: "night",
        Name: "NIGHT",
        Effect: "More stealth. Towers acquire unmarked targets slowly.",
        TowerRangeFactor: 1f,
        RangeExemptTowerIds: System.Array.Empty<string>(),
        AcquisitionDelaySeconds: 0.2f,
        AcquisitionDelayExemptsMarked: true,
        HeroRangeFactor: 1f,
        StealthWeightFactor: 1.5f,
        ChannelDurationFactors: NoChannelFactors);

    public static readonly IReadOnlyDictionary<string, ConditionDef> All =
        new Dictionary<string, ConditionDef>
        {
            [Fog.Id] = Fog,
            [Night.Id] = Night,
        };

    /// <summary>Which condition rides a given wave, or null for clear weather.
    /// Authored on the map so it is enumerable by the sweep — the harness runs
    /// a per-condition column, which only works if the schedule is data.</summary>
    public static ConditionDef? ForWave(MapDef map, int waveIndex) =>
        map.ConditionSchedule.TryGetValue(waveIndex, out var id) ? All[id] : null;
}
