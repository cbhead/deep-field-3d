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
        Surfaces.Drive = (ToasterLayout.DriveCentre, ToasterLayout.DriveRadius, ToasterLayout.DriveWidth);

        BuildToasterRoads();
        BuildToasterDrive();
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
        BuildToasterInteriors();

        float beltX = map.HalfX - ToasterLayout.BeltDepth;
        float beltZ = map.HalfZ - ToasterLayout.BeltDepth;
        BuildContainment(beltX, beltZ);
        BuildTreeBelt(map.HalfX, map.HalfZ, ToasterLayout.BeltDepth);
        // Ground scatter is one of the dressing's instanced rows here rather
        // than ScatterTerrain's one-prop-per-spot: the clump is four parts, and
        // a hundred and forty of them as subtrees was six hundred nodes of the
        // same four meshes.
        BuildToasterDressing();
    }

    // --- Things on the map that move ---------------------------------------
    /// <summary>Parts a delivered prop asked to have turned: the group declares
    /// <c>userData.spin = { part, axis, rpm }</c> (the windmill's wheel) and the
    /// client turns that node without knowing its geometry — the moving-parts
    /// contract from the weapons, applied to a prop.</summary>
    private readonly List<(Node3D Part, Vector3 Axis, float RadPerSec)> _spinners = new();

    private void ArmSpin(Node3D? prop)
    {
        if (prop is null || FindExtras(prop, "spin") is not { } spin) return;
        if (!spin.TryGetValue("part", out var partName) || prop.FindChild(partName.AsString(), true, false) is not Node3D part) return;
        var axis = Vector3.Forward;
        if (spin.TryGetValue("axis", out var axisVar) && axisVar.VariantType == Variant.Type.Array)
        {
            var a = axisVar.AsGodotArray();
            if (a.Count == 3) axis = new Vector3((float)a[0].AsDouble(), (float)a[1].AsDouble(), (float)a[2].AsDouble());
        }
        float rpm = spin.TryGetValue("rpm", out var rpmVar) ? (float)rpmVar.AsDouble() : 10f;
        if (axis.LengthSquared() < 0.001f) return;
        _spinners.Add((part, axis.Normalized(), rpm * Mathf.Tau / 60f));
    }

    private void TickSpinners(double delta)
    {
        foreach (var (part, axis, rate) in _spinners)
            if (IsInstanceValid(part)) part.RotateObjectLocal(axis, rate * (float)delta);
    }

    /// <summary>The glTF extras dictionary carrying <paramref name="key"/>, on
    /// this node or any child — the importer parks a group's userData on the
    /// node as "extras" meta.</summary>
    private static Godot.Collections.Dictionary? FindExtras(Node node, string key)
    {
        if (node.HasMeta("extras") && node.GetMeta("extras") is { VariantType: Variant.Type.Dictionary } extras
            && extras.AsGodotDictionary().TryGetValue(key, out var found) && found.VariantType == Variant.Type.Dictionary)
            return found.AsGodotDictionary();
        foreach (var child in node.GetChildren())
            if (FindExtras(child, key) is { } deeper) return deeper;
        return null;
    }

    /// <summary>The circular drive: twelve arcs of thirty degrees, instanced.
    /// The arc's origin is at the centre of curvature and it starts on +X
    /// sweeping toward +Z, so the k-th is turned by -k·30° — a positive yaw
    /// takes +X toward -Z in this engine, and twelve of them the other way
    /// round is a dodecagon laid inside out.</summary>
    private void BuildToasterDrive()
    {
        var arcs = new List<Transform3D>();
        for (int k = 0; k < 12; k++)
            arcs.Add(new Transform3D(
                Basis.FromEuler(new Vector3(0, -Mathf.DegToRad(30f * k), 0)), ToasterLayout.DriveCentre));
        if (MapKit.Instanced(this, "toaster_road_arc", arcs, castShadow: false) == 0)
            GD.Print("[map] circular drive: toaster_road_arc not delivered, surface only");
    }

    /// <summary>The rooms, arranged around the teleport pad and the roof
    /// socket rather than around nothing — placed here, not baked into the
    /// shells, for exactly that reason. Design's own positions, on the 0.15 m
    /// floor slab the graybox builds.</summary>
    private void BuildToasterInteriors()
    {
        const float floor = 0.15f;
        void Put(string id, float x, float z, float yaw)
            => MapKit.Prop(this, id, new Vector3(x, floor, z), Mathf.RadToDeg(yaw));

        // Barn: a workbench and shelving along the back wall.
        Put("toaster_dress_workbench", -116f, 62f, Mathf.Pi);
        Put("toaster_dress_shelving", -100f, 62.8f, Mathf.Pi);
        // Vehickle house garage bay.
        Put("toaster_dress_workbench", 16f, 31f, Mathf.Pi / 2f);
        // The three houses: a living room and a kitchen each.
        Put("toaster_dress_furniture_living", -132f, 2f, 0f);
        Put("toaster_dress_furniture_kitchen", -134f, -18.6f, 0f);
        Put("toaster_dress_furniture_living", 34f, 38f, Mathf.Pi);
        Put("toaster_dress_furniture_kitchen", 38f, 42.6f, Mathf.Pi);
        Put("toaster_dress_furniture_living", 93f, -44f, 0f);
        Put("toaster_dress_furniture_kitchen", 90f, -63.6f, 0f);
    }

    /// <summary>Everything that punctuates the property, generated rather than
    /// listed and filtered against the same rule the client refuses placements
    /// on: nothing within 3 m of a walked lane, a socket, the spawn, the core,
    /// the armory or a hero station — measured from the piece's own FOOTPRINT,
    /// not its origin, because a 4 m fence module placed 3.1 m from a lane still
    /// has two metres of itself in the lane — and nothing generated inside a
    /// building's volume, footprint plus its own radius plus a metre, because
    /// the first drop stood two tanks and two woodpiles inside walls and
    /// nothing checked.
    ///
    /// Design's first pass hand-placed these and put eleven inside the margin;
    /// six times the area of any map before it, a hand-listed row is a row that
    /// is wrong the next time a route moves. The rows below are design's own
    /// generator (docs/design/models/levels.js: <c>fence()</c>, <c>poleLine()</c>,
    /// the orchard, crop and hedgerow rules), run here so the filter is the
    /// one that ships. Fence lines are the design of a farm — where a wire
    /// goes is what turns 320 m of lawn into a property.
    ///
    /// Roadside furniture may stand at the kerb; scenery may not stand on the
    /// road. Fences, gates, poles, a hedgerow and a mailbox are the point of a
    /// verge, so they get the road test at the carriageway edge; the rest get
    /// the same 2 m margin as the trees.</summary>
    private void BuildToasterDressing()
    {
        // Plan radius per id — the BUILT footprint, not the nominal size: a
        // "4 m clump" whose parts are a rock one side and a rotted post the
        // other reaches 2.4 m from its origin. The open-grown oak is a round
        // plan (crown r 7.5), measured from its centre less radius rather than
        // as a box whose corners reached r√2.
        static float Reach(string id) => id switch
        {
            "toaster_mailbox" => 0.4f,
            "toaster_fence_wood" => 2.1f,
            "toaster_fence_wire" => 2.1f,
            "toaster_gate_farm" => 2.2f,
            "toaster_hay_bale" => 0.8f,
            "toaster_understory" => 2.4f,
            "toaster_leaf_pile" => 1.5f,
            "toaster_terrain_scatter" => 2.4f,
            "toaster_hedgerow" => 2.2f,
            "toaster_tree_oak_open" => 7.6f,
            "toaster_tree_apple" => 2.5f,
            "toaster_crop_rows" => 7.1f,
            "toaster_reeds" => 1.1f,
            "toaster_utility_pole" or "toaster_utility_pole_v1" => 1.2f,
            "toaster_propane_tank" => 1.6f,
            "toaster_woodpile" => 1.5f,
            "toaster_wreck_pickup" => 2.5f,
            "toaster_dock" => 2.8f,
            "toaster_grain_bin" => 3.3f,
            "toaster_grain_bin_v1" => 4.2f,
            "toaster_fuel_tank" => 1.5f,
            "toaster_windmill" => 2.0f,
            "toaster_stock_tank" => 1.3f,
            "toaster_bale_feeder" => 1.3f,
            "toaster_shed_small" => 2.2f,
            "toaster_garden_plot" => 5.4f,
            "toaster_clothesline" => 4.1f,
            "toaster_hay_wagon" => 3.8f,
            _ => 1f,
        };
        static bool Verge(string id) => id is "toaster_mailbox" or "toaster_fence_wood" or "toaster_fence_wire"
            or "toaster_gate_farm" or "toaster_utility_pole" or "toaster_utility_pole_v1" or "toaster_hedgerow";

        var rows = new Dictionary<string, List<Transform3D>>();
        int placed = 0, refused = 0;

        // Inside a building, allowing for the piece's own size: the registered
        // footprints already carry a 2 m margin, so that is taken back before
        // the piece's radius and the metre go on.
        bool InsideBuilding(Vector3 at, float reach)
        {
            foreach (var footprint in _footprints)
                if (footprint.Grow(reach + 1f - 2f).HasPoint(new Vector2(at.X, at.Z))) return true;
            return false;
        }

        bool Clear(string id, Vector3 at, bool oneOff = false)
        {
            float reach = Reach(id), margin = 3f + reach + 0.2f;
            bool blocked = Blocked(at, margin, outsideBuildings: false)
                || InsideBuilding(at, reach);
            bool onRoad = Surfaces.NearRoad(at, Verge(id) ? 0f : 2f);
            if (blocked || onRoad)
            {
                refused++;
                // A row losing a few members is the rule working; a one-off
                // that vanishes is a placement that is wrong, so it is named.
                if (oneOff) GD.Print($"[map] dressing refused: {id} at ({at.X:0},{at.Z:0}) — "
                    + (onRoad ? "on a road" : "inside a clearance"));
                return false;
            }
            placed++;
            return true;
        }
        bool Row(string id, float x, float z, float yaw)
        {
            var at = new Vector3(x, 0, z);
            if (!Clear(id, at)) return false;
            if (!rows.TryGetValue(id, out var list)) rows[id] = list = new List<Transform3D>();
            list.Add(new Transform3D(Basis.FromEuler(new Vector3(0, yaw, 0)), at));
            return true;
        }
        Node3D? One(string id, float x, float z, float yaw)
        {
            var at = new Vector3(x, 0, z);
            if (!Clear(id, at, oneOff: true)) return null;
            return MapKit.Prop(this, id, at, Mathf.RadToDeg(yaw));
        }
        // A fence run from a to b in 4 m modules, posts landing on module ends,
        // a gate in place of the modules `gates` names.
        void Fence(string id, Vector2 a, Vector2 b, params int[] gates)
        {
            var d = b - a;
            float length = d.Length();
            int n = Mathf.Max(1, Mathf.RoundToInt(length / 4f));
            float yaw = Mathf.Atan2(-d.Y, d.X);
            for (int i = 0; i < n; i++)
            {
                float t = (i + 0.5f) / n;
                Row(gates.Contains(i) ? "toaster_gate_farm" : id, a.X + d.X * t, a.Y + d.Y * t, yaw);
            }
        }

        // Mailboxes at the three road heads, on the verge: design's first
        // numbers put two of them a metre and a half from the centreline,
        // which its own kerb rule refuses as much as ours does.
        // Solved against the sockets, stations, lanes and kerbs rather than
        // eyeballed: the junction of the county road and the barn drive, the
        // head of the barn drive, and the island in the middle of the
        // circular drive — the only verge at that junction that is not
        // inside a build pad's clearance.
        One("toaster_mailbox", -50f, 42f, 1.57f);
        One("toaster_mailbox", -86f, 48f, 0.14f);
        One("toaster_mailbox", 25f, 7f, -0.34f);
        // Post-and-rail both sides of the shed drive — the one dressed fence.
        for (int i = 0; i < 8; i++) Row("toaster_fence_wood", -88f + i * 4f, 41.5f, 0.14f);
        for (int i = 0; i < 9; i++) Row("toaster_fence_wood", -88f + i * 4f, 53f, 0.14f);

        // Field lines: wire on T-posts. The south-west pasture (mill, tank,
        // feeders), the orchard plot beside it, the crop field north of the
        // core yard, and the hayfield east of the spawn — four enclosures
        // that turn 320 m of lawn into a property. Gates face the yard each
        // field is worked from.
        Fence("toaster_fence_wire", new Vector2(-118, -60), new Vector2(-24, -60));
        Fence("toaster_fence_wire", new Vector2(-24, -60), new Vector2(-24, -28), 4);
        Fence("toaster_fence_wire", new Vector2(-24, -28), new Vector2(-118, -28), 12);
        Fence("toaster_fence_wire", new Vector2(-118, -28), new Vector2(-118, -60));
        Fence("toaster_fence_wire", new Vector2(-146, -36), new Vector2(-122, -36), 3);
        Fence("toaster_fence_wire", new Vector2(-122, -36), new Vector2(-122, -60));
        Fence("toaster_fence_wire", new Vector2(-41, 7), new Vector2(-9, 7), 4);
        Fence("toaster_fence_wire", new Vector2(-9, 7), new Vector2(-9, 49));
        Fence("toaster_fence_wire", new Vector2(-9, 49), new Vector2(-41, 49));
        Fence("toaster_fence_wire", new Vector2(-41, 49), new Vector2(-41, 7));
        Fence("toaster_fence_wire", new Vector2(85, -16), new Vector2(85, 62), 9);
        Fence("toaster_fence_wire", new Vector2(85, 62), new Vector2(146, 62));
        Fence("toaster_fence_wire", new Vector2(146, 62), new Vector2(146, -16));
        Fence("toaster_fence_wire", new Vector2(146, -16), new Vector2(85, -16), 7);

        // Hedgerow along the hayfield frontage, with open-grown oaks standing in it.
        for (int z = -12; z <= 58; z += 4)
            Row("toaster_hedgerow", 82f, z, Mathf.Pi / 2f + ((z / 4) % 2 != 0 ? 0.05f : -0.05f));
        foreach (var (x, z, yaw) in new[] { (86f, -10f, 0.3f), (82f, 40f, 2.2f), (82f, 58f, 0.8f) })
            Row("toaster_tree_oak_open", x, z, yaw);
        // Specimen oaks: in the pasture, over the pond, at the road, by the
        // shed. One at a time, never by the belt lattice.
        foreach (var (x, z, yaw) in new[]
        {
            (-30f, -35f, 0.4f), (-48f, -52f, 1.7f), (-100f, -44f, 2.6f), (-72f, -57f, 0.9f),
            (-20f, 61f, 1.3f), (-75f, 60f, 2.1f), (112f, -30f, 0.5f), (38f, -40f, 2.9f),
        }) Row("toaster_tree_oak_open", x, z, yaw);
        // Orchard: four rows of four on 6 m centres, south of the Buggy house.
        for (int c = 0; c < 4; c++)
            for (int r = 0; r < 4; r++)
                Row("toaster_tree_apple", -143f + c * 6f, -57f + r * 6f, c * 1.3f + r * 0.7f);
        // Row crop: twelve 10 m modules inside the wire north of the core yard.
        for (int c = 0; c < 3; c++)
            for (int r = 0; r < 4; r++)
                Row("toaster_crop_rows", -35f + c * 10f, 13f + r * 10f, 0f);
        // Round bales through the hayfield on a spiral, so they do not read as a grid.
        for (int i = 0; i < 26; i++)
        {
            float r = i * 2.399f;
            Row("toaster_hay_bale",
                110f + Mathf.Cos(r) * (6f + (i % 5) * 5.5f),
                16f + Mathf.Sin(r) * (6f + (i % 4) * 5f), r);
        }
        // Reeds on the pond's wet shelf — not on the dock side, not where the lane passes.
        foreach (float a in new[] { 0.35f, 0.95f, 1.6f, 3.55f, 4.25f, 4.95f, 5.6f, 6.05f })
            Row("toaster_reeds", ToasterLayout.PondCentre.X + Mathf.Cos(a) * 16.2f,
                ToasterLayout.PondCentre.Z + Mathf.Sin(a) * 16.2f, a);

        // Understory and cut-grass drift along the inner edge of the treeline.
        for (int i = 0; i < 18; i++)
        {
            float side = i % 2 == 1 ? 1f : -1f, t = (i * 0.137f) % 1f;
            Row("toaster_understory", -148f + t * 296f, side * (66f - (i % 3) * 3f), i * 0.7f);
        }
        for (int i = 0; i < 12; i++)
        {
            float side = i % 2 == 1 ? 1f : -1f, t = (i * 0.211f + 0.07f) % 1f;
            Row("toaster_leaf_pile", -146f + t * 292f, side * (64f - (i % 4) * 4f), i * 1.1f);
        }
        // Ground scatter: the detail the terrain tile is forbidden from
        // carrying, on a hashed lattice across the whole field and filtered
        // like the rest.
        for (int i = 0; i < 64; i++)
        {
            float x = -140f + (i * 53) % 280 + (i * 7) % 11 - 5f;
            float z = -62f + (i * 29) % 124 + (i * 3) % 7 - 3f;
            Row("toaster_terrain_scatter", x, z, i * 0.7f);
        }

        // The one-offs. Each tank and woodpile 3 m off the wall it serves,
        // OUTSIDE the footprint: design's first four were inside a building.
        One("toaster_propane_tank", 44f, 47.5f, 0f);
        One("toaster_propane_tank", -146.4f, -12f, 1.57f);
        One("toaster_propane_tank", 105f, -50f, 1.57f);
        One("toaster_woodpile", -120f, 40f, 0.2f);
        One("toaster_woodpile", 106f, -42f, -1.3f);
        One("toaster_wreck_pickup", -128f, 63f, 1.2f);
        One("toaster_dock", 8f, -30f, -1.1f);

        // The working farm: bins and fuel by the shed; the mill, its tank and
        // two feeders in the pasture; a hen house and a garden shed; gardens
        // and washing behind the two houses that are lived in; the wagon
        // where the baler stopped.
        One("toaster_grain_bin_v1", -137f, 50f, 0.4f);
        One("toaster_grain_bin", -137f, 59.5f, 1.1f);
        One("toaster_fuel_tank", -127f, 38f, 0f);
        ArmSpin(One("toaster_windmill", -85f, -46f, 0f));
        One("toaster_stock_tank", -81f, -46f, 0f);
        One("toaster_bale_feeder", -60f, -50f, 0f);
        One("toaster_bale_feeder", -100f, -36f, 0.8f);
        One("toaster_shed_small", 110f, -58f, Mathf.Pi);
        One("toaster_shed_small", 36f, 52f, Mathf.Pi);
        One("toaster_garden_plot", 26f, 53f, 0f);
        One("toaster_garden_plot", -134f, -30f, 0f);
        One("toaster_clothesline", 14f, 50f, 0f);
        One("toaster_clothesline", 112f, -48f, Mathf.Pi / 2f);
        One("toaster_hay_wagon", 100f, 44f, 0.3f);

        // Power lines: poles down the county road and the south road, every
        // third one carrying a transformer, each through the same clearance
        // test at the kerb. The conductors are strung between the poles that
        // PASSED, so a refused pole shortens a span rather than leaving wire
        // over nothing.
        var lines = new List<List<(Vector3 At, float Yaw)>>();
        foreach (var (points, spacing, offset) in ToasterLayout.PoleLines)
        {
            var line = new List<(Vector3, float)>();
            float carry = spacing * 0.5f;
            int k = 0;
            for (int i = 0; i < points.Length - 1; i++)
            {
                float ax = points[i].X, az = points[i].Z;
                float dx = points[i + 1].X - ax, dz = points[i + 1].Z - az;
                float length = Mathf.Sqrt(dx * dx + dz * dz);
                if (length < 0.01f) continue;
                float nx = dz / length, nz = -dx / length;
                float yaw = Mathf.Atan2(-nz, nx);
                float s = carry;
                for (; s < length; s += spacing, k++)
                {
                    float t = s / length;
                    float x = ax + dx * t + nx * offset, z = az + dz * t + nz * offset;
                    if (Mathf.Abs(x) > 152f || Mathf.Abs(z) > 76f) continue;
                    string pole = k % 3 == 1 ? "toaster_utility_pole_v1" : "toaster_utility_pole";
                    if (Row(pole, x, z, yaw)) line.Add((new Vector3(x, 0, z), yaw));
                }
                carry = s - length;
            }
            if (line.Count > 1) lines.Add(line);
        }

        int drawn = 0;
        foreach (var (id, list) in rows)
            drawn += id is "toaster_tree_oak_open" or "toaster_tree_apple"
                // Trees: the full model near, the billboard beyond 90 m, chunked
                // so each cell switches on its own — the belt's rule.
                ? MapKit.InstancedChunked(this, id, list, 40f, castShadow: true, visibleTo: 90f)
                  + MapKit.InstancedChunked(this, $"{id}_lod1", list, 40f, castShadow: false, visibleFrom: 90f)
                : MapKit.InstancedChunked(this, id, list, 80f,
                    castShadow: id is not ("toaster_leaf_pile" or "toaster_terrain_scatter" or "toaster_crop_rows"));
        drawn += BuildToasterWires(lines);
        GD.Print($"[map] dressing: {placed} placed, {refused} refused for clearance, {rows.Count} instanced set(s) in {drawn} multimesh(es), {lines.Sum(l => l.Count)} poles on {lines.Count} line(s)");
    }

    /// <summary>Three conductors between consecutive placed poles, with sag.
    /// Attachment points are the pole's own: outer insulators ±0.95 m along
    /// the crossarm at 9.6, the pin at the pole top at 10.2. Sag follows span
    /// (w·L²/8T, clamped), so a 36 m span dips about 0.9 m and nothing dips
    /// into a lane's 5.5 m headroom. One multimesh of a unit box for the whole
    /// map — eighteen segments a span, and the map has forty spans.</summary>
    private int BuildToasterWires(List<List<(Vector3 At, float Yaw)>> lines)
    {
        var segments = new List<Transform3D>();
        foreach (var line in lines)
            for (int i = 0; i < line.Count - 1; i++)
            {
                var (a, ay) = line[i];
                var (b, by) = line[i + 1];
                float span = new Vector2(b.X - a.X, b.Z - a.Z).Length();
                float sag = Mathf.Min(1.8f, 0.00065f * span * span + 0.25f);
                foreach (var (off, y) in new[] { (-0.95f, 9.6f), (0.95f, 9.6f), (0f, 10.2f) })
                {
                    // A yaw turns local +X to (cos, 0, -sin): the crossarm.
                    var from = new Vector3(a.X + Mathf.Cos(ay) * off, y, a.Z - Mathf.Sin(ay) * off);
                    var to = new Vector3(b.X + Mathf.Cos(by) * off, y, b.Z - Mathf.Sin(by) * off);
                    var prev = from;
                    for (int k = 1; k <= 6; k++)
                    {
                        float t = k / 6f;
                        var p = from.Lerp(to, t) + new Vector3(0, -sag * 4f * t * (1f - t), 0);
                        var d = p - prev;
                        var basis = Basis.LookingAt(d.Normalized(), Vector3.Up);
                        basis = new Basis(basis.X, basis.Y, basis.Z * d.Length());
                        segments.Add(new Transform3D(basis, (prev + p) * 0.5f));
                        prev = p;
                    }
                }
            }
        if (segments.Count == 0) return 0;

        var multi = new MultiMesh
        {
            TransformFormat = MultiMesh.TransformFormatEnum.Transform3D,
            Mesh = new BoxMesh
            {
                Size = new Vector3(0.032f, 0.032f, 1f),
                Material = new StandardMaterial3D { AlbedoColor = new Color(0.12f, 0.11f, 0.10f), Roughness = 0.7f },
            },
            InstanceCount = segments.Count,
        };
        for (int i = 0; i < segments.Count; i++) multi.SetInstanceTransform(i, segments[i]);
        AddChild(new MultiMeshInstance3D
        {
            Multimesh = multi,
            Name = "toaster_wires",
            CastShadow = GeometryInstance3D.ShadowCastingSetting.Off,
        });
        return 1;
    }

    /// <summary>The made roads: a flat strip per segment, dressed with the 4 m
    /// road module exactly as an enemy lane is dressed. Layer 0 — a road is
    /// something you drive on, not something you bump into, and the ground slab
    /// underneath is what holds anything up. A road wider than its module is
    /// laid in rows across it: the shed apron is 8 m of the same gravel the
    /// drive is made of, not a wider module, and sits 15 mm under the drive
    /// that crosses it so the two do not fight.</summary>
    private void BuildToasterRoads()
    {
        foreach (var road in ToasterLayout.Roads)
        {
            string asset = road.Kind == Surface.Asphalt
                ? "toaster_road_asphalt" : "toaster_road_gravel";
            var color = road.Kind == Surface.Asphalt
                ? new Color(0.18f, 0.18f, 0.2f) : new Color(0.55f, 0.5f, 0.42f);
            float moduleWidth = AssetManifest.Width(asset, road.Kind == Surface.Asphalt ? 6f : 4f);
            int rowCount = Mathf.Max(1, Mathf.RoundToInt(road.Width / moduleWidth));
            // Asphalt sits a centimetre proud of gravel so their junction
            // is a road crossing a drive rather than z-fighting.
            float height = road.Kind == Surface.Asphalt ? 0.05f : 0.04f;
            if (road.Apron) height -= 0.015f;

            for (int i = 0; i < road.Points.Length - 1; i++)
            {
                var a = road.Points[i];
                var b = road.Points[i + 1];
                var run = b - a;
                float span = run.Length();
                if (span < 1f) continue;
                var across = new Vector3(-run.Z, 0, run.X).Normalized();

                for (int q = 0; q < rowCount; q++)
                {
                    var offset = across * ((q - (rowCount - 1) * 0.5f) * moduleWidth);
                    var strip = AddStaticBox((a + b) * 0.5f + offset + new Vector3(0, height * 0.5f, 0),
                        new Vector3(span, height, moduleWidth), color, layer: 0);
                    strip.Rotation = new Vector3(0, Mathf.Atan2(-run.Z, run.X), 0);
                    MapKit.MountRun(strip, asset, span, 4f, alongX: true, MapKit.GroundLocal(strip));
                }
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
                if (Surfaces.At(at) != Surface.Grass || Surfaces.NearRoad(at, 2f)) continue;
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
