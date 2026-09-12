using Godot;
using System.Collections.Generic;
using System.Linq;

namespace DeepField.Game;

/// <summary>Mounts design's environment kit onto the existing graybox.
///
/// The load-bearing rule: **the graybox stays**. Every `AddStaticBox` keeps its
/// collision shape, its layer and its Area3D children exactly as they were —
/// we only hide its box mesh and hang the real geometry off the same node. So
/// the world looks completely different and the player walks on precisely the
/// same surfaces, which is what keeps the harness's baked socket→route
/// visibility honest.
///
/// Kit pieces are modular tiles pivoted at their own base and authored at true
/// world height (a deck segment's mesh already sits at y≈6). They therefore
/// mount at world y = 0 regardless of where the graybox box's centre is —
/// <see cref="GroundLocal"/> does that conversion.</summary>
public static class MapKit
{
    /// <summary>Local Y that puts a kit piece's base on world zero.</summary>
    public static float GroundLocal(Node3D body) => -body.Position.Y;

    /// <summary>Hides the graybox mesh without touching collision.</summary>
    public static void HideBox(Node3D body)
    {
        foreach (var child in body.GetChildren().OfType<MeshInstance3D>())
            child.Visible = false;
    }

    /// <summary>One piece, centred on the body. Returns false (leaving the
    /// graybox visible) when design hasn't shipped that asset.</summary>
    public static bool Mount(Node3D body, string asset, float localY, float yawDegrees = 0f,
        Vector3? scale = null)
    {
        var piece = AssetLibrary.TryInstantiate(asset);
        if (piece is null) return false;

        piece.Position = new Vector3(0, localY, 0);
        piece.RotationDegrees = new Vector3(0, yawDegrees, 0);
        if (scale is { } s) piece.Scale = s;
        body.AddChild(piece);
        HideBox(body);
        return true;
    }

    /// <summary>Tiles a piece along the body's local X (or Z) to cover a span —
    /// decks, walls, roadway, catwalks. Always lays a whole number of pieces
    /// and centres the run, so a 28 m deck reads as deck segments rather than
    /// one stretched slab.</summary>
    public static bool MountRun(Node3D body, string asset, float span, float pieceLength,
        bool alongX, float localY, float yawDegrees = 0f, params string[] everyFifth)
    {
        if (AssetLibrary.TryInstantiate(asset) is not { } probe) return false;
        probe.QueueFree();

        int count = Mathf.Max(1, Mathf.RoundToInt(span / pieceLength));
        float step = span / count;
        float start = -span * 0.5f + step * 0.5f;

        for (int i = 0; i < count; i++)
        {
            var piece = AssetLibrary.TryInstantiate(asset);
            if (piece is null) break;
            float offset = start + step * i;
            piece.Position = alongX
                ? new Vector3(offset, localY, 0)
                : new Vector3(0, localY, offset);
            piece.RotationDegrees = new Vector3(0, yawDegrees, 0);
            body.AddChild(piece);
            ThinOut(piece, i, everyFifth);
        }
        HideBox(body);
        return true;
    }

    /// <summary>Keeps a named part on one piece in five and hides it on the
    /// rest.
    ///
    /// A tiling module may only contain features that are true at its own
    /// repeat distance, and these modules are four metres long. Switchyard's
    /// lane module carries a mile-marker post and the freight cut carries a
    /// lamp post, both drawn as the once-in-a-while detail they would be on a
    /// real line — so laid end to end they became a marker post every four
    /// metres along every road and a lamp every four metres down the cutting.
    /// One in five is twenty metres, which is the spacing those things want.
    /// The real fix is design shipping them as separate props; this is what
    /// the delivered module allows in the meantime.</summary>
    public static void ThinOut(Node3D piece, int index, params string[] names)
    {
        if (names.Length == 0 || index % 5 == 0) return;
        foreach (string name in names) HideNamed(piece, name);
    }

    /// <summary>Hides every descendant with a given name.    /// <summary>Hides every descendant with a given name. Design's models name
    /// their parts, which is what lets a placement borrow a model and leave one
    /// piece of it out — the yard lane wants the ballast and the sleeper kerbs
    /// but not the rail down the middle.</summary>
    public static void HideNamed(Node root, string name)
    {
        if (root is Node3D node && node.Name.ToString() == name) node.Visible = false;
        foreach (var child in root.GetChildren()) HideNamed(child, name);
    }

    /// <summary>Yaw that lays a piece's local +X along a direction. MountRun and
    /// the lane modules run along X; YawTowards aligns +Z, so it is the wrong
    /// one for track.</summary>
    public static float YawAlongX(Vector3 direction)
        => Mathf.RadToDeg(Mathf.Atan2(-direction.Z, direction.X));

