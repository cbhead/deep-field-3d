using DeepField.Sim;
using DeepField.Sim.Content;

namespace DeepField.Harness;

/// <summary>What a competent player builds, generated rather than typed.
///
/// `MatchRunner.BuildOrders` is four hand-tuned lists, and their own comments
/// describe the algorithm that produced them — *"derived by measurement, not
/// taste: a greedy pass picks whichever free socket adds the most
/// previously-uncovered lane"*, with anti-air *"scored on the spiral"* and
/// Detectors *"scored on stair coverage instead"*. That pass was run once, by
/// hand, offline, and only its output was kept.
///
/// It has to become code, because the map is mutable now. A build order tuned
/// for one configuration measures every other configuration as a disaster:
/// hold Switchyard's towers still and shut the switchback instead of the cut,
/// and the reference build loses the map with twenty lives gone — not because
/// that door is a bad choice but because nobody rebuilt for it. Comparing
/// configurations means each one has to be *played*, and playing means
/// building against the lanes that are actually open.
///
/// This is a floor, not a forecast, and deliberately dumb in the same ways the
/// hand lists are: it buys coverage, it does not plan, and it has no idea what
/// is coming.</summary>
public static class FloorPolicy
{
    /// <summary>How finely lanes are sampled when scoring coverage. The same
    /// 4 m the map validator uses, so "covered" means one thing in both.</summary>
    private const float SampleMeters = 4f;

    /// <summary>Damage per second on one stretch of lane past which more stops
    /// helping much.
    ///
    /// The first version of this scored on coverage alone, exactly as the hand
    /// lists' own comment describes, and it produced a *better-covered and much
    /// worse* build: 98% of Foundry against the hand list's 71%, and a wall of
    /// Novas, because Nova has the longest ground reach in the game and reach
    /// was the only thing being counted. It is also a slow mortar with a five
    /// metre hole in the middle. The map was covered and nothing died.
    ///
    /// So the objective is damage delivered along the lane, with a ceiling. The
    /// ceiling is what makes it spread out: past this much dps on one stretch,
    /// the next tower is worth more somewhere thinner. It is a dial, and it is
    /// the one thing in this file that is tuned rather than derived.</summary>
    private const float UsefulDpsPerStretch = 60f;

    /// <summary>Build order for a world as it currently stands — open edges,
    /// occupied sockets and all. Returns "tower:socket" strings in the same
    /// form the hand lists use, so it drops straight into MatchRunner.</summary>
    public static string[] Build(World world, int picks)
    {
        var samples = Samples(world);
        var dps = new float[samples.Count];
        var order = new List<string>();
        var taken = new HashSet<string>(world.Towers.Select(t => t.SocketId));

        // Candidates in a fixed order — sockets by id, towers by id — so the
        // greedy pass resolves ties the same way every run and on every machine.
        var sockets = world.Map.Sockets
            .Where(s => s.Tag is SocketTag.Ground or SocketTag.Wall)
            .OrderBy(s => s.Id, StringComparer.Ordinal)
            .ToList();
        var towers = Towers.All.Values
            .Where(t => t.Kind != TowerKind.Barricade && t.Damage > 0f)
            .OrderBy(t => t.Id, StringComparer.Ordinal)
            .ToList();

        for (int pick = 0; pick < picks; pick++)
        {
            string? bestEntry = null;
            string? bestSocket = null;
            int bestGain = 0;

            float bestScore = 0f;
            foreach (var socket in sockets)
            {
                if (taken.Contains(socket.Id)) continue;
                foreach (var def in towers)
                {
                    float add = def.Damage * def.ShotsPerSecond;
                    if (add <= 0f) continue;
                    float gain = 0f;
                    for (int i = 0; i < samples.Count; i++)
                    {
                        if (!Reaches(def, socket.Pos, samples[i])) continue;
                        gain += MathF.Min(dps[i] + add, UsefulDpsPerStretch)
                              - MathF.Min(dps[i], UsefulDpsPerStretch);
                    }
                    // Per credit, not per tower. Without this the pass always
                    // reaches for the longest range in the game and pays
                    // whatever it costs — which is the second way it produced a
                    // wall of Novas. A floor policy buys what it can afford,
                    // and Nova is the most expensive ground tower there is.
                    gain /= def.Cost;
                    if (gain > bestScore)
                    {
                        bestScore = gain;
                        bestEntry = $"{def.Id}:{socket.Id}";
                        bestSocket = socket.Id;
                    }
                }
            }
            bestGain = bestScore > 0f ? 1 : 0;

            // Nothing left uncovered that anything can reach. Stop rather than
            // buying towers that answer nothing — a floor that spends its last
            // credits on a pad covering no lane is not what a player does.
            if (bestEntry is null || bestSocket is null) break;

            order.Add(bestEntry);
            taken.Add(bestSocket);

            var chosen = Towers.All[bestEntry.Split(':')[0]];
            var at = world.Map.Sockets.First(s => s.Id == bestSocket).Pos;
            float chosenDps = chosen.Damage * chosen.ShotsPerSecond;
            for (int i = 0; i < samples.Count; i++)
                if (Reaches(chosen, at, samples[i])) dps[i] += chosenDps;
        }

        return order.ToArray();
    }

