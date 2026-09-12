namespace DeepField.Sim.Content;

/// <summary>The lane graph: the map's routes as nodes and the authored spans
/// between them, which is the shape the content has always had and never said.
///
/// `Maps.cs` stores a *list of polylines*, and where two routes overlap it
/// stores the overlap twice, as byte-identical `Vec3` literals. The Toaster's
/// `direct` has nine waypoints and **not one of them is its own**: it is a
/// sub-path of `long`, typed out again. `long` and `west` share an identical
/// thirteen-waypoint prefix. Nothing can check that duplication, and nothing
/// can act on it — which is why a barricade is a route swap decided at spawn
/// rather than a door, and why `docs/MAP-AUTHORING.md` §3 has to list
/// "re-routing mid-walk" as something the sim cannot model.
///
/// A graph can be acted on. An edge can close; a path can be found again. That
/// is the whole point of the M5 mutable-map work, and this file is step one of
/// it: **derive** the graph from the routes that exist, change nothing, and
/// prove the derivation is lossless. Hand-authored graphs and mutable state
/// come after, on top of a decomposition that has already been shown to
/// reproduce every metre of every lane.</summary>
public enum LaneNodeKind
{
    /// <summary>Where a route begins — a spawn gate, or a warp arrival pad.</summary>
    Spawn,

    /// <summary>Where routes meet, split, or where a warp departs.</summary>
    Junction,

    /// <summary>Where a route ends. Today every ground route ends at the core.</summary>
    Core,
}

public enum LaneEdgeKind
{
    /// <summary>Walked at the enemy's speed, along its authored waypoints.</summary>
    Walk,

    /// <summary>Crossed in a tick and length zero for every purpose — distance
    /// travelled, coverage sampling, lane geometry. A `RouteDef.TeleportLegs`
    /// entry becomes one of these, and the three content invariants the harness
    /// gates for those legs (never first, never last, never adjacent) stop being
    /// rules and become a structural fact: a node is never a position an enemy
    /// occupies between ticks.</summary>
    Warp,
}

public sealed record LaneNodeDef(string Id, Vec3 Pos, LaneNodeKind Kind);

/// <summary>One authored span between two nodes. <see cref="Waypoints"/>
/// includes both endpoints, so an edge carries its whole geometry and a walker
/// needs nothing but the edge to know where it is.</summary>
public sealed record LaneEdgeDef(
    string Id,
    string From,
    string To,
    EnemyLayer Layer,
    IReadOnlyList<Vec3> Waypoints,
    LaneEdgeKind Kind = LaneEdgeKind.Walk,
    float CostFactor = 1f)
{
    /// <summary>Metres walked along this edge. Zero for a warp, by definition.</summary>
    public float WalkedLength
    {
        get
        {
            if (Kind == LaneEdgeKind.Warp) return 0f;
            float total = 0f;
            for (int i = 0; i < Waypoints.Count - 1; i++)
                total += Waypoints[i].DistanceTo(Waypoints[i + 1]);
            return total;
        }
    }
}

/// <summary>What a wave group means by "the long way round".
///
/// `WaveGroup.RouteId` names a route 132 times across the wave tables, and that
/// vocabulary is how the campaign's teaching arc is written — the Toaster's is
/// explicitly about which route a group takes. It survives as an *itinerary*: an
/// ordered list of nodes a group must pass through on its way to the core.
///
/// Deliberately mandatory vias rather than a soft cost bias. A bias is not
/// sweep-enumerable, it re-routes all 132 authored references on any cost tweak,
/// and it turns "wave 1 is `direct` and nothing else" into "wave 1 mostly
/// prefers direct". And the via list for a route that exists today is exactly
/// its junction sequence — which is what lets the movement switch land with the
/// event log unchanged.</summary>
public sealed record ItineraryDef(string Id, EnemyLayer Layer, IReadOnlyList<string> Via);

/// <summary>A derived or authored lane graph, plus the derivation.</summary>
public sealed class LaneGraph
{
    public IReadOnlyList<LaneNodeDef> Nodes { get; }
    public IReadOnlyList<LaneEdgeDef> Edges { get; }
    public IReadOnlyList<ItineraryDef> Itineraries { get; }

    /// <summary>Pairs of waypoints close enough to look like the same point and
    /// not close enough to be one. Coalescing is **exact equality only** — see
    /// <see cref="FromRoutes"/> — so these are reported rather than merged.</summary>
    public IReadOnlyList<string> NearMisses { get; }

    private LaneGraph(IReadOnlyList<LaneNodeDef> nodes, IReadOnlyList<LaneEdgeDef> edges,
        IReadOnlyList<ItineraryDef> itineraries, IReadOnlyList<string> nearMisses)
    {
        Nodes = nodes;
        Edges = edges;
        Itineraries = itineraries;
        NearMisses = nearMisses;
    }