    /// <summary>A piece scaled to span two points — the zipline cable, whose
    /// model is a one-metre unit meant to be stretched.</summary>
    public static Node3D? MountSpan(Node3D parent, string asset, Vector3 from, Vector3 to)
    {
        var piece = AssetLibrary.TryInstantiate(asset);
        if (piece is null) return null;

        var delta = to - from;
        piece.Position = from;
        piece.LookAtFromPosition(from, to, Vector3.Up);
        piece.Scale = new Vector3(1, 1, delta.Length());
        parent.AddChild(piece);
        return piece;
    }

    /// <summary>Draws one asset many times as a MultiMesh instead of as many
    /// copies of its scene.
    ///
    /// Every placement in this client up to now has been a node subtree per
    /// copy. Switchyard's ground is twenty-four copies of a two-hundred-part
    /// tile: five thousand nodes and a draw call per part per copy per shadow
    /// cascade, for one flat field. That was affordable on 110 by 80. The
    /// Toaster is six times the area and wants fifteen hundred trees, and the
    /// same mechanism would be twenty-eight thousand nodes before a single one
    /// of them was drawn.
    ///
    /// Two things follow from how MultiMesh works, and both are why the asset
    /// brief carries part budgets rather than a request to keep things light:
    /// a MultiMeshInstance3D draws exactly one Mesh, so a file with N parts
    /// costs N of them; and culling, visibility ranges and mesh LOD all act on
    /// the instance's whole bounding box, so anything spread across a map has
    /// to be chunked or it never culls. Use <see cref="InstancedChunked"/> for
    /// those.
    ///
    /// Returns how many multimeshes it made, or 0 when the asset is missing —
    /// in which case nothing is drawn, the same graceful nothing a missing prop
    /// leaves behind.</summary>
    public static int Instanced(Node parent, string asset, IReadOnlyList<Transform3D> at,
        bool castShadow = true, float visibleFrom = 0f, float visibleTo = 0f,
        params string[] hide)
    {
        if (at.Count == 0) return 0;
        var prototype = AssetLibrary.TryInstantiate(asset);
        if (prototype is null) return 0;

        // Named parts come out on the prototype, before anything is measured,
        // which is what lets a placement borrow a model and leave a piece of it
        // behind — the yard's ground wants the ballast and not the two sidings
        // baked into it. Hiding per copy is impossible once the copies are one
        // multimesh, so it has to happen here or not at all.
        foreach (string name in hide) HideNamed(prototype, name);

        // Parented so the parts' global transforms resolve, then measured
        // relative to the prototype's own origin and thrown away.
        parent.AddChild(prototype);
        var toLocal = prototype.GlobalTransform.AffineInverse();

        var byMesh = new List<(Mesh Mesh, List<Transform3D> Offsets)>();
        foreach (var node in prototype.FindChildren("*", nameof(MeshInstance3D), true, false))
        {
            if (node is not MeshInstance3D piece || piece.Mesh is null) continue;
            if (!piece.IsVisibleInTree()) continue;

            // A MultiMesh draws the Mesh and nothing else: a material the
            // import hung on the MeshInstance as an override is not part of
            // the Mesh, so it has to be baked into a copy or the instanced
            // trees come out in the default grey. Only ArrayMesh can carry
            // one; a primitive mesh already has its material on the resource.
            var mesh = piece.Mesh;
            if (mesh is ArrayMesh)
            {
                ArrayMesh? baked = null;
                for (int surface = 0; surface < mesh.GetSurfaceCount(); surface++)
                {
                    if (piece.GetSurfaceOverrideMaterial(surface) is not { } material) continue;
                    baked ??= (ArrayMesh)mesh.Duplicate();
                    baked.SurfaceSetMaterial(surface, material);
                }
                if (baked is not null) mesh = baked;
            }

            var offset = toLocal * piece.GlobalTransform;
            var slot = byMesh.FirstOrDefault(x => ReferenceEquals(x.Mesh, mesh));
            if (slot.Mesh is null) byMesh.Add((mesh, new List<Transform3D> { offset }));
            else slot.Offsets.Add(offset);
        }

        int made = 0;
        foreach (var (mesh, offsets) in byMesh)
        {
            var multi = new MultiMesh
            {
                TransformFormat = MultiMesh.TransformFormatEnum.Transform3D,
                Mesh = mesh,
                InstanceCount = at.Count * offsets.Count,
            };
            int i = 0;
            foreach (var placement in at)
                foreach (var offset in offsets)
                    multi.SetInstanceTransform(i++, placement * offset);

            var instance = new MultiMeshInstance3D
            {
                Multimesh = multi,
                Name = $"{asset}_mm{made}",
                CastShadow = castShadow
                    ? GeometryInstance3D.ShadowCastingSetting.On
                    : GeometryInstance3D.ShadowCastingSetting.Off,
            };
            if (visibleFrom > 0f) instance.VisibilityRangeBegin = visibleFrom;
            if (visibleTo > 0f) instance.VisibilityRangeEnd = visibleTo;
            if (visibleFrom > 0f || visibleTo > 0f)
            {
                // Dissolve rather than pop: a tree that swaps to its flat
                // stand-in in one frame is the most visible thing on the map.
                instance.VisibilityRangeFadeMode = GeometryInstance3D.VisibilityRangeFadeModeEnum.Self;
                instance.VisibilityRangeBeginMargin = visibleFrom > 0f ? 8f : 0f;
                instance.VisibilityRangeEndMargin = visibleTo > 0f ? 8f : 0f;
            }
            parent.AddChild(instance);
            made++;
        }

        prototype.QueueFree();
        return made;
    }

