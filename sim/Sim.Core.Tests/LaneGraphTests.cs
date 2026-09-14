using DeepField.Sim;
using DeepField.Sim.Content;
using Xunit;

namespace DeepField.Sim.Tests;

/// <summary>The lane graph is derived from the routes and nothing reads it yet.
/// These pin the structural claims the M5 work is built on, so that when
/// movement does read it, what it reads has already been argued for.</summary>
public class LaneGraphTests
{
    [Fact]
    public void TheToastersDirectRouteAddsAConnectionButNoPlaces()
    {
        // The claim that says the content is already a graph — stated precisely,
        // because the loose version is wrong in an interesting way.
        //
        // `direct` has nine waypoints and **not one of them is its own**: every
        // place it visits, `long` visits too. But it is not a sub-path of
        // `long`, because it reaches the drive straight from the gate while
        // `long` arrives there the long way round. So it contributes exactly one
        // thing the graph did not already have: an *edge*. Which is what
        // "direct" means, and what a polyline list cannot express — the
        // difference between the two routes is one connection, stored as nine
        // duplicated coordinates.
        var graph = LaneGraph.FromRoutes(Maps.Toaster);
        var direct = Edges(graph, "direct");
        var longWay = Edges(graph, "long");

        var longPoints = Maps.Toaster.Routes.First(r => r.Id == "long").Waypoints.ToHashSet();
        foreach (var p in Maps.Toaster.Routes.First(r => r.Id == "direct").Waypoints)
            Assert.Contains(p, longPoints);

        foreach (var node in Itinerary(graph, "direct").Via)
            Assert.Contains(node, Itinerary(graph, "long").Via);

        Assert.Single(direct.Except(longWay));
    }

    [Fact]
    public void ParallelRoutesBetweenTheSameTwoPlacesStayDistinguishable()
    {
        // Switchyard's shortcut shares its spawn gate and its core with the long
        // way round and nothing in between, so both would decompose to one span
        // between the same pair of nodes — and an itinerary written as a list of
        // nodes could not say which it meant. The derivation promotes a waypoint
        // in the middle of each so they differ by where they go.
        var graph = LaneGraph.FromRoutes(Maps.Switchyard);
        var ground = Itinerary(graph, "ground").Via;
        var shortcut = Itinerary(graph, "groundShort").Via;

        Assert.Equal(ground[0], shortcut[0]);                   // same gate
        Assert.Equal(ground[^1], shortcut[^1]);                 // same core
        Assert.NotEqual(ground, shortcut);                      // different way there
        Assert.Empty(ground.Intersect(shortcut).Except(new[] { ground[0], ground[^1] }));
    }

    [Fact]
    public void EveryItineraryRunsFromASpawnToACore()
    {
        foreach (var map in Maps.All.Values)
        {
            var graph = LaneGraph.FromRoutes(map);
            foreach (var itinerary in graph.Itineraries)
            {
                Assert.Equal(LaneNodeKind.Spawn, graph.Node(itinerary.Via[0]).Kind);
                Assert.Equal(LaneNodeKind.Core, graph.Node(itinerary.Via[^1]).Kind);
            }
        }
    }

    [Fact]
    public void WarpEdgesAreZeroLengthAndNeverAdjacent()
    {
        // The invariant three separate harness gates enforce on TeleportLegs —
        // never first, never last, never two in a row — becomes structural here:
        // a node is not a position an enemy occupies between ticks, so a warp
        // simply cannot be walked onto or off a walk edge's middle.
        foreach (var map in Maps.All.Values)
        {
            var graph = LaneGraph.FromRoutes(map);
            foreach (var itinerary in graph.Itineraries)
            {
                var edges = Edges(graph, itinerary.Id);
                for (int i = 0; i < edges.Count; i++)
                {
                    if (edges[i].Kind != LaneEdgeKind.Warp) continue;
                    Assert.Equal(0f, edges[i].WalkedLength);
                    Assert.NotEqual(0, i);                                   // never first
                    Assert.NotEqual(edges.Count - 1, i);                     // never last
                    Assert.NotEqual(LaneEdgeKind.Warp, edges[i - 1].Kind);   // never adjacent
                    Assert.NotEqual(LaneEdgeKind.Warp, edges[i + 1].Kind);
                }
            }
        }
    }

    [Fact]
    public void AWarpsArrivalPadCountsAsAnEntrance()
    {
        // Same rule the coverage sampler already applies: the metres after an
        // arrival pad are apron exactly as the metres after a spawn gate are,
        // because an enemy standing on one has its whole walk ahead of it.
        var graph = LaneGraph.FromRoutes(Maps.Toaster);
        var warps = graph.Edges.Where(e => e.Kind == LaneEdgeKind.Warp).ToList();

        Assert.NotEmpty(warps);
        Assert.All(warps, w => Assert.Equal(LaneNodeKind.Spawn, graph.Node(w.To).Kind));
    }

    [Fact]
    public void DecompositionIsLosslessOnEveryMap()
    {
        // The gate asserts this too; it is here as well because it is the one
        // property everything later depends on, and a unit test names the map
        // and the waypoint when it breaks.
        foreach (var map in Maps.All.Values)
        {
            var graph = LaneGraph.FromRoutes(map);
            foreach (var route in map.Routes)
                Assert.Equal(route.Waypoints, graph.Rebuild(route.Id));
        }
    }

    [Fact]
    public void SharedSpansAreOneEdgeNotTwo()
    {
        // The duplication this decomposition exists to remove: across the whole
        // campaign, no two edges have the same endpoints, layer and geometry.
        foreach (var map in Maps.All.Values)
        {
            var graph = LaneGraph.FromRoutes(map);
            var shapes = graph.Edges.Select(e =>
                $"{e.From}|{e.To}|{e.Layer}|{e.Kind}|{string.Join(";", e.Waypoints)}");
            Assert.Equal(graph.Edges.Count, shapes.Distinct().Count());
        }
    }

    private static ItineraryDef Itinerary(LaneGraph graph, string id) =>
        graph.Itineraries.First(i => i.Id == id);

    private static List<LaneEdgeDef> Edges(LaneGraph graph, string itineraryId)
    {
        var itinerary = Itinerary(graph, itineraryId);
        var edges = new List<LaneEdgeDef>();
        for (int i = 0; i < itinerary.Via.Count - 1; i++)
            edges.Add(graph.Edges.First(e => e.From == itinerary.Via[i]
                && e.To == itinerary.Via[i + 1] && e.Layer == itinerary.Layer));
        return edges;
    }
}
