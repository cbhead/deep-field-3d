namespace DeepField.Sim.Content;

/// <summary>Authored wave composition. The wave plan stays a pure function of
/// (seed, map, waveIndex, playerCount): these tables give the shape, the seeded
/// stream jitters spacing, scaling multiplies counts. One new thing per wave —
/// the "one to learn on" convention.</summary>
public sealed record WaveGroup(
    string EnemyId,
    int Count,
    int SpacingTicks,     // base gap between spawns in the group
    int StartDelayTicks,  // offset from wave start
    string RouteId);

public static class Waves
{
    public static readonly IReadOnlyDictionary<string, IReadOnlyList<IReadOnlyList<WaveGroup>>> ByMap =
        new Dictionary<string, IReadOnlyList<IReadOnlyList<WaveGroup>>>
        {
            ["testlane"] = new IReadOnlyList<WaveGroup>[]
            {
                new[] { new WaveGroup("drifter", 5, 24, 0, "ground") },
                new[] { new WaveGroup("drifter", 8, 22, 0, "ground") },
                new[] { new WaveGroup("drifter", 11, 20, 0, "ground") },
            },

            ["foundry"] = new IReadOnlyList<WaveGroup>[]
            {
                // W1: the reference grunt.
                new[] { new WaveGroup("drifter", 6, 26, 0, "ground") },
                // W2: pace picks up.
                new[] { new WaveGroup("drifter", 9, 22, 0, "ground") },
                // W3: Motes — the scatter swarm teaches splash placement.
                new[]
                {
                    new WaveGroup("drifter", 4, 26, 0, "ground"),
                    new WaveGroup("mote", 12, 8, 120, "ground"),
                },
                // W4: Skiffs debut alone on the air lane — the coverage question.
                new[]
                {
                    new WaveGroup("skiff", 4, 40, 0, "air"),
                    new WaveGroup("drifter", 6, 24, 60, "ground"),
                },
                // W5: Aegis debuts — somebody has to move.
                new[]
                {
                    new WaveGroup("aegis", 2, 90, 0, "ground"),
                    new WaveGroup("drifter", 8, 20, 40, "ground"),
                },
                // W6: Monolith debuts — the wall, with cover-seekers behind it.
                new[]
                {
                    new WaveGroup("monolith", 1, 0, 0, "ground"),
                    new WaveGroup("mote", 10, 10, 30, "ground"),
                },
                // W7: split attention — both layers at once.
                new[]
                {
                    new WaveGroup("skiff", 5, 34, 0, "air"),
                    new WaveGroup("drifter", 8, 20, 0, "ground"),
                    new WaveGroup("aegis", 1, 0, 200, "ground"),
                },
                // W8: the Aegis column — flanking becomes mandatory.
                new[]
                {
                    new WaveGroup("aegis", 4, 70, 0, "ground"),
                    new WaveGroup("mote", 8, 10, 150, "ground"),
                },
                // W9: everything so far, in force.
                new[]
                {
                    new WaveGroup("monolith", 1, 0, 0, "ground"),
                    new WaveGroup("drifter", 10, 18, 40, "ground"),
                    new WaveGroup("skiff", 6, 30, 80, "air"),
                },
                // W10: paired walls plus escorts — the slice finale.
                new[]
                {
                    new WaveGroup("monolith", 2, 160, 0, "ground"),
                    new WaveGroup("aegis", 2, 90, 100, "ground"),
                    new WaveGroup("mote", 14, 8, 200, "ground"),
                    new WaveGroup("skiff", 4, 36, 260, "air"),
                },
            },

            // Switchyard: 12 waves, the M2 roster's teaching arc. Shortcut-route
            // groups reroute to the long way when a barricade holds b1.
            ["switchyard"] = new IReadOnlyList<WaveGroup>[]
            {
                new[] { new WaveGroup("drifter", 6, 24, 0, "groundShort") },
                new[]
                {
                    new WaveGroup("drifter", 6, 22, 0, "ground"),
                    new WaveGroup("mote", 6, 9, 100, "groundShort"),
                },
                // Warden debuts — coverage gaps get punished.
                new[]
                {
                    new WaveGroup("warden", 2, 80, 0, "ground"),
                    new WaveGroup("drifter", 6, 20, 40, "groundShort"),
                },
                // Mole debuts — hit the windows.
                new[]
                {
                    new WaveGroup("mole", 3, 50, 0, "groundShort"),
                    new WaveGroup("drifter", 5, 22, 60, "ground"),
                },
                // Cluster debuts — pre-place the splash.
                new[]
                {
                    new WaveGroup("cluster", 3, 60, 0, "ground"),
                    new WaveGroup("skiff", 3, 36, 40, "air"),
                },
                new[]
                {
                    new WaveGroup("warden", 2, 70, 0, "groundShort"),
                    new WaveGroup("mole", 2, 55, 100, "ground"),
                    new WaveGroup("mote", 6, 9, 160, "groundShort"),
                },
                // W7 rides FOG. The wave keeps its authored weight: the whole
                // point of a factor system is that weather adjusts the baseline
                // rather than the baseline being rewritten around the weather.
                new[]
                {
                    new WaveGroup("monolith", 1, 0, 0, "ground"),
                    new WaveGroup("cluster", 2, 65, 40, "groundShort"),
                    new WaveGroup("skiff", 4, 32, 80, "air"),
                },
                new[]
                {
                    new WaveGroup("aegis", 2, 80, 0, "groundShort"),
                    new WaveGroup("warden", 2, 80, 120, "ground"),
                    new WaveGroup("mote", 6, 9, 200, "groundShort"),
                },
                // Shade debuts here rather than in a wave of its own: the arc
                // stays twelve waves, so the swept difficulty curve holds. It
                // replaces two of the skiffs, which keeps the wave's weight
                // roughly where it was while changing what the wave asks.
                new[]
                {
                    new WaveGroup("shade", 2, 70, 0, "ground"),
                    new WaveGroup("mole", 3, 45, 40, "ground"),
                    new WaveGroup("cluster", 2, 55, 60, "groundShort"),
                    new WaveGroup("skiff", 3, 28, 100, "air"),
                },
                // Mender debuts behind the warden pair — shoot the healer, not
                // the shield. One aegis steps aside to pay for it.
                new[]
                {
                    new WaveGroup("monolith", 1, 0, 0, "groundShort"),
                    new WaveGroup("aegis", 1, 90, 80, "ground"),
                    new WaveGroup("warden", 2, 80, 140, "groundShort"),
                    new WaveGroup("mender", 1, 0, 200, "groundShort"),
                },
                // Ram debuts here and not one wave earlier, where it was first
                // written. That wave is the Mender's debut, and a Ram beside a
                // Mender is not a hard wave, it is an unanswerable one: the
                // measurement was 437 damage into a 220 hp body that healed
                // through all of it. Two debuts in one wave also breaks the
                // one-to-learn-on rule the rest of the arc keeps. Here it
                // arrives alone, into the map's filler wave, where the thing
                // to notice is that it stops at your buildings instead of
                // walking past them. Four drifters step aside to pay for it.
                new[]
                {
                    new WaveGroup("ram", 1, 0, 0, "groundShort"),
                    new WaveGroup("drifter", 6, 16, 60, "groundShort"),
                    new WaveGroup("mole", 2, 50, 100, "ground"),
                    new WaveGroup("cluster", 2, 60, 200, "groundShort"),
                    new WaveGroup("skiff", 4, 30, 240, "air"),
                },
                // Finale: everything, both routes, both layers.
                new[]
                {
                    new WaveGroup("monolith", 2, 180, 0, "ground"),
                    new WaveGroup("aegis", 2, 90, 100, "groundShort"),
                    new WaveGroup("warden", 2, 80, 200, "ground"),
                    new WaveGroup("cluster", 3, 55, 280, "groundShort"),
                    new WaveGroup("shade", 2, 60, 240, "ground"),
                    new WaveGroup("mender", 1, 0, 300, "groundShort"),
                    new WaveGroup("skiff", 3, 30, 320, "air"),
                    new WaveGroup("mote", 6, 9, 380, "groundShort"),
                },
            },

            // Spire: twelve waves that climb. The teaching order is different
            // from the earlier maps because the map is — the first lesson is
            // that there are two ways up and you cannot hold both.
            ["spire"] = new IReadOnlyList<WaveGroup>[]
            {
                // W1: the stair only. Learn the interior.
                new[] { new WaveGroup("drifter", 6, 24, 0, "stair") },
                // W2: the fire escape opens. Both ways up, at once, forever.
                new[]
                {
                    new WaveGroup("drifter", 5, 24, 0, "stair"),
                    new WaveGroup("drifter", 5, 24, 40, "escape"),
                },
                // W3: motes up the open outside, where splash pays.
                new[]
                {
                    new WaveGroup("mote", 10, 8, 0, "escape"),
                    new WaveGroup("drifter", 4, 26, 80, "stair"),
                },
                // W4: skiffs skip the whole building. The roof needs answering.
                new[]
                {
                    new WaveGroup("skiff", 4, 36, 0, "air"),
                    new WaveGroup("drifter", 5, 24, 60, "stair"),
                },
                new[]
                {
                    new WaveGroup("aegis", 2, 90, 0, "stair"),
                    new WaveGroup("mote", 6, 9, 60, "escape"),
                },
                // W6: a Monolith in a stairwell is a cork; the escape is the
                // only way past it, which is the point.
                new[]
                {
                    new WaveGroup("monolith", 1, 0, 0, "stair"),
                    new WaveGroup("drifter", 6, 20, 40, "escape"),
                    new WaveGroup("skiff", 3, 34, 90, "air"),
                },
                new[]
                {
                    new WaveGroup("warden", 2, 70, 0, "escape"),
                    new WaveGroup("mole", 2, 55, 80, "stair"),
                },
                // W8 rides NIGHT: shades in a dark stairwell.
                new[]
                {
                    new WaveGroup("shade", 2, 70, 0, "stair"),
                    new WaveGroup("drifter", 5, 22, 80, "escape"),
                },
                new[]
                {
                    new WaveGroup("cluster", 3, 60, 0, "escape"),
                    new WaveGroup("skiff", 4, 30, 40, "air"),
                    new WaveGroup("mole", 2, 55, 100, "stair"),
                },
                new[]
                {
                    new WaveGroup("mender", 1, 0, 0, "stair"),
                    new WaveGroup("warden", 2, 80, 40, "stair"),
                    new WaveGroup("aegis", 2, 90, 120, "escape"),
                },
                new[]
                {
                    new WaveGroup("monolith", 1, 0, 0, "escape"),
                    new WaveGroup("shade", 2, 60, 60, "stair"),
                    new WaveGroup("cluster", 2, 60, 140, "escape"),
                    new WaveGroup("skiff", 3, 30, 180, "air"),
                },
                // W12 rides FOG, on a roof, forty metres up. Everything comes.
                new[]
                {
                    new WaveGroup("monolith", 1, 0, 0, "stair"),
                    new WaveGroup("aegis", 2, 90, 60, "escape"),
                    new WaveGroup("mender", 1, 0, 140, "stair"),
                    new WaveGroup("warden", 2, 80, 180, "escape"),
                    new WaveGroup("shade", 2, 70, 220, "stair"),
                    new WaveGroup("skiff", 4, 28, 260, "air"),
                    new WaveGroup("mote", 6, 9, 320, "escape"),
                },
            },
        };
}