    /// <summary>As above, split into square cells so each one culls, ranges and
    /// picks its own level of detail. Anything spread across a whole map wants
    /// this; a terrain grid that is always half on screen does not.</summary>
    public static int InstancedChunked(Node parent, string asset, IReadOnlyList<Transform3D> at,
        float cellSize, bool castShadow = true, float visibleFrom = 0f, float visibleTo = 0f,
        params string[] hide)
    {
        var cells = new Dictionary<(int, int), List<Transform3D>>();
        foreach (var placement in at)
        {
            var key = (Mathf.FloorToInt(placement.Origin.X / cellSize),
                       Mathf.FloorToInt(placement.Origin.Z / cellSize));
            if (!cells.TryGetValue(key, out var list)) cells[key] = list = new List<Transform3D>();
            list.Add(placement);
        }

        int made = 0;
        foreach (var cell in cells.Values)
            made += Instanced(parent, asset, cell, castShadow, visibleFrom, visibleTo, hide);
        return made;
    }

    /// <summary>Free-standing dressing: no collision, never on a sightline the
    /// sim cares about. Placed straight into the world, not onto a graybox.</summary>
    public static Node3D? Prop(Node parent, string asset, Vector3 position, float yawDegrees = 0f,
        Vector3? scale = null)
    {
        var piece = AssetLibrary.TryInstantiate(asset);
        if (piece is null) return null;

        piece.Position = position;
        piece.RotationDegrees = new Vector3(0, yawDegrees, 0);
        if (scale is { } s) piece.Scale = s;
        parent.AddChild(piece);
        return piece;
    }

    /// <summary>Yaw that points a model's local −Z... no: local +Z along
    /// <paramref name="direction"/>. Every kit piece with a front (gates,
    /// kiosks, the core) is authored facing +Z, so this is how a piece gets
    /// turned to face down a lane instead of across it.</summary>
    public static float YawTowards(Vector3 direction)
        => Mathf.RadToDeg(Mathf.Atan2(direction.X, direction.Z));

    /// <summary>Stops a model casting shadows. The skybox is real geometry —
    /// a 700 m dome — and a shadow-casting sun inside it puts the entire map
    /// in shade, which is exactly as dark as it sounds.</summary>
    public static void NoShadow(Node node)
    {
        if (node is GeometryInstance3D geometry)
            geometry.CastShadow = GeometryInstance3D.ShadowCastingSetting.Off;
        foreach (var child in node.GetChildren()) NoShadow(child);
    }

    /// <summary>Renders a model from the inside. Design's skybox is a sphere
    /// whose faces point outward and whose material is back-facing in three.js
    /// — a sidedness glTF cannot carry, so the file arrives single-sided and
    /// Godot culls the whole dome from where the player stands. The sky simply
    /// wasn't there, and the procedural sky underneath looked plausible enough
    /// that nothing failed. Cull nothing on it; there is one dome, and its
    /// materials are duplicated so the import cache stays untouched.</summary>
    public static void SeenFromInside(Node node)
    {
        if (node is MeshInstance3D mesh)
        {
            for (int i = 0; i < mesh.GetSurfaceOverrideMaterialCount(); i++)
            {
                if (mesh.GetActiveMaterial(i) is not BaseMaterial3D material) continue;
                var inside = (BaseMaterial3D)material.Duplicate();
                inside.CullMode = BaseMaterial3D.CullModeEnum.Disabled;
                mesh.SetSurfaceOverrideMaterial(i, inside);
            }
        }
        foreach (var child in node.GetChildren()) SeenFromInside(child);
    }
}