    /// <summary>Fraction of sampled lane a build order covers. The number the
    /// hand lists are held to — a generator that reaches less of the map than
    /// the human did is a worse floor, whatever else it is.</summary>
    public static float Coverage(World world, IReadOnlyList<string> order)
    {
        var samples = Samples(world);
        if (samples.Count == 0) return 1f;

        var covered = new bool[samples.Count];
        foreach (var entry in order)
        {
            var parts = entry.Split(':');
            if (!Towers.All.TryGetValue(parts[0], out var def)) continue;
            var socket = world.Map.Sockets.FirstOrDefault(s => s.Id == parts[1]);
            if (socket is null) continue;
            for (int i = 0; i < samples.Count; i++)
                if (!covered[i] && Reaches(def, socket.Pos, samples[i])) covered[i] = true;
        }
        return (float)covered.Count(c => c) / samples.Count;
    }

    /// <summary>Whether a tower on this pad could hit something standing here.
    /// Level-1 range only: a lane covered once someone has paid for upgrades is
    /// not covered on the wave it first matters — the same rule the map
    /// validator applies.</summary>
    private static bool Reaches(TowerDef def, Vec3 socket, (Vec3 At, EnemyLayer Layer) sample)
    {
        if (!def.TargetLayers.Contains(sample.Layer)) return false;
        float d = socket.DistanceTo(sample.At);
        return d <= def.RangeMeters && d >= def.MinRangeMeters;
    }

    /// <summary>Points along every lane that is open in this world. Closed
    /// edges are not sampled: covering a lane nothing walks is not coverage,
    /// and the whole reason this exists is that which lanes are open is now a
    /// thing the player decides.</summary>
    private static List<(Vec3 At, EnemyLayer Layer)> Samples(World world)
    {
        var samples = new List<(Vec3, EnemyLayer)>();
        for (int e = 0; e < world.Graph.Edges.Count; e++)
        {
            if (!world.EdgeOpen[e]) continue;
            var edge = world.Graph.Edges[e];
            if (edge.Kind == LaneEdgeKind.Warp) continue;

            for (int i = 0; i < edge.Waypoints.Count - 1; i++)
            {
                var a = edge.Waypoints[i];
                var b = edge.Waypoints[i + 1];
                float length = a.DistanceTo(b);
                int steps = System.Math.Max(1, (int)MathF.Floor(length / SampleMeters));
                for (int k = 0; k <= steps; k++)
                    samples.Add((Vec3.Lerp(a, b, (float)k / steps), edge.Layer));
            }
        }
        return samples;
    }
}
