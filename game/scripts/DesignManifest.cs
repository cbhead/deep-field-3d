using Godot;
using System.Collections.Generic;

namespace DeepField.Game;

/// <summary>Design's receipt for the drop: bounds, triangle counts, rig nodes
/// and limits for every file it shipped, written beside the models by the
/// export (<c>game/assets/structures/manifest.json</c>, see
/// tools/design-export/).
///
/// One reader, because two would drift — <see cref="TowerRig"/> takes its rig
/// rows from here, and the enemy overheads take the delivered model's measured
/// height rather than guessing it from a graybox multiplier.</summary>
public static class DesignManifest
{
    private const string ManifestPath = "res://assets/structures/manifest.json";
    private static Godot.Collections.Array? _files;
    private static Dictionary<string, (Vector3 Min, Vector3 Max)>? _bounds;

    /// <summary>Every row design shipped. Empty when the file is missing or
    /// unreadable; each caller then falls back to its own defaults.</summary>
    public static Godot.Collections.Array Files => _files ??= Load();

    private static Godot.Collections.Array Load()
    {
        if (!FileAccess.FileExists(ManifestPath))
        {
            GD.Print($"[manifest] {ManifestPath} not found; design's measurements are unavailable");
            return new Godot.Collections.Array();
        }
        using var file = FileAccess.Open(ManifestPath, FileAccess.ModeFlags.Read);
        var json = new Json();
        if (file is null || json.Parse(file.GetAsText()) != Error.Ok)
        {
            GD.PushWarning($"[manifest] could not parse {ManifestPath}");
            return new Godot.Collections.Array();
        }
        var root = json.Data.AsGodotDictionary();
        return root.TryGetValue("files", out var files) ? files.AsGodotArray() : new Godot.Collections.Array();
    }

    /// <summary>The model's own bounding box in metres, as measured at export
    /// over its *visible* meshes. False when design has not shipped that name —
    /// the caller is looking at a graybox and should use its own numbers.</summary>
    public static bool TryBounds(string asset, out Vector3 min, out Vector3 max)
    {
        _bounds ??= LoadBounds();
        if (_bounds.TryGetValue(asset, out var box))
        {
            min = box.Min; max = box.Max;
            return true;
        }
        min = max = Vector3.Zero;
        return false;
    }

    private static Dictionary<string, (Vector3, Vector3)> LoadBounds()
    {
        var map = new Dictionary<string, (Vector3, Vector3)>();
        foreach (var entry in Files)
        {
            var row = entry.AsGodotDictionary();
            if (!row.TryGetValue("file", out var name)) continue;
            // Stage modules that add nothing carry "bounds": null.
            if (!row.TryGetValue("bounds", out var bounds) || bounds.VariantType != Variant.Type.Dictionary) continue;
            var box = bounds.AsGodotDictionary();
            if (!box.TryGetValue("min", out var mn) || !box.TryGetValue("max", out var mx)) continue;
            map[name.AsString().Replace(".glb", "")] = (Vec(mn.AsGodotArray()), Vec(mx.AsGodotArray()));
        }
        return map;
    }

    private static Vector3 Vec(Godot.Collections.Array a) => a.Count < 3
        ? Vector3.Zero
        : new Vector3((float)a[0].AsDouble(), (float)a[1].AsDouble(), (float)a[2].AsDouble());

    /// <summary>Static Godot resources must be released before the SceneTree
    /// tears down (see UiTheme.ReleaseCaches).</summary>
    public static void ReleaseCaches()
    {
        _files = null;
        _bounds = null;
    }
}
