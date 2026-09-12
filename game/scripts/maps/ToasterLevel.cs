using Godot;
using System.Collections.Generic;
using System.Linq;
using DeepField.Sim.Content;

namespace DeepField.Game;

public partial class GameRoot
{
    /// <summary>The Toaster, built.
    ///
    /// Three hundred and twenty metres by a hundred and sixty, which is six
    /// times the area of any map before it, so nothing here is placed the way
    /// a yard was placed. The roads are laid along authored polylines because
    /// vehicles read their handling off those same lines; the treeline is one
    /// instanced draw rather than fifteen hundred nodes; the four buildings are
    /// volumes with insides, which is new; and the edge of the world is a wall
    /// you hit rather than a slab you walk off.</summary>
    private void BuildToasterStructures(MapDef map)
    {
        Surfaces.Roads = ToasterLayout.Roads;
        Surfaces.Water = (ToasterLayout.PondCentre, ToasterLayout.PondRadius);

        BuildToasterRoads();
        BuildToasterPond();

        // Centre, footprint, eaves, doors, and which wall carries the ladder to
        // the roof. Each is named for the vehicle parked outside it.
        //
        // The ladder goes on whichever wall its roof socket is nearest, which
        // is not decoration: a climb that tops out a building's width away
        // from the pad it serves leaves the player standing on rungs with
        // nothing to step onto, and that is the mistake this project has now
        // made on three maps. The traversal probe measures it.
        BuildHouse("barn", new Vector3(-108, 0, 54), 30f, 21f, 6.0f, new[]
        {
            // Wide and tall enough to drive the Gator through, which is the
            // whole reason a barn has a door that size.
            new Door(Side.South, 0f, 5f, 4f),
            new Door(Side.North, -8f),
        }, Side.East, -8.5f, "toaster_barn_shell", "toaster_barn_roof");

        BuildHouse("buggy", new Vector3(-134, 0, -7), 19f, 28f, 3.4f, new[]
        {
            new Door(Side.East, 6f),
            new Door(Side.West, -8f),
        }, Side.West, 12f, "toaster_house_buggy_shell", "toaster_house_buggy_roof");

        BuildHouse("vehickle", new Vector3(30, 0, 31), 34f, 26f, 3.4f, new[]
        {
            new Door(Side.North, -10f),      // onto the drive
            new Door(Side.South, 12f),
            new Door(Side.West, 0f, 4.5f, 2.8f),
        }, Side.East, -11f, "toaster_house_vehickle_shell", "toaster_house_vehickle_roof");

        BuildHouse("grnmchn", new Vector3(90, 0, -50), 24f, 30f, 3.4f, new[]
        {
            new Door(Side.West, 0f),         // toward the pond
            new Door(Side.East, 8f),
        }, Side.West, 13f, "toaster_house_grnmchn_shell", "toaster_house_grnmchn_roof");

        foreach (var (id, label, at) in ToasterLayout.Pads) AddTeleportPad(id, label, at);

        float beltX = map.HalfX - ToasterLayout.BeltDepth;
        float beltZ = map.HalfZ - ToasterLayout.BeltDepth;
        BuildContainment(beltX, beltZ);
        BuildTreeBelt(map.HalfX, map.HalfZ, ToasterLayout.BeltDepth);
        // The scatter count is the field's, not a yard's: twenty-two pieces of
        // clutter on six times the area is an empty map with litter in it.
        ScatterTerrain("toaster_terrain_scatter", beltX - 2f, beltZ - 2f, attempts: 140);
    }

    /// <summary>The made roads: a flat strip per segment, dressed with the 4 m
    /// road module exactly as an enemy lane is dressed. Layer 0 — a road is
    /// something you drive on, not something you bump into, and the ground slab
    /// underneath is what holds anything up.</summary>
    private void BuildToasterRoads()
    {
        foreach (var road in ToasterLayout.Roads)
        {
            string asset = road.Kind == Surface.Asphalt
                ? "toaster_road_asphalt" : "toaster_road_gravel";
            var color = road.Kind == Surface.Asphalt
                ? new Color(0.18f, 0.18f, 0.2f) : new Color(0.55f, 0.5f, 0.42f);

            for (int i = 0; i < road.Points.Length - 1; i++)
            {
                var a = road.Points[i];
                var b = road.Points[i + 1];
                var run = b - a;
                float span = run.Length();
                if (span < 1f) continue;

                // Asphalt sits a centimetre proud of gravel so their junction
                // is a road crossing a drive rather than z-fighting.
                float height = road.Kind == Surface.Asphalt ? 0.05f : 0.04f;
                var strip = AddStaticBox((a + b) * 0.5f + new Vector3(0, height * 0.5f, 0),
                    new Vector3(span, height, road.Width), color, layer: 0);
                strip.Rotation = new Vector3(0, Mathf.Atan2(-run.Z, run.X), 0);
                MapKit.MountRun(strip, asset, span, 4f, alongX: true, MapKit.GroundLocal(strip));
            }
        }
    }