    public LaneEdgeDef Edge(string id) => Edges.First(e => e.Id == id);
    public LaneNodeDef Node(string id) => Nodes.First(n => n.Id == id);

    /// <summary>Decompose a map's routes into nodes and edges, losslessly.
    ///
    /// A waypoint becomes a **node** when it is a route's first or last point,
    /// when it is either end of a warp, or when the routes passing through it do
    /// not all agree on where they came from and where they go. Everything else
    /// stays an interior point of an edge, so the graph is as small as the
    /// content allows rather than one node per waypoint.
    ///
    /// **Coalescing is exact `Vec3` equality with no epsilon.** Two waypoints a
    /// centimetre apart are two places. Merging on a tolerance would be a
    /// content edit performed by a derivation — silently moving a lane — and the
    /// one thing this step must not do is change where anything walks. Points
    /// that look like near-duplicates are collected in <see cref="NearMisses"/>
    /// for a human to fix as content, before movement ever reads the graph.</summary>
    public static LaneGraph FromRoutes(MapDef map)
    {
        // --- 1. Every occurrence of every waypoint, by exact position.
        var occurrences = new Dictionary<Vec3, List<(RouteDef Route, int Index)>>();
        foreach (var route in map.Routes)
            for (int i = 0; i < route.Waypoints.Count; i++)
            {
                var at = route.Waypoints[i];
                if (!occurrences.TryGetValue(at, out var list))
                    occurrences[at] = list = new List<(RouteDef, int)>();
                list.Add((route, i));
            }

        // --- 2. Which of them are nodes.
        var isNode = new HashSet<Vec3>();
        foreach (var (at, uses) in occurrences)
        {
            bool node = false;
            var context = new HashSet<(Vec3? Prev, Vec3? Next)>();
            foreach (var (route, i) in uses)
            {
                // Ends of a route are always nodes: a spawn gate and a core.
                if (i == 0 || i == route.Waypoints.Count - 1) node = true;
                // Both ends of a warp are nodes, so the warp is its own edge.
                if (i > 0 && route.IsTeleportLeg(i - 1)) node = true;
                if (route.IsTeleportLeg(i)) node = true;

                context.Add((
                    i > 0 ? route.Waypoints[i - 1] : null,
                    i < route.Waypoints.Count - 1 ? route.Waypoints[i + 1] : null));
            }
            // Routes disagree about how they pass through here, so it is a fork,
            // a join, or both.
            if (context.Count > 1) node = true;
            if (node) isNode.Add(at);
        }

        // --- 2b. Parallel spans need somewhere to differ.
        //
        // Switchyard's `ground` and `groundShort` share their spawn gate and
        // their core and nothing in between, so both decompose to a single span
        // from one node to the same other node — and an itinerary written as a
        // list of nodes cannot say which of the two it means. That is not an
        // accident of this map: a shortcut *is* a second way between the same
        // two places, and every gated route in the game is one.
        //
        // So when two spans would connect the same pair, promote a waypoint in
        // the middle of each to a node. The itineraries then differ by naming
        // where they go, which is also the honest description — "the short way"
        // is a route through somewhere, not a different pair of endpoints.
        // Iterated, because promoting a waypoint re-splits the routes and can
        // expose a new pair; it converges quickly and is capped so a pathology
        // cannot hang the build.
        for (int pass = 0; pass < 8; pass++)
        {
            var spans = new Dictionary<(Vec3 From, Vec3 To, EnemyLayer Layer), List<(RouteDef Route, int Start, int End)>>();
            foreach (var route in map.Routes)
            {
                int start = 0;
                for (int i = 1; i < route.Waypoints.Count; i++)
                {
                    if (!isNode.Contains(route.Waypoints[i])) continue;
                    var key = (route.Waypoints[start], route.Waypoints[i], route.Layer);
                    if (!spans.TryGetValue(key, out var list)) spans[key] = list = new();
                    list.Add((route, start, i));
                    start = i;
                }
            }

            var promoted = new List<Vec3>();
            foreach (var (_, group) in spans)
            {
                // Identical geometry is one edge, not a parallel pair.
                var distinct = group
                    .Select(g => string.Join(";", Enumerable.Range(g.Start, g.End - g.Start + 1)
                        .Select(k => g.Route.Waypoints[k].ToString())))
                    .Distinct().Count();
                if (distinct < 2) continue;

                foreach (var (route, s, e) in group)
                {
                    if (e - s < 2) continue;                 // no interior to promote
                    var mid = route.Waypoints[s + (e - s) / 2];
                    if (isNode.Add(mid)) promoted.Add(mid);
                }
            }
            if (promoted.Count == 0) break;
        }

        // --- 3. Name them, in a stable order.
        var ordered = isNode
            .OrderBy(p => p.X).ThenBy(p => p.Y).ThenBy(p => p.Z)
            .ToList();
        var nodeId = new Dictionary<Vec3, string>();
        for (int i = 0; i < ordered.Count; i++) nodeId[ordered[i]] = $"n{i}";

        var kinds = new Dictionary<Vec3, LaneNodeKind>();
        foreach (var at in ordered) kinds[at] = LaneNodeKind.Junction;
        foreach (var route in map.Routes)
        {
            kinds[route.Waypoints[0]] = LaneNodeKind.Spawn;
            kinds[route.Waypoints[^1]] = LaneNodeKind.Core;
            // An arrival pad is an entrance to the route in every sense the
            // sampler already means by "apron", so it is a spawn too.
            for (int leg = 0; leg < route.LegCount; leg++)
                if (route.IsTeleportLeg(leg)) kinds[route.Waypoints[leg + 1]] = LaneNodeKind.Spawn;
        }

        var nodes = ordered.Select(p => new LaneNodeDef(nodeId[p], p, kinds[p])).ToList();

        // --- 4. Split each route at its nodes; dedupe identical spans.
        var edges = new List<LaneEdgeDef>();
        var edgeByShape = new Dictionary<string, LaneEdgeDef>();
        var itineraries = new List<ItineraryDef>();

        foreach (var route in map.Routes)
        {
            var via = new List<string> { nodeId[route.Waypoints[0]] };
            int start = 0;
            for (int i = 1; i < route.Waypoints.Count; i++)
            {
                if (!isNode.Contains(route.Waypoints[i])) continue;

                var span = new List<Vec3>();
                for (int k = start; k <= i; k++) span.Add(route.Waypoints[k]);

                bool warp = i == start + 1 && route.IsTeleportLeg(start);
                string from = nodeId[route.Waypoints[start]];
                string to = nodeId[route.Waypoints[i]];

                // Two routes walking the same span share one edge — which is the
                // duplication this whole decomposition exists to remove.
                string shape = $"{from}|{to}|{route.Layer}|{(warp ? "warp" : "walk")}|"
                    + string.Join(";", span.Select(p => $"{p.X:R},{p.Y:R},{p.Z:R}"));
                if (!edgeByShape.TryGetValue(shape, out var edge))
                {
                    edge = new LaneEdgeDef($"e{edges.Count}", from, to, route.Layer, span,
                        warp ? LaneEdgeKind.Warp : LaneEdgeKind.Walk);
                    edgeByShape[shape] = edge;
                    edges.Add(edge);
                }

                via.Add(to);
                start = i;
            }
            itineraries.Add(new ItineraryDef(route.Id, route.Layer, via));
        }

        // --- 5. Near misses: distinct points close enough to be a typo.
        var nearMisses = new List<string>();
        var all = occurrences.Keys.OrderBy(p => p.X).ThenBy(p => p.Y).ThenBy(p => p.Z).ToList();
        for (int i = 0; i < all.Count; i++)
            for (int j = i + 1; j < all.Count; j++)
            {
                float d = all[i].DistanceTo(all[j]);
                if (d > 0f && d < NearMissMeters)
                    nearMisses.Add($"({all[i].X:0.##},{all[i].Y:0.##},{all[i].Z:0.##}) and "
                        + $"({all[j].X:0.##},{all[j].Y:0.##},{all[j].Z:0.##}) are {d:0.###} m apart");
            }

        return new LaneGraph(nodes, edges, itineraries, nearMisses);
    }

    /// <summary>Under this and not equal, two waypoints are probably meant to be
    /// one place. Half a metre is well inside the 3.4 m lane, so nothing this
    /// flags is a deliberate two-places-close-together.</summary>
    public const float NearMissMeters = 0.5f;

    /// <summary>Walk an itinerary back into the polyline it came from. The
    /// derivation is lossless exactly when this returns the route's original
    /// waypoints, which is what the harness gate asserts.</summary>
    public IReadOnlyList<Vec3> Rebuild(string itineraryId)
    {
        var itinerary = Itineraries.First(i => i.Id == itineraryId);
        var points = new List<Vec3>();
        for (int i = 0; i < itinerary.Via.Count - 1; i++)
        {
            var edge = Edges.First(e => e.From == itinerary.Via[i]
                && e.To == itinerary.Via[i + 1]
                && e.Layer == itinerary.Layer);
            // Each edge repeats the node it starts on, which the previous edge
            // already ended on.
            for (int k = i == 0 ? 0 : 1; k < edge.Waypoints.Count; k++)
                points.Add(edge.Waypoints[k]);
        }
        return points;
    }
}
