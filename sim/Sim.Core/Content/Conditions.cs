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
    /// The range factor is the 0.7 design specced. It shipped at 0.9 for a
    /// while on the belief that the maps could not take 0.7 — 28 of
    /// switchyard's 30 sockets reach the ground route at full range and only
    /// 17 do at 0.7 — and that reading was wrong about the cause. The maps were
    /// fine. What could not take 0.7 was the harness's own reference build,
    /// which had put two Foundry towers on the deck's middle row: 4 m behind
    /// the front lip, reaching the lane in clear weather and not reaching it
    /// under fog. The floor died on Foundry's weather wave every time, and the
    /// number took the blame for the build order. Moving those two picks to the
    /// lip let 0.7 ship, and the floor now clears all ten waves rather than
    /// nine.
    ///
    /// The lesson is worth more than the number: a socket graph with a spread
    /// of ranges is doing its job, and weather is what turns "these two sockets
    /// are equivalent" into a decision. The deck stayed exactly as it was.
    ///
    /// Skywatch is exempt for a reason that is design rather than balance: fog
    /// lies low. Flyers are above it and so is the anti-air looking up at them.
    /// The air lane needs that exemption more than most, being covered by only
    /// seven sockets on switchyard — it was raised deliberately so the elevated
    /// tiers would own a stretch, and a lane with no redundancy survives no
    /// range factor at all.</summary>
    public static readonly ConditionDef Fog = new(
        Id: "fog",
        Name: "FOG",
        Effect: "Tower range reduced. Detectors see through it.",
        TowerRangeFactor: 0.7f,
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