    /// <summary>The pond.
    ///
    /// The ground is one flat slab and always will be, so a pond cannot be a
    /// hole — it is drawn upward: a bank ring standing proud of grade with
    /// water inside it. The ring is solid, which is what stops a vehicle doing
    /// twenty metres a second from driving across the surface of the water, and
    /// hidden, because the bank is the delivered model's job.</summary>
    private void BuildToasterPond()
    {
        MapKit.Prop(this, "toaster_pond", ToasterLayout.PondCentre);

        const int segments = 24;
        float step = Mathf.Tau / segments;
        float chord = 2f * ToasterLayout.PondRadius * Mathf.Sin(step * 0.5f) + 0.4f;
        for (int i = 0; i < segments; i++)
        {
            float angle = i * step;
            var at = ToasterLayout.PondCentre
                + new Vector3(Mathf.Cos(angle), 0, Mathf.Sin(angle)) * ToasterLayout.PondRadius;
            var bank = AddStaticBox(at + new Vector3(0, 0.4f, 0),
                new Vector3(chord, 0.8f, 0.8f), new Color(0.34f, 0.36f, 0.3f), layer: 1);
            // The box runs along its own +X, so it has to be laid on the
            // tangent. Turned to the radius instead — which is a quarter turn
            // out and the same mistake the freight cut made — every segment
            // points outward like a spoke and a five-metre length of bank
            // reaches three metres into the lane forty metres away.
            var tangent = new Vector3(-Mathf.Sin(angle), 0, Mathf.Cos(angle));
            bank.RotationDegrees = new Vector3(0, MapKit.YawAlongX(tangent), 0);
            MapKit.HideBox(bank);
        }
    }

    /// <summary>The treeline: a belt of woodland round the whole property,
    /// drawn as two instanced meshes per species rather than as fifteen hundred
    /// scene subtrees.
    ///
    /// Placement is a lattice with a hashed jitter and no RNG at all — the same
    /// rule the terrain scatter follows, and for the same reason: the sim's
    /// streams stay untouched and every client draws the identical wood without
    /// a byte crossing the wire.</summary>
    private void BuildTreeBelt(float halfX, float halfZ, float depth)
    {
        string[] species = { "toaster_tree_oak", "toaster_tree_maple", "toaster_tree_pine" };
        var placements = new Dictionary<string, List<Transform3D>>();
        foreach (string s in species) placements[s] = new List<Transform3D>();

        int i = 0;
        for (float x = -halfX + 2f; x <= halfX - 2f; x += 3.5f)
            for (float z = -halfZ + 2f; z <= halfZ - 2f; z += 3.5f, i++)
            {
                if (Mathf.Abs(x) < halfX - depth && Mathf.Abs(z) < halfZ - depth) continue;

                float jitterX = (i * 31 % 17) / 17f * 2.4f - 1.2f;
                float jitterZ = (i * 43 % 13) / 13f * 2.4f - 1.2f;
                var at = new Vector3(x + jitterX, 0, z + jitterZ);
                // The roads leave the property through the wood; so does every
                // route, and the spawn gate stands in the gap.
                if (Surfaces.At(at) != Surface.Grass) continue;
                if (_laneMouths.Any(m => Flat(m).DistanceTo(Flat(at)) < 8f)) continue;
                if (Blocked(at, 4f)) continue;

                float scale = 0.85f + (i * 7 % 10) * 0.04f;
                var basis = Basis.FromEuler(new Vector3(0, Mathf.DegToRad(i * 47f % 360f), 0))
                    .Scaled(Vector3.One * scale);
                // Positive modulo: half this map is at negative x, and C#'s %
                // keeps the sign of its left operand.
                int pick = ((i * 3 + Mathf.FloorToInt(x)) % species.Length + species.Length) % species.Length;
                placements[species[pick]].Add(new Transform3D(basis, at));
            }

        int drawn = 0, trees = 0;
        foreach (var (asset, spots) in placements)
        {
            if (spots.Count == 0) continue;
            trees += spots.Count;
            // Near cells in full, far cells as the flat stand-in. Chunked,
            // because a visibility range and a frustum test both act on a
            // multimesh's whole bounding box: one belt-wide draw would never
            // switch and never cull.
            drawn += MapKit.InstancedChunked(this, asset, spots, 40f,
                castShadow: true, visibleTo: 90f);
            drawn += MapKit.InstancedChunked(this, $"{asset}_lod1", spots, 40f,
                castShadow: false, visibleFrom: 90f);
        }
        GD.Print($"[map] treeline: {trees} trees in {drawn} multimesh(es)");
    }
}
