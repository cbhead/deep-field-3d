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

    /// <summary>Damage per second on the *busiest* stretch of lane past which
    /// more stops helping much. Quieter stretches saturate proportionally
    /// sooner — see <see cref="Traffic"/>.
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

    /// <summary>The least share of the busiest lane's demand any lane gets,
    /// however little walks it. The second tuned dial in this file, and the
    /// only one with a cliff in it: on the Spire, 0.25 generates a build that
    /// clears the campaign with twelve lives and 0.35 generates one that dies
    /// on wave two, because that map's air strand is long enough that a
    /// slightly higher floor makes anti-air outbid the ground everywhere at
    /// once. Recorded rather than smoothed over — a dial with a cliff near the
    /// shipped value is a thing to know before the next map is authored, and
    /// the four build orders in MatchRunner are frozen strings that do not
    /// move when it does.</summary>
    private const float QuietLaneShare = 0.25f;

    /// <summary>How many enemies the wave tables send down each edge, over the
    /// whole campaign. Every sample is worth its edge's traffic.
    ///
    /// This is the third version of the same mistake and the comment above
    /// describes the first two. Coverage alone covered the map and killed
    /// nothing; damage-per-credit fixed that but still counted a metre of lane
    /// nothing walks as worth a metre of the lane everything walks. Three maps
    /// hid it, because their air strands are short beside their ground lanes.
    /// The Spire's is 147 m against 218 m of ground and carries a tenth of the
    /// enemies, and unweighted the pass bought **eight Skywatches out of
    /// sixteen picks** — a build that clears the campaign with a hero carrying
    /// it and dies on wave two without one. Damage delivered has to mean
    /// delivered to something.
    ///
    /// Traffic sets each stretch's *ceiling*, not its worth. Weighting the
    /// gain by traffic instead is the obvious version and it is wrong in the
    /// mirror way: a life is a life whichever lane it came down, so a stretch
    /// nothing can shoot is a catastrophe however few walk it, and scaled by
    /// traffic the pass bought no anti-air at all. As a ceiling it says the
    /// true thing — a quiet lane needs answering and then stops paying — so
    /// the ground saturates first and the strand gets covered after, which is
    /// the order a player buys them in.</summary>
    private static float[] Traffic(World world)
    {
        var perEdge = new float[world.Graph.Edges.Count];
        if (!Waves.ByMap.TryGetValue(world.Map.Id, out var table))
        {
            Array.Fill(perEdge, 1f);
            return perEdge;
        }
        for (int i = 0; i < world.Graph.Itineraries.Count; i++)
        {
            string id = world.Graph.Itineraries[i].Id;
            float walked = table.Sum(wave => wave.Where(g => g.RouteId == id)
                .Sum(g => (float)g.Count));
            foreach (int e in world.ItineraryEdges[i]) perEdge[e] += walked;
        }
        // An edge no wave is authored onto still gets a floor weight rather
        // than a zero: routing is dynamic now, so "nothing is sent this way"
        // and "nothing ever goes this way" stopped being the same statement
        // the day a shut gate could turn a wave onto it.
        for (int e = 0; e < perEdge.Length; e++)
            if (perEdge[e] <= 0f) perEdge[e] = 1f;

        // Per metre, not per edge. Two lanes that carry the same wave are not
        // equally busy if one of them is three times as long: the Spire's air
        // strand is 40% of that map's lane *length* and a tenth of its
        // traffic, and counted per edge it still outbid the ground because
        // there was simply more of it to score against.
        for (int e = 0; e < perEdge.Length; e++)
            perEdge[e] /= MathF.Max(1f, world.Graph.Edges[e].WalkedLength);

        // As a fraction of the busiest lane, so the dial above keeps meaning
        // what it meant: the busiest lane on every map still saturates at
        // exactly UsefulDpsPerStretch.
        //
        // The floor is not slack. An enemy needs a fixed amount of damage to
        // die however few of them come, so the dps a quiet lane wants does not
        // fall to nothing with its traffic — and a lane nothing can shoot
        // leaks all of it. Without a floor the pass bought no anti-air at all.
        float busiest = perEdge.Max();
        for (int e = 0; e < perEdge.Length; e++)
            perEdge[e] = UsefulDpsPerStretch
                * MathF.Max(QuietLaneShare, perEdge[e] / busiest);
        return perEdge;
    }

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
                        gain += MathF.Min(dps[i] + add, samples[i].Demand)
                              - MathF.Min(dps[i], samples[i].Demand);
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
    private static bool Reaches(TowerDef def, Vec3 socket,
        (Vec3 At, EnemyLayer Layer, float Demand) sample)
    {
        if (!def.TargetLayers.Contains(sample.Layer)) return false;
        float d = socket.DistanceTo(sample.At);
        return d <= def.RangeMeters && d >= def.MinRangeMeters;
    }

    /// <summary>Points along every lane that is open in this world. Closed
    /// edges are not sampled: covering a lane nothing walks is not coverage,
    /// and the whole reason this exists is that which lanes are open is now a
    /// thing the player decides.</summary>
    private static List<(Vec3 At, EnemyLayer Layer, float Demand)> Samples(World world)
    {
        var traffic = Traffic(world);
        var samples = new List<(Vec3, EnemyLayer, float)>();
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
                    samples.Add((Vec3.Lerp(a, b, (float)k / steps), edge.Layer, traffic[e]));
            }
        }
        return samples;
    }
}
