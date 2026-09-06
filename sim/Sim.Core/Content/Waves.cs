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
        };
}
