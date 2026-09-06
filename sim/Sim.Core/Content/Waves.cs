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
                new[] { new WaveGroup("drifter", 7, 24, 0, "groundShort") },
                new[]
                {
                    new WaveGroup("drifter", 8, 22, 0, "ground"),
                    new WaveGroup("mote", 8, 9, 100, "groundShort"),
                },
                // Warden debuts — coverage gaps get punished.
                new[]
                {
                    new WaveGroup("warden", 2, 80, 0, "ground"),
                    new WaveGroup("drifter", 8, 20, 40, "groundShort"),
                },
                // Mole debuts — hit the windows.
                new[]
                {
                    new WaveGroup("mole", 4, 50, 0, "groundShort"),
                    new WaveGroup("drifter", 6, 22, 60, "ground"),
                },
                // Cluster debuts — pre-place the splash.
                new[]
                {
                    new WaveGroup("cluster", 4, 60, 0, "ground"),
                    new WaveGroup("skiff", 4, 36, 40, "air"),
                },
                new[]
                {
                    new WaveGroup("warden", 3, 70, 0, "groundShort"),
                    new WaveGroup("mole", 3, 55, 100, "ground"),
                    new WaveGroup("mote", 8, 9, 160, "groundShort"),
                },
                // W7 rides FOG. The wave keeps its authored weight: the whole
                // point of a factor system is that weather adjusts the baseline
                // rather than the baseline being rewritten around the weather.
                new[]
                {
                    new WaveGroup("monolith", 1, 0, 0, "ground"),
                    new WaveGroup("cluster", 3, 65, 40, "groundShort"),
                    new WaveGroup("skiff", 5, 32, 80, "air"),
                },
                new[]
                {
                    new WaveGroup("aegis", 3, 80, 0, "groundShort"),
                    new WaveGroup("warden", 2, 80, 120, "ground"),
                    new WaveGroup("mote", 8, 9, 200, "groundShort"),
                },
                // Shade debuts here rather than in a wave of its own: the arc
                // stays twelve waves, so the swept difficulty curve holds. It
                // replaces two of the skiffs, which keeps the wave's weight
                // roughly where it was while changing what the wave asks.
                new[]
                {
                    new WaveGroup("shade", 3, 70, 0, "ground"),
                    new WaveGroup("mole", 4, 45, 40, "ground"),
                    new WaveGroup("cluster", 3, 55, 60, "groundShort"),
                    new WaveGroup("skiff", 4, 28, 100, "air"),
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
                new[]
                {
                    new WaveGroup("drifter", 12, 16, 0, "groundShort"),
                    new WaveGroup("mole", 3, 50, 100, "ground"),
                    new WaveGroup("cluster", 3, 60, 200, "groundShort"),
                    new WaveGroup("skiff", 5, 30, 240, "air"),
                },
                // Finale: everything, both routes, both layers.
                new[]
                {
                    new WaveGroup("monolith", 2, 180, 0, "ground"),
                    new WaveGroup("aegis", 2, 90, 100, "groundShort"),
                    new WaveGroup("warden", 2, 80, 200, "ground"),
                    new WaveGroup("cluster", 4, 55, 280, "groundShort"),
                    new WaveGroup("shade", 3, 60, 240, "ground"),
                    new WaveGroup("mender", 1, 0, 300, "groundShort"),
                    new WaveGroup("skiff", 4, 30, 320, "air"),
                    new WaveGroup("mote", 8, 9, 380, "groundShort"),
                },
            },
        };
}
